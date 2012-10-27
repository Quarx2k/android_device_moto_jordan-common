/*
 *  linux/include/asm/setup.h
 *
 *  Copyright (C) 1997-1999 Russell King
 *  Copyright(C) 2006-2007 Motorola, Inc.
 *    
 * Date         Author          Comment
 * 10/2006      Motorola        Added Motorola specific ATAGs support.
 * 12/2006      Motorola        Added Motorola specific ATAGs support.
 * 05/2007      Motorola        Added Motorola specific ATAGs support.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Structure passed to kernel to tell it about the
 *  hardware it's running on.  See Documentation/arm/Setup
 *  for more info.
 */
#ifndef __ASMARM_SETUP_H
#define __ASMARM_SETUP_H

#include "types.h"

#define COMMAND_LINE_SIZE 1024

/* Magic number at the beginning of the serialized device tree. */
#define FLATTREE_BEGIN_SERIALIZED           0xD00DFEED
#define FLATTREE_BEGIN_SERIALIZED_OTHEREND  0xEDFE0DD0

/* The list ends with an ATAG_NONE node. */
#define ATAG_NONE	0x00000000

struct tag_header 
{
	u32 size;
	u32 tag;
};

/* The list must start with an ATAG_CORE node */
#define ATAG_CORE	0x54410001

struct tag_core 
{
	u32 flags;		/* bit 0 = read-only */
	u32 pagesize;
	u32 rootdev;
};

/* it is allowed to have multiple ATAG_MEM nodes */
#define ATAG_MEM	0x54410002

struct tag_mem32 
{
	u32	size;
	u32	start;	/* physical start address */
};

/* VGA text type displays */
#define ATAG_VIDEOTEXT	0x54410003

struct tag_videotext 
{
	u8		x;
	u8		y;
	u16		video_page;
	u8		video_mode;
	u8		video_cols;
	u16		video_ega_bx;
	u8		video_lines;
	u8		video_isvga;
	u16		video_points;
};

/* describes how the ramdisk will be used in kernel */
#define ATAG_RAMDISK	0x54410004

struct tag_ramdisk 
{
	u32 flags;	/* bit 0 = load, bit 1 = prompt */
	u32 size;	/* decompressed ramdisk size in _kilo_ bytes */
	u32 start;	/* starting block of floppy-based RAM disk image */
};

/* describes where the compressed ramdisk image lives (virtual address) */
/*
 * this one accidentally used virtual addresses - as such,
 * it's deprecated.
 */
#define ATAG_INITRD	0x54410005

/* describes where the compressed ramdisk image lives (physical address) */
#define ATAG_INITRD2	0x54420005

struct tag_initrd 
{
	u32 start;	/* physical start address */
	u32 size;	/* size of compressed ramdisk image in bytes */
};

/* board serial number. "64 bits should be enough for everybody" */
#define ATAG_SERIAL	0x54410006

struct tag_serialnr 
{
	u32 low;
	u32 high;
};

/* board revision */
#define ATAG_REVISION	0x54410007

struct tag_revision 
{
	u32 rev;
};

/* initial values for vesafb-type framebuffers. see struct screen_info
 * in include/linux/tty.h
 */
#define ATAG_VIDEOLFB	0x54410008

struct tag_videolfb 
{
	u16		lfb_width;
	u16		lfb_height;
	u16		lfb_depth;
	u16		lfb_linelength;
	u32		lfb_base;
	u32		lfb_size;
	u8		red_size;
	u8		red_pos;
	u8		green_size;
	u8		green_pos;
	u8		blue_size;
	u8		blue_pos;
	u8		rsvd_size;
	u8		rsvd_pos;
};

/* command line: \0 terminated string */
#define ATAG_CMDLINE	0x54410009

struct tag_cmdline 
{
	char	cmdline[1];	/* this is the minimum size */
};

/* acorn RiscPC specific information */
#define ATAG_ACORN	0x41000101

struct tag_acorn 
{
	u32 memc_control_reg;
	u32 vram_pages;
	u8 sounddefault;
	u8 adfsdrives;
};

/* TI OMAP specific information */
#define ATAG_OMAP       0x414f4d50

struct tag_omap 
{
	u8 data[0];
};

/* footbridge memory clock, see arch/arm/mach-footbridge/arch.c */
#define ATAG_MEMCLK	0x41000402

struct tag_memclk 
{
	u32 fmemclk;
};

#define ATAG_POWERUP_REASON 0xf1000401

struct tag_powerup_reason 
{
	u32 powerup_reason;
};

/* ipu_buffer_address */
#define ATAG_IPU_BUFFER_ADDRESS 0xf1000402
struct tag_ipu_buffer_address
{
	u32 ipu_buffer_address;
};

/* is_ipu_initialized */
#define ATAG_IS_IPU_INITIALIZED 0xf1000403
struct tag_is_ipu_initialized{
	u32 is_ipu_initialized;
};

/* gpu_context */
#define ATAG_GPU_CONTEXT 0xf1000404
struct tag_gpu_context
{
	u32 gpu_context;
};

/* Address of USB HS firmware */
#define ATAG_USB_FIRMWARE_ADDRESS 0xf1000405
struct tag_usb_firmware_address 
{
	u32 usb_firmware_address;
};

