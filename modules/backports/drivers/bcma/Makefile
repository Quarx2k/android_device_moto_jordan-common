bcma-y					+= main.o scan.o core.o sprom.o
bcma-y					+= driver_chipcommon.o driver_chipcommon_pmu.o
bcma-$(CPTCFG_BCMA_SFLASH)		+= driver_chipcommon_sflash.o
bcma-$(CPTCFG_BCMA_NFLASH)		+= driver_chipcommon_nflash.o
bcma-y					+= driver_pci.o
bcma-$(CPTCFG_BCMA_DRIVER_PCI_HOSTMODE)	+= driver_pci_host.o
bcma-$(CPTCFG_BCMA_DRIVER_MIPS)		+= driver_mips.o
bcma-$(CPTCFG_BCMA_DRIVER_GMAC_CMN)	+= driver_gmac_cmn.o
bcma-$(CPTCFG_BCMA_DRIVER_GPIO)		+= driver_gpio.o
bcma-$(CPTCFG_BCMA_HOST_PCI)		+= host_pci.o
bcma-$(CPTCFG_BCMA_HOST_SOC)		+= host_soc.o
obj-$(CPTCFG_BCMA)			+= bcma.o

ccflags-$(CPTCFG_BCMA_DEBUG)		:= -DDEBUG
