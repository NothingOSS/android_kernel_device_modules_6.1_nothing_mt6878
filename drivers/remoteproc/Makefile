# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/remoteproc

#obj-$(CONFIG_MTK_CCD_REMOTEPROC)	+= mtk_ccd.o mtk_ccd_mem.o
obj-$(CONFIG_MTK_CCU_RPROC)		+= mtk_ccu.o
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/ \
			 -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
mtk_ccu-objs += mtk_ccu_isp71.o mtk_ccu_common.o mtk_ccu_ipi.o
mtk_ccd_remoteproc-objs := mtk_ccd.o mtk_ccd_mem.o
obj-$(CONFIG_MTK_CCD_REMOTEPROC) += mtk_ccd_remoteproc.o
