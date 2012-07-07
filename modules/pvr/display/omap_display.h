/*
 * drivers/gpu/pvr/display/omap_display.h
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

#include <plat/vrfb.h>
#include <plat/display.h>
#include <linux/completion.h>

#ifndef __OMAP_DISPLAY_H_
#define __OMAP_DISPLAY_H_

/* Max overlay managers for virtual display */
#define OMAP_DISP_MAX_MANAGERS 2
/* 3 for triple buffering, 4 for virtual display */
#define OMAP_DISP_MAX_FLIPCHAIN_BUFFERS 4
#define OMAP_DISP_NUM_DISPLAYS 4 /* lcd, 2lcd, tv, virtual */

struct omap_display_device;

/* On OMAP 4 we can only manage 3 displays at the same time + virtual */
enum omap_display_id {
	OMAP_DISPID_PRIMARY = 1 << 0,
	OMAP_DISPID_SECONDARY = 1 << 1,
	OMAP_DISPID_TERTIARY = 1 << 2,
	OMAP_DISPID_VIRTUAL = 1 << 15, /* Multiple displays */
	OMAP_DISPID_BADSTATE = 1 << 30, /* Used to say a display is unusable*/
};

enum omap_display_pixel_format {
	RGB_565 = 0,
	ARGB_8888  = 1,
};

/* Primary display location for virtual display */
enum omap_display_feature {
	ORIENTATION_VERTICAL = 1 << 0,
	ORIENTATION_HORIZONTAL = 1 << 1,
	ORIENTATION_INVERT = 1 << 2,
};

struct omap_display_buffer {
	unsigned long physical_addr;
	unsigned long virtual_addr;
	unsigned long size;
	struct omap_display_device *display;
};

struct omap_display_flip_chain {
	int buffer_count;
	struct omap_display_buffer *buffers[OMAP_DISP_MAX_FLIPCHAIN_BUFFERS];
	struct omap_display_device *display;
};

struct omap_display_sync_item {
	struct omap_display_buffer *buffer;
	struct completion *task;
	int invalidate;
};

struct omap_display_device {
	char *name;
	enum omap_display_id id;
	enum omap_display_pixel_format pixel_format;
	enum omap_display_feature features;
	unsigned int width;
	unsigned int height;
	unsigned int bits_per_pixel;
	unsigned int bytes_per_pixel;
	unsigned int byte_stride;
	enum omap_dss_rotation_angle rotation;
	unsigned int reference_count;
	unsigned int buffers_available;
	struct omap_display_buffer *main_buffer;
	struct omap_display_flip_chain *flip_chain;
	struct omap_overlay_manager *overlay_managers[OMAP_DISP_MAX_MANAGERS];
	unsigned int overlay_managers_count;
	int (*open)(struct omap_display_device *display,
		enum omap_display_feature features);
	int (*close) (struct omap_display_device *display);
	int (*create_flip_chain) (struct omap_display_device *display,
		unsigned int buffer_count);
	int (*destroy_flip_chain) (struct omap_display_device *display);
	int (*rotate) (struct omap_display_device *display,
		enum omap_dss_rotation_angle rotation);
	int (*present_buffer) (struct omap_display_buffer *buffer);
	int (*sync) (struct omap_display_device *display);
	int (*present_buffer_sync) (struct omap_display_buffer *buffer);
};

int omap_display_init(void);
int omap_display_deinit(void);
int omap_display_count(void);
struct omap_display_device *omap_display_get(enum omap_display_id id);

#endif
