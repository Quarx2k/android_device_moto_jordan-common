/*
 * drivers/gpu/pvr/display/omap_display.c
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fb.h>

#include <plat/vrfb.h>
#include <plat/display.h>

/* Workaround for DEBUG macro clash in framebuffer */
#ifdef RELEASE
#include <../drivers/video/omap2/omapfb/omapfb.h>
#undef DEBUG
#else
#undef DEBUG
#include <../drivers/video/omap2/omapfb/omapfb.h>
#endif

#define OMAP_DISP_DRV_NAME "omap_display"
#define OMAP_DISP_FRAMEBUFFER_COUNT num_registered_fb

#define OMAP_DISP_PAGE_MASK (PAGE_SIZE - 1)
#define OMAP_DISP_PAGE_TRUNCATE (~OMAP_DISP_PAGE_MASK)
#define OMAP_DISP_PAGE_ROUND_UP(x) \
	(((x)+OMAP_DISP_PAGE_MASK) & OMAP_DISP_PAGE_TRUNCATE)

#define OMAP_DISP_IRQ_TIMEOUT 500

#ifdef DEBUG
#define DBG_PRINT(format, ...) printk(KERN_DEBUG OMAP_DISP_DRV_NAME \
	" (%s %i): " format "\n", __func__, __LINE__, ## __VA_ARGS__)
#define WRN_PRINT(format, ...) printk(KERN_WARNING OMAP_DISP_DRV_NAME \
	" (%s %i): " format "\n", __func__, __LINE__, ## __VA_ARGS__)
#define ERR_PRINT(format, ...) printk(KERN_ERR OMAP_DISP_DRV_NAME \
	" (%s %i): " format "\n", __func__, __LINE__, ## __VA_ARGS__)
#else
#define DBG_PRINT(format, ...)
#define WRN_PRINT(format, ...)
#define ERR_PRINT(format, ...)
#endif

#include "omap_display.h"

/* List for the available displays */
static struct omap_display_device *omap_display_list;
static unsigned int omap_display_number;

/* Forward declarations */
static struct omap_display_buffer *create_main_buffer(
	struct omap_display_device *display);
static int display_destroy_buffer(struct omap_display_buffer *buffer);

static int open_display(struct omap_display_device *display,
	enum omap_display_feature features)
{
	int i;

	DBG_PRINT("Opening display '%s'", display->name);

	/* TODO: Support horizontal orientation */
	if (features & ORIENTATION_HORIZONTAL) {
		DBG_PRINT("Horizontal orientation is not supported yet , "
			"falling back to vertical orientation");
		features = ORIENTATION_VERTICAL;
	}

	display->features = features;
	display->reference_count++;
	for (i = 0; i < display->overlay_managers_count; i++)
		omap_dss_get_device(display->overlay_managers[i]->device);

	/* If the main buffer doesn't exist create it */
	if (!display->main_buffer) {
		DBG_PRINT("Main buffer doesn't exist for display '%s', create"
			" one", display->name);
		display->main_buffer = create_main_buffer(display);
		if (!display->main_buffer) {
			ERR_PRINT("Failed to create main buffer for '%s'",
				display->name);
			return 1;
		}
	}

	return 0;
}

static int close_display(struct omap_display_device *display)
{
	int err;
	int i;

	/* TODO: Is it the same thing to close a virtual and single display? */
	DBG_PRINT("Closing display '%s'", display->name);

	display->reference_count--;
	for (i = 0; i < display->overlay_managers_count; i++)
		omap_dss_put_device(display->overlay_managers[i]->device);

	if (display->flip_chain) {
		err = display->destroy_flip_chain(display);
		display->flip_chain = 0;
		if (err)
			WRN_PRINT("An error happened when destroying flip "
				"chain for '%s'", display->name);
	}

	return 0;
}

