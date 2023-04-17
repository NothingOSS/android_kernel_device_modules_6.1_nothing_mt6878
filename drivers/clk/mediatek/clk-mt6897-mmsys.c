// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Benjamin Chao <benjamin.chao@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6897-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs dispsys0_config0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs dispsys0_config0_hwv_regs = {
	.set_ofs = 0x0018,
	.clr_ofs = 0x001C,
	.sta_ofs = 0x1C0C,
};

static const struct mtk_gate_regs dispsys0_config1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs dispsys0_config1_hwv_regs = {
	.set_ofs = 0x0020,
	.clr_ofs = 0x0024,
	.sta_ofs = 0x1C10,
};

static const struct mtk_gate_regs dispsys0_config2_cg_regs = {
	.set_ofs = 0x1A4,
	.clr_ofs = 0x1A8,
	.sta_ofs = 0x1A0,
};

static const struct mtk_gate_regs dispsys0_config2_hwv_regs = {
	.set_ofs = 0x0028,
	.clr_ofs = 0x002C,
	.sta_ofs = 0x1C14,
};

#define GATE_DISPSYS0_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dispsys0_config0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_DISPSYS0_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys0_config0_cg_regs,			\
		.hwv_regs = &dispsys0_config0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

#define GATE_DISPSYS0_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dispsys0_config1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_DISPSYS0_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys0_config1_cg_regs,			\
		.hwv_regs = &dispsys0_config1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

