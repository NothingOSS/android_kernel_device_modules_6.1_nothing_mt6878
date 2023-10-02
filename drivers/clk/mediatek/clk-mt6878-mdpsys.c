// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chuan-wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6878-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs mdp_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_MDP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mdp_clks[] = {
	GATE_MDP(CLK_MDP_MUTEX0, "mdp_mutex0",
			"top_disp0_ck"/* parent */, 0),
	GATE_MDP(CLK_MDP_APB_BUS, "mdp_apb_bus",
			"top_disp0_ck"/* parent */, 1),
	GATE_MDP(CLK_MDP_SMI0, "mdp_smi0",
			"top_disp0_ck"/* parent */, 2),
	GATE_MDP(CLK_MDP_RDMA0, "mdp_rdma0",
			"top_disp0_ck"/* parent */, 3),
	GATE_MDP(CLK_MDP_HDR0, "mdp_hdr0",
			"top_disp0_ck"/* parent */, 5),
	GATE_MDP(CLK_MDP_AAL0, "mdp_aal0",
			"top_disp0_ck"/* parent */, 6),
	GATE_MDP(CLK_MDP_RSZ0, "mdp_rsz0",
			"top_disp0_ck"/* parent */, 7),
	GATE_MDP(CLK_MDP_TDSHP0, "mdp_tdshp0",
			"top_disp0_ck"/* parent */, 8),
	GATE_MDP(CLK_MDP_WROT0, "mdp_wrot0",
			"top_disp0_ck"/* parent */, 10),
	GATE_MDP(CLK_MDP_RDMA1, "mdp_rdma1",
			"top_disp0_ck"/* parent */, 15),
	GATE_MDP(CLK_MDP_RSZ1, "mdp_rsz1",
			"top_disp0_ck"/* parent */, 19),
	GATE_MDP(CLK_MDP_WROT1, "mdp_wrot1",
			"top_disp0_ck"/* parent */, 22),
};

static const struct mtk_clk_desc mdp_mcd = {
	.clks = mdp_clks,
	.num_clks = CLK_MDP_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6878_mdpsys[] = {
	{
		.compatible = "mediatek,mt6878-mdpsys",
		.data = &mdp_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6878_mdpsys_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6878_mdpsys_drv = {
	.probe = clk_mt6878_mdpsys_grp_probe,
	.driver = {
		.name = "clk-mt6878-mdpsys",
		.of_match_table = of_match_clk_mt6878_mdpsys,
	},
};

module_platform_driver(clk_mt6878_mdpsys_drv);
MODULE_LICENSE("GPL");
