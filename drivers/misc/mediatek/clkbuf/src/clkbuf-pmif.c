// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
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
#include "clkbuf-pmif.h"

struct match_pmif {
	char *name;
	struct clkbuf_hdlr *hdlr;
	int (*init)(struct clkbuf_dts *array, struct match_pmif *match);
};

static int read_with_ofs(struct clkbuf_hw *hw, struct reg_t *reg, u32 *val,
			 u32 ofs)
{
	int ret = 0;

	if (!reg)
		return -EREG_NOT_SUPPORT;

	if (!reg->mask)
		return -EREG_NOT_SUPPORT;

	*val = 0;

	switch (hw->hw_type) {
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
		CLKBUF_DBG("pmif read failed: %d\n", ret);
		break;
	}

	return ret;
}

static int write_with_ofs(struct clkbuf_hw *hw, struct reg_t *reg, u32 val,
			  u32 ofs)
{
	int ret = 0;

	if (!reg)
		return -EREG_NOT_SUPPORT;

	if (!reg->mask)
		return -EREG_NOT_SUPPORT;

	switch (hw->hw_type) {
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
		CLKBUF_DBG("pmif write failed: %d\n", ret);
		break;
	}

	return ret;
}

static int pmif_read(struct clkbuf_hw *hw, struct reg_t *reg, u32 *val)
{
	return read_with_ofs(hw, reg, val, 0);
}

static int pmif_write(struct clkbuf_hw *hw, struct reg_t *reg, u32 val)
{
	return write_with_ofs(hw, reg, val, 0);
}

static int pmif_init_v1(struct clkbuf_dts *array, struct match_pmif *match)
{
	struct clkbuf_hdlr *hdlr = match->hdlr;
	struct plat_pmifdata *pd;
	static DEFINE_SPINLOCK(lock);

	CLKBUF_DBG("array<%x>,%s %d, id<%d>\n", array, array->pmif_name,
		   array->hw.hw_type, array->pmif_id);

	pd = (struct plat_pmifdata *)(hdlr->data);
	pd->hw = array->hw;
	pd->lock = &lock;

	/* hook hdlr to array */
	array->hdlr = hdlr;
	return 0;
}

