#
# Makefile for the Linux Bluetooth subsystem.
#

obj-$(CPTCFG_BT)	+= bluetooth.o
obj-$(CPTCFG_BT_RFCOMM)	+= rfcomm/
obj-$(CPTCFG_BT_BNEP)	+= bnep/
obj-$(CPTCFG_BT_CMTP)	+= cmtp/
obj-$(CPTCFG_BT_HIDP)	+= hidp/

bluetooth-y := af_bluetooth.o hci_core.o hci_conn.o hci_event.o mgmt.o \
	hci_sock.o hci_sysfs.o l2cap_core.o l2cap_sock.o smp.o sco.o lib.o \
	a2mp.o amp.o

subdir-ccflags-y += -D__CHECK_ENDIAN__
