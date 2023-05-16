# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/remoteproc

obj-$(CONFIG_MTK_CCU_RPROC)		+= mtk_ccu.o
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/ \
			 -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include \
			 -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu \
			 -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3

mtk_ccu-objs += mtk_ccu_isp71.o mtk_ccu_common.o mtk_ccu_ipi.o
