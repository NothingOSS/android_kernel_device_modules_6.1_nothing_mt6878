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

# pbm include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm

# platform driver
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6878) += snd-soc-mt6878-afe.o
snd-soc-mt6878-afe-objs += \
	mt6878-afe-pcm.o \
	mt6878-afe-clk.o \
	mt6878-afe-gpio.o \
	mt6878-dai-adda.o \
	mt6878-afe-control.o \
	mt6878-dai-i2s.o \
	mt6878-dai-hw-gain.o \
	mt6878-dai-src.o \
	mt6878-dai-pcm.o \
	mt6878-dai-hostless.o \
	mt6878-misc-control.o \
	mt6878-afe-cm.o

#	mt6878-dai-tdm.o \

# machine driver
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6878_MT6369) += mt6878-mt6369.o
