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

static const struct mtk_gate_regs scp_cg_regs = {
	.set_ofs = 0x154,
	.clr_ofs = 0x158,
	.sta_ofs = 0x154,
};

#define GATE_SCP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate scp_clks[] = {
	GATE_SCP(CLK_SCP_SET_SPI0, "scp_set_spi0",
			"top_f26m_ck"/* parent */, 0),
	GATE_SCP(CLK_SCP_SET_SPI1, "scp_set_spi1",
			"top_f26m_ck"/* parent */, 1),
	GATE_SCP(CLK_SCP_SET_SPI2, "scp_set_spi2",
			"top_f26m_ck"/* parent */, 2),
	GATE_SCP(CLK_SCP_SET_SPI3, "scp_set_spi3",
			"top_f26m_ck"/* parent */, 3),
};

static const struct mtk_clk_desc scp_mcd = {
	.clks = scp_clks,
	.num_clks = CLK_SCP_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6878_vlp[] = {
	{
		.compatible = "mediatek,mt6878-scp",
		.data = &scp_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6878_vlp_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6878_vlp_drv = {
	.probe = clk_mt6878_vlp_grp_probe,
	.driver = {
		.name = "clk-mt6878-vlp",
		.of_match_table = of_match_clk_mt6878_vlp,
	},
};

module_platform_driver(clk_mt6878_vlp_drv);
MODULE_LICENSE("GPL");
