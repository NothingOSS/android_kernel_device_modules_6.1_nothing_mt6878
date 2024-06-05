# Makefile for the focaltech touchscreen drivers.

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-$(CONFIG_TOUCHSCREEN_FT3519_I2C) += focaltech_tp.o
focaltech_tp-y := focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o


focaltech_tp-y += focaltech_i2c.o





focaltech_tp-y += focaltech_flash.o
focaltech_tp-y += focaltech_flash/focaltech_upgrade_ft3519t.o
focaltech_tp-y += focaltech_test/focaltech_test.o
focaltech_tp-y += focaltech_test/focaltech_test_ini.o
focaltech_tp-y += focaltech_test/supported_ic/focaltech_test_ft3519t.o
focaltech_tp-y += touchpanel_event_notify/touchpanel_event_notify.o
