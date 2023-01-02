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
#include "srclken.h"

struct match_srclken {
	char *name;
	struct clkbuf_hdlr *hdlr;
	int (*init)(struct clkbuf_dts *array, struct match_srclken *match);
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

	default:
		ret = -EREG_NOT_SUPPORT;
		CLKBUF_DBG("srclken read failed: %d\n", ret);
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

	default:
		ret = -EREG_NOT_SUPPORT;
		CLKBUF_DBG("srclken write failed: %d\n", ret);
		break;
	}

	return ret;
}

static int rc_read(struct clkbuf_hw *hw, struct reg_t *reg, u32 *val)
{
	return read_with_ofs(hw, reg, val, 0);
}


//   static int rc_write(struct clkbuf_hw *hw, struct reg_t *reg, u32 val)
//   {
//   return write_with_ofs(hw, reg, val, 0);
//   }

int count_src_node(struct device_node *clkbuf_node)
{
	struct device_node *src_node, *sub;
	int num_sub = 0;

	src_node = of_parse_phandle(clkbuf_node, "srclken-rc", 0);

	if (!src_node)
		CLKBUF_DBG("find src_node failed, not support srclken-rc\n");

	/*count subsys numbers*/
	for_each_child_of_node(src_node, sub) {
		num_sub++;
	}

	CLKBUF_DBG("subsys support numbers: %d", num_sub);
	return num_sub;
}

struct clkbuf_dts *parse_srclken_dts(struct clkbuf_dts *array,
				     struct device_node *clkbuf_node, int nums)
{
	struct device_node *src_node, *sub;
	struct platform_device *src_dev;
	unsigned int num_sub = 0, iomap_idx = 0;
	void __iomem *rc_cfg_base, *rc_sta_base;

	src_node = of_parse_phandle(clkbuf_node, "srclken-rc", 0);

	if (!src_node)
		CLKBUF_DBG("find src_node failed, not support srclken-rc\n");

	/*count subsys numbers*/
	for_each_child_of_node(src_node, sub) {
		num_sub++;
	}
	/*start parsing srclken dts*/
	src_dev = of_find_device_by_node(src_node);
	rc_cfg_base = of_iomap(src_node, iomap_idx++);
	rc_sta_base = of_iomap(src_node, iomap_idx++);

	for_each_child_of_node(src_node, sub) {
		/* default for optional field */
		const char *xo_name = NULL;
		const char *comp = NULL;
		int perms = 0xffff;
		int sub_id, sub_req;

		of_property_read_string(src_node, "compatible", &comp);
		of_property_read_string(sub, "xo-buf", &xo_name);
		of_property_read_u32(sub, "sub-id", &sub_id);
		of_property_read_u32(sub, "sub-req", &sub_req);
		of_property_read_u32(sub, "perms", &perms);
		array->num_sub = num_sub;
		array->nums = nums;
		array->comp = (char *)comp;
		array->subsys_name = (char *)sub->name;
		array->sub_id = sub_id;
		array->xo_name = (char *)xo_name;
		array->hw.hw_type = SRCLKEN_CFG;
		array->hw.base.cfg = rc_cfg_base;
		array->hw.base.sta = rc_sta_base;
		array->perms = perms;
		array->init_dts_cmd.sub_req = sub_req;
		array++;
	}

	return array;
}

static int srclken_init_v1(struct clkbuf_dts *array,
			   struct match_srclken *match)
{
	struct clkbuf_hdlr *hdlr = match->hdlr;
	struct plat_rcdata *pd;
	int ret = 0;
	static DEFINE_SPINLOCK(lock);

	CLKBUF_DBG("array<%x>,%s %d, id<%d>\n", array, array->subsys_name,
		   array->hw.hw_type, array->sub_id);

	pd = (struct plat_rcdata *)(hdlr->data);

	/*pass dts hw info(base addr) to complete platform data */
	pd->hw = array->hw;
	pd->lock = &lock;

