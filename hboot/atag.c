#include "types.h"
#include "memory.h"
#include "atag_priv.h"
#include "images.h"
#include "stdio.h"
#include "atag.h"
#include "common.h"
#include "string.h"
#include "board.h"

void *atag_build() 
{
	struct memory_image image;
	struct tag* tag = (struct tag*)ATAG_BASE_ADDR;
	char* atag_cmdline;

	tag->hdr.tag = ATAG_CORE;
	tag->hdr.size = tag_size (tag_core);
	tag->u.core.flags = 0;
	tag->u.core.pagesize = 0x00001000;
	tag->u.core.rootdev = 0x0000;
	tag = tag_next(tag);
	
	atag_cmdline = tag->u.cmdline.cmdline;
	
	tag->hdr.tag = ATAG_CMDLINE;
	
	if (image_find(IMG_CMDLINE, &image) != NULL) 
	{
		memcpy(atag_cmdline, image.data, image.size);
		
		if (atag_cmdline[image.size-1] == '\xa' || atag_cmdline[image.size-1] == '\n') 
			atag_cmdline[image.size-1] = '\0';
		else
			atag_cmdline[image.size] = '\0';
		
		tag->hdr.size = (sizeof(struct tag_header) + image.size + 1 + 3) >> 2;
	}
	else
	{
		strcpy(atag_cmdline, board_get_cmdline());
		tag->hdr.size = (sizeof(struct tag_header) + strlen(atag_cmdline) + 1 + 3) >> 2;
	}
	printf("Cmdline: %s\n", atag_cmdline);
	tag = tag_next(tag);

	/* Add special MBM and MBMLOADER versions ATAGs */
	tag->hdr.tag = ATAG_MBM_VERSION;
	tag->hdr.size = tag_size(tag_mbm_version);
	tag->u.mbm_version.mbm_version = 0x1234;
	printf("MBM Version: 0x%04x\n", tag->u.mbm_version.mbm_version);
	tag = tag_next(tag);
	
	tag->hdr.tag = ATAG_MBM_LOADER_VERSION;
	tag->hdr.size = tag_size(tag_mbm_loader_version);
	tag->u.mbm_loader_version.mbm_loader_version = 0x1234;
	printf("MBMLOADER Version: 0x%04x\n", tag->u.mbm_loader_version.mbm_loader_version);
	tag = tag_next(tag);

	/* Device tree atag */
	if (image_find(IMG_DEVTREE, &image) != NULL) 
	{
		tag->hdr.tag = ATAG_FLAT_DEV_TREE_ADDRESS;
		tag->hdr.size = tag_size (tag_flat_dev_tree_address);
		tag->u.flat_dev_tree_address.flat_dev_tree_address = (u32)image.data;
		tag->u.flat_dev_tree_address.flat_dev_tree_size = (u32)image.size;
		printf("DEVTREE is on 0x%08x!\n", tag->u.flat_dev_tree_address.flat_dev_tree_address);
		tag = tag_next(tag);
	}

	/* Initramfs tag */
	if (image_find(IMG_INITRAMFS, &image) != NULL) 
	{
		tag->hdr.tag = ATAG_INITRD2;
		tag->hdr.size = tag_size(tag_initrd);
		tag->u.initrd.start = (u32)image.data;
		tag->u.initrd.size = (u32)image.size;
		printf("INITRD is on 0x%08x!\n", tag->u.initrd.start);
		tag = tag_next(tag);
	}
	
	/* Powerup reason */
	tag->hdr.tag = ATAG_POWERUP_REASON;
	tag->hdr.size = tag_size (tag_powerup_reason);
	tag->u.powerup_reason.powerup_reason = cfg_powerup_reason;
	printf("Powerup Reason: 0x%08x\n", tag->u.powerup_reason.powerup_reason);
	tag = tag_next(tag);
	
	/* Battery status at boot */
	tag->hdr.tag = ATAG_BATTERY_STATUS_AT_BOOT;
	tag->hdr.size = tag_size (tag_battery_status_at_boot);
	tag->u.battery_status_at_boot.battery_status_at_boot = cfg_batt_status;
	printf("Battery status: 0x%04x\n", tag->u.battery_status_at_boot.battery_status_at_boot);
	tag = tag_next(tag);
	
	/* CID Recover boot */
	tag->hdr.tag = ATAG_CID_RECOVER_BOOT;
	tag->hdr.size = tag_size (tag_cid_recover_boot);
	tag->u.cid_recover_boot.cid_recover_boot = cfg_cid_recovery;
	printf("CID Recovery: 0x%02x\n", tag->u.cid_recover_boot.cid_recover_boot);
	tag = tag_next(tag);
	
	tag->hdr.tag = ATAG_NONE;
	tag->hdr.size = 0;
	printf("ATAG created!\n");
	return (void*)ATAG_BASE_ADDR;
}