load(
    "//build/kernel/kleaf:constants.bzl",
    "aarch64_gz_outs",
)
load(
    "//build/kernel/kleaf:kernel.bzl",
    "kernel_build",
    "kernel_images",
    "kernel_module",
    "kernel_modules_install",
)


common_kernel_dir = "common-mainline"
device_kernel_dir = "kernel-mainline"
device_modules_dir = "kernel_device_modules-mainline"


def define_mgk(
        name,
        kleaf_modules,
        common_modules,
        device_modules,
        device_eng_modules,
        device_userdebug_modules,
        device_user_modules):
    native.config_setting(
        name = "entry_level",
        define_values = {
            "entry_level": "true",
        },
    )

    native.filegroup(
        name = "device_sources",
        srcs = native.glob(
            ["**"],
            exclude = [
                "BUILD.bazel",
                "**/*.bzl",
                ".git/**",
            ],
        ),
    )
    native.filegroup(
        name = "device_configs",
        srcs = native.glob([
            "arch/arm64/configs/*",
            "kernel/configs/**",
            "**/Kconfig",
            "drivers/cpufreq/Kconfig.*",
        ]) + [
            "Kconfig.ext",
        ],
    )

    mgk_defconfig = name + "_defconfig"

    mgk_build_config(
        name = "kernel_build_config.eng",
        kernel_dir = device_kernel_dir,
        device_modules_dir = device_modules_dir,
        defconfig = mgk_defconfig,
        defconfig_overlays = [],
        build_config_overlays = [],
        build_variant = "eng",
        kleaf_modules = kleaf_modules,
        gki_mixed_build = False,
    )
    mgk_build_config(
        name = "kernel_build_config.userdebug",
        kernel_dir = device_kernel_dir,
        device_modules_dir = device_modules_dir,
        defconfig = mgk_defconfig,
        defconfig_overlays = [],
        build_config_overlays = [],
        build_variant = "userdebug",
        kleaf_modules = kleaf_modules,
        gki_mixed_build = False,
    )
    mgk_build_config(
        name = "kernel_build_config.user",
        kernel_dir = device_kernel_dir,
        device_modules_dir = device_modules_dir,
        defconfig = mgk_defconfig,
        defconfig_overlays = [],
        build_config_overlays = [],
        build_variant = "user",
        kleaf_modules = kleaf_modules,
        gki_mixed_build = False,
    )
    mgk_build_config(
        name = "build_config.eng",
        kernel_dir = device_kernel_dir,
        device_modules_dir = device_modules_dir,
        defconfig = mgk_defconfig,
        defconfig_overlays = [],
        build_config_overlays = [],
        build_variant = "eng",
        kleaf_modules = kleaf_modules,
        gki_mixed_build = True,
    )
    mgk_build_config(
        name = "build_config.userdebug",
        kernel_dir = device_kernel_dir,
        device_modules_dir = device_modules_dir,
        defconfig = mgk_defconfig,
        defconfig_overlays = [],
        build_config_overlays = [],
        build_variant = "userdebug",
        kleaf_modules = kleaf_modules,
        gki_mixed_build = True,
    )
    mgk_build_config(
        name = "build_config.user",
        kernel_dir = device_kernel_dir,
        device_modules_dir = device_modules_dir,
        defconfig = mgk_defconfig,
        defconfig_overlays = [],
        build_config_overlays = [],
        build_variant = "user",
        kleaf_modules = kleaf_modules,
        gki_mixed_build = True,
    )
    mgk_build_config(
        name = "build_config.ack",
        kernel_dir = common_kernel_dir,
        device_modules_dir = device_modules_dir,
        defconfig = mgk_defconfig,
        defconfig_overlays = [],
        build_config_overlays = [],
        build_variant = "user",
        kleaf_modules = kleaf_modules,
        gki_mixed_build = True,
    )

    kernel_build(
        name = "mgk.eng",
        srcs = [
            "//{}:kernel_aarch64_sources".format(device_kernel_dir),
            ":device_sources",
        ],
        outs = [
        ],
        module_outs = common_modules,
        build_config = ":build_config.eng",
        kconfig_ext = "Kconfig.ext",
        base_kernel = "//{}:kernel_aarch64.eng".format(device_kernel_dir),
    )
    kernel_build(
        name = "mgk.userdebug",
        srcs = [
            "//{}:kernel_aarch64_sources".format(device_kernel_dir),
            ":device_sources",
        ],
        outs = [
        ],
        module_outs = common_modules,
        build_config = ":build_config.userdebug",
        kconfig_ext = "Kconfig.ext",
        base_kernel = "//{}:kernel_aarch64.userdebug".format(device_kernel_dir),
    )
    kernel_build(
        name = "mgk.user",
        srcs = [
            "//{}:kernel_aarch64_sources".format(device_kernel_dir),
            ":device_sources",
        ],
        outs = [
        ],
        module_outs = common_modules,
        build_config = ":build_config.user",
        kconfig_ext = "Kconfig.ext",
        base_kernel = "//{}:kernel_aarch64.user".format(device_kernel_dir),
    )
    kernel_build(
        name = "mgk.ack",
        srcs = [
            "//{}:kernel_aarch64_sources".format(common_kernel_dir),
            ":device_sources",
        ],
        outs = [
        ],
        module_outs = common_modules,
        build_config = ":build_config.ack",
        kconfig_ext = "Kconfig.ext",
        base_kernel = "//{}:kernel_aarch64_debug".format(common_kernel_dir),
    )

    kernel_module(
        name = "device_modules.eng",
        srcs = [":device_sources"],
        outs = device_modules + device_eng_modules,
        kernel_build = ":mgk.eng",
    )
    kernel_module(
        name = "device_modules.userdebug",
        srcs = [":device_sources"],
        outs = device_modules + device_userdebug_modules,
        kernel_build = ":mgk.userdebug",
    )
    kernel_module(
        name = "device_modules.user",
        srcs = [":device_sources"],
        outs = device_modules + device_user_modules,
        kernel_build = ":mgk.user",
    )
    kernel_module(
        name = "device_modules.ack",
        srcs = [":device_sources"],
        outs = device_modules + device_user_modules,
        kernel_build = ":mgk.ack",
    )

    kernel_modules_install(
        name = "modules_install.eng",
        kernel_modules = [
            ":device_modules.eng",
        ] + ["{}.eng".format(m) for m in kleaf_modules],
        kernel_build = ":mgk.eng",
    )
    kernel_modules_install(
        name = "modules_install.userdebug",
        kernel_modules = [
            ":device_modules.userdebug",
        ] + ["{}.userdebug".format(m) for m in kleaf_modules],
        kernel_build = ":mgk.userdebug",
    )
    kernel_modules_install(
        name = "modules_install.user",
        kernel_modules = [
            ":device_modules.user",
        ] + ["{}.user".format(m) for m in kleaf_modules],
        kernel_build = ":mgk.user",
    )
    kernel_modules_install(
        name = "modules_install.ack",
        kernel_modules = [
            ":device_modules.ack",
        ] + ["{}.ack".format(m) for m in kleaf_modules],
        kernel_build = ":mgk.ack",
    )


