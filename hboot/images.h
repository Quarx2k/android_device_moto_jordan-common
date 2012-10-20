#ifndef __IMAGES_H__
#define __IMAGES_H__
#include "types.h"
#include "buffers.h"

#define KERNEL_DEST     0x81100800
#define RAMDISK_DEST    0x8F210000
#define DEVTREE_DEST    0x8F090000
#define CMDLINE_DEST    0x8F190000

struct memory_image 
{
  void *data;
  size_t size;
};

struct buffer_handle 
{
	struct abstract_buffer abstract;
	void *rest;
	addr_t dest;
	uint32_t maxsize;
	uint32_t attrs;
	char name[0x24];
};

struct memory_image *image_find(uint8_t tag, struct memory_image *dest);
struct memory_image *image_unpack(uint8_t tag, struct memory_image *dest);
void image_complete();

#endif // __IMAGES_H__
