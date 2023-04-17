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

static const struct mtk_gate_regs cam_m0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs cam_m1_cg_regs = {
	.set_ofs = 0x50,
	.clr_ofs = 0x54,
	.sta_ofs = 0x4C,
};

#define GATE_CAM_M0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_M1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_m_clks[] = {
	/* CAM_M0 */
	GATE_CAM_M0(CLK_CAM_MAIN_LARB13_CON_0, "cam_m_larb13_ck",
			"cam_ck"/* parent */, 0),
	GATE_CAM_M0(CLK_CAM_MAIN_LARB14_CON_0, "cam_m_larb14_ck",
			"cam_ck"/* parent */, 1),
	GATE_CAM_M0(CLK_CAM_MAIN_LARB27_CON_0, "cam_m_larb27_ck",
			"cam_ck"/* parent */, 2),
	GATE_CAM_M0(CLK_CAM_MAIN_LARB29_CON_0, "cam_m_larb29_ck",
			"cam_ck"/* parent */, 3),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_CON_0, "cam_m_cam_ck",
			"cam_ck"/* parent */, 4),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBA_CON_0, "cam_m_cam_suba_ck",
			"cam_ck"/* parent */, 5),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBB_CON_0, "cam_m_cam_subb_ck",
			"cam_ck"/* parent */, 6),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_SUBC_CON_0, "cam_m_cam_subc_ck",
			"cam_ck"/* parent */, 7),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_MRAW_CON_0, "cam_m_cam_mraw_ck",
			"cam_ck"/* parent */, 8),
	GATE_CAM_M0(CLK_CAM_MAIN_CAMTG_CON_0, "cam_m_camtg_ck",
			"camtm_ck"/* parent */, 9),
	GATE_CAM_M0(CLK_CAM_MAIN_SENINF_CON_0, "cam_m_seninf_ck",
			"cam_ck"/* parent */, 10),
	GATE_CAM_M0(CLK_CAM_MAIN_CAMSV_TOP_CON_0, "cam_m_camsv_ck",
			"cam_ck"/* parent */, 11),
	GATE_CAM_M0(CLK_CAM_MAIN_ADLRD_CON_0, "cam_m_adlrd_ck",
			"cam_ck"/* parent */, 12),
	GATE_CAM_M0(CLK_CAM_MAIN_ADLWR_CON_0, "cam_m_adlwr_ck",
			"cam_ck"/* parent */, 13),
	GATE_CAM_M0(CLK_CAM_MAIN_UISP_CON_0, "cam_m_uisp_ck",
			"cam_ck"/* parent */, 14),
	GATE_CAM_M0(CLK_CAM_MAIN_FAKE_ENG_CON_0, "cam_m_fake_eng_ck",
			"cam_ck"/* parent */, 15),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM2MM0_GALS_CON_0, "cam_m_cam2mm0_gcon_0",
			"cam_ck"/* parent */, 16),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM2MM1_GALS_CON_0, "cam_m_cam2mm1_gcon_0",
			"cam_ck"/* parent */, 17),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM2SYS_GALS_CON_0, "cam_m_cam2sys_gcon_0",
			"cam_ck"/* parent */, 18),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM2MM2_GALS_CON_0, "cam_m_cam2mm2_gcon_0",
			"cam_ck"/* parent */, 19),
	GATE_CAM_M0(CLK_CAM_MAIN_CCUSYS_CON_0, "cam_m_ccusys_ck",
			"vlp_ccusys_ck"/* parent */, 20),
	GATE_CAM_M0(CLK_CAM_MAIN_IPS_CON_0, "cam_m_ips_ck",
			"cam_ck"/* parent */, 21),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_DPE_CON_0, "cam_m_cam_dpe_ck",
			"cam_ck"/* parent */, 26),
	GATE_CAM_M0(CLK_CAM_MAIN_CAM_ASG_CON_0, "cam_m_cam_asg_ck",
			"cam_ck"/* parent */, 27),
	/* CAM_M1 */
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_A_CON_1, "cam_m_camsv_a_con_1",
			"cam_ck"/* parent */, 0),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_B_CON_1, "cam_m_camsv_b_con_1",
			"cam_ck"/* parent */, 1),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_C_CON_1, "cam_m_camsv_c_con_1",
			"cam_ck"/* parent */, 2),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_D_CON_1, "cam_m_camsv_d_con_1",
			"cam_ck"/* parent */, 3),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_E_CON_1, "cam_m_camsv_e_con_1",
			"cam_ck"/* parent */, 4),
	GATE_CAM_M1(CLK_CAM_MAIN_CAMSV_CON_1, "cam_m_camsv_con_1",
			"cam_ck"/* parent */, 5),
};

