# SPDX-License-Identifier: GPL-2.0
# Mediatek UFS extended features makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/scsi/ufs/vendor
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/scsi/ufs/

# must after -I$(DEVICE_MODULES_PATH)/drivers/scsi/ufs/
ccflags-y += -I$(srctree)/drivers/scsi/ufs/

obj-$(CONFIG_DEVICE_MODULES_SCSI_UFS_MEDIATEK) += ufs-mediatek-mod.o
ufs-mediatek-mod-y 			+= ../ufs-mediatek.o ../ufs-mediatek-mcq.o
ufs-mediatek-mod-$(CONFIG_UFSFEATURE) 	+= ufsfeature.o ufsfeature-mimic.o
ufs-mediatek-mod-$(CONFIG_UFSSHPB) 	+= ufsshpb.o
ufs-mediatek-mod-$(CONFIG_UFSTW) 	+= ufstw.o
ufs-mediatek-mod-$(CONFIG_UFSHID) 	+= ufshid.o
ufs-mediatek-mod-$(CONFIG_UFSRINGBUF)	+= ufsringbuf.o
