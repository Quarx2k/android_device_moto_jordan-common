#
# Makefile for the Linux NFC NCI layer.
#

obj-$(CPTCFG_NFC_NCI) += nci.o

nci-objs := core.o data.o lib.o ntf.o rsp.o

nci-$(CPTCFG_NFC_NCI_SPI) += spi.o