def _mgk_build_config_impl(ctx):
    ext_content = []
    ext_content.append("EXT_MODULES=\"")
    ext_content.append(ctx.attr.device_modules_dir)
    has_fpsgo = False
    has_met = False
    for m in ctx.attr.kleaf_modules:
        path = m.partition(":")[0].removeprefix("//")
        if "fpsgo" in path:
            has_fpsgo = True
        elif "met_drv" in path:
            has_met = True
        else:
            ext_content.append(path)
    ext_content.append("\"")
    if has_fpsgo:
        ext_content.append("""
if [ -d "vendor/mediatek/kernel_modules/fpsgo_int" ]; then
EXT_MODULES+=" vendor/mediatek/kernel_modules/fpsgo_int"
else
EXT_MODULES+=" vendor/mediatek/kernel_modules/fpsgo_cus"
fi""")
    if has_met:
        ext_content.append("")
        ext_content.append("EXT_MODULES+=\" vendor/mediatek/kernel_modules/met_drv_v3\"")
        ext_content.append("""if [ -d "vendor/mediatek/kernel_modules/met_drv_secure_v3" ]; then
EXT_MODULES+=" vendor/mediatek/kernel_modules/met_drv_secure_v3"
fi""")
        ext_content.append("EXT_MODULES+=\" vendor/mediatek/kernel_modules/met_drv_v3/met_api\"")
    content = []
    content.append("DEVICE_MODULES_DIR={}".format(ctx.attr.device_modules_dir))
    content.append("KERNEL_DIR={}".format(ctx.attr.kernel_dir))
    #content.append("DEVICE_MODULES_REL_DIR=$(rel_path {} {})".format(ctx.attr.device_modules_dir, ctx.attr.kernel_dir))
    content.append("DEVICE_MODULES_REL_DIR=../{}".format(ctx.attr.device_modules_dir))
    content.append("""
. ${ROOT_DIR}/${KERNEL_DIR}/build.config.common
. ${ROOT_DIR}/${KERNEL_DIR}/build.config.gki
. ${ROOT_DIR}/${KERNEL_DIR}/build.config.aarch64

DEVICE_MODULES_PATH="\\$(srctree)/\\$(DEVICE_MODULES_REL_DIR)"
DEVCIE_MODULES_INCLUDE="-I\\$(DEVICE_MODULES_PATH)/include"
""")
    defconfig = []
    defconfig.append("${ROOT_DIR}/${KERNEL_DIR}/arch/arm64/configs/gki_defconfig")
    defconfig.append("${ROOT_DIR}/" + ctx.attr.device_modules_dir + "/arch/arm64/configs/${DEFCONFIG}")
    if ctx.attr.build_variant == "eng":
        defconfig.append("${ROOT_DIR}/" + ctx.attr.device_modules_dir + "/kernel/configs/eng.config")
    elif ctx.attr.build_variant == "userdebug":
        defconfig.append("${ROOT_DIR}/" + ctx.attr.device_modules_dir + "/kernel/configs/userdebug.config")
    content.append("DEFCONFIG={}".format(ctx.attr.defconfig))
    content.append("PRE_DEFCONFIG_CMDS=\"KCONFIG_CONFIG=${ROOT_DIR}/${KERNEL_DIR}/arch/arm64/configs/${DEFCONFIG} ${ROOT_DIR}/${KERNEL_DIR}/scripts/kconfig/merge_config.sh -m -r " + " ".join(defconfig) + "\"")
    content.append("POST_DEFCONFIG_CMDS=\"rm ${ROOT_DIR}/${KERNEL_DIR}/arch/arm64/configs/${DEFCONFIG}\"")
    content.append("")
    content.extend(ext_content)
    content.append("")

    if ctx.attr.gki_mixed_build:
        content.append("MAKE_GOALS=\"modules\"")
        content.append("FILES=\"\"")
    else:
        content.append("MAKE_GOALS=\"${MAKE_GOALS} Image.lz4 Image.gz\"")
        content.append("FILES=\"${FILES} arch/arm64/boot/Image.lz4 arch/arm64/boot/Image.gz\"")

    build_config_file = ctx.actions.declare_file("{}/build.config".format(ctx.attr.name))
    ctx.actions.write(
        output = build_config_file,
        content = "\n".join(content) + "\n",
    )
    return DefaultInfo(files = depset([build_config_file]))


mgk_build_config = rule(
    implementation = _mgk_build_config_impl,
    doc = "Defines a kernel build.config target.",
    attrs = {
        "kernel_dir": attr.string(mandatory = True),
        "device_modules_dir": attr.string(mandatory = True),
        "defconfig": attr.string(mandatory = True),
        "defconfig_overlays": attr.string_list(),
        "build_config_overlays": attr.string_list(),
        "kleaf_modules": attr.string_list(),
        "build_variant": attr.string(mandatory = True),
        "gki_mixed_build": attr.bool(),
    },
)