static int get_max_buffers(struct omap_display_device *display)
{
	/* TODO: If TILER is wanted to be used how do you calculate this? */
	int fb_idx;
	switch (display->id) {
	case OMAP_DISPID_PRIMARY:
		fb_idx = 0;
		break;
	case OMAP_DISPID_SECONDARY:
		fb_idx = 1;
		break;
	case OMAP_DISPID_TERTIARY:
		fb_idx = 2;
		break;
	case OMAP_DISPID_VIRTUAL:
		fb_idx = 0;
		break;
	case OMAP_DISPID_BADSTATE:
	default:
		ERR_PRINT("Unknown display id %i", display->id);
		BUG();
	}

	/* Use the framebuffer memory */
	if (fb_idx >= 0 && fb_idx < num_registered_fb) {
		struct fb_info *framebuffer = registered_fb[fb_idx];
		unsigned long buffer_size;

		/* Single buffer size */
		buffer_size = display->width * display->height *
			display->bytes_per_pixel;
		/* Page align the buffer size, round up to the page size */
		buffer_size = OMAP_DISP_PAGE_ROUND_UP(buffer_size);

		return (int) (framebuffer->fix.smem_len / buffer_size);
	} else {
		ERR_PRINT("Framebuffer %i doesn't exist for display '%s'",
			fb_idx, display->name);
		return 0;
	}
}

static int create_flip_chain(struct omap_display_device *display,
	unsigned int buffer_count)
{
	int fb_idx;

	/* TODO: What about TILER buffers */
	if (buffer_count <= 1) {
		ERR_PRINT("Flip chains with %i buffers not supported",
			buffer_count);
		return 1;
	} else if (buffer_count > display->buffers_available) {
		ERR_PRINT("Requesting %i buffers when there is %i available"
			" for '%s'", buffer_count, display->buffers_available,
			display->name);
		return 1;
	} else if (display->flip_chain) {
		ERR_PRINT("Flip chain already exists for '%s'", display->name);
		return 1;
	}

	/* Create the flip chain with the framebuffer memory */
	switch (display->id) {
	case OMAP_DISPID_PRIMARY:
		fb_idx = 0;
		break;
	case OMAP_DISPID_SECONDARY:
		fb_idx = 1;
		break;
	case OMAP_DISPID_TERTIARY:
		fb_idx = 2;
		break;
	case OMAP_DISPID_VIRTUAL:
		fb_idx = 0;
		break;
	case OMAP_DISPID_BADSTATE:
	default:
		ERR_PRINT("Unknown display id %i", display->id);
		BUG();
	}

	/* Use the framebuffer memory */
	if (fb_idx >= 0 && fb_idx < num_registered_fb) {
		struct fb_info *framebuffer = registered_fb[fb_idx];
		unsigned long buffer_size;
		struct omap_display_flip_chain *flip_chain;
		int i;

		if (!framebuffer || !framebuffer->fix.smem_start ||
			!framebuffer->screen_base) {
			ERR_PRINT("Framebuffer %i doesn't seem to be "
				"initialized", fb_idx);
			return 1;
		}

		/*
		 * Check if there is enough memory in the fb for the requested
		 * buffers
		 */
		buffer_size = display->width * display->height *
			display->bytes_per_pixel;
		/* Page align the buffer size, round up to the page size */
		buffer_size = OMAP_DISP_PAGE_ROUND_UP(buffer_size);

		if (buffer_size * buffer_count > framebuffer->fix.smem_len) {
			ERR_PRINT("Not enough memory to allocate %i buffers "
				"(%lu bytes each), memory available %lu for "
				"display '%s'", buffer_count, buffer_size,
				(unsigned long)framebuffer->fix.smem_len,
				display->name);
			return 1;
		}

		flip_chain = kzalloc(sizeof(*flip_chain), GFP_KERNEL);

		if (!flip_chain) {
			ERR_PRINT("Out of memory");
			return 1;
		}

		for (i = 0; i < buffer_count; i++) {
			struct omap_display_buffer *buffer;

			/*
			 * Reuse the main buffer as the first buffer in the
			 * flip chain
			 */
			if (i == 0) {
				buffer = display->main_buffer;
				flip_chain->buffers[i] = buffer;
				DBG_PRINT("Flip chain buffer %i has address "
					"%lx for display '%s'", i,
					buffer->physical_addr, display->name);
				continue;
			}

			buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);

			if (!buffer) {
				/*
				 * FIXME: If one buffer allocation fails,
				 * deallocate flip chain and buffers
				 */
				ERR_PRINT("Out of memory");
				return 1;
			}

			buffer->physical_addr = framebuffer->fix.smem_start +
				(buffer_size * i);
			buffer->virtual_addr =
				(unsigned long) framebuffer->screen_base +
				(buffer_size * i);
			buffer->size = buffer_size;
			buffer->display = display;
			flip_chain->buffers[i] = buffer;

			DBG_PRINT("Flip chain buffer %i has address %lx for"
				" display '%s'", i, buffer->physical_addr,
				display->name);
		}

		display->flip_chain = flip_chain;
		return 0;
	} else {
		ERR_PRINT("Framebuffer %i doesn't exist for display '%s'",
			fb_idx, display->name);
		return 1;
	}

	return 0;
}

