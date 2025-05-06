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

#MODULE_NAME = snd-soc-tfa9873

# additional debugging
#TFA_DEBUG=1

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common

# debugging support (also enables trace_printk)
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/codecs/tfa9873/inc
ccflags-y += -DDEBUG -DTFA_NON_DSP_SOLUTION
ccflags-y += -Werror
ccflags-y += $(call cc-option,-Wno-date-time,)

snd-soc-tfa9873-objs += src/tfa98xx.o
snd-soc-tfa9873-objs += src/tfa_container.o
snd-soc-tfa9873-objs += src/tfa_dsp.o
snd-soc-tfa9873-objs += src/tfa_init.o

snd-soc-tfa_haptic-objs += src/tfa_haptic.o

obj-$(CONFIG_SND_SOC_TFA9873) += snd-soc-tfa9873.o