#define GATE_DISPSYS0_CONFIG2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dispsys0_config2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_DISPSYS0_CONFIG2(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys0_config2_cg_regs,			\
		.hwv_regs = &dispsys0_config2_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate dispsys0_config_clks[] = {
	/* DISPSYS0_CONFIG0 */
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISPSYS_CONFIG, "dispsys0_disp_cfg",
			"disp0_ck"/* parent */, 0),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_MUTEX0, "dispsys0_disp_mutex0",
			"disp0_ck"/* parent */, 1),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_AAL0, "dispsys0_disp_aal0",
			"disp0_ck"/* parent */, 2),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_C3D0, "dispsys0_disp_c3d0",
			"disp0_ck"/* parent */, 3),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_CCORR0, "dispsys0_disp_ccorr0",
			"disp0_ck"/* parent */, 4),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_CCORR1, "dispsys0_disp_ccorr1",
			"disp0_ck"/* parent */, 5),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_CHIST0, "dispsys0_disp_chist0",
			"disp0_ck"/* parent */, 6),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_CHIST1, "dispsys0_disp_chist1",
			"disp0_ck"/* parent */, 7),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_COLOR0, "dispsys0_disp_color0",
			"disp0_ck"/* parent */, 8),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DITHER0, "dispsys0_di0",
			"disp0_ck"/* parent */, 9),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DITHER1, "dispsys0_di1",
			"disp0_ck"/* parent */, 10),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DLI_ASYNC0, "dispsys0_dli0",
			"disp0_ck"/* parent */, 11),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DLI_ASYNC1, "dispsys0_dli1",
			"disp0_ck"/* parent */, 12),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DLI_ASYNC2, "dispsys0_dli2",
			"disp0_ck"/* parent */, 13),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DLI_ASYNC3, "dispsys0_dli3",
			"disp0_ck"/* parent */, 14),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DLI_ASYNC4, "dispsys0_dli4",
			"disp0_ck"/* parent */, 15),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DLI_ASYNC5, "dispsys0_dli5",
			"disp0_ck"/* parent */, 16),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DLO_ASYNC0, "dispsys0_dlo0",
			"disp0_ck"/* parent */, 17),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DLO_ASYNC1, "dispsys0_dlo1",
			"disp0_ck"/* parent */, 18),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DP_INTF0, "dispsys0_dp_intclk",
			"disp0_ck"/* parent */, 19),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DSC_WRAP0, "dispsys0_dscw0",
			"disp0_ck"/* parent */, 20),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_DSI0, "dispsys0_clk0",
			"disp0_ck"/* parent */, 21),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_GAMMA0, "dispsys0_disp_gamma0",
			"disp0_ck"/* parent */, 22),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_MDP_AAL0, "dispsys0_mdp_aal0",
			"disp0_ck"/* parent */, 23),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_MDP_RDMA0, "dispsys0_mdp_rdma0",
			"disp0_ck"/* parent */, 24),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_MERGE0, "dispsys0_disp_merge0",
			"disp0_ck"/* parent */, 25),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_MERGE1, "dispsys0_disp_merge1",
			"disp0_ck"/* parent */, 26),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_ODDMR0, "dispsys0_disp_oddmr0",
			"disp0_ck"/* parent */, 27),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_POSTALIGN0, "dispsys0_palign0",
			"disp0_ck"/* parent */, 28),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_POSTMASK0, "dispsys0_pmask0",
			"disp0_ck"/* parent */, 29),
	GATE_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_RELAY0, "dispsys0_disp_relay0",
			"disp0_ck"/* parent */, 30),
	GATE_HWV_DISPSYS0_CONFIG0(CLK_DISPSYS0_DISP_RSZ0, "dispsys0_disp_rsz0",
			"disp0_ck"/* parent */, 31),
	/* DISPSYS0_CONFIG1 */
	GATE_DISPSYS0_CONFIG1(CLK_DISPSYS0_DISP_SPR0, "dispsys0_disp_spr0",
			"disp0_ck"/* parent */, 0),
	GATE_DISPSYS0_CONFIG1(CLK_DISPSYS0_DISP_TDSHP0, "dispsys0_disp_tdshp0",
			"disp0_ck"/* parent */, 1),
	GATE_DISPSYS0_CONFIG1(CLK_DISPSYS0_DISP_TDSHP1, "dispsys0_disp_tdshp1",
			"disp0_ck"/* parent */, 2),
	GATE_DISPSYS0_CONFIG1(CLK_DISPSYS0_DISP_UFBC_WDMA1, "dispsys0_wdma1",
			"disp0_ck"/* parent */, 3),
	GATE_DISPSYS0_CONFIG1(CLK_DISPSYS0_DISP_VDCM0, "dispsys0_disp_vdcm0",
			"disp0_ck"/* parent */, 4),
	GATE_HWV_DISPSYS0_CONFIG1(CLK_DISPSYS0_DISP_WDMA1, "dispsys0_disp_wdma1",
			"disp0_ck"/* parent */, 5),
	GATE_HWV_DISPSYS0_CONFIG1(CLK_DISPSYS0_SMI_SUB_COMM0, "dispsys0_smi_comm0",
			"disp0_ck"/* parent */, 6),
	GATE_DISPSYS0_CONFIG1(CLK_DISPSYS0_DISP_Y2R0, "dispsys0_disp_y2r0",
			"disp0_ck"/* parent */, 7),
	GATE_DISPSYS0_CONFIG1(CLK_DISPSYS0_DISP_CCORR2, "dispsys0_disp_ccorr2",
			"disp0_ck"/* parent */, 8),
	GATE_DISPSYS0_CONFIG1(CLK_DISPSYS0_DISP_CCORR3, "dispsys0_disp_ccorr3",
			"disp0_ck"/* parent */, 9),
	GATE_DISPSYS0_CONFIG1(CLK_DISPSYS0_DISP_GAMMA1, "dispsys0_disp_gamma1",
			"disp0_ck"/* parent */, 10),
	/* DISPSYS0_CONFIG2 */
	GATE_HWV_DISPSYS0_CONFIG2(CLK_DISPSYS0_DSI_CLK, "dispsys0_dsi_clk",
			"disp0_ck"/* parent */, 0),
	GATE_DISPSYS0_CONFIG2(CLK_DISPSYS0_DP_CLK, "dispsys0_dp_clk",
			"disp0_ck"/* parent */, 1),
	GATE_HWV_DISPSYS0_CONFIG2(CLK_DISPSYS0_26M_CLK, "dispsys0_26m_clk",
			"disp0_ck"/* parent */, 10),
};

static const struct mtk_clk_desc dispsys0_config_mcd = {
	.clks = dispsys0_config_clks,
	.num_clks = CLK_DISPSYS0_CONFIG_NR_CLK,
};

static const struct mtk_gate_regs dispsys1_config0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs dispsys1_config0_hwv_regs = {
	.set_ofs = 0x0030,
	.clr_ofs = 0x0034,
	.sta_ofs = 0x1C18,
};

static const struct mtk_gate_regs dispsys1_config1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs dispsys1_config1_hwv_regs = {
	.set_ofs = 0x0038,
	.clr_ofs = 0x003C,
	.sta_ofs = 0x1C1C,
};

static const struct mtk_gate_regs dispsys1_config2_cg_regs = {
	.set_ofs = 0x1A4,
	.clr_ofs = 0x1A8,
	.sta_ofs = 0x1A0,
};