	/* hook hdlr to array */
	array->hdlr = hdlr;

	/*if defined dts cmd, then do dts init flow*/
	if (array->init_dts_cmd.sub_req) {
		if (!hdlr->ops->srclken_subsys_ctrl) {
			CLKBUF_DBG("Error: srclken_subsys_ctrl is null\n");
			return 0;
		}
		ret = hdlr->ops->srclken_subsys_ctrl(
			pd, array->init_dts_cmd.sub_req, array->sub_id,
			array->perms);
		if (ret)
			CLKBUF_DBG("Error code: %x\n", ret);
	}

	return 0;
}

int __get_rc_MXX_cfg(void *data, int sub_id, char *buf)
{
	struct plat_rcdata *pd = (struct plat_rcdata *)data;
	struct clkbuf_hw hw;
	struct srclken_rc_cfg *cfg;
	struct reg_t m00_cfg;
	int len = 0;
	int ret = 0;
	u32 out;

	if (!IS_RC_HW((pd->hw).hw_type)) {
		CLKBUF_DBG("get HW type Err\n");
		return len;
	}

	hw = pd->hw;
	/*switch to RC CFG*/
	hw.hw_type = SRCLKEN_CFG;
	cfg = pd->cfg;
	if (!cfg) {
		CLKBUF_DBG("cfg is null");
		return len;
	}

	m00_cfg = cfg->_m00_cfg;
	ret = read_with_ofs(&hw, &m00_cfg, &out, sub_id * 4);
	if (ret) {
		CLKBUF_DBG("read m00_cfg fail\n");
		return len;
	}

	if (!buf)
		CLKBUF_DBG("CFG reg addr: 0x%08x Vals: 0x%08x\n",
			   m00_cfg.ofs + (sub_id * 4), out);
	else
		len += snprintf(buf + len, PAGE_SIZE - len,
				"CFG reg addr: 0x%08x Vals: 0x%08x\n",
				m00_cfg.ofs + (sub_id * 4), out);

	return len;
}

int __get_rc_MXX_req_sta(void *data, int sub_id, char *buf)
{
	struct plat_rcdata *pd = (struct plat_rcdata *)data;
	struct clkbuf_hw hw;
	struct srclken_rc_sta *sta;
	struct reg_t m00_sta;
	int len = 0;
	int ret = 0;
	u32 sta_ofs = 0, out;

	if (!IS_RC_HW((pd->hw).hw_type)) {
		CLKBUF_DBG("get HW type Err\n");
		return len;
	}

	hw = pd->hw;
	/*switch to RC STA*/
	hw.hw_type = SRCLKEN_STA;
	sta = pd->sta;
	if (!sta) {
		CLKBUF_DBG("sta is null");
		return len;
	}

	sta_ofs = sta->sta_base_ofs;
	m00_sta = sta->_m00_sta;
	ret = read_with_ofs(&hw, &m00_sta, &out, sub_id * 4);
	if (ret) {
		CLKBUF_DBG("read m00_sta fail\n");
		return len;
	}

	if (!buf)
		CLKBUF_DBG("STA reg addr: 0x%08x Vals: 0x%08x\n",
			   m00_sta.ofs + (sub_id * 4) + sta_ofs, out);
	else
		len += snprintf(buf + len, PAGE_SIZE - len,
				"STA reg addr: 0x%08x Vals: 0x%08x\n",
				m00_sta.ofs + (sub_id * 4) + sta_ofs, out);

	return len;
}

