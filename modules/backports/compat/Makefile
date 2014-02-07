ccflags-y += -I$(src)
obj-m += compat.o
compat-y += main.o

#obj-$(CPTCFG_BACKPORT_BUILD_FW_LOADER) += compat_firmware_class.o

obj-$(CPTCFG_BACKPORT_USERSEL_NET_SCH_CODEL) += sch_codel.o

sch_fq_codel-y = sch_fq_codel_core.o flow_dissector.o
obj-$(CPTCFG_BACKPORT_USERSEL_NET_SCH_FQ_CODEL) += sch_fq_codel.o

obj-$(CPTCFG_BACKPORT_BUILD_CORDIC) += cordic.o
obj-$(CPTCFG_BACKPORT_BUILD_CRC8) += crc8.o

# Compat kernel compatibility code
compat-$(CPTCFG_BACKPORT_KERNEL_2_6_26) += compat-2.6.26.o
compat-$(CPTCFG_BACKPORT_KERNEL_2_6_27) += compat-2.6.27.o
compat-$(CPTCFG_BACKPORT_KERNEL_2_6_28) += compat-2.6.28.o
compat-$(CPTCFG_BACKPORT_KERNEL_2_6_29) += compat-2.6.29.o
compat-$(CPTCFG_BACKPORT_KERNEL_2_6_32) += compat-2.6.32.o
compat-$(CPTCFG_BACKPORT_KERNEL_2_6_33) += compat-2.6.33.o
compat-$(CPTCFG_BACKPORT_KERNEL_2_6_34) += compat-2.6.34.o
compat-$(CPTCFG_BACKPORT_KERNEL_2_6_35) += compat-2.6.35.o
compat-$(CPTCFG_BACKPORT_KERNEL_2_6_36) += compat-2.6.36.o
compat-$(CPTCFG_BACKPORT_KERNEL_2_6_37) += compat-2.6.37.o
compat-$(CPTCFG_BACKPORT_KERNEL_2_6_39) += compat-2.6.39.o kstrtox.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_0) += compat-3.0.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_1) += compat-3.1.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_3) += compat-3.3.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_4) += compat-3.4.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_5) += compat-3.5.o user_namespace.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_6) += compat-3.6.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_7) += compat-3.7.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_8) += compat-3.8.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_9) += compat-3.9.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_10) += backport-3.10.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_12) += backport-3.12.o
compat-$(CPTCFG_BACKPORT_KERNEL_3_13) += backport-3.13.o

compat-$(CPTCFG_BACKPORT_BUILD_KFIFO) += kfifo.o
compat-$(CPTCFG_BACKPORT_BUILD_GENERIC_ATOMIC64) += compat_atomic.o
compat-$(CPTCFG_BACKPORT_BUILD_DMA_SHARED_HELPERS) += dma-shared-helpers.o
compat-$(CPTCFG_BACKPORT_BUILD_AVERAGE) += lib-average.o
compat-$(CPTCFG_BACKPORT_BUILD_DMA_SHARED_BUFFER) += drivers-base-dma-buf.o
