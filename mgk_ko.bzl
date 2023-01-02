load(
    "//build/kernel/kleaf:kernel.bzl",
    "kernel_module",
)
load(
    ":mgk.bzl",
    "device_modules_dir",
)


def define_mgk_ko(
        name,
        srcs = None,
        outs = None,
        deps = []):
    if srcs == None:
        srcs = native.glob([
            "**/*.c",
            "**/*.h",
            "**/Kbuild",
            "**/Makefile",
        ])
    if outs == None:
        outs = [name + ".ko"]
    kernel_module(
        name = "{}.eng".format(name),
        srcs = srcs,
        outs = outs,
        kernel_build = "//{}:mgk.eng".format(device_modules_dir),
        kernel_module_deps = [
            "//{}:device_modules.eng".format(device_modules_dir),
        ] + ["{}.eng".format(m) for m in deps],
    )
    kernel_module(
        name = "{}.userdebug".format(name),
        srcs = srcs,
        outs = outs,
        kernel_build = "//{}:mgk.userdebug".format(device_modules_dir),
        kernel_module_deps = [
            "//{}:device_modules.userdebug".format(device_modules_dir),
        ] + ["{}.userdebug".format(m) for m in deps],
    )
    kernel_module(
        name = "{}.user".format(name),
        srcs = srcs,
        outs = outs,
        kernel_build = "//{}:mgk.user".format(device_modules_dir),
        kernel_module_deps = [
            "//{}:device_modules.user".format(device_modules_dir),
        ] + ["{}.user".format(m) for m in deps],
    )
    kernel_module(
        name = "{}.ack".format(name),
        srcs = srcs,
        outs = outs,
        kernel_build = "//{}:mgk.ack".format(device_modules_dir),
        kernel_module_deps = [
            "//{}:device_modules.ack".format(device_modules_dir),
        ] + ["{}.ack".format(m) for m in deps],
    )