int __srclken_subsys_ctrl(void *data, int cmd, int sub_id, int perms)
{
	struct plat_rcdata *pd = (struct plat_rcdata *)data;
	struct clkbuf_hw hw;
	struct srclken_rc_cfg *cfg;
	struct reg_t m00_cfg;
	struct reg_t sw_srclken_rc_en;
	struct reg_t sw_rc_req;
	struct reg_t sw_fpm_en;
	struct reg_t sw_bblpm_en;
	int ret = 0;
	u32 val = 0;
	unsigned long flags = 0;
	spinlock_t *lock = pd->lock;

	if (!IS_RC_HW((pd->hw).hw_type)) {
		CLKBUF_DBG("get HW type Err\n");
		goto REQ_FAIL;
	}

	hw = pd->hw;
	/*switch to RC CFG*/
	hw.hw_type = SRCLKEN_CFG;
	cfg = pd->cfg;
	if (!cfg) {
		CLKBUF_DBG("cfg is null");
		goto REQ_FAIL;
	}

	/*platform data*/
	m00_cfg = cfg->_m00_cfg;
	sw_srclken_rc_en = cfg->_sw_srclken_rc_en;
	sw_rc_req = cfg->_sw_rc_req;
	sw_fpm_en = cfg->_sw_fpm_en;
	sw_bblpm_en = cfg->_sw_bblpm_en;

	spin_lock_irqsave(lock, flags);
	ret = read_with_ofs(&hw, &m00_cfg, &val, sub_id * 4);

	if (ret) {
		CLKBUF_DBG("read srclken_rc subsys cfg failed\n");
		goto REQ_FAIL;
	}

	val &= (~(sw_srclken_rc_en.mask << sw_srclken_rc_en.shift));
	val |= (sw_srclken_rc_en.mask << sw_srclken_rc_en.shift);

	CLKBUF_DBG("re_req: %x, perms: %x\n", cmd, perms);
	switch (cmd & perms) {
	case RC_FPM_REQ:
		val &= (~(sw_rc_req.mask << sw_rc_req.shift));
		val |= (sw_fpm_en.mask << sw_fpm_en.shift);

		break;
	case RC_LPM_REQ:
		val &= (~(sw_rc_req.mask << sw_rc_req.shift));

		break;
	case RC_BBLPM_REQ:
		val &= (~(sw_rc_req.mask << sw_rc_req.shift));
		val |= (sw_bblpm_en.mask << sw_bblpm_en.shift);

		break;

	case RC_NONE_REQ: //HW RC control
	default:
		val &= (~(sw_rc_req.mask << sw_rc_req.shift));
		val &= (~(sw_srclken_rc_en.mask << sw_srclken_rc_en.shift));
		break;
	}

	ret = write_with_ofs(&hw, &m00_cfg, val, sub_id * 4);

	spin_unlock_irqrestore(lock, flags);

	if (ret)
		goto REQ_FAIL;

	/*wait for xo hw behavior*/
	udelay(400);

	return ret;

REQ_FAIL:
	return cmd & perms;
}

