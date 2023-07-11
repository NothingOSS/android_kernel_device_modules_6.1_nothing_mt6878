#!/bin/bash

set -e

ROOT_DIR=$(readlink -f $PWD)
DEVICE_MODULES_DIR=$(basename $(dirname $0))

echo ROOT_DIR: ${ROOT_DIR}
echo DEVICE_MODULES_DIR: ${DEVICE_MODULES_DIR}

if [ -z ${KERNEL_VERSION} ]
then
  echo "ERROR: KERNEL_VERSION must be set!"
  exit 1
fi

KERNEL_OUT=${OUT}
if [ -z ${KERNEL_OUT} ]
then
  KERNEL_OUT=${ROOT_DIR}/out
fi
if ! [ "x${KERNEL_OUT}" = "x/*" ]
then
  KERNEL_OUT=$(readlink -f ${ROOT_DIR}/${KERNEL_OUT})
fi

if [ "x${DEBUG}" == "x1" ]
then
DEBUG_ARGS="--sandbox_debug --verbose_failures"
fi
if [ -z ${SANDBOX} ]
then
  SANDBOX=1
else
  if [ "x${SANDBOX}" == "x0" ]
  then
    SANDBOX_ARGS="--config=local"
  fi
fi

update_list_cmd="tools/bazel --output_user_root=${KERNEL_OUT} --output_base=${KERNEL_OUT}/bazel/output_user_root/output_base run ${DEBUG_ARGS} ${SANDBOX_ARGS} --experimental_writable_outputs --//build/bazel_mgk_rules:kernel_version=${KERNEL_VERSION} //${DEVICE_MODULES_DIR}:mgk.user_abi_update_symbol_list"
${update_list_cmd}
echo run: ${update_list_cmd}

update_abi_cmd="tools/bazel --output_user_root=${KERNEL_OUT} --output_base=${KERNEL_OUT}/bazel/output_user_root/output_base run ${DEBUG_ARGS} ${SANDBOX_ARGS} --experimental_writable_outputs //kernel-${KERNEL_VERSION}:kernel_aarch64_abi_update"
${update_abi_cmd}
echo run: ${update_abi_cmd}