static int destroy_flip_chain(struct omap_display_device *display)
{
	int i;
	int err;

	if (!display->flip_chain) {
		DBG_PRINT("No flip chain to destroy for '%s'", display->name);
		return 0;
	}

	for (i = 0; i < display->flip_chain->buffer_count; i++) {
		struct omap_display_buffer *buffer =
			display->flip_chain->buffers[i];
		/* If buffer is main buffer don't touch it */
		if (display->main_buffer == buffer)
			continue;

		err = display_destroy_buffer(buffer);
		if (err) {
			ERR_PRINT("Error destroying buffer in flip chain for"
			" '%s'", display->name);
			return 1;
		}
	}

	DBG_PRINT("Destroying flip chain for '%s'", display->name);
	kfree(display->flip_chain);
	display->flip_chain = 0;

	return 0;
}

static int rotate_display(struct omap_display_device *display,
	unsigned int rotation)
{
	ERR_PRINT("Not supported yet");
	return 1;
}

static int display_destroy_buffer(struct omap_display_buffer *buffer)
{
	kfree(buffer);
	return 0;
}

static int present_buffer_virtual(struct omap_display_buffer *buffer)
{
	/*
	 * TODO: Support for ORIENTATION_VERTICAL is in place,
	 * ORIENTATION_HORIZONTAL is missing
	 */
	struct omap_display_device *display_virtual = buffer->display;
	struct omap_display_device *display_primary;
	struct omap_display_device *display_secondary;
	struct omap_display_buffer temp_buffer;
	unsigned int buffer_offset;

	if (display_virtual->id != OMAP_DISPID_VIRTUAL) {
		ERR_PRINT("Not a virtual display");
		BUG();
	}

	display_primary = omap_display_get(OMAP_DISPID_PRIMARY);
	display_secondary = omap_display_get(OMAP_DISPID_SECONDARY);
	/*
	 * Calculate offset without page alignment round up otherwise second
	 * display may see incorrect data
	 */
	buffer_offset = display_primary->height * display_virtual->byte_stride;

	/* The first buffer will be the base */
	temp_buffer.physical_addr = buffer->physical_addr;
	temp_buffer.virtual_addr = buffer->virtual_addr;
	temp_buffer.size = buffer->size >> 1;

	if (display_virtual->features & ORIENTATION_INVERT) {
		/* Secondary display has the base */
		temp_buffer.display = display_secondary;
		display_secondary->present_buffer(&temp_buffer);
	} else {
		/* Primary display has the base */
		temp_buffer.display = display_primary;
		display_primary->present_buffer(&temp_buffer);
	}

	/* Remaining display will show the rest */
	temp_buffer.physical_addr = buffer->physical_addr + buffer_offset;
	temp_buffer.virtual_addr = buffer->virtual_addr + buffer_offset;

	if (display_virtual->features & ORIENTATION_INVERT) {
		temp_buffer.display = display_primary;
		display_primary->present_buffer(&temp_buffer);
	} else {
		temp_buffer.display = display_secondary;
		display_secondary->present_buffer(&temp_buffer);
	}

	return 0;
}

