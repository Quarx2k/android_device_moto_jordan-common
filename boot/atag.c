#include "types.h"
#include "memory.h"
#include "atag_priv.h"
#include "images.h"
#include "stdio.h"
#include "atag.h"
#include "common.h"
#include "string.h"

void *atag_build() 
{
	struct memory_image image;
	struct tag *tag = (struct tag*)ATAG_BASE_ADDR;

	printf("Creating ATAGS\n");

	tag->hdr.tag = ATAG_CORE;
	tag->hdr.size = tag_size (tag_core);
	tag->u.core.flags = 0;
	tag->u.core.pagesize = 0x00001000;
	tag->u.core.rootdev = 0x0000;
	tag = tag_next(tag);

	if (image_find(IMG_CMDLINE, &image) != NULL) 
	{
		char *atag_cmdline = tag->u.cmdline.cmdline;

		tag->hdr.tag = ATAG_CMDLINE;
		tag->hdr.size = (sizeof(struct tag_header) + image.size + 1 + 3) >> 2;
		memcpy(atag_cmdline, image.data, image.size);
		
		if (atag_cmdline[image.size-1] == '\xa') 
			atag_cmdline[image.size-1] = '\0';
		else
			atag_cmdline[image.size] = '\0';
		
		printf("CMDLINE: %s\n", atag_cmdline);
		tag = tag_next(tag);
	}

	/* Add special MBM and MBMLOADER versions ATAG */
	tag->hdr.tag = ATAG_MBM_VERSION;
	tag->hdr.size = tag_size(tag_mbm_version);
	tag->u.mbm_version.mbm_version = 0x1234;
	printf("MBM Version: 0x1234\n");
	tag = tag_next(tag);
	
	tag->hdr.tag = ATAG_MBM_LOADER_VERSION;
	tag->hdr.size = tag_size(tag_mbm_loader_version);
	tag->u.mbm_loader_version.mbm_loader_version = 0x1234;
	printf("MBMLOADER Version: 0x1234\n");
	tag = tag_next(tag);

	/* Device tree atag */
	if (image_find(IMG_DEVTREE, &image) != NULL) 
	{
		tag->hdr.tag = ATAG_FLAT_DEV_TREE_ADDRESS;
		tag->hdr.size = tag_size (tag_flat_dev_tree_address);
		tag->u.flat_dev_tree_address.flat_dev_tree_address = (u32)image.data;
		tag->u.flat_dev_tree_address.flat_dev_tree_size = (u32)image.size;
		tag = tag_next(tag);
		printf("DEVTREE FOUND!\n");
	}

	/* Initramfs tag */
	if (image_find(IMG_INITRAMFS, &image) != NULL) 
	{
		tag->hdr.tag = ATAG_INITRD2;
		tag->hdr.size = tag_size(tag_initrd);
		tag->u.initrd.start = (u32)image.data;
		tag->u.initrd.size = (u32)image.size;
		tag = tag_next(tag);
		printf("INITRD FOUND!\n");
	}

	tag->hdr.tag = ATAG_NONE;
	tag->hdr.size = 0;
	printf("ATAGS CREATED!\n");
	return (void*)ATAG_BASE_ADDR;
}