ssize_t __dump_srclken_status(void *data, char *buf)
{
	struct plat_rcdata *pd = (struct plat_rcdata *)data;
	struct clkbuf_hw hw;
	struct srclken_rc_sta *sta;
	struct srclken_rc_cfg *cfg;
	struct reg_t *reg_p;
	u32 sta_ofs = 0, out;
	int i;
	int len = 0;

	if (!IS_RC_HW((pd->hw).hw_type))
		goto DUMP_FAIL;

	hw = pd->hw;
	/*switch to RC CFG*/
	hw.hw_type = SRCLKEN_CFG;
	cfg = pd->cfg;
	if (!cfg) {
		CLKBUF_DBG("cfg is null");
		goto DUMP_FAIL;
	}

	for (i = 0; i < sizeof(struct srclken_rc_cfg) / sizeof(struct reg_t);
	     ++i) {
		if (!((((struct reg_t *)cfg) + i)->mask))
			continue;

		reg_p = ((struct reg_t *)cfg) + i;

		if (rc_read(&hw, reg_p, &out))
			goto DUMP_FAIL;

		if (!buf)
			CLKBUF_DBG("CFG regs: %s Addr: 0x%08x Vals: 0x%08x\n",
				   reg_p->name, reg_p->ofs, out);
		else
			len += snprintf(
				buf + len, PAGE_SIZE - len,
				"CFG regs: %s Addr: 0x%08x Vals: 0x%08x\n",
				reg_p->name, reg_p->ofs, out);
	}

	/*switch to RC STA*/
	hw.hw_type = SRCLKEN_STA;
	sta = pd->sta;
	if (!sta) {
		CLKBUF_DBG("sta is null");
		goto DUMP_FAIL;
	}
	sta_ofs = sta->sta_base_ofs;

	for (i = 0; i < sizeof(struct srclken_rc_sta) / sizeof(struct reg_t);
	     ++i) {
		if (!((((struct reg_t *)sta) + i)->mask))
			continue;

		reg_p = ((struct reg_t *)sta) + i;

		if (rc_read(&hw, reg_p, &out))
			goto DUMP_FAIL;

		if (!buf)
			CLKBUF_DBG("STA regs: %s Addr: 0x%08x Vals: 0x%08x\n",
				   reg_p->name, reg_p->ofs + sta_ofs, out);
		else
			len += snprintf(
				buf + len, PAGE_SIZE - len,
				"STA regs: %s Addr: 0x%08x Vals: 0x%08x\n",
				reg_p->name, reg_p->ofs + sta_ofs, out);
	}
	return len;

DUMP_FAIL:
	CLKBUF_DBG("HW_TYPE is not RC HW or READ FAIL\n");
	return len;
}

ssize_t __dump_srclken_trace(void *data, u32 num_dump, u32 num_timer, char *buf)
{
	struct plat_rcdata *pd = (struct plat_rcdata *)data;
	struct clkbuf_hw hw;
	struct srclken_rc_sta *sta;
	struct reg_t reg_msb, reg_lsb;
	int i;
	int len = 0;
	u32 out_msb = 0, out_lsb = 0, sta_ofs = 0, msb_ofs, lsb_ofs;

	if (!IS_RC_HW((pd->hw).hw_type))
		goto DUMP_FAIL;

	hw = pd->hw;
	/*switch to RC STA*/
	hw.hw_type = SRCLKEN_STA;
	sta = pd->sta;
	if (!sta) {
		CLKBUF_DBG("sta is null");
		goto DUMP_FAIL;
	}

	sta_ofs = sta->sta_base_ofs;
	num_dump = (num_dump < RC_HW_TRACE_MAX_DUMP) ? num_dump :
						       RC_HW_TRACE_MAX_DUMP;

	for (i = 0, msb_ofs = 0, lsb_ofs = 0; i < num_dump;
	     ++i, msb_ofs += 8, lsb_ofs += 8) {
		reg_msb = sta->_trace_msb;
		reg_lsb = sta->_trace_lsb;

		if (read_with_ofs(&hw, &reg_msb, &out_msb, msb_ofs))
			goto DUMP_FAIL;

		if (read_with_ofs(&hw, &reg_lsb, &out_lsb, lsb_ofs))
			goto DUMP_FAIL;

		if (!buf) {
			CLKBUF_DBG("trace msb addr: 0x%08x Vals: 0x%08x\n",
				   reg_msb.ofs + msb_ofs + sta_ofs, out_msb);
			CLKBUF_DBG("trace lsb addr: 0x%08x Vals: 0x%08x\n",
				   reg_lsb.ofs + lsb_ofs + sta_ofs, out_lsb);
		} else {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"trace msb addr: 0x%08x Vals: 0x%08x\n",
					reg_msb.ofs + msb_ofs + sta_ofs, out_msb);
			len += snprintf(buf + len, PAGE_SIZE - len,
					"trace lsb addr: 0x%08x Vals: 0x%08x\n",
					reg_lsb.ofs + lsb_ofs + sta_ofs, out_lsb);
		}
	}

	num_timer = (num_timer < RC_HW_TRACE_MAX_DUMP) ? num_timer :
							 RC_HW_TRACE_MAX_DUMP;

	for (i = 0, msb_ofs = 0, lsb_ofs = 0; i < num_timer;
	     ++i, msb_ofs += 8, lsb_ofs += 8) {
		reg_msb = sta->_timer_msb;
		reg_lsb = sta->_timer_lsb;

		if (read_with_ofs(&hw, &reg_msb, &out_msb, msb_ofs))
			goto DUMP_FAIL;

		if (read_with_ofs(&hw, &reg_lsb, &out_lsb, lsb_ofs))
			goto DUMP_FAIL;

		if (!buf) {
			CLKBUF_DBG("timer msb addr: 0x%08x Vals: 0x%08x\n",
				   reg_msb.ofs + msb_ofs + sta_ofs, out_msb);
			CLKBUF_DBG("timer lsb addr: 0x%08x Vals: 0x%08x\n",
				   reg_lsb.ofs + lsb_ofs + sta_ofs, out_lsb);
		} else {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"timer msb addr: 0x%08x Vals: 0x%08x\n",
					reg_msb.ofs + msb_ofs + sta_ofs, out_msb);
			len += snprintf(buf + len, PAGE_SIZE - len,
					"timer lsb addr: 0x%08x Vals: 0x%08x\n",
					reg_lsb.ofs + lsb_ofs + sta_ofs, out_lsb);
		}
	}

	return len;

