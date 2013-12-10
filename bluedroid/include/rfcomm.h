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
#ifndef __RFCOMM_H
#define __RFCOMM_H
#ifdef __cplusplus
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#include <sys/socket.h>
#define RFCOMM_DEFAULT_MTU 127
#define RFCOMM_PSM 3
struct sockaddr_rc {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 sa_family_t rc_family;
 bdaddr_t rc_bdaddr;
 uint8_t rc_channel;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define RFCOMM_CONNINFO 0x02
struct rfcomm_conninfo {
 uint16_t hci_handle;
 uint8_t dev_class[3];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define RFCOMM_LM 0x03
#define RFCOMM_LM_MASTER 0x0001
#define RFCOMM_LM_AUTH 0x0002
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define RFCOMM_LM_ENCRYPT 0x0004
#define RFCOMM_LM_TRUSTED 0x0008
#define RFCOMM_LM_RELIABLE 0x0010
#define RFCOMM_LM_SECURE 0x0020
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define RFCOMM_MAX_DEV 256
#define RFCOMMCREATEDEV _IOW('R', 200, int)
#define RFCOMMRELEASEDEV _IOW('R', 201, int)
#define RFCOMMGETDEVLIST _IOR('R', 210, int)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define RFCOMMGETDEVINFO _IOR('R', 211, int)
struct rfcomm_dev_req {
 int16_t dev_id;
 uint32_t flags;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 bdaddr_t src;
 bdaddr_t dst;
 uint8_t channel;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define RFCOMM_REUSE_DLC 0
#define RFCOMM_RELEASE_ONHUP 1
#define RFCOMM_HANGUP_NOW 2
#define RFCOMM_TTY_ATTACHED 3
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct rfcomm_dev_info {
 int16_t id;
 uint32_t flags;
 uint16_t state;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 bdaddr_t src;
 bdaddr_t dst;
 uint8_t channel;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct rfcomm_dev_list_req {
 uint16_t dev_num;
 struct rfcomm_dev_info dev_info[0];
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#ifdef __cplusplus
#endif
#endif

