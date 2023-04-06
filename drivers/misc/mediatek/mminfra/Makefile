# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/tinysys_scmi/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc

obj-$(CONFIG_MTK_MMINFRA) += mtk-mminfra-debug.o mtk-mminfra-imax.o
obj-$(CONFIG_MTK_MMINFRA) += mm-fake-engine.o
