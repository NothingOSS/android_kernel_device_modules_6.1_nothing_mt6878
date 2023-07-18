# SPDX-License-Identifier: GPL-2.0
#!/bin/bash

set -e

DEVICE_MODULES_DIR=$(basename $(dirname $0))
source "${DEVICE_MODULES_DIR}/kernel/kleaf/_setup_env.sh"

# run kleaf commands or legacy build.sh
result=$(echo ${KLEAF_SUPPORTED_PROJECTS} | grep -wo ${PROJECT}) || result=""
if [[ ${result} != "" ]] && [[ ${DEFCONFIG_OVERLAYS} == "" ]]
then # run kleaf commands

#FIXME
PROJECT=mgk

KLEAF_OUT=("--output_user_root=${OUT_DIR} --output_base=${OUT_DIR}/bazel/output_user_root/output_base")
KLEAF_ARGS=("${DEBUG_ARGS} ${SANDBOX_ARGS} --experimental_writable_outputs")

set -x
(
  tools/bazel ${KLEAF_OUT} run ${KLEAF_ARGS} --//build/bazel_mgk_rules:kernel_version=${KERNEL_VERSION} //${DEVICE_MODULES_DIR}:${PROJECT}.user_abi_update_symbol_list
  tools/bazel ${KLEAF_OUT} run ${KLEAF_ARGS} //kernel-${KERNEL_VERSION}:kernel_aarch64_abi_update
)

else
  echo "Cannnot support ABI check for ${PROJECT}!"
  exit 1
fi