static const struct mtk_clk_desc cam_m_mcd = {
	.clks = cam_m_clks,
	.num_clks = CLK_CAM_M_NR_CLK,
};

static const struct mtk_gate_regs cam_mr_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_MR(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_mr_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_mr_clks[] = {
	GATE_CAM_MR(CLK_CAM_MR_LARBX, "cam_mr_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_MR(CLK_CAM_MR_GALS, "cam_mr_gals",
			"cam_ck"/* parent */, 1),
	GATE_CAM_MR(CLK_CAM_MR_CAMTG, "cam_mr_camtg",
			"camtm_ck"/* parent */, 2),
	GATE_CAM_MR(CLK_CAM_MR_MRAW0, "cam_mr_mraw0",
			"cam_ck"/* parent */, 3),
	GATE_CAM_MR(CLK_CAM_MR_MRAW1, "cam_mr_mraw1",
			"cam_ck"/* parent */, 4),
	GATE_CAM_MR(CLK_CAM_MR_MRAW2, "cam_mr_mraw2",
			"cam_ck"/* parent */, 5),
	GATE_CAM_MR(CLK_CAM_MR_MRAW3, "cam_mr_mraw3",
			"cam_ck"/* parent */, 6),
	GATE_CAM_MR(CLK_CAM_MR_PDA0, "cam_mr_pda0",
			"cam_ck"/* parent */, 7),
	GATE_CAM_MR(CLK_CAM_MR_PDA1, "cam_mr_pda1",
			"cam_ck"/* parent */, 8),
};

static const struct mtk_clk_desc cam_mr_mcd = {
	.clks = cam_mr_clks,
	.num_clks = CLK_CAM_MR_NR_CLK,
};

static const struct mtk_gate_regs cam_ra_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ra_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_ra_clks[] = {
	GATE_CAM_RA(CLK_CAM_RA_LARBX, "cam_ra_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RA(CLK_CAM_RA_CAM, "cam_ra_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RA(CLK_CAM_RA_CAMTG, "cam_ra_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAM_RA(CLK_CAM_RA_RAW2MM_GALS, "cam_ra_raw2mm_gals",
			"cam_ck"/* parent */, 3),
	GATE_CAM_RA(CLK_CAM_RA_YUV2RAW2MM_GALS, "cam_ra_yuv2mm_gals",
			"cam_ck"/* parent */, 4),
};

static const struct mtk_clk_desc cam_ra_mcd = {
	.clks = cam_ra_clks,
	.num_clks = CLK_CAM_RA_NR_CLK,
};

static const struct mtk_gate_regs cam_rb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_rb_clks[] = {
	GATE_CAM_RB(CLK_CAM_RB_LARBX, "cam_rb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RB(CLK_CAM_RB_CAM, "cam_rb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RB(CLK_CAM_RB_CAMTG, "cam_rb_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAM_RB(CLK_CAM_RB_RAW2MM_GALS, "cam_rb_raw2mm_gals",
			"cam_ck"/* parent */, 3),
	GATE_CAM_RB(CLK_CAM_RB_YUV2RAW2MM_GALS, "cam_rb_yuv2mm_gals",
			"cam_ck"/* parent */, 4),
};

static const struct mtk_clk_desc cam_rb_mcd = {
	.clks = cam_rb_clks,
	.num_clks = CLK_CAM_RB_NR_CLK,
};

static const struct mtk_gate_regs cam_rc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_rc_clks[] = {
	GATE_CAM_RC(CLK_CAM_RC_LARBX, "cam_rc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RC(CLK_CAM_RC_CAM, "cam_rc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RC(CLK_CAM_RC_CAMTG, "cam_rc_camtg",
			"cam_ck"/* parent */, 2),
	GATE_CAM_RC(CLK_CAM_RC_RAW2MM_GALS, "cam_rc_raw2mm_gals",
			"cam_ck"/* parent */, 3),
	GATE_CAM_RC(CLK_CAM_RC_YUV2RAW2MM_GALS, "cam_rc_yuv2mm_gals",
			"cam_ck"/* parent */, 4),
};

static const struct mtk_clk_desc cam_rc_mcd = {
	.clks = cam_rc_clks,
	.num_clks = CLK_CAM_RC_NR_CLK,
};

static const struct mtk_gate_regs cam_ya_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_YA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ya_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_ya_clks[] = {
	GATE_CAM_YA(CLK_CAM_YA_LARBX, "cam_ya_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YA(CLK_CAM_YA_CAM, "cam_ya_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YA(CLK_CAM_YA_CAMTG, "cam_ya_camtg",
			"cam_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_ya_mcd = {
	.clks = cam_ya_clks,
	.num_clks = CLK_CAM_YA_NR_CLK,
};

static const struct mtk_gate_regs cam_yb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_YB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_yb_clks[] = {
	GATE_CAM_YB(CLK_CAM_YB_LARBX, "cam_yb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YB(CLK_CAM_YB_CAM, "cam_yb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YB(CLK_CAM_YB_CAMTG, "cam_yb_camtg",
			"cam_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_yb_mcd = {
	.clks = cam_yb_clks,
	.num_clks = CLK_CAM_YB_NR_CLK,
};

static const struct mtk_gate_regs cam_yc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_YC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_yc_clks[] = {
	GATE_CAM_YC(CLK_CAM_YC_LARBX, "cam_yc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YC(CLK_CAM_YC_CAM, "cam_yc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YC(CLK_CAM_YC_CAMTG, "cam_yc_camtg",
			"cam_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_yc_mcd = {
	.clks = cam_yc_clks,
	.num_clks = CLK_CAM_YC_NR_CLK,
};

static const struct mtk_gate_regs cam_vcore_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x2C,
	.sta_ofs = 0x2C,
};

#define GATE_CAM_VCORE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_vcore_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate cam_vcore_clks[] = {
	GATE_CAM_VCORE(CLK_CAM_VCORE_MM0_SUBCOMMON_DCM_DIS, "camv_subc_dis",
			"mminfra_ck"/* parent */, 1),
};

static const struct mtk_clk_desc cam_vcore_mcd = {
	.clks = cam_vcore_clks,
	.num_clks = CLK_CAM_VCORE_NR_CLK,
};

static const struct mtk_gate_regs camsys_ipe_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_IPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_ipe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate camsys_ipe_clks[] = {
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_LARB19, "camsys_ipe_larb19",
			"dpe_ck"/* parent */, 0),
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_DPE, "camsys_ipe_dpe",
			"dpe_ck"/* parent */, 1),
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_FUS, "camsys_ipe_fus",
			"dpe_ck"/* parent */, 2),
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_DHZE, "camsys_ipe_dhze",
			"dpe_ck"/* parent */, 3),
	GATE_CAMSYS_IPE(CLK_CAMSYS_IPE_GALS, "camsys_ipe_gals",
			"dpe_ck"/* parent */, 4),
};

static const struct mtk_clk_desc camsys_ipe_mcd = {
	.clks = camsys_ipe_clks,
	.num_clks = CLK_CAMSYS_IPE_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6897_cam[] = {
	{
		.compatible = "mediatek,mt6897-camsys_main",
		.data = &cam_m_mcd,
	}, {
		.compatible = "mediatek,mt6897-camsys_mraw",
		.data = &cam_mr_mcd,
	}, {
		.compatible = "mediatek,mt6897-camsys_rawa",
		.data = &cam_ra_mcd,
	}, {
		.compatible = "mediatek,mt6897-camsys_rawb",
		.data = &cam_rb_mcd,
	}, {
		.compatible = "mediatek,mt6897-camsys_rawc",
		.data = &cam_rc_mcd,
	}, {
		.compatible = "mediatek,mt6897-camsys_yuva",
		.data = &cam_ya_mcd,
	}, {
		.compatible = "mediatek,mt6897-camsys_yuvb",
		.data = &cam_yb_mcd,
	}, {
		.compatible = "mediatek,mt6897-camsys_yuvc",
		.data = &cam_yc_mcd,
	}, {
		.compatible = "mediatek,mt6897-cam_vcore",
		.data = &cam_vcore_mcd,
	}, {
		.compatible = "mediatek,mt6897-camsys_ipe",
		.data = &camsys_ipe_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6897_cam_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6897_cam_drv = {
	.probe = clk_mt6897_cam_grp_probe,
	.driver = {
		.name = "clk-mt6897-cam",
		.of_match_table = of_match_clk_mt6897_cam,
	},
};

module_platform_driver(clk_mt6897_cam_drv);
MODULE_LICENSE("GPL");
