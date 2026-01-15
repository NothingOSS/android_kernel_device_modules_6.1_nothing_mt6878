# Makefile for the focaltech touchscreen drivers.

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/

#add for err code begin
ifeq ($(BUILD_SMT),true)
subdir-ccflags-y += -DBUILD_SMT
endif
#add for err code end
obj-$(CONFIG_TOUCHSCREEN_EPH861) += eph861.o
eph861-y := eswin_eph861x.o \
	eswin_eph861x_bootloader.o \
	eswin_eph861x_comms.o \
	eswin_eph861x_eswin.o \
	eswin_eph861x_spi.o \
	eswin_eph861x_tlv_command.o \
	eswin_eph861x_tlv_report.o \
	eswin_eph861x_testing.o