static const struct mtk_gate_regs dispsys1_config2_hwv_regs = {
	.set_ofs = 0x0040,
	.clr_ofs = 0x0044,
	.sta_ofs = 0x1C20,
};

#define GATE_DISPSYS1_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dispsys1_config0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_DISPSYS1_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys1_config0_cg_regs,			\
		.hwv_regs = &dispsys1_config0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

#define GATE_DISPSYS1_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dispsys1_config1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_DISPSYS1_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys1_config1_cg_regs,			\
		.hwv_regs = &dispsys1_config1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

#define GATE_DISPSYS1_CONFIG2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dispsys1_config2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_DISPSYS1_CONFIG2(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys1_config2_cg_regs,			\
		.hwv_regs = &dispsys1_config2_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate dispsys1_config_clks[] = {
	/* DISPSYS1_CONFIG0 */
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISPSYS_CONFIG, "dispsys1_disp_cfg",
			"disp0_ck"/* parent */, 0),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_MUTEX0, "dispsys1_disp_mutex0",
			"disp0_ck"/* parent */, 1),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_AAL0, "dispsys1_disp_aal0",
			"disp0_ck"/* parent */, 2),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_C3D0, "dispsys1_disp_c3d0",
			"disp0_ck"/* parent */, 3),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_CCORR0, "dispsys1_disp_ccorr0",
			"disp0_ck"/* parent */, 4),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_CCORR1, "dispsys1_disp_ccorr1",
			"disp0_ck"/* parent */, 5),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_CHIST0, "dispsys1_disp_chist0",
			"disp0_ck"/* parent */, 6),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_CHIST1, "dispsys1_disp_chist1",
			"disp0_ck"/* parent */, 7),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_COLOR0, "dispsys1_disp_color0",
			"disp0_ck"/* parent */, 8),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DITHER0, "dispsys1_di0",
			"disp0_ck"/* parent */, 9),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DITHER1, "dispsys1_di1",
			"disp0_ck"/* parent */, 10),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DLI_ASYNC0, "dispsys1_dli0",
			"disp0_ck"/* parent */, 11),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DLI_ASYNC1, "dispsys1_dli1",
			"disp0_ck"/* parent */, 12),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DLI_ASYNC2, "dispsys1_dli2",
			"disp0_ck"/* parent */, 13),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DLI_ASYNC3, "dispsys1_dli3",
			"disp0_ck"/* parent */, 14),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DLI_ASYNC4, "dispsys1_dli4",
			"disp0_ck"/* parent */, 15),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DLI_ASYNC5, "dispsys1_dli5",
			"disp0_ck"/* parent */, 16),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DLO_ASYNC0, "dispsys1_dlo0",
			"disp0_ck"/* parent */, 17),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DLO_ASYNC1, "dispsys1_dlo1",
			"disp0_ck"/* parent */, 18),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DP_INTF0, "dispsys1_dp_intclk",
			"disp0_ck"/* parent */, 19),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DSC_WRAP0, "dispsys1_dscw0",
			"disp0_ck"/* parent */, 20),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_DSI0, "dispsys1_clk0",
			"disp0_ck"/* parent */, 21),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_GAMMA0, "dispsys1_disp_gamma0",
			"disp0_ck"/* parent */, 22),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_MDP_AAL0, "dispsys1_mdp_aal0",
			"disp0_ck"/* parent */, 23),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_MDP_RDMA0, "dispsys1_mdp_rdma0",
			"disp0_ck"/* parent */, 24),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_MERGE0, "dispsys1_disp_merge0",
			"disp0_ck"/* parent */, 25),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_MERGE1, "dispsys1_disp_merge1",
			"disp0_ck"/* parent */, 26),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_ODDMR0, "dispsys1_disp_oddmr0",
			"disp0_ck"/* parent */, 27),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_POSTALIGN0, "dispsys1_palign0",
			"disp0_ck"/* parent */, 28),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_POSTMASK0, "dispsys1_pmask0",
			"disp0_ck"/* parent */, 29),
	GATE_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_RELAY0, "dispsys1_disp_relay0",
			"disp0_ck"/* parent */, 30),
	GATE_HWV_DISPSYS1_CONFIG0(CLK_DISPSYS1_DISP_RSZ0, "dispsys1_disp_rsz0",
			"disp0_ck"/* parent */, 31),
	/* DISPSYS1_CONFIG1 */
	GATE_DISPSYS1_CONFIG1(CLK_DISPSYS1_DISP_SPR0, "dispsys1_disp_spr0",
			"disp0_ck"/* parent */, 0),
	GATE_DISPSYS1_CONFIG1(CLK_DISPSYS1_DISP_TDSHP0, "dispsys1_disp_tdshp0",
			"disp0_ck"/* parent */, 1),
	GATE_DISPSYS1_CONFIG1(CLK_DISPSYS1_DISP_TDSHP1, "dispsys1_disp_tdshp1",
			"disp0_ck"/* parent */, 2),
	GATE_DISPSYS1_CONFIG1(CLK_DISPSYS1_DISP_UFBC_WDMA1, "dispsys1_wdma1",
			"disp0_ck"/* parent */, 3),
	GATE_DISPSYS1_CONFIG1(CLK_DISPSYS1_DISP_VDCM0, "dispsys1_disp_vdcm0",
			"disp0_ck"/* parent */, 4),
	GATE_HWV_DISPSYS1_CONFIG1(CLK_DISPSYS1_DISP_WDMA1, "dispsys1_disp_wdma1",
			"disp0_ck"/* parent */, 5),
	GATE_HWV_DISPSYS1_CONFIG1(CLK_DISPSYS1_SMI_SUB_COMM0, "dispsys1_smi_comm0",
			"disp0_ck"/* parent */, 6),
	GATE_DISPSYS1_CONFIG1(CLK_DISPSYS1_DISP_Y2R0, "dispsys1_disp_y2r0",
			"disp0_ck"/* parent */, 7),
	GATE_DISPSYS1_CONFIG1(CLK_DISPSYS1_DISP_CCORR2, "dispsys1_disp_ccorr2",
			"disp0_ck"/* parent */, 8),
	GATE_DISPSYS1_CONFIG1(CLK_DISPSYS1_DISP_CCORR3, "dispsys1_disp_ccorr3",
			"disp0_ck"/* parent */, 9),
	GATE_DISPSYS1_CONFIG1(CLK_DISPSYS1_DISP_GAMMA1, "dispsys1_disp_gamma1",
			"disp0_ck"/* parent */, 10),
	/* DISPSYS1_CONFIG2 */
	GATE_HWV_DISPSYS1_CONFIG2(CLK_DISPSYS1_DSI_CLK, "dispsys1_dsi_clk",
			"disp0_ck"/* parent */, 0),
	GATE_DISPSYS1_CONFIG2(CLK_DISPSYS1_DP_CLK, "dispsys1_dp_clk",
			"disp0_ck"/* parent */, 1),
	GATE_HWV_DISPSYS1_CONFIG2(CLK_DISPSYS1_26M_CLK, "dispsys1_26m_clk",
			"disp0_ck"/* parent */, 10),
};

