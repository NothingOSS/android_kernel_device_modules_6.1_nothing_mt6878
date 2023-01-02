// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "clkbuf-util.h"
#include "clkbuf-ctrl.h"
#include "platform-hdlr.h"

struct match_platform {
	char *name;
	struct platform_hdlr *hdlr;
	int (*init)(struct clkbuf_dts *array, struct match_platform *match);
};

int read_with_ofs(struct clkbuf_hw *hw, struct reg_t *reg, u32 *val, u32 ofs)
{
	int ret = 0;

	if (!reg)
		return -EREG_NOT_SUPPORT;

	if (!reg->mask)
		return -EREG_NOT_SUPPORT;

	*val = 0;

	switch (hw->hw_type) {
	case PMIC:
		ret = regmap_read(hw->base.map, reg->ofs + ofs, val);
		(*val) = ((*val) & (reg->mask << reg->shift)) >> reg->shift;
		break;
	case SRCLKEN_CFG:
		(*val) = (readl(hw->base.cfg + reg->ofs + ofs) &
			  (reg->mask << reg->shift)) >>
			 reg->shift;
		break;
	case SRCLKEN_STA:
		(*val) = (readl(hw->base.sta + reg->ofs + ofs) &
			  (reg->mask << reg->shift)) >>
			 reg->shift;
		break;
	case PMIF_M:
		(*val) = (readl(hw->base.pmif_m + reg->ofs + ofs) &
			  (reg->mask << reg->shift)) >>
			 reg->shift;
		break;
	case PMIF_P:
		(*val) = (readl(hw->base.pmif_p + reg->ofs + ofs) &
			  (reg->mask << reg->shift)) >>
			 reg->shift;
		break;
	default:
		ret = -EREG_NOT_SUPPORT;
		CLKBUF_DBG("platform-hdlr read failed: %d\n", ret);
		break;
	}

	return ret;
}

int write_with_ofs(struct clkbuf_hw *hw, struct reg_t *reg, u32 val, u32 ofs)
{
	int ret = 0;
	u32 mask;

	if (!reg)
		return -EREG_NOT_SUPPORT;

	if (!reg->mask)
		return -EREG_NOT_SUPPORT;

	switch (hw->hw_type) {
	case PMIC:
		mask = reg->mask << reg->shift;
		val <<= reg->shift;
		ret = regmap_update_bits(hw->base.map, reg->ofs + ofs, mask,
					 val);
		break;
	case SRCLKEN_CFG:
		writel((readl(hw->base.cfg + reg->ofs + ofs) &
			(~(reg->mask << reg->shift))) |
			       (val << reg->shift),
		       hw->base.cfg + reg->ofs + ofs);
		break;
	case SRCLKEN_STA:
		writel((readl(hw->base.sta + reg->ofs + ofs) &
			(~(reg->mask << reg->shift))) |
			       (val << reg->shift),
		       hw->base.sta + reg->ofs + ofs);
		break;
	case PMIF_M:
		writel((readl(hw->base.pmif_m + reg->ofs + ofs) &
			(~(reg->mask << reg->shift))) |
			       (val << reg->shift),
		       hw->base.pmif_m + reg->ofs + ofs);
		break;
	case PMIF_P:
		writel((readl(hw->base.pmif_p + reg->ofs + ofs) &
			(~(reg->mask << reg->shift))) |
			       (val << reg->shift),
		       hw->base.pmif_p + reg->ofs + ofs);
		break;
	default:
		ret = -EREG_NOT_SUPPORT;
		CLKBUF_DBG("platform-hdlr write failed: %d\n", ret);
		break;
	}

	return ret;
}

int platform_read(struct clkbuf_hw *hw, struct reg_t *reg, u32 *val)
{
	return read_with_ofs(hw, reg, val, 0);
}

int platform_write(struct clkbuf_hw *hw, struct reg_t *reg, u32 val)
{
	return write_with_ofs(hw, reg, val, 0);
}

/* example: */
//static int __mtxxxx_get_pmrcen (void *data, u32 *out) {
//
//	CLKBUF_DBG("mtxxxx in\n");
//	return 0;
//}

static int mtxxxx_init(struct clkbuf_dts *array, struct match_platform *match)
{
	struct clkbuf_hdlr *hdlr;

	if (!array)
		return -EINVAL;

	hdlr = array->hdlr;

	CLKBUF_DBG("array<%lx> type: %d\n", (unsigned long)array, array->hw.hw_type);

	switch (array->hw.hw_type) {
	case PMIC:

		/* implementation here, overwrite original func */
		/* example: */
		//	if(!hdlr->ops->get_pmrcen)
		//		break;
		//	hdlr->ops->get_pmrcen = match->hdlr->ops->get_pmrcen;
		//	match->hdlr->ops->get_pmrcen(NULL, NULL);
		break;
	case SRCLKEN_CFG:
		break;
	case SRCLKEN_STA:
		break;
	case PMIF_M:
		break;
	case PMIF_P:
		break;
	default:
		CLKBUF_DBG("not handle array[%lx]: hw_type %d\n",
			   (unsigned long)array, array->hw.hw_type);
		break;
	}

	return 0;
}

/*use call back if we need to create debug node*/
static struct platform_operation mtxxxx_ops = {
	/* example: */
//	.get_pmrcen = __mtxxxx_get_pmrcen,
};

/*if adb write function is not enough, then we need*/
//1. define following call back
//2. create platform hdlr node
//3. create adb debug node and hook mtxxxx special implementation

static struct platform_hdlr mtxxxx_hdlr = {
	.ops = &mtxxxx_ops,
};

static struct match_platform mtxxxx_match = {
	.name = "mediatek,mtxxxx-clkbuf",
	.hdlr = &mtxxxx_hdlr,
	.init = &mtxxxx_init,
};

static struct match_platform *matches_platform[] = {
	&mtxxxx_match,
	NULL,
};

int count_plat_hdlr_node(struct device_node *clkbuf_node)
{
	/*create a node to represent platform handler management*/
	return 1;
}

int clkbuf_platform_init(struct clkbuf_dts *array, struct device *dev)
{
	struct match_platform **match_platform = matches_platform;
	const struct of_device_id *match;
	struct device_node *root;
	int i;
	int nums = array->nums;
	char *target;

	/*handle for special platform feaute, not affect MP chip*/
	CLKBUF_DBG("\n");

	root = dev->of_node;
	match = of_match_node(dev->driver->of_match_table, root);
	target = (char *)match->compatible;

	/* find match by compatible */
	for (; (*match_platform) != NULL; match_platform++) {
		char *comp = (*match_platform)->name;

		if (strcmp(comp, target) == 0)
			break;
	}

	if (*match_platform == NULL) {
		CLKBUF_DBG("no match platform compatible!\n");
		return -1;
	}

	for (i = 0; i < nums; i++, array++) {
		/* init flow */
		(*match_platform)->init(array, *match_platform);
	}
	CLKBUF_DBG("\n");
	return 0;
}
