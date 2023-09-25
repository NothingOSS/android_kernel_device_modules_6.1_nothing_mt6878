# SPDX-License-Identifier: GPL-2.0
#!/bin/bash

set -e

DEVICE_MODULES_DIR=$(basename $(dirname $0))
source "${DEVICE_MODULES_DIR}/kernel/kleaf/_setup_env.sh"

# run kleaf commands or legacy build.sh
result=$(echo ${KLEAF_SUPPORTED_PROJECTS} | grep -wo ${PROJECT}) || result=""
if [[ ${result} != "" ]]
then # run kleaf commands

if [ -z ${TARGET} ]
then
  TARGET=internal_modules_install
  KLEAF_BUILD_TARGET=//${DEVICE_MODULES_DIR}:${PROJECT}_${TARGET}.${MODE}
else
  KLEAF_BUILD_TARGET=${TARGET}.${MODE}
fi
KLEAF_DIST_TARGET=//${DEVICE_MODULES_DIR}:${PROJECT}_internal_dist.${MODE}

KLEAF_OUT=("--output_user_root=${OUT_DIR} --output_base=${OUT_DIR}/bazel/output_user_root/output_base")
KLEAF_ARGS=("${DEBUG_ARGS} ${SANDBOX_ARGS} \
	--experimental_writable_outputs \
	--//build/bazel_mgk_rules:kernel_version=${KERNEL_VERSION_NUM}")

set -x
(
  export ${BAZEL_EXPORT_ENV} && \
	tools/bazel ${KLEAF_OUT} build ${KLEAF_ARGS} ${KLEAF_BUILD_TARGET}
  export ${BAZEL_EXPORT_ENV} && \
	tools/bazel ${KLEAF_OUT} run ${KLEAF_ARGS} \
	--nokmi_symbol_list_violations_check ${KLEAF_DIST_TARGET} -- --dist_dir=${OUT_DIR}/dist
)
set +x

else # run legacy build.sh
set -x
(
  OUT_DIR=${OUT_DIR} python ${DEVICE_MODULES_DIR}/scripts/gen_build_config.py -p ${PROJECT} \
	-o ${BUILD_CONFIG}.legacy -m ${MODE} --kernel-defconfig-overlays "${DEFCONFIG_OVERLAYS}"
  OUT_DIR=${OUT_DIR} BUILD_CONFIG=${BUILD_CONFIG}.legacy CC_WRAPPER=${CC_WRAPPER} build/build.sh
)
set +x
fi