static const struct mtk_clk_desc dispsys1_config_mcd = {
	.clks = dispsys1_config_clks,
	.num_clks = CLK_DISPSYS1_CONFIG_NR_CLK,
};

static const struct mtk_gate_regs mminfra_config0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mminfra_config0_hwv_regs = {
	.set_ofs = 0x0058,
	.clr_ofs = 0x005C,
	.sta_ofs = 0x1C2C,
};

static const struct mtk_gate_regs mminfra_config1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mminfra_config1_hwv_regs = {
	.set_ofs = 0x0060,
	.clr_ofs = 0x0064,
	.sta_ofs = 0x1C30,
};

#define GATE_MMINFRA_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_MMINFRA_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config0_cg_regs,			\
		.hwv_regs = &mminfra_config0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

#define GATE_MMINFRA_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_MMINFRA_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config1_cg_regs,			\
		.hwv_regs = &mminfra_config1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate mminfra_config_clks[] = {
	/* MMINFRA_CONFIG0 */
	GATE_HWV_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_D, "mminfra_gce_d",
			"mminfra_ck"/* parent */, 0),
	GATE_HWV_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_M, "mminfra_gce_m",
			"mminfra_ck"/* parent */, 1),
	GATE_HWV_MMINFRA_CONFIG0(CLK_MMINFRA_SMI, "mminfra_smi",
			"mminfra_ck"/* parent */, 2),
	/* MMINFRA_CONFIG1 */
	GATE_HWV_MMINFRA_CONFIG1(CLK_MMINFRA_GCE_26M, "mminfra_gce_26m",
			"mminfra_ck"/* parent */, 17),
};

static const struct mtk_clk_desc mminfra_config_mcd = {
	.clks = mminfra_config_clks,
	.num_clks = CLK_MMINFRA_CONFIG_NR_CLK,
};

