NUXINCLUDE += -include $(srctree)/kernel/sched/sched.h
ccflags-y += -I$(src)              # needed for trace events
ccflags-y += -I$(srctree)/drivers/misc/mediatek/wlcdrv/

$(info CONFIG_MTK_WLC is defined as $(CONFIG_MTK_WLC))

obj-$(CONFIG_MTK_WLC) += wlcdrv.o
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/include
ccflags-y += -I$(srctree)/include/linux/soc/mediatek

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

wlcdrv-y += wlcdrv_main.o
wlcdrv-y += wlcdrv_ipi.o
wlcdrv-y += wlcdrv_pmu.o
wlcdrv-y += v8_pmu_hw.o
