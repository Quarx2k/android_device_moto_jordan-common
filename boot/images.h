#ifndef __IMAGES_H__
#define __IMAGES_H__
#include "types.h"
#include "buffers.h"

#define KERNEL_DEST	0x83000000	

struct memory_image 
{
  void *data;
  size_t size;
};

struct memory_image *image_find(uint8_t tag, struct memory_image *dest);
struct memory_image *image_unpack(uint8_t tag, struct memory_image *dest);
void image_complete();

#endif // __IMAGES_H__
