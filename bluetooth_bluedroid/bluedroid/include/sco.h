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
#ifndef __SCO_H
#define __SCO_H
#ifdef __cplusplus
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SCO_DEFAULT_MTU 500
#define SCO_DEFAULT_FLUSH_TO 0xFFFF
#define SCO_CONN_TIMEOUT (HZ * 40)
#define SCO_DISCONN_TIMEOUT (HZ * 2)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SCO_CONN_IDLE_TIMEOUT (HZ * 60)
struct sockaddr_sco {
 sa_family_t sco_family;
 bdaddr_t sco_bdaddr;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint16_t sco_pkt_type;
};
#define SCO_OPTIONS 0x01
struct sco_options {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint16_t mtu;
};
#define SCO_CONNINFO 0x02
struct sco_conninfo {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint16_t hci_handle;
 uint8_t dev_class[3];
};
#ifdef __cplusplus
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#endif

