// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/gzvm_drv.h>
#include "gzvm_common.h"

int gzvm_irqchip_inject_irq(struct gzvm *gzvm, unsigned int vcpu_idx,
			    u32 irq_type, u32 irq, bool level)
{
	return gzvm_arch_inject_irq(gzvm, vcpu_idx, irq_type, irq, level);
}
