#include "images.h"
#include "buffers.h"
#include "memory.h"
#include "stdio.h"
#include "crc32.h"
#include "common.h"

uint8_t unpack_buffer(addr_t dest, void *handle);

struct buffer_handle 
{
  struct abstract_buffer abstract;
  void *rest;
  addr_t dest;
  uint32_t maxsize;
  uint32_t attrs;
  uint32_t reserved[1];
};


struct buffer_handle buffers_list[IMG_LAST_TAG+1] = 
{
	[IMG_LINUX] = 
	{
		.dest = KERNEL_DEST,
		.maxsize = 4*1024*1024,
	},
	[IMG_INITRAMFS] = 
	{
		.dest = KERNEL_DEST + 4*1024*1024,
		.maxsize = 2*1024*1024,
	},
	[IMG_DEVTREE] = 
	{
		.dest = 0x85000000,
		.maxsize = 1*1024*1024,
	},
	[IMG_CMDLINE] =
	{
		.dest = 0x85100000,
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

void image_complete() 
{
	int i;
	struct abstract_buffer *ab;

	for (i = 1; i <= IMG_LAST_TAG; ++i) 
	{
		ab = &buffers_list[i].abstract;

		if (ab->state == B_STAT_CREATED) 
			printf("IMAGE [%d]: CREATED\n", i);

		if ((ab->state == B_STAT_COMPLETED) && (ab->attrs & B_ATTR_VERIFY)) 
		{
#ifdef HBOOT_VERIFY_CRC32
			if (ab->checksum != crc32(buffers_list[i].dest, (size_t)ab->size)) 
			{ 
				ab->state = B_STAT_CRCERROR;
				printf("IMAGE [%d]: CRC ERROR\n", i);
			}
			else
				printf("IMAGE [%d]: CRC OK\n", i);
#else
			printf("IMAGE [%d]: LOADED\n", i);
#endif
		}
	}
}

