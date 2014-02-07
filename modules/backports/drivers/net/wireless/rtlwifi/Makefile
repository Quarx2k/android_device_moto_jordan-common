obj-$(CPTCFG_RTLWIFI) 		+= rtlwifi.o
rtlwifi-objs	:=		\
		base.o		\
		cam.o		\
		core.o		\
		debug.o		\
		efuse.o		\
		ps.o		\
		rc.o		\
		regd.o		\
		stats.o

rtl8192c_common-objs +=		\

obj-$(CPTCFG_RTLWIFI_PCI)	+= rtl_pci.o
rtl_pci-objs	:=		pci.o

obj-$(CPTCFG_RTLWIFI_USB)	+= rtl_usb.o
rtl_usb-objs	:=		usb.o

obj-$(CPTCFG_RTL8192C_COMMON)	+= rtl8192c/
obj-$(CPTCFG_RTL8192CE)		+= rtl8192ce/
obj-$(CPTCFG_RTL8192CU)		+= rtl8192cu/
obj-$(CPTCFG_RTL8192SE)		+= rtl8192se/
obj-$(CPTCFG_RTL8192DE)		+= rtl8192de/
obj-$(CPTCFG_RTL8723AE)		+= rtl8723ae/
obj-$(CPTCFG_RTL8188EE)		+= rtl8188ee/

ccflags-y += -D__CHECK_ENDIAN__
