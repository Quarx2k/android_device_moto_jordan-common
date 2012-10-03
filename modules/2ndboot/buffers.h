#ifndef __BUFFERS_H__
#define __BUFFERS_H__

#if defined(__KERNEL__) || defined(__LOADER__)

#ifndef __ASSEMBLER__

#if defined(__KERNEL__)
#include <linux/types.h>
#elif defined(__LOADER__)
#include "types.h"
#endif

#pragma pack(push, 1)
struct abstract_buffer {
  uint8_t tag;
  uint8_t state;
  uint8_t type;
  uint8_t attrs;
  uint32_t size;
  uint32_t checksum;
};
#pragma pack(pop)
#endif // __ASSEMBLER__

#define B_STAT_NONE      (0)
#define B_STAT_CREATED   (1)
#define B_STAT_COMPLETED (2)
#define B_STAT_CRCERROR  (3)
#define B_STAT_OVERFLOW  (4)
#define B_STAT_ERROR     (5)

#endif // defined(__KERNEL__) || defined(__LOADER__)

#define B_TYPE_PLAIN     (0)
#define B_TYPE_SCATTERED (1)
#define B_TYPE_NAND      (2)

#define B_ATTR_VERIFY    (1 << 0)

#define IMG_ATTR_LAZY    (1 << 0)

#define IMG_LINUX        (1)
#define IMG_INITRAMFS    (2)
#define IMG_MOTFLATTREE  (3)
#define IMG_CMDLINE      (4)
#define IMG_USBFW        (5)
#define IMG_BPLOADER     (6)
#define IMG_MBM         (7)
#define IMG_MBMLOADER         (8)

#define IMG_LAST_TAG     IMG_MBMLOADER

#endif // __BUFFERS_H__
