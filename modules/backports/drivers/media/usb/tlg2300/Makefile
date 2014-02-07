poseidon-objs := pd-video.o pd-alsa.o pd-dvb.o pd-radio.o pd-main.o

obj-$(CPTCFG_VIDEO_TLG2300) += poseidon.o

ccflags-y += -I$(backport_srctree)/drivers/media/i2c
ccflags-y += -I$(backport_srctree)/drivers/media/tuners
ccflags-y += -I$(backport_srctree)/drivers/media/dvb-core
ccflags-y += -I$(backport_srctree)/drivers/media/dvb-frontends