DUMP_FAIL:
	CLKBUF_DBG("HW_TYPE is not RC HW or READ FAIL\n");
	return len;
}

static struct clkbuf_operation clkbuf_ops_v1 = {
	.dump_srclken_status = __dump_srclken_status,
	.dump_srclken_trace = __dump_srclken_trace,
	.srclken_subsys_ctrl = __srclken_subsys_ctrl,
	.get_rc_MXX_req_sta = __get_rc_MXX_req_sta,
	.get_rc_MXX_cfg = __get_rc_MXX_cfg,
};

static struct clkbuf_hdlr clkbuf_hdlr_v1 = {
	.ops = &clkbuf_ops_v1,
	.data = &rc_data_v1,
};

static struct match_srclken match_srclken_v1 = {
	.name = "mediatek,srclken-rc",
	.hdlr = &clkbuf_hdlr_v1,
	.init = &srclken_init_v1,
};

static struct match_srclken *matches_srclken[] = {
	&match_srclken_v1,
	NULL,
};

int clkbuf_srclken_init(struct clkbuf_dts *array, struct device *dev)
{
	struct match_srclken **match_srclken = matches_srclken;
	struct clkbuf_hdlr *hdlr;
	struct clkbuf_hw hw;
	int i;
	int nums = array->nums;
	int num_sub = 0;

	CLKBUF_DBG("\n");

	for (i = 0; i < nums; i++, array++) {
		hdlr = array->hdlr;
		hw = array->hw;
		/*only need first RC obj, no need loop all obj*/
		if (IS_RC_HW(hw.hw_type)) {
			num_sub = array->num_sub;
			break;
		}
	}

	if (!IS_RC_HW(array->hw.hw_type)) {
		CLKBUF_DBG("no dts srclken HW!\n");
		return -1;
	}

	for (; (*match_srclken) != NULL; match_srclken++) {
		char *comp = (*match_srclken)->name;
		char *target =
			array->comp;

		if (strcmp(comp, target) == 0)
			break;
	}

	if (*match_srclken == NULL) {
		CLKBUF_DBG("no match srclken compatible!\n");
		return -1;
	}

	/* init flow: prepare rc obj to specific array element*/
	for (i = 0; i < num_sub; i++, array++) {
		char *src = array->comp;
		char *plat_target = (*match_srclken)->name;

		if (strcmp(src, plat_target) == 0)
			(*match_srclken)->init(array, *match_srclken);
	}
	CLKBUF_DBG("\n");
	return 0;
}