static int __set_pmif_inf(void *data, int cmd, int pmif_id, int onoff)
{
	struct plat_pmifdata *pd = (struct plat_pmifdata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct pmif_m *pmif_m;
	struct reg_t reg;
	unsigned long flags = 0;
	int ret = 0;
	spinlock_t *lock = pd->lock;

	spin_lock_irqsave(lock, flags);

	pmif_m = pd->pmif_m;
	if (!pmif_m) {
		CLKBUF_DBG("pmif_m is null");
		goto WRITE_FAIL;
	}

	CLKBUF_DBG("cmd: %x\n", cmd);
	switch (cmd) {
	case SET_PMIF_CONN_INF: // = 0x0001,

		reg = pmif_m->_conn_inf_en;
		ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
		if (ret)
			goto WRITE_FAIL;
		break;

	case SET_PMIF_NFC_INF: // = 0x0002,

		reg = pmif_m->_nfc_inf_en;
		ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
		if (ret)
			goto WRITE_FAIL;
		break;

	case SET_PMIF_RC_INF: // = 0x0004,

		reg = pmif_m->_rc_inf_en;
		ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
		if (ret)
			goto WRITE_FAIL;
		break;

	default:
		goto WRITE_FAIL;
	}

	spin_unlock_irqrestore(lock, flags);
	return ret;
WRITE_FAIL:
	spin_unlock_irqrestore(lock, flags);
	return cmd;
}

ssize_t __dump_pmif_status(void *data, char *buf)
{
	struct plat_pmifdata *pd = (struct plat_pmifdata *)data;
	struct clkbuf_hw hw;
	struct pmif_m *pmif_m;
	struct reg_t *reg_p;
	int len = 0, i;
	u32 out;

	if (!IS_PMIF_HW((pd->hw).hw_type))
		goto DUMP_FAIL;

	hw = pd->hw;
	/*switch to PMIF_M*/
	hw.hw_type = PMIF_M;
	pmif_m = pd->pmif_m;
	if (!pmif_m) {
		CLKBUF_DBG("pmif_m is null");
		goto DUMP_FAIL;
	}

	for (i = 0; i < sizeof(struct pmif_m) / sizeof(struct reg_t); ++i) {
		if (!((((struct reg_t *)pmif_m) + i)->mask))
			continue;

		reg_p = ((struct reg_t *)pmif_m) + i;

		if (pmif_read(&hw, reg_p, &out))
			goto DUMP_FAIL;

		if (!buf)
			CLKBUF_DBG(
				"PMIF_M regs: %s Addr: 0x%08x Vals: 0x%08x\n",
				reg_p->name, reg_p->ofs, out);
		else
			len += snprintf(
				buf + len, PAGE_SIZE - len,
				"PMIF_M regs: %s Addr: 0x%08x Vals: 0x%08x\n",
				reg_p->name, reg_p->ofs, out);
	}

	return len;

DUMP_FAIL:
	CLKBUF_DBG("HW_TYPE is not PMIF HW or READ FAIL\n");
	return len;
}

void __spmi_dump_pmif_record(void)
{
	spmi_dump_pmif_record_reg();
}

static struct clkbuf_operation clkbuf_ops_v1 = {
	.dump_pmif_status = __dump_pmif_status,
	.set_pmif_inf = __set_pmif_inf,
#ifdef LOG_6985_SPMI_CMD
	.spmi_dump_pmif_record = __spmi_dump_pmif_record,
#endif
};

static struct clkbuf_hdlr pmif_hdlr_v2 = {
	.ops = &clkbuf_ops_v1,
	.data = &pmif_data_v2,
};

static struct match_pmif mt6897_match_pmif = {
	.name = "mediatek,mt6897-spmi",
	.hdlr = &pmif_hdlr_v2,
	.init = &pmif_init_v1,
};

static struct match_pmif mt6985_match_pmif = {
	.name = "mediatek,mt6985-spmi",
	.hdlr = &pmif_hdlr_v2,
	.init = &pmif_init_v1,
};

static struct match_pmif *matches_pmif[] = {
	&mt6897_match_pmif,
	&mt6985_match_pmif,
	NULL,
};

int count_pmif_node(struct device_node *clkbuf_node)
{
	/*add logic if not only PMIF_M*/
	return 1;
}

struct clkbuf_dts *parse_pmif_dts(struct clkbuf_dts *array,
				  struct device_node *clkbuf_node, int nums)
{
	struct device_node *pmif_node;
	struct platform_device *pmif_dev;
	unsigned int num_pmif = 0, iomap_idx = 0;
	const char *comp = NULL;
	void __iomem *pmif_m_base;
	int perms = 0xffff;

	pmif_node = of_parse_phandle(clkbuf_node, "pmif", 0);

	if (!pmif_node) {
		CLKBUF_DBG("find pmif_node failed, not support PMIF\n");
		return NULL;
	}
	/*count pmif numbers, assume only PMIF_M*/
	num_pmif = 1;

	pmif_dev = of_find_device_by_node(pmif_node);
	pmif_m_base = of_iomap(pmif_node, iomap_idx++);

	/*start parsing pmif dts*/

	of_property_read_string(pmif_node, "compatible", &comp);

	array->nums = nums;
	array->num_pmif = num_pmif;
	array->comp = (char *)comp;

	/*assume only PMIF_M*/
	array->pmif_name = "PMIF_M";
	array->hw.hw_type = PMIF_M;

	array->pmif_id = 0;
	array->perms = perms;
	array->hw.base.pmif_m = pmif_m_base;
	array++;

	return array;
}

int clkbuf_pmif_init(struct clkbuf_dts *array, struct device *dev)
{
	struct match_pmif **match_pmif = matches_pmif;
	struct clkbuf_hdlr *hdlr;
	struct clkbuf_hw hw;
	int i;
	int nums = array->nums;
	int num_pmif = 0;

	CLKBUF_DBG("\n");

	for (i = 0; i < nums; i++, array++) {
		hdlr = array->hdlr;
		hw = array->hw;
		/*only need one pmif element*/
		if (IS_PMIF_HW(hw.hw_type)) {
			num_pmif = array->num_pmif;
			break;
		}
	}

	if (!IS_PMIF_HW(array->hw.hw_type)) {
		CLKBUF_DBG("no dts pmif HW!\n");
		return -1;
	}

	/* find match by compatible */
	for (; (*match_pmif) != NULL; match_pmif++) {
		char *comp = (*match_pmif)->name;
		char *target = array->comp;

		if (strcmp(comp, target) == 0)
			break;
	}

	if (*match_pmif == NULL) {
		CLKBUF_DBG("no match pmif compatible!\n");
		return -1;
	}

	/* init flow: prepare pmif obj to specific array element*/
	for (i = 0; i < num_pmif; i++, array++) {
		char *src = array->comp;
		char *plat_target = (*match_pmif)->name;

		if (strcmp(src, plat_target) == 0)
			(*match_pmif)->init(array, *match_pmif);
	}

	CLKBUF_DBG("\n");
	return 0;
}
