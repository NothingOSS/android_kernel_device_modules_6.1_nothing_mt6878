# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/rpmsg

obj-$(CONFIG_RPMSG_MTK)	+= mtk_rpmsg_mbox.o
ccd_rpmsg-objs := mtk_ccd_rpmsg.o mtk_ccd_rpmsg_ipi.o
obj-$(CONFIG_RPMSG_MTK_CCD) += ccd_rpmsg.o