static int present_buffer(struct omap_display_buffer *buffer)
{
	struct omap_display_device *display = buffer->display;
	int fb_idx;

	switch (display->id) {
	case OMAP_DISPID_PRIMARY:
		fb_idx = 0;
		break;
	case OMAP_DISPID_SECONDARY:
		fb_idx = 1;
		break;
	case OMAP_DISPID_TERTIARY:
		fb_idx = 2;
		break;
	case OMAP_DISPID_VIRTUAL:
	case OMAP_DISPID_BADSTATE:
	default:
		ERR_PRINT("Unable to handle display %i", display->id);
		BUG();
	}

	if (fb_idx >= 0 && fb_idx < num_registered_fb) {
		struct fb_info *framebuffer = registered_fb[fb_idx];
		struct omapfb_info *ofbi = FB2OFB(framebuffer);
		struct omapfb2_device *fbdev = ofbi->fbdev;
		struct omap_overlay *overlay;
		struct omap_overlay_info overlay_info;
		int i;

		omapfb_lock(fbdev);

		/* Get the overlays attached to the framebuffer */
		for (i = 0; i < ofbi->num_overlays ; i++) {
			overlay = ofbi->overlays[i];
			overlay->get_overlay_info(overlay, &overlay_info);

			/* If the overlay is not enabled don't update it */
			if (!overlay_info.enabled)
				continue;

			overlay_info.paddr = buffer->physical_addr;
			overlay_info.vaddr = (void *) buffer->virtual_addr;
			overlay->set_overlay_info(overlay, &overlay_info);

			if (overlay->manager) {
				overlay->manager->apply(overlay->manager);
				if (overlay->manager->device &&
					overlay->manager->device->update) {
					overlay->manager->device->update(
						overlay->manager->device,
						0, 0,
						overlay_info.width,
						overlay_info.height);
				}
			}
		}

		omapfb_unlock(fbdev);
	} else {
		ERR_PRINT("Framebuffer %i doesn't exist for display '%s'",
			fb_idx, display->name);
		return 1;
	}

	return 0;
}

static u32 get_dss_irq(struct omap_dss_device *dss_device)
{
	u32 irq;

	if (dss_device->type == OMAP_DISPLAY_TYPE_VENC)
		irq = DISPC_IRQ_EVSYNC_ODD;
	else if (dss_device->type == OMAP_DISPLAY_TYPE_HDMI)
		irq = DISPC_IRQ_EVSYNC_EVEN;
	else if (dss_device->type == OMAP_DISPLAY_TYPE_DSI)
		if (!strcmp(dss_device->name, "lcd"))
			irq = DISPC_IRQ_FRAMEDONE;
		else
			irq = DISPC_IRQ_FRAMEDONE2;
	else
		if (!strcmp(dss_device->name, "lcd"))
			irq = DISPC_IRQ_VSYNC;
		else
			irq = DISPC_IRQ_VSYNC2;

	return irq;
}

static int present_buffer_sync(struct omap_display_buffer *buffer)
{
	/* TODO: Cloning may tear with this implementation */
	int err = 0;
	struct omap_display_device *display = buffer->display;

	err = display->sync(display);
	err |= display->present_buffer(buffer);

	return err;
}

