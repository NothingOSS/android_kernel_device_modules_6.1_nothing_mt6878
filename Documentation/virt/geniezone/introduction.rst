.. SPDX-License-Identifier: GPL-2.0

======================
GenieZone Introduction
======================


Overview
========
GenieZone hypervisor(gzvm) is MediaTek proprietary solution for virtualization
on embedded system, and it is running in EL2 stand alone as a type-I
hypervisor. It is a pure EL2 implementation which implies it does not rely any
specific host VM, and this behavior improves GenieZone's security as it limits
its interface.

GenieZone hypervisor also supports most of the features from Android
Virtualization Framework(AVF) such as establishing protection of memory,
guest VM memory sharing to host for VirtIO, host memory sharing to guest VM
(restricted DMA), MMIO supporting, host PSCI supporting...etc.

To get the guest VMs up and running, the driver for GenieZone hypervisor
(gzvm-ko) is provided in this patches series. It wraps up the generaic driver
logic and architecture depedent operations for Virtual Machine Monitor(VMM)
like Crosvm to control guest VMs via GenieZone hypervisor. Further detail of
gzvm-ko is described in the following section.

Description
===========

Overall speaking, the gzvm-ko could be sperated into 2 parts, the driver layer
for bindings and firmware layer to wrap up ARM dependent implementations. The
driver layer would provide interface for virtual machine monitor(VMM) to talk
to hypervisor e.g. GenieZone hypervisor(also called gzvm for short), and code
path would be located in `drivers/virt/geniezone/`. Meanwhile, the firmware
layer would provide the architecture dependent(aarch64 only for now)
implementations, and the code path would be located in `arch/arm64/geniezone/`.

|----------|---------|-----------|--------------|------------|----------------|
|  driver  | gzvm_vm | gzvm_vcpu | gzvm_irqchip | gzvm_irqfd | gzvm_ioeventfd |
|----------|---------|-----------|--------------|------------|----------------|
| firmware |   vm    |    vcpu   |     vgic     |
|----------|---------|-----------|--------------|

The functions in driver layer provide operations to virtual machines via
GenieZone hypervisor.

The functions in firmware layers are basically related to architecture
dependent implementations especially ARM specific since GenieZone
hypervisor only supports aarch64 for now.

For vm and vcpu firmware, wrappers of HVC are provided for vm and vcpu
lifecycle control, such as create and destroy operations.

Interfaces of GET_ONE_REG/SET_ONE_REG are also provided in vcpu firmware for
virtual timer control.

For vgic firmware, we leverage GIC protocol to provide virtual interrupt
injections.


Supported Architecture
======================
GenieZone now only supports MTK SoC with ARM aarch64 and SMMU.


Platform Virtualization
=======================
We leverages arm64's timer virtualization and GIC virtualization for timer and
interrupts controller.


Device Virtualizaton
====================
We simply pass the I/O trap to VMM for device emulation in a general way.
