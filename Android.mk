# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2019 MediaTek Inc.

LOCAL_PATH := $(call my-dir)

ifeq ($(word 2,$(subst -, ,$(notdir $(LOCAL_PATH)))),$(word 2,$(subst -, ,$(strip $(LINUX_KERNEL_VERSION)))))

include $(LOCAL_PATH)/kenv.mk

ifeq ($(wildcard $(TARGET_PREBUILT_KERNEL)),)
KERNEL_MAKE_DEPENDENCIES := $(shell find $(KERNEL_DIR) -name .git -prune -o -type f | sort)
KERNEL_MAKE_DEPENDENCIES += $(shell find vendor/mediatek/kernel_modules -name .git -prune -o -type f | sort)
ifdef MTK_GKI_PREBUILTS_DIR
KERNEL_MAKE_DEPENDENCIES += $(wildcard $(MTK_GKI_PREBUILTS_DIR)/*)
endif
ifdef MTK_GKI_BUILD_CONFIG
KERNEL_MAKE_DEPENDENCIES += $(shell find kernel/$(REL_ACK_DIR)/ -name .git -prune -o -type f | sort)
endif
ifneq ($(wildcard kernel/build),)
KERNEL_MAKE_DEPENDENCIES += $(shell find kernel/build -name .git -prune -o -type f | sort)
endif
ifneq ($(wildcard vendor/mediatek/tests),)
KERNEL_MAKE_DEPENDENCIES += $(shell find vendor/mediatek/tests/kernel/ktf_testcase -name .git -prune -o -type f | sort)
KERNEL_MAKE_DEPENDENCIES += $(shell find vendor/mediatek/tests/ktf/kernel -name .git -prune -o -type f | sort)
endif

$(GEN_KERNEL_BUILD_CONFIG): PRIVATE_GEN_BUILD_CONFIG := $(REL_KERNEL_DIR)/scripts/gen_build_config.py
$(GEN_KERNEL_BUILD_CONFIG): PRIVATE_KERNEL_DEFCONFIG := $(KERNEL_DEFCONFIG)
$(GEN_KERNEL_BUILD_CONFIG): PRIVATE_KERNEL_DEFCONFIG_OVERLAYS := $(KERNEL_DEFCONFIG_OVERLAYS)
$(GEN_KERNEL_BUILD_CONFIG): PRIVATE_KERNEL_BUILD_CONFIG := $(REL_GEN_KERNEL_BUILD_CONFIG)
$(GEN_KERNEL_BUILD_CONFIG): PRIVATE_KERNEL_BUILD_CONFIG_OVERLAYS := $(addprefix $(REL_KERNEL_DIR)/,$(KERNEL_BUILD_CONFIG_OVERLAYS))
$(GEN_KERNEL_BUILD_CONFIG): $(KERNEL_DIR)/kernel/configs/ext_modules.list
$(GEN_KERNEL_BUILD_CONFIG): $(KERNEL_DIR)/scripts/gen_build_config.py $(wildcard $(KERNEL_DIR)/build.config.*) $(build_config_file) $(KERNEL_CONFIG_FILE) $(LOCAL_PATH)/Android.mk
	$(hide) mkdir -p $(dir $@)
	$(hide) cd kernel && python $(PRIVATE_GEN_BUILD_CONFIG) --kernel-defconfig $(PRIVATE_KERNEL_DEFCONFIG) --kernel-defconfig-overlays "$(PRIVATE_KERNEL_DEFCONFIG_OVERLAYS)" --kernel-build-config-overlays "$(PRIVATE_KERNEL_BUILD_CONFIG_OVERLAYS)" -m $(TARGET_BUILD_VARIANT) -o $(PRIVATE_KERNEL_BUILD_CONFIG) && cd ..

ifeq (yes,$(strip $(BUILD_KERNEL)))
ifneq ($(KERNEL_USE_BAZEL),yes)
$(KERNEL_ZIMAGE_OUT): .KATI_IMPLICIT_OUTPUTS += $(TARGET_KERNEL_CONFIG)
$(KERNEL_ZIMAGE_OUT): PRIVATE_DIR := $(KERNEL_DIR)
$(KERNEL_ZIMAGE_OUT): PRIVATE_KERNEL_OUT := $(REL_KERNEL_OUT)
$(KERNEL_ZIMAGE_OUT): PRIVATE_DIST_DIR := $(REL_KERNEL_OUT)/dist
$(KERNEL_ZIMAGE_OUT): PRIVATE_CC_WRAPPER := $(CCACHE_EXEC)
$(KERNEL_ZIMAGE_OUT): PRIVATE_KERNEL_BUILD_CONFIG := $(REL_GEN_KERNEL_BUILD_CONFIG)
ifeq (user,$(strip $(KERNEL_BUILD_VARIANT)))
ifneq (,$(strip $(shell grep "^CONFIG_ABI_MONITOR\s*=\s*y" $(KERNEL_CONFIG_FILE))))
$(KERNEL_ZIMAGE_OUT): PRIVATE_KERNEL_BUILD_SCRIPT := ./build/build.sh
else
$(KERNEL_ZIMAGE_OUT): PRIVATE_KERNEL_BUILD_SCRIPT := ./build/build.sh
endif
else
$(KERNEL_ZIMAGE_OUT): PRIVATE_KERNEL_BUILD_SCRIPT := ./build/build.sh
endif
$(KERNEL_ZIMAGE_OUT): $(GEN_KERNEL_BUILD_CONFIG) $(KERNEL_MAKE_DEPENDENCIES) | kernel-outputmakefile
	$(hide) mkdir -p $(dir $@)
	$(hide) cd kernel && CC_WRAPPER=$(PRIVATE_CC_WRAPPER) SKIP_MRPROPER=1 BUILD_CONFIG=$(PRIVATE_KERNEL_BUILD_CONFIG) OUT_DIR=$(PRIVATE_KERNEL_OUT) DIST_DIR=$(PRIVATE_DIST_DIR) $(PRIVATE_KERNEL_BUILD_SCRIPT) && cd ..
	$(hide) $(call fixup-kernel-cmd-file,$(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/compressed/.piggy.xzkern.cmd)
else
$(KERNEL_ZIMAGE_OUT): PRIVATE_BAZEL_BUILD_FLAG := $(addprefix --//build/bazel_mgk_rules:,kernel_version=$(patsubst kernel-%,%,$(LINUX_KERNEL_VERSION)) $(patsubst %.config,%_config,$(KERNEL_DEFCONFIG_OVERLAYS))) --experimental_writable_outputs
ifneq ($(filter yes no,$(COVERITY_LOCAL_SCAN)),)
$(KERNEL_ZIMAGE_OUT): PRIVATE_BAZEL_BUILD_FLAG += --config=local
endif
$(KERNEL_ZIMAGE_OUT): PRIVATE_BAZEL_BUILD_OUT := $(KERNEL_BAZEL_BUILD_OUT)
$(KERNEL_ZIMAGE_OUT): PRIVATE_BAZEL_DIST_OUT := $(KERNEL_BAZEL_DIST_OUT)
ifneq ($(wildcard vendor/mediatek/tests/kernel),)
$(KERNEL_ZIMAGE_OUT): PRIVATE_BAZEL_BUILD_GOAL := //$(patsubst kernel/%,%,$(KERNEL_DIR)):mgk_internal_modules_install.$(strip $(KERNEL_BUILD_VARIANT))
$(KERNEL_ZIMAGE_OUT): PRIVATE_BAZEL_DIST_GOAL := //$(patsubst kernel/%,%,$(KERNEL_DIR)):mgk_internal_dist.$(strip $(KERNEL_BUILD_VARIANT))
else
$(KERNEL_ZIMAGE_OUT): PRIVATE_BAZEL_BUILD_GOAL := //$(patsubst kernel/%,%,$(KERNEL_DIR)):mgk_customer_modules_install.$(strip $(KERNEL_BUILD_VARIANT))
$(KERNEL_ZIMAGE_OUT): PRIVATE_BAZEL_DIST_GOAL := //$(patsubst kernel/%,%,$(KERNEL_DIR)):mgk_customer_dist.$(strip $(KERNEL_BUILD_VARIANT))
endif
$(KERNEL_ZIMAGE_OUT): $(KERNEL_MAKE_DEPENDENCIES)
	$(hide) cd kernel && export BAZEL_DO_NOT_DETECT_CPP_TOOLCHAIN=1 && tools/bazel --output_root=$(abspath $(PRIVATE_BAZEL_BUILD_OUT)) --output_base=$(abspath $(PRIVATE_BAZEL_BUILD_OUT))/bazel/output_user_root/output_base build $(PRIVATE_BAZEL_BUILD_FLAG) $(PRIVATE_BAZEL_BUILD_GOAL)
	$(hide) cd kernel && export BAZEL_DO_NOT_DETECT_CPP_TOOLCHAIN=1 && tools/bazel --output_root=$(abspath $(PRIVATE_BAZEL_BUILD_OUT)) --output_base=$(abspath $(PRIVATE_BAZEL_BUILD_OUT))/bazel/output_user_root/output_base run $(PRIVATE_BAZEL_BUILD_FLAG) $(PRIVATE_BAZEL_DIST_GOAL) -- --dist_dir=$(abspath $(PRIVATE_BAZEL_DIST_OUT))
endif

ifneq ($(BUILT_KERNEL_TARGET),$(KERNEL_ZIMAGE_OUT))
$(BUILT_KERNEL_TARGET): $(KERNEL_ZIMAGE_OUT) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-target)
endif

$(TARGET_PREBUILT_KERNEL): $(BUILT_KERNEL_TARGET) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-new-target)

endif#BUILD_KERNEL
endif #TARGET_PREBUILT_KERNEL is empty

ifeq (yes,$(strip $(BUILD_KERNEL)))
ifneq ($(strip $(TARGET_NO_KERNEL)),true)
$(INSTALLED_KERNEL_TARGET): $(BUILT_KERNEL_TARGET) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-target)
endif#TARGET_NO_KERNEL
endif#BUILD_KERNEL

.PHONY: kernel save-kernel kernel-menuconfig menuconfig-kernel savedefconfig-kernel clean-kernel
kernel: $(INSTALLED_KERNEL_TARGET)
kernel: $(KERNEL_ZIMAGE_OUT)
save-kernel: $(TARGET_PREBUILT_KERNEL)

kernel-menuconfig:
	$(hide) mkdir -p $(KERNEL_OUT)
	$(PREBUILT_MAKE_PREFIX)/$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) menuconfig

menuconfig-kernel savedefconfig-kernel:
	$(hide) mkdir -p $(KERNEL_OUT)
	$(PREBUILT_MAKE_PREFIX)/$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(patsubst %config-kernel,%config,$@)

ifneq ($(KERNEL_USE_BAZEL),yes)
clean-kernel:
	$(hide) rm -rf $(KERNEL_OUT) $(INSTALLED_KERNEL_TARGET)
else
clean-kernel: PRIVATE_BAZEL_BUILD_OUT := $(KERNEL_BAZEL_BUILD_OUT)
clean-kernel: PRIVATE_BAZEL_DIST_OUT := $(KERNEL_BAZEL_DIST_OUT)
clean-kernel:
	$(hide) cd kernel && export BAZEL_DO_NOT_DETECT_CPP_TOOLCHAIN=1 && tools/bazel --output_root=$(abspath $(PRIVATE_BAZEL_BUILD_OUT)) --output_base=$(abspath $(PRIVATE_BAZEL_BUILD_OUT))/bazel/output_user_root/output_base clean
	$(hide) rm -rf $(PRIVATE_BAZEL_BUILD_OUT) $(PRIVATE_BAZEL_DIST_OUT)
endif

.PHONY: kernel-outputmakefile
kernel-outputmakefile: PRIVATE_DIR := kernel/$(REL_ACK_DIR)
kernel-outputmakefile: PRIVATE_KERNEL_OUT := $(shell bash $(dir $(mkfile_path))scripts/get_rel_path.sh $(KERNEL_OUT) kernel/$(REL_ACK_DIR))
kernel-outputmakefile:
	$(PREBUILT_MAKE_PREFIX)/$(MAKE) -C $(PRIVATE_DIR) O=$(PRIVATE_KERNEL_OUT) outputmakefile

### DTB build template
MTK_DTBIMAGE_DTS := $(addsuffix .dts,$(addprefix $(KERNEL_DIR)/arch/$(KERNEL_TARGET_ARCH)/boot/dts/,$(PLATFORM_DTB_NAME)))
include device/mediatek/build/core/build_dtbimage.mk

MTK_DTBOIMAGE_DTS := $(addsuffix .dts,$(addprefix $(KERNEL_DIR)/arch/$(KERNEL_TARGET_ARCH)/boot/dts/,$(PROJECT_DTB_NAMES)))
include device/mediatek/build/core/build_dtboimage.mk

endif #LINUX_KERNEL_VERSION