static int present_buffer_sync_virtual(struct omap_display_buffer *buffer)
{
	void display_irq_wait_1(void *data, u32 mask)
	{
		struct omap_display_sync_item *sync_item =
			(struct omap_display_sync_item *) data;

		if (sync_item->invalidate)
			return;

		/* IRQ happened, present immediately */
		sync_item->invalidate = 1;
		sync_item->buffer->display->present_buffer(sync_item->buffer);
		complete(sync_item->task);
	}

	void display_irq_wait_2(void *data, u32 mask)
	{
		struct omap_display_sync_item *sync_item =
			(struct omap_display_sync_item *) data;

		if (sync_item->invalidate)
			return;

		/* IRQ happened, present immediately */
		sync_item->invalidate = 1;
		sync_item->buffer->display->present_buffer(sync_item->buffer);
		complete(sync_item->task);
	}

	/*
	 * TODO: Support for ORIENTATION_VERTICAL is in place,
	 * ORIENTATION_HORIZONTAL is missing. Some code can be reduced here,
	 * it will be simplified in the future.
	 */
	struct omap_display_device *display_virtual = buffer->display;
	struct omap_display_device *display_primary;
	struct omap_display_device *display_secondary;
	struct omap_display_buffer temp_buffer_top;
	struct omap_display_buffer temp_buffer_bottom;
	struct omap_display_sync_item sync_item_primary;
	struct omap_display_sync_item sync_item_secondary;
	DECLARE_COMPLETION_ONSTACK(completion_primary);
	DECLARE_COMPLETION_ONSTACK(completion_secondary);
	unsigned int buffer_offset;

	if (display_virtual->id != OMAP_DISPID_VIRTUAL) {
		ERR_PRINT("Not a virtual display");
		BUG();
	}

	display_primary = omap_display_get(OMAP_DISPID_PRIMARY);
	display_secondary = omap_display_get(OMAP_DISPID_SECONDARY);
	/*
	 * Calculate offset without page alignment round up otherwise second
	 * display may see incorrect data
	 */
	buffer_offset = display_primary->height * display_virtual->byte_stride;

	/* The first buffer will be the top */
	temp_buffer_top.physical_addr = buffer->physical_addr;
	temp_buffer_top.virtual_addr = buffer->virtual_addr;
	temp_buffer_top.size = buffer->size >> 1;
	/* Then the bottom */
	temp_buffer_bottom.physical_addr = buffer->physical_addr +
		buffer_offset;
	temp_buffer_bottom.virtual_addr = buffer->virtual_addr + buffer_offset;
	temp_buffer_bottom.size = buffer->size >> 1;

	if (display_virtual->features & ORIENTATION_INVERT) {
		/* Secondary display has the base */
		temp_buffer_top.display = display_secondary;
		temp_buffer_bottom.display = display_primary;
		sync_item_primary.buffer = &temp_buffer_bottom;
		sync_item_secondary.buffer = &temp_buffer_top;

	} else {
		/* Primary display has the base */
		temp_buffer_top.display = display_primary;
		temp_buffer_bottom.display = display_secondary;
		sync_item_primary.buffer = &temp_buffer_top;
		sync_item_secondary.buffer = &temp_buffer_bottom;
	}

	sync_item_primary.task = &completion_primary;
	sync_item_secondary.task = &completion_secondary;
	sync_item_primary.invalidate = 0;
	sync_item_secondary.invalidate = 0;

	/* Register an ISR per display with its corresponding IRQ */
	omap_dispc_register_isr(display_irq_wait_1, &sync_item_primary,
		get_dss_irq(display_primary->overlay_managers[0]->device));

	omap_dispc_register_isr(display_irq_wait_2, &sync_item_secondary,
		get_dss_irq(display_secondary->overlay_managers[0]->device));

	/* Wait until each display sync and present */
	wait_for_completion_interruptible_timeout(&completion_primary,
			OMAP_DISP_IRQ_TIMEOUT);
	wait_for_completion_interruptible_timeout(&completion_secondary,
			OMAP_DISP_IRQ_TIMEOUT);

	/* Unregister ISRs */
	omap_dispc_unregister_isr(display_irq_wait_1, &sync_item_primary,
		get_dss_irq(display_primary->overlay_managers[0]->device));

	omap_dispc_unregister_isr(display_irq_wait_2, &sync_item_secondary,
		get_dss_irq(display_secondary->overlay_managers[0]->device));

	return 0;
}

static int display_sync(struct omap_display_device *display)
{
	/* TODO: Synchronize properly with multiple managers */
	struct omap_dss_device *dss_device =
		display->overlay_managers[0]->device;
	if (!dss_device || !dss_device->wait_vsync) {
		ERR_PRINT("Unable to synchronize with '%s'", display->name);
		return 1;
	}
	dss_device->wait_vsync(dss_device);
	return 0;
}

static int display_sync_virtual(struct omap_display_device *display_virtual)
{
	void display_irq_wait(void *data, u32 mask)
	{
		complete((struct completion *)data);
	}

	/*
	 * Return as soon as one display generates an IRQ
	 */
	struct omap_display_device *display_primary;
	struct omap_display_device *display_secondary;
	u32 irq_primary;
	u32 irq_secondary;
	u32 irq_mask;
	DECLARE_COMPLETION_ONSTACK(completion);

	if (display_virtual->id != OMAP_DISPID_VIRTUAL) {
		ERR_PRINT("Not a virtual display");
		BUG();
	}

	display_primary = omap_display_get(OMAP_DISPID_PRIMARY);
	display_secondary = omap_display_get(OMAP_DISPID_SECONDARY);

	irq_primary = get_dss_irq(
		display_primary->overlay_managers[0]->device);

	irq_secondary = get_dss_irq(
		display_secondary->overlay_managers[0]->device);

	irq_mask = irq_primary | irq_secondary;

	/* Register an ISR with both IRQs and wait, then unregister */
	omap_dispc_register_isr(display_irq_wait, &completion, irq_mask);

	wait_for_completion_interruptible_timeout(&completion,
		OMAP_DISP_IRQ_TIMEOUT);

	omap_dispc_unregister_isr(display_irq_wait, &completion, irq_mask);

	return 0;
}

