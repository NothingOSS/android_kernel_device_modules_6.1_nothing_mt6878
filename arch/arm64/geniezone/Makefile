# SPDX-License-Identifier: GPL-2.0-only
#
# Main Makefile for gzvm, this one includes drivers/virt/geniezone/Makefile
#
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/uapi
ccflags-y += -I$(DEVICE_MODULES_PATH)/arch/arm64/include/uapi
ccflags-y += -I$(DEVICE_MODULES_PATH)/include

include $(DEVICE_MODULES_PATH)/drivers/virt/geniezone/Makefile

gzvm-y += vm.o vcpu.o vgic.o

obj-$(CONFIG_MTK_GZVM) += gzvm.o