static const struct mtk_gate_regs ovlsys0_config_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs ovlsys0_config_hwv_regs = {
	.set_ofs = 0x0068,
	.clr_ofs = 0x006C,
	.sta_ofs = 0x1C34,
};

#define GATE_OVLSYS0_CONFIG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovlsys0_config_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_OVLSYS0_CONFIG(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ovlsys0_config_cg_regs,			\
		.hwv_regs = &ovlsys0_config_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate ovlsys0_config_clks[] = {
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_CONFIG, "ovlsys0_ovl_config",
			"ovl0_ck"/* parent */, 0),
	GATE_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_FAKE_ENG0, "ovlsys0_ovl_fake_e0",
			"ovl0_ck"/* parent */, 1),
	GATE_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_FAKE_ENG1, "ovlsys0_ovl_fake_e1",
			"ovl0_ck"/* parent */, 2),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_MUTEX0, "ovlsys0_ovl_mutex0",
			"ovl0_ck"/* parent */, 3),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_OVL0_2L, "ovlsys0_disp_ovl0_2l",
			"ovl0_ck"/* parent */, 4),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_OVL1_2L, "ovlsys0_disp_ovl1_2l",
			"ovl0_ck"/* parent */, 5),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_OVL2_2L, "ovlsys0_disp_ovl2_2l",
			"ovl0_ck"/* parent */, 6),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_OVL3_2L, "ovlsys0_disp_ovl3_2l",
			"ovl0_ck"/* parent */, 7),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_RSZ1, "ovlsys0_ovl_rsz1",
			"ovl0_ck"/* parent */, 8),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_MDP_RSZ0, "ovlsys0_ovl_mdp",
			"ovl0_ck"/* parent */, 9),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_WDMA0, "ovlsys0_ovl_wdma0",
			"ovl0_ck"/* parent */, 10),
	GATE_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_UFBC_WDMA0, "ovlsys0_wdma0",
			"ovl0_ck"/* parent */, 11),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_WDMA2, "ovlsys0_ovl_wdma2",
			"ovl0_ck"/* parent */, 12),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_DLI_ASYNC0, "ovlsys0_dli0",
			"ovl0_ck"/* parent */, 13),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_DLI_ASYNC1, "ovlsys0_dli1",
			"ovl0_ck"/* parent */, 14),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_DLI_ASYNC2, "ovlsys0_dli2",
			"ovl0_ck"/* parent */, 15),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_DLO_ASYNC0, "ovlsys0_dlo0",
			"ovl0_ck"/* parent */, 16),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_DLO_ASYNC1, "ovlsys0_dlo1",
			"ovl0_ck"/* parent */, 17),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_DLO_ASYNC2, "ovlsys0_dlo2",
			"ovl0_ck"/* parent */, 18),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_DLO_ASYNC3, "ovlsys0_dlo3",
			"ovl0_ck"/* parent */, 19),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_DLO_ASYNC4, "ovlsys0_dlo4",
			"ovl0_ck"/* parent */, 20),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_DLO_ASYNC5, "ovlsys0_dlo5",
			"ovl0_ck"/* parent */, 21),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_DLO_ASYNC6, "ovlsys0_dlo6",
			"ovl0_ck"/* parent */, 22),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_INLINEROT, "ovlsys0_ovl_irot",
			"ovl0_ck"/* parent */, 23),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_SMI_SUB_COMMON0, "ovlsys0_cg0_smi_com0",
			"ovl0_ck"/* parent */, 24),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_Y2R0, "ovlsys0_ovl_y2r0",
			"ovl0_ck"/* parent */, 25),
	GATE_HWV_OVLSYS0_CONFIG(CLK_OVLSYS0_DISP_Y2R1, "ovlsys0_ovl_y2r1",
			"ovl0_ck"/* parent */, 26),
};

static const struct mtk_clk_desc ovlsys0_config_mcd = {
	.clks = ovlsys0_config_clks,
	.num_clks = CLK_OVLSYS0_CONFIG_NR_CLK,
};

static const struct mtk_gate_regs ovlsys1_config_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs ovlsys1_config_hwv_regs = {
	.set_ofs = 0x0070,
	.clr_ofs = 0x0074,
	.sta_ofs = 0x1C38,
};

