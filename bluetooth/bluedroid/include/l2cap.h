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
#ifndef __L2CAP_H
#define __L2CAP_H
#ifdef __cplusplus
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#include <sys/socket.h>
#define L2CAP_DEFAULT_MTU 672
#define L2CAP_DEFAULT_FLUSH_TO 0xFFFF
struct sockaddr_l2 {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 sa_family_t l2_family;
 unsigned short l2_psm;
 bdaddr_t l2_bdaddr;
 unsigned short l2_cid;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define L2CAP_OPTIONS 0x01
struct l2cap_options {
 uint16_t omtu;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint16_t imtu;
 uint16_t flush_to;
 uint8_t mode;
 uint8_t fcs;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint8_t max_tx;
 uint16_t txwin_size;
};
#define L2CAP_CONNINFO 0x02
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct l2cap_conninfo {
 uint16_t hci_handle;
 uint8_t dev_class[3];
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_LM 0x03
#define L2CAP_LM_MASTER 0x0001
#define L2CAP_LM_AUTH 0x0002
#define L2CAP_LM_ENCRYPT 0x0004
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_LM_TRUSTED 0x0008
#define L2CAP_LM_RELIABLE 0x0010
#define L2CAP_LM_SECURE 0x0020
#define L2CAP_COMMAND_REJ 0x01
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_CONN_REQ 0x02
#define L2CAP_CONN_RSP 0x03
#define L2CAP_CONF_REQ 0x04
#define L2CAP_CONF_RSP 0x05
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_DISCONN_REQ 0x06
#define L2CAP_DISCONN_RSP 0x07
#define L2CAP_ECHO_REQ 0x08
#define L2CAP_ECHO_RSP 0x09
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_INFO_REQ 0x0a
#define L2CAP_INFO_RSP 0x0b
typedef struct {
 uint16_t len;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint16_t cid;
} __attribute__ ((packed)) l2cap_hdr;
#define L2CAP_HDR_SIZE 4
typedef struct {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint8_t code;
 uint8_t ident;
 uint16_t len;
} __attribute__ ((packed)) l2cap_cmd_hdr;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_CMD_HDR_SIZE 4
typedef struct {
 uint16_t reason;
} __attribute__ ((packed)) l2cap_cmd_rej;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_CMD_REJ_SIZE 2
typedef struct {
 uint16_t psm;
 uint16_t scid;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__ ((packed)) l2cap_conn_req;
#define L2CAP_CONN_REQ_SIZE 4
typedef struct {
 uint16_t dcid;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint16_t scid;
 uint16_t result;
 uint16_t status;
} __attribute__ ((packed)) l2cap_conn_rsp;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_CONN_RSP_SIZE 8
#define L2CAP_CR_SUCCESS 0x0000
#define L2CAP_CR_PEND 0x0001
#define L2CAP_CR_BAD_PSM 0x0002
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_CR_SEC_BLOCK 0x0003
#define L2CAP_CR_NO_MEM 0x0004
#define L2CAP_CS_NO_INFO 0x0000
#define L2CAP_CS_AUTHEN_PEND 0x0001
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_CS_AUTHOR_PEND 0x0002
typedef struct {
 uint16_t dcid;
 uint16_t flags;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint8_t data[0];
} __attribute__ ((packed)) l2cap_conf_req;
#define L2CAP_CONF_REQ_SIZE 4
typedef struct {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint16_t scid;
 uint16_t flags;
 uint16_t result;
 uint8_t data[0];
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} __attribute__ ((packed)) l2cap_conf_rsp;
#define L2CAP_CONF_RSP_SIZE 6
#define L2CAP_CONF_SUCCESS 0x0000
#define L2CAP_CONF_UNACCEPT 0x0001
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_CONF_REJECT 0x0002
#define L2CAP_CONF_UNKNOWN 0x0003
typedef struct {
 uint8_t type;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint8_t len;
 uint8_t val[0];
} __attribute__ ((packed)) l2cap_conf_opt;
#define L2CAP_CONF_OPT_SIZE 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_CONF_MTU 0x01
#define L2CAP_CONF_FLUSH_TO 0x02
#define L2CAP_CONF_QOS 0x03
#define L2CAP_CONF_RFC 0x04
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_CONF_FCS 0x05
#define L2CAP_CONF_MAX_SIZE 22
#define L2CAP_MODE_BASIC 0x00
#define L2CAP_MODE_RETRANS 0x01
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_MODE_FLOWCTL 0x02
#define L2CAP_MODE_ERTM 0x03
#define L2CAP_MODE_STREAMING 0x04
typedef struct {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint16_t dcid;
 uint16_t scid;
} __attribute__ ((packed)) l2cap_disconn_req;
#define L2CAP_DISCONN_REQ_SIZE 4
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
typedef struct {
 uint16_t dcid;
 uint16_t scid;
} __attribute__ ((packed)) l2cap_disconn_rsp;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_DISCONN_RSP_SIZE 4
typedef struct {
 uint16_t type;
} __attribute__ ((packed)) l2cap_info_req;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_INFO_REQ_SIZE 2
typedef struct {
 uint16_t type;
 uint16_t result;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 uint8_t data[0];
} __attribute__ ((packed)) l2cap_info_rsp;
#define L2CAP_INFO_RSP_SIZE 4
#define L2CAP_IT_CL_MTU 0x0001
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define L2CAP_IT_FEAT_MASK 0x0002
#define L2CAP_IR_SUCCESS 0x0000
#define L2CAP_IR_NOTSUPP 0x0001
#ifdef __cplusplus
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
#endif

