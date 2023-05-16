# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y += -Werror

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp

obj-$(CONFIG_MTK_DPC) := mtk_dpc.o

mtk_dpc-objs += mtk_dpc_driver.o
mtk_dpc-objs += mtk_dpc_mmp.o
