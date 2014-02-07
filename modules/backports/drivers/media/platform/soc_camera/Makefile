obj-$(CPTCFG_SOC_CAMERA)		+= soc_camera.o soc_mediabus.o
obj-$(CPTCFG_SOC_CAMERA_SCALE_CROP)	+= soc_scale_crop.o

# a platform subdevice driver stub, allowing to support cameras by adding a
# couple of callback functions to the board code
obj-$(CPTCFG_SOC_CAMERA_PLATFORM)	+= soc_camera_platform.o

# soc-camera host drivers have to be linked after camera drivers
obj-$(CPTCFG_VIDEO_ATMEL_ISI)		+= atmel-isi.o
obj-$(CPTCFG_VIDEO_MX1)			+= mx1_camera.o
obj-$(CPTCFG_VIDEO_MX2)			+= mx2_camera.o
obj-$(CPTCFG_VIDEO_MX3)			+= mx3_camera.o
obj-$(CPTCFG_VIDEO_OMAP1)		+= omap1_camera.o
obj-$(CPTCFG_VIDEO_PXA27x)		+= pxa_camera.o
obj-$(CPTCFG_VIDEO_SH_MOBILE_CEU)	+= sh_mobile_ceu_camera.o
obj-$(CPTCFG_VIDEO_SH_MOBILE_CSI2)	+= sh_mobile_csi2.o
obj-$(CPTCFG_VIDEO_RCAR_VIN)		+= rcar_vin.o