static struct omap_display_buffer *create_main_buffer(
	struct omap_display_device *display)
{
	int fb_idx;
	switch (display->id) {
	case OMAP_DISPID_PRIMARY:
		fb_idx = 0;
		break;
	case OMAP_DISPID_SECONDARY:
		fb_idx = 1;
		break;
	case OMAP_DISPID_TERTIARY:
		fb_idx = 2;
		break;
	case OMAP_DISPID_VIRTUAL:
		/* Use fb0 for virtual display */
		fb_idx = 0;
		break;
	case OMAP_DISPID_BADSTATE:
	default:
		ERR_PRINT("Unknown display id %i", display->id);
		BUG();
	}

	/* Use the framebuffer memory */
	if (fb_idx >= 0 && fb_idx < num_registered_fb) {
		struct fb_info *framebuffer = registered_fb[fb_idx];
		unsigned long buffer_size;
		struct omap_display_buffer *buffer;

		if (!framebuffer || !framebuffer->fix.smem_start ||
			!framebuffer->screen_base) {
			ERR_PRINT("Framebuffer %i doesn't seem to be "
				"initialized", fb_idx);
			return NULL;
		}

		/*
		 * Check if there is enough memory in the fb for the
		 * main buffer
		 */
		buffer_size = display->width * display->height *
			display->bytes_per_pixel;
		/* Page align the buffer size */
		buffer_size = OMAP_DISP_PAGE_ROUND_UP(buffer_size);

		if (buffer_size > framebuffer->fix.smem_len) {
			ERR_PRINT("Main buffer needs %lu bytes while the "
				"framebuffer %i has only %lu bytes for display"
				" '%s'", buffer_size, fb_idx,
				(unsigned long)framebuffer->fix.smem_len,
				display->name);
			return NULL;
		}

		buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);

		if (!buffer) {
			ERR_PRINT("Out of memory");
			return NULL;
		}

		/* Use base addresses reported by the framebuffer */
		buffer->physical_addr = framebuffer->fix.smem_start;
		buffer->virtual_addr =
			(unsigned long) framebuffer->screen_base;
		buffer->size = buffer_size;
		buffer->display = display;

		DBG_PRINT("Created main buffer %lx for display '%s'",
			buffer->physical_addr, display->name);

		return buffer;
	} else {
		ERR_PRINT("Framebuffer %i doesn't exist for display '%s'",
			fb_idx, display->name);
		return NULL;
	}
}

static int populate_display_info(struct omap_display_device *display,
	struct omap_overlay_manager *overlay_manager)
{
	struct omap_dss_device *dss_device = overlay_manager->device;
	u16 xres;
	u16 yres;
	int i;

	if (!strcmp(dss_device->name, "lcd")) {
		display->id = OMAP_DISPID_PRIMARY;
		display->name = "primary";
	} else if (!strcmp(dss_device->name, "2lcd")) {
		display->id = OMAP_DISPID_SECONDARY;
		display->name = "secondary";
	} else if (!strcmp(dss_device->name, "hdmi")) {
		display->id = OMAP_DISPID_TERTIARY;
		display->name = "tertiary";
	} else {
		ERR_PRINT("Display id '%s' not supported", dss_device->name);
		return 1;
	}

	dss_device->get_resolution(dss_device, &xres, &yres);
	if (xres == 0 || yres == 0) {
		ERR_PRINT("Unable to handle display '%s' with width %i "
		"and height %i", dss_device->name, xres, yres);
		return 1;
	}

	display->width = xres;
	display->height = yres;

	display->bits_per_pixel = dss_device->get_recommended_bpp(dss_device);
	switch (display->bits_per_pixel) {
	case 16:
		/*
		 * TODO: Asume RGB_565, maybe need to double check in
		 * the DSS if this is true
		 */
		display->pixel_format = RGB_565;
		display->bytes_per_pixel = 2;
		break;
	case 24: /* 24 bits are encapsulated with 32 bits */
	case 32:
		/*
		 * TODO: Asume ARGB_8888, maybe need to double check in
		 * the DSS if this is true
		 */
		display->pixel_format = ARGB_8888;
		display->bytes_per_pixel = 4;
		break;
	default:
		ERR_PRINT("Unable to handle %i bpp", display->bits_per_pixel);
		return 1;
	}

