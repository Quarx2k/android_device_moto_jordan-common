/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Bluez header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to Android. It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef __BLUETOOTH_H
#define __BLUETOOTH_H
#ifdef __cplusplus
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <endian.h>
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#include <byteswap.h>
#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH 31
#define PF_BLUETOOTH AF_BLUETOOTH
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#define BTPROTO_L2CAP 0
#define BTPROTO_HCI 1
#define BTPROTO_SCO 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define BTPROTO_RFCOMM 3
#define BTPROTO_BNEP 4
#define BTPROTO_CMTP 5
#define BTPROTO_HIDP 6
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define BTPROTO_AVDTP 7
#define SOL_HCI 0
#define SOL_L2CAP 6
#define SOL_SCO 17
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SOL_RFCOMM 18
#ifndef SOL_BLUETOOTH
#define SOL_BLUETOOTH 274
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define BT_SECURITY 4
struct bt_security {
 uint8_t level;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define BT_SECURITY_SDP 0
#define BT_SECURITY_LOW 1
#define BT_SECURITY_MEDIUM 2
#define BT_SECURITY_HIGH 3
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define BT_DEFER_SETUP 7
#define BT_FLUSHABLE 8
#define BT_FLUSHABLE_OFF 0
#define BT_FLUSHABLE_ON 1
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define BT_POWER 9
struct bt_power {
 uint8_t force_active;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum {
 BT_CONNECTED = 1,
 BT_OPEN,
 BT_BOUND,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 BT_LISTEN,
 BT_CONNECT,
 BT_CONNECT2,
 BT_CONFIG,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 BT_DISCONN,
 BT_CLOSED
};
#if __BYTE_ORDER == __LITTLE_ENDIAN
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define htobs(d) (d)
#define htobl(d) (d)
#define btohs(d) (d)
#define btohl(d) (d)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#elif __BYTE_ORDER == __BIG_ENDIAN
#define htobs(d) bswap_16(d)
#define htobl(d) bswap_32(d)
#define btohs(d) bswap_16(d)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define btohl(d) bswap_32(d)
#else
#error "Unknown byte order"
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define bt_get_unaligned(ptr)  ({   struct __attribute__((packed)) {   typeof(*(ptr)) __v;   } *__p = (void *) (ptr);   __p->__v;  })
#define bt_put_unaligned(val, ptr)  do {   struct __attribute__((packed)) {   typeof(*(ptr)) __v;   } *__p = (void *) (ptr);   __p->__v = (val);  } while(0)
typedef struct {
 uint8_t b[6];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__((packed)) bdaddr_t;
#define BDADDR_ANY (&(bdaddr_t) {{0, 0, 0, 0, 0, 0}})
#define BDADDR_ALL (&(bdaddr_t) {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}})
#define BDADDR_LOCAL (&(bdaddr_t) {{0, 0, 0, 0xff, 0xff, 0xff}})
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#ifdef __cplusplus
#endif
#endif