#define GATE_OVLSYS1_CONFIG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovlsys1_config_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_OVLSYS1_CONFIG(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ovlsys1_config_cg_regs,			\
		.hwv_regs = &ovlsys1_config_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate ovlsys1_config_clks[] = {
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_CONFIG, "ovlsys1_ovl_config",
			"ovl0_ck"/* parent */, 0),
	GATE_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_FAKE_ENG0, "ovlsys1_ovl_fake_e0",
			"ovl0_ck"/* parent */, 1),
	GATE_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_FAKE_ENG1, "ovlsys1_ovl_fake_e1",
			"ovl0_ck"/* parent */, 2),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_MUTEX0, "ovlsys1_ovl_mutex0",
			"ovl0_ck"/* parent */, 3),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_OVL0_2L, "ovlsys1_disp_ovl0_2l",
			"ovl0_ck"/* parent */, 4),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_OVL1_2L, "ovlsys1_disp_ovl1_2l",
			"ovl0_ck"/* parent */, 5),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_OVL2_2L, "ovlsys1_disp_ovl2_2l",
			"ovl0_ck"/* parent */, 6),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_OVL3_2L, "ovlsys1_disp_ovl3_2l",
			"ovl0_ck"/* parent */, 7),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_RSZ1, "ovlsys1_ovl_rsz1",
			"ovl0_ck"/* parent */, 8),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_MDP_RSZ0, "ovlsys1_ovl_mdp",
			"ovl0_ck"/* parent */, 9),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_WDMA0, "ovlsys1_ovl_wdma0",
			"ovl0_ck"/* parent */, 10),
	GATE_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_UFBC_WDMA0, "ovlsys1_wdma0",
			"ovl0_ck"/* parent */, 11),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_WDMA2, "ovlsys1_ovl_wdma2",
			"ovl0_ck"/* parent */, 12),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_DLI_ASYNC0, "ovlsys1_dli0",
			"ovl0_ck"/* parent */, 13),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_DLI_ASYNC1, "ovlsys1_dli1",
			"ovl0_ck"/* parent */, 14),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_DLI_ASYNC2, "ovlsys1_dli2",
			"ovl0_ck"/* parent */, 15),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_DLO_ASYNC0, "ovlsys1_dlo0",
			"ovl0_ck"/* parent */, 16),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_DLO_ASYNC1, "ovlsys1_dlo1",
			"ovl0_ck"/* parent */, 17),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_DLO_ASYNC2, "ovlsys1_dlo2",
			"ovl0_ck"/* parent */, 18),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_DLO_ASYNC3, "ovlsys1_dlo3",
			"ovl0_ck"/* parent */, 19),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_DLO_ASYNC4, "ovlsys1_dlo4",
			"ovl0_ck"/* parent */, 20),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_DLO_ASYNC5, "ovlsys1_dlo5",
			"ovl0_ck"/* parent */, 21),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_DLO_ASYNC6, "ovlsys1_dlo6",
			"ovl0_ck"/* parent */, 22),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_INLINEROT, "ovlsys1_ovl_irot",
			"ovl0_ck"/* parent */, 23),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_SMI_SUB_COMMON0, "ovlsys1_cg0_smi_com0",
			"ovl0_ck"/* parent */, 24),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_Y2R0, "ovlsys1_ovl_y2r0",
			"ovl0_ck"/* parent */, 25),
	GATE_HWV_OVLSYS1_CONFIG(CLK_OVLSYS1_DISP_Y2R1, "ovlsys1_ovl_y2r1",
			"ovl0_ck"/* parent */, 26),
};

static const struct mtk_clk_desc ovlsys1_config_mcd = {
	.clks = ovlsys1_config_clks,
	.num_clks = CLK_OVLSYS1_CONFIG_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6897_mmsys[] = {
	{
		.compatible = "mediatek,mt6897-dispsys0_config",
		.data = &dispsys0_config_mcd,
	}, {
		.compatible = "mediatek,mt6897-mmsys1",
		.data = &dispsys1_config_mcd,
	}, {
		.compatible = "mediatek,mt6897-mminfra_config",
		.data = &mminfra_config_mcd,
	}, {
		.compatible = "mediatek,mt6897-ovlsys0_config",
		.data = &ovlsys0_config_mcd,
	}, {
		.compatible = "mediatek,mt6897-ovlsys1_config",
		.data = &ovlsys1_config_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6897_mmsys_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6897_mmsys_drv = {
	.probe = clk_mt6897_mmsys_grp_probe,
	.driver = {
		.name = "clk-mt6897-mmsys",
		.of_match_table = of_match_clk_mt6897_mmsys,
	},
};

module_platform_driver(clk_mt6897_mmsys_drv);
MODULE_LICENSE("GPL");