	display->byte_stride = display->bytes_per_pixel * display->width;
	display->rotation = OMAP_DSS_ROT_0; /* Asume rotation 0 degrees */
	display->main_buffer = 0;
	display->flip_chain = 0;

	/* Add the manager to the list */
	for (i = 0; i < OMAP_DISP_MAX_MANAGERS; i++)
		display->overlay_managers[i] = 0;

	display->overlay_managers[0] = overlay_manager;
	display->overlay_managers_count = 1;

	/* Assign function pointers for display operations */
	display->open = open_display;
	display->close = close_display;
	display->create_flip_chain = create_flip_chain;
	display->destroy_flip_chain = destroy_flip_chain;
	display->rotate = rotate_display;
	display->present_buffer = present_buffer;
	display->sync = display_sync;
	display->present_buffer_sync = present_buffer_sync;

	display->main_buffer = create_main_buffer(display);
	if (!display->main_buffer)
		WRN_PRINT("Failed to create main buffer for '%s'",
			display->name);

	display->buffers_available = get_max_buffers(display);

	/* Just print some display info */
	DBG_PRINT("Found display '%s-%s' (%i,%i) %i bpp (%i bytes per pixel)"
		" rotation %i", display->name, dss_device->name,
		display->width, display->height, display->bits_per_pixel,
		display->bytes_per_pixel, display->rotation);

	return 0;
}

static int populate_virtual_display_info(struct omap_display_device *display)
{
	struct omap_display_device *display_primary ;
	struct omap_display_device *display_secondary;
	int i;

	display->id = OMAP_DISPID_VIRTUAL;
	display->name = "virtual";

	display_primary = omap_display_get(OMAP_DISPID_PRIMARY);
	display_secondary = omap_display_get(OMAP_DISPID_SECONDARY);

	if (!display_primary) {
		ERR_PRINT("Primary display doesn't exist");
		return 1;
	} else if (!display_secondary) {
		ERR_PRINT("Secondary display doesn't exist");
		return 1;
	}

	/* Combine primary and secondary display resolutions */
	if (display_primary->width != display_secondary->width ||
		display_primary->height != display_secondary->height) {
		ERR_PRINT("Primary and seconday displays resolution are not"
			" the same");
		return 1;
	}

	/*
	 * TODO: Here it is hardcoded the resolution asumming a vertical
	 * virtual config, what about horizontal?
	 */
	display->width = display_primary->width;
	display->height = display_primary->height * 2;

	if (display_primary->bits_per_pixel !=
		display_secondary->bits_per_pixel) {
		ERR_PRINT("Primary and seconday displays format are"
			" not the same");
		return 1;
	}

	display->bits_per_pixel = display_primary->bits_per_pixel;
	switch (display->bits_per_pixel) {
	case 16:
		/*
		 * TODO: Asume RGB_565, maybe need to double check in
		 * the DSS if this is true
		 */
		display->pixel_format = RGB_565;
		display->bytes_per_pixel = 2;
		break;
	case 24: /* 24 bits are encapsulated with 32 bits */
	case 32:
		/*
		 * TODO: Asume ARGB_8888, maybe need to double check in
		 * the DSS if this is true
		 */
		display->pixel_format = ARGB_8888;
		display->bytes_per_pixel = 4;
		break;
	default:
		ERR_PRINT("Unable to handle %i bpp",
			display->bits_per_pixel);
		return 1;
	}

	/* TODO: Asumming a vertical virtual config too for stride */
	display->byte_stride = display->bytes_per_pixel * display->width;
	display->rotation = OMAP_DSS_ROT_0; /* Asume rotation 0 degrees */
	display->main_buffer = 0;
	display->flip_chain = 0;

	/* Add the primary and secondary overlay managers */
	for (i = 0; i < OMAP_DISP_MAX_MANAGERS; i++)
		display->overlay_managers[i] = 0;

	display->overlay_managers[0] = display_primary->overlay_managers[0];
	display->overlay_managers[1] = display_secondary->overlay_managers[0];
	display->overlay_managers_count = 2;

	/* Assign function pointers for display operations */
	display->open = open_display;
	display->close = close_display;
	display->create_flip_chain = create_flip_chain;
	display->destroy_flip_chain = destroy_flip_chain;
	display->rotate = rotate_display;
	display->present_buffer = present_buffer_virtual;
	display->sync = display_sync_virtual;
	display->present_buffer_sync = present_buffer_sync_virtual;

	display->main_buffer = create_main_buffer(display);
	if (!display->main_buffer)
		WRN_PRINT("Failed to create main buffer for '%s'",
			display->name);

	display->buffers_available = get_max_buffers(display);

	/* Just print some display info */
	DBG_PRINT("Found display '%s' (%i,%i) %i bpp (%i bytes per pixel)"
		" rotation %i", display->name, display->width, display->height,
		display->bits_per_pixel, display->bytes_per_pixel,
		display->rotation);

	return 0;
}

