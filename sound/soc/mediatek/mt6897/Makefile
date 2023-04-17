# SPDX-License-Identifier: GPL-2.0

# MENU Governor Predict(low power) include path
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# adsp include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/audio_ipi/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/audio_dsp

# spm include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

# platform driver
obj-$(CONFIG_SND_SOC_MT6897) += snd-soc-mt6897-afe.o
snd-soc-mt6897-afe-objs += \
	mt6897-afe-pcm.o \
	mt6897-afe-clk.o \
	mt6897-afe-gpio.o \
	mt6897-dai-adda.o \
	mt6897-afe-control.o \
	mt6897-dai-i2s.o \
	mt6897-dai-hw-gain.o \
	mt6897-dai-src.o \
	mt6897-dai-pcm.o \
	mt6897-dai-hostless.o \
	mt6897-misc-control.o \
	mt6897-dai-tdm.o \
	mt6897-afe-cm.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6897_MT6368) += mt6897-mt6368.o
