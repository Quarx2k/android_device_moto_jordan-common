#
# Makefile for the video capture/playback device drivers.
#

omap2cam-objs	:=	omap24xxcam.o omap24xxcam-dma.o

obj-$(CPTCFG_VIDEO_VINO) += indycam.o
obj-$(CPTCFG_VIDEO_VINO) += vino.o

obj-$(CPTCFG_VIDEO_TIMBERDALE)	+= timblogiw.o
obj-$(CPTCFG_VIDEO_M32R_AR_M64278) += arv.o

obj-$(CPTCFG_VIDEO_VIA_CAMERA) += via-camera.o
obj-$(CPTCFG_VIDEO_CAFE_CCIC) += marvell-ccic/
obj-$(CPTCFG_VIDEO_MMP_CAMERA) += marvell-ccic/

obj-$(CPTCFG_VIDEO_OMAP2)		+= omap2cam.o
obj-$(CPTCFG_VIDEO_OMAP3)	+= omap3isp/

obj-$(CPTCFG_VIDEO_VIU) += fsl-viu.o
obj-$(CPTCFG_VIDEO_VIVI) += vivi.o

obj-$(CPTCFG_VIDEO_MEM2MEM_TESTDEV) += mem2mem_testdev.o

obj-$(CPTCFG_VIDEO_TI_VPE)		+= ti-vpe/

obj-$(CPTCFG_VIDEO_MX2_EMMAPRP)		+= mx2_emmaprp.o
obj-$(CPTCFG_VIDEO_CODA) 		+= coda.o

obj-$(CPTCFG_VIDEO_SH_VEU)		+= sh_veu.o

obj-$(CPTCFG_VIDEO_MEM2MEM_DEINTERLACE)	+= m2m-deinterlace.o

obj-$(CPTCFG_VIDEO_S3C_CAMIF) 		+= s3c-camif/
obj-$(CPTCFG_VIDEO_SAMSUNG_EXYNOS4_IS) 	+= exynos4-is/
obj-$(CPTCFG_VIDEO_SAMSUNG_S5P_JPEG)	+= s5p-jpeg/
obj-$(CPTCFG_VIDEO_SAMSUNG_S5P_MFC)	+= s5p-mfc/
obj-$(CPTCFG_VIDEO_SAMSUNG_S5P_TV)	+= s5p-tv/

obj-$(CPTCFG_VIDEO_SAMSUNG_S5P_G2D)	+= s5p-g2d/
obj-$(CPTCFG_VIDEO_SAMSUNG_EXYNOS_GSC)	+= exynos-gsc/

obj-$(CONFIG_BLACKFIN)                  += blackfin/

obj-$(CONFIG_ARCH_DAVINCI)		+= davinci/

obj-$(CPTCFG_VIDEO_SH_VOU)		+= sh_vou.o

obj-$(CPTCFG_SOC_CAMERA)		+= soc_camera/

obj-$(CPTCFG_VIDEO_RENESAS_VSP1)	+= vsp1/

obj-y	+= davinci/

obj-$(CONFIG_ARCH_OMAP)	+= omap/

ccflags-y += -I$(backport_srctree)/drivers/media/i2c