static int create_display_list(void)
{
	int i;
	unsigned int bytes_to_alloc;
	struct omap_display_device *display;

	/* Query number of possible displays available first */
	omap_display_number = omap_dss_get_num_overlay_managers();
	/* For virtual display */
	omap_display_number++;

	/* Allocate custom display list */
	omap_display_list = kzalloc(
		sizeof(*display) * omap_display_number, GFP_KERNEL);

	if (!omap_display_list) {
		ERR_PRINT("Out of memory");
		return 1;
	}

	/* Populate each display info */
	for (i = 0; i < omap_display_number - 1; i++) {
		struct omap_overlay_manager *overlay_manager =
			omap_dss_get_overlay_manager(i);
		display = &omap_display_list[i];
		if (!overlay_manager->device) {
			WRN_PRINT("Display '%s' doesn't have a dss device "
				"attached to it, ignoring",
				overlay_manager->name);
			display->id = OMAP_DISPID_BADSTATE;
			continue;
		}
		if (populate_display_info(display, overlay_manager)) {
			ERR_PRINT("Error populating display %i info with "
				"manager '%s'", i,
				overlay_manager->device->name);
			display->id = OMAP_DISPID_BADSTATE;
			continue;
		}
	}

	/* Populate virtual display */
	display = &omap_display_list[omap_display_number - 1];
	if (populate_virtual_display_info(display)) {
		ERR_PRINT("Error populating virtual display info");
		display->id = OMAP_DISPID_BADSTATE;
	}

	return 0;
}

struct omap_display_device *omap_display_get(enum omap_display_id id)
{
	int i;
	struct omap_display_device *display;

	if (id == OMAP_DISPID_BADSTATE) {
		ERR_PRINT("Oops.. user must never request a bad display");
		BUG();
	}

	for (i = 0; i < omap_display_number; i++) {
		display = &omap_display_list[i];
		if (display->id == id)
			return display;
	}

	ERR_PRINT("Unknown display %i requested", id);
	return 0;
}
EXPORT_SYMBOL(omap_display_get);

int omap_display_count(void)
{
	return omap_display_number;
}
EXPORT_SYMBOL(omap_display_count);

int omap_display_init(void)
{
	/*
	 * TODO: Is there a better way to check if list is already created?
	 */
	if (!omap_display_list) {
		DBG_PRINT("Initializing driver");
		if (create_display_list()) {
			ERR_PRINT("Error loading driver");
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(omap_display_init);

int omap_display_deinit(void)
{
	int i;
	int err = 0;
	DBG_PRINT("Driver exiting");

	for (i = 0; i < omap_display_number; i++) {
		struct omap_display_device *display = &omap_display_list[i];

		if (!display)
			continue;

		if (display->main_buffer) {
			err = display_destroy_buffer(display->main_buffer);
			display->main_buffer = 0;
			if (err)
				WRN_PRINT("An error happened when destroying "
					"main buffer for '%s'", display->name);
		}

		err = display->close(display);

		if (err)
			ERR_PRINT("Unable to close display '%s'",
				display->name);
	}

	kfree(omap_display_list);
	omap_display_list = 0;

	return err;
}
EXPORT_SYMBOL(omap_display_deinit);

