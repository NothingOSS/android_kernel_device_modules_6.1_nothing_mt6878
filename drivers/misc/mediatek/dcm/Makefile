ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dcm/include/

# Core
obj-$(CONFIG_MTK_DCM_COMMON)	+= mtk_dcm.o

# SoC Drivers
ifneq ($(wildcard $(srctree)/$(src)/mt6985_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6985) += mt6985_dcm.o
mt6985_dcm-objs := mt6985_dcm_internal.o mt6985_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6897_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6897) += mt6897_dcm.o
mt6897_dcm-objs := mt6897_dcm_internal.o mt6897_dcm_autogen.o
endif
