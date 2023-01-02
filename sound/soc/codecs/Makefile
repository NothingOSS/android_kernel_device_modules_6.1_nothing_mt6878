# SPDX-License-Identifier: GPL-2.0

ifneq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include
endif

ifneq ($(CONFIG_DEVICE_MODULES_SND_SOC_MEDIATEK),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common
endif

snd-soc-mt6338-objs := mt6338.o
snd-soc-mt6338-accdet-objs := mt6338-accdet.o
snd-soc-mt6351-objs := mt6351.o
snd-soc-mt6358-objs := mt6358.o
snd-soc-mt6359-objs := mt6359.o
snd-soc-mt6359-accdet-objs := mt6359-accdet.o
snd-soc-mt6368-objs := mt6368.o
snd-soc-mt6359p-accdet-objs := mt6359p-accdet.o
snd-soc-hdmi-codec-objs := hdmi-codec.o

obj-$(CONFIG_SND_SOC_MT6338)	+= snd-soc-mt6338.o
obj-$(CONFIG_SND_SOC_MT6338_ACCDET) += mt6338-accdet.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6351)	+= snd-soc-mt6351.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6358)	+= snd-soc-mt6358.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6359)	+= snd-soc-mt6359.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6359_ACCDET) += mt6359-accdet.o
obj-$(CONFIG_SND_SOC_MT6660)	+= snd-soc-mt6660.o
obj-$(CONFIG_SND_SOC_MT6359P)	+= snd-soc-mt6359.o
obj-$(CONFIG_SND_SOC_MT6359P_ACCDET) += mt6359p-accdet.o
obj-$(CONFIG_SND_SOC_MT6368_ACCDET) += mt6368-accdet.o
obj-$(CONFIG_SND_SOC_MT6368)	+= snd-soc-mt6368.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_HDMI_CODEC)	+= snd-soc-hdmi-codec.o
obj-$(CONFIG_SND_SOC_RT5512)	+= richtek/
obj-$(CONFIG_SND_SOC_TFA9874)	+= tfa98xx/
