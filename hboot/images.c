#include "images.h"
#include "buffers.h"
#include "memory.h"
#include "stdio.h"
#include "crc32.h"
#include "common.h"

uint8_t unpack_buffer(addr_t dest, void *handle);

struct buffer_handle buffers_list[IMG_LAST_TAG+1] = 
{
	[IMG_HBOOT] = 
	{
		.name = "hboot",
	},
	[IMG_LINUX] = 
	{
		.dest = KERNEL_DEST,
		.name = "zImage",
		.maxsize = 4*1024*1024,
	},
	[IMG_INITRAMFS] = 
	{
		.dest = RAMDISK_DEST,
		.name = "initrd",
		.maxsize = 1*1024*1024,
	},
	[IMG_DEVTREE] = 
	{
		.dest = DEVTREE_DEST,
		.name = "devtree",
		.maxsize = 1*1024*1024,
	},
	[IMG_CMDLINE] =
	{
		.dest = CMDLINE_DEST,
		.name = "cmdline",
		.maxsize = 1024,
	},
};

struct memory_image *image_find(uint8_t tag, struct memory_image *dest) 
{
	if (tag > IMG_LAST_TAG)
		return NULL;

	if (buffers_list[tag].abstract.state == B_STAT_COMPLETED) 
	{
		dest->data = (void*)buffers_list[tag].dest;
		dest->size = (size_t)buffers_list[tag].abstract.size;
		return dest;
	}
	return NULL;
}

struct memory_image *image_unpack(uint8_t tag, struct memory_image *dest) 
{
	if (tag > IMG_LAST_TAG)
		return NULL;

	if (dest->size < buffers_list[tag].abstract.size)
		return NULL;

	if (buffers_list[tag].abstract.state == B_STAT_CREATED)
	{
		if (unpack_buffer((addr_t)dest->data, &buffers_list[tag]) == B_STAT_COMPLETED)
			return dest;
		
	}
	return NULL;
}

int image_complete()
{
	int i, fail = NULL;
	struct abstract_buffer *ab;
	
	for (i = IMG_HBOOT + 1; i <= IMG_LAST_TAG; i++) 
	{
		ab = &buffers_list[i].abstract;
		
		if (ab->state == B_STAT_CREATED) 
			printf("IMAGE [%s]: CREATED\n", buffers_list[i].name);
		
		if ((ab->state == B_STAT_COMPLETED) && (ab->attrs & B_ATTR_VERIFY)) 
		{
#ifdef HBOOT_VERIFY_CRC32
			if (ab->checksum != crc32(buffers_list[i].dest, (size_t)ab->size)) 
			{ 
				ab->state = B_STAT_CRCERROR;
				printf("IMAGE [%s]: CRC ERROR\n", buffers_list[i].name);
				fail = 1;
			}
			else
				printf("IMAGE [%s]: CRC OK\n", buffers_list[i].name);
#else
				printf("IMAGE [%s]: LOADED\n", buffers_list[i].name);
#endif
		}
	}
	return fail;
}
