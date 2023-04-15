#
# Makefile for trusty components
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusty/include

obj-$(CONFIG_TRUSTY) += ise-trusty.o
ise-trusty-$(CONFIG_TRUSTY) += trusty.o trusty-mem.o trusty-shm.o

obj-$(CONFIG_TRUSTY_VIRTIO_IPC) += ise-trusty-ipc.o
ise-trusty-ipc-$(CONFIG_TRUSTY_VIRTIO_IPC) += trusty-ipc.o

obj-$(CONFIG_TRUSTY_VIRTIO) += ise-trusty-virtio.o
ise-trusty-virtio-$(CONFIG_TRUSTY_VIRTIO) += trusty-virtio.o
