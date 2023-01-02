# SPDX-License-Identifier: GPL-2.0
# UFSHCD makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/blocktag/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/scsi/ufs/vendor
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/scsi/ufs/
ccflags-y += -I$(srctree)/drivers/scsi/ufs/

obj-$(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK) += vendor/
obj-$(CONFIG_SCSI_UFS_MEDIATEK_DBG) += ufs-mediatek-dbg.o
