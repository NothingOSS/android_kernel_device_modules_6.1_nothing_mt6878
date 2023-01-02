# SPDX-License-Identifier: GPL-2.0
#
# Makefile for USB Host Controller Drivers
#

# tell define_trace.h where to find the xhci trace header

ccflags-y += -I$(srctree)/drivers/usb/host

xhci-mtk-hcd-y := xhci-mtk.o xhci-mtk-sch.o

obj-$(CONFIG_USB_XHCI_MTK)	+= xhci-mtk-hcd.o

