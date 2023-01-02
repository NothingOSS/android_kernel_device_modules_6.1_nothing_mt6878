# SPDX-License-Identifier: GPL-2.0
#
# Makefile for MMC/SD host controller drivers
#
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/char/rpmb/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/scsi/ufs/
ccflags-y += -I$(srctree)/drivers/scsi/ufs/
ccflags-y += -I$(srctree)/drivers/mmc/host

obj-$(CONFIG_DEVICE_MODULES_MMC_MTK)			+= mtk-sd.o
obj-$(CONFIG_MMC_MTK_PRO)		+= mtk-mmc.o
obj-$(CONFIG_DEVICE_MODULES_MMC_DEBUG)			+= mtk-mmc-dbg.o
