# Makefile for the focaltech touchscreen drivers.

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-$(CONFIG_TOUCHSCREEN_FT3683G_SPI) += ft3683g.o
ft3683g-y := focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o \
		focaltech_proximity.o

ft3683g-y += focaltech_spi_v2.o

ft3683g-y += focaltech_debug.o

ft3683g-y += focaltech_hp.o

ft3683g-y += focaltech_flash.o
ft3683g-y += focaltech_flash/focaltech_upgrade_ft3683g.o
ft3683g-y += focaltech_test/focaltech_test.o
ft3683g-y += focaltech_test/focaltech_test_ini.o
ft3683g-y += focaltech_test/supported_ic/focaltech_test_ft3683g.o
