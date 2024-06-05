// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef HL5280_I2C_H
#define HL5280_I2C_H

#include <linux/of.h>
#include <linux/notifier.h>

enum hl_function {
	HL_MIC_GND_SWAP,
	HL_USBC_ORIENTATION_CC1,
	HL_USBC_ORIENTATION_CC2,
	HL_USBC_DISPLAYPORT_DISCONNECTED,
	HL_EVENT_MAX,
};

#if 1 // IS_ENABLED(CONFIG_USB_SWITCH_HL5280)
int hl5280_switch_event(struct device_node *node,
					enum hl_function event);
int hl5280_reg_notifier(struct notifier_block *nb,
					struct device_node *node);
int hl5280_unreg_notifier(struct notifier_block *nb,
					struct device_node *node);
#else
static inline int hl5280_switch_event(struct device_node *node,
								enum hl_function event)
{
		return 0;
}

static inline int hl5280_reg_notifier(struct notifier_block *node,
								enum device_node event)
{
		return 0;
}

static inline int hl5280_unreg_notifier(struct notifier_block *node,
								enum device_node event)
{
		return 0;
}
#endif /* CONFIG_MTK_HL5280_I2C */
#endif /* HL5280_I2C_H */