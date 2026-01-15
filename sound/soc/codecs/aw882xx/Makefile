# make sure that the environment variables ARCH and CROSS_COMPILE
# are set for your architecture and cross compiler
#
# e.g. for Beaglebone black:
# export ARCH := arm
# export CROSS_COMPILE := arm-linux-gnueabihf-
#
# e.g. Samsung Galaxy S6:
# export ARCH := arm64
# export CROSS_COMPILE := aarch64-linux-android-

#MODULE_NAME = snd-soc-aw882xx

# additional debugging
#TFA_DEBUG=1

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common

# debugging support (also enables trace_printk)
ccflags-y += -Werror
ccflags-y += $(call cc-option,-Wno-date-time,)

snd-soc-aw882xx-objs += aw882xx.o
snd-soc-aw882xx-objs += aw882xx_monitor.o
snd-soc-aw882xx-objs += aw882xx_init.o
snd-soc-aw882xx-objs += aw882xx_dsp.o
snd-soc-aw882xx-objs += aw882xx_device.o
snd-soc-aw882xx-objs += aw882xx_calib.o
snd-soc-aw882xx-objs += aw882xx_bin_parse.o
snd-soc-aw882xx-objs += aw882xx_spin.o

obj-$(CONFIG_SND_SMARTPA_AW882XX) += snd-soc-aw882xx.o