/* Size of USB HS firmware partition */
#define ATAG_USB_FIRMWARE_SIZE 0xf1000406
struct tag_usb_firmware_size 
{
	u32 usb_firmware_size;
};

/* MBM version */
#define ATAG_MBM_VERSION 0xf1000407
struct tag_mbm_version 
{
	u32 mbm_version;
};

/* MBM loader version */
#define ATAG_MBM_LOADER_VERSION 0xf1000408
struct tag_mbm_loader_version 
{
	u32 mbm_loader_version;
};

/* Boardid */
#define ATAG_BOARDID 0xf1000409
struct tag_boardid 
{
	u32 boardid;
};

/* Flat dev tree address */
#define ATAG_FLAT_DEV_TREE_ADDRESS 0xf100040A
struct tag_flat_dev_tree_address 
{
	u32 flat_dev_tree_address;
	u32 flat_dev_tree_size;
};

/* Flashing completed */
#define ATAG_FLASHING_COMPLETED 0xf100040B
struct tag_flashing_completed 
{
	u32 flashing_completed;
};

/* Logo version */
#define MOT_LOGO_VERSION_SIZE_OLD	24
#define MOT_LOGO_VERSION_SIZE		128
#define ATAG_LOGO_VERSION 0xf100040C
struct tag_logo_version 
{
	u8 * logo_version;
	u8   logo_version_string[MOT_LOGO_VERSION_SIZE_OLD];
	u32  logo_version_max_length;
};

/* Memory type */
#define ATAG_MEMORY_TYPE 0xf100040D
struct tag_memory_type 
{
	u16 memory_type;
	u16 padding; /* each atag must be at least 4 bytes */
};

/* Battery status at boot */
#define ATAG_BATTERY_STATUS_AT_BOOT 0xf100040E
struct tag_battery_status_at_boot 
{
	u16 battery_status_at_boot;
	u16 padding; /* each atag must be at least 4 bytes */
};

/* Boot frequency */
#define ATAG_BOOT_FREQUENCY 0xf100040F
struct tag_boot_frequency 
{
	u32 boot_frequency;
};

/* MEDL info */
#define ATAG_MEDL_INFO 0xf1000410
struct tag_medl_info 
{
	u32 medl_panel_tag_id;
	u32 medl_panel_pixel_format;
	u32 medl_panel_status;
};

/* MBM bootup time */
#define ATAG_MBM_BOOTUP_TIME 0xf1000411
struct tag_mbm_bootup_time 
{
	u32 mbm_bootup_time;
};

/* BP loader version */
#define ATAG_BP_LOADER_VERSION 0xf1000412
struct tag_bp_loader_version 
{
	u32 bp_loader_version;
};

/* CLI logo version */
#define ATAG_CLI_LOGO_VERSION 0xf1000413
struct tag_cli_logo_version 
{
	u8 * cli_logo_version;
	u32  cli_logo_version_max_length;
};

/* CID recover boot */
#define ATAG_CID_RECOVER_BOOT 0xf1000414
struct tag_cid_recover_boot
{
	u8 cid_recover_boot;
};

struct tag {
	struct tag_header hdr;
	union {
		struct tag_core		core;
		struct tag_mem32	mem;
		struct tag_videotext	videotext;
		struct tag_ramdisk	ramdisk;
		struct tag_initrd	initrd;
		struct tag_serialnr	serialnr;
		struct tag_revision	revision;
		struct tag_videolfb	videolfb;
		struct tag_cmdline	cmdline;

		/*
		 * Acorn specific
		 */
		struct tag_acorn	acorn;

		/*
		 * OMAP specific
		 */
		struct tag_omap         omap;

		/*
		 * DC21285 specific
		 */
		struct tag_memclk	memclk;

		/*
		 * Motorola specific ATAGs
		 */
		struct tag_powerup_reason powerup_reason;
		struct tag_ipu_buffer_address ipu_buffer_address;
		struct tag_is_ipu_initialized is_ipu_initialized;
		struct tag_gpu_context gpu_context;
		struct tag_usb_firmware_address usb_firmware_address;
		struct tag_usb_firmware_size usb_firmware_size;
		struct tag_mbm_version mbm_version;
		struct tag_mbm_loader_version mbm_loader_version;
		struct tag_boardid boardid;
		struct tag_flat_dev_tree_address flat_dev_tree_address;
		struct tag_flashing_completed flashing_completed;
		struct tag_logo_version logo_version;
		struct tag_memory_type memory_type;
		struct tag_battery_status_at_boot battery_status_at_boot;
		struct tag_boot_frequency boot_frequency;
		struct tag_medl_info medl_info;
		struct tag_mbm_bootup_time mbm_bootup_time;
		struct tag_bp_loader_version bp_loader_version;
		struct tag_cli_logo_version cli_logo_version;
		struct tag_cid_recover_boot cid_recover_boot;
	} u;
};

#define tag_member_present(tag,member)				\
	((unsigned long)(&((struct tag *)0L)->member + 1)	\
		<= (tag)->hdr.size * 4)

#define tag_next(t)	((struct tag *)((u32 *)(t) + (t)->hdr.size))
#define tag_size(type)	((sizeof(struct tag_header) + sizeof(struct type)) >> 2)

#define for_each_tag(t,base)		\
	for (t = base; t->hdr.size; t = tag_next(t))

#endif
