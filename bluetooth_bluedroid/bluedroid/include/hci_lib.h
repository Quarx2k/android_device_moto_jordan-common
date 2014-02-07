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
#ifndef __HCI_LIB_H
#define __HCI_LIB_H
#ifdef __cplusplus
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct hci_request {
 uint16_t ogf;
 uint16_t ocf;
 int event;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 void *cparam;
 int clen;
 void *rparam;
 int rlen;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct hci_version {
 uint16_t manufacturer;
 uint8_t hci_ver;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint16_t hci_rev;
 uint8_t lmp_ver;
 uint16_t lmp_subver;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#ifdef __cplusplus
#endif
#endif

