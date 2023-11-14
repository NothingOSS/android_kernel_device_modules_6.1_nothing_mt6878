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

static const struct mtk_gate_regs im_c_s_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IM_C_S(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &im_c_s_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate im_c_s_clks[] = {
	GATE_IM_C_S(CLK_IM_C_S_I3C5_W1S, "im_c_s_i3c5_w1s",
			"top_i2c_ck"/* parent */, 0),
	GATE_IM_C_S(CLK_IM_C_S_SEC_EN_W1S, "im_c_s_sec_w1s",
			"top_i2c_ck"/* parent */, 1),
};

static const struct mtk_clk_desc im_c_s_mcd = {
	.clks = im_c_s_clks,
	.num_clks = CLK_IM_C_S_NR_CLK,
};

static const struct mtk_gate_regs imp_es_s_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMP_ES_S(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imp_es_s_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate imp_es_s_clks[] = {
	GATE_IMP_ES_S(CLK_IMP_ES_S_I3C10_W1S, "imp_es_s_i3c10_w1s",
			"top_i2c_ck"/* parent */, 0),
	GATE_IMP_ES_S(CLK_IMP_ES_S_I3C11_W1S, "imp_es_s_i3c11_w1s",
			"top_i2c_ck"/* parent */, 1),
	GATE_IMP_ES_S(CLK_IMP_ES_S_I3C12_W1S, "imp_es_s_i3c12_w1s",
			"top_i2c_ck"/* parent */, 2),
	GATE_IMP_ES_S(CLK_IMP_ES_S_SEC_EN_W1S, "imp_es_s_sec_w1s",
			"top_i2c_ck"/* parent */, 3),
};

static const struct mtk_clk_desc imp_es_s_mcd = {
	.clks = imp_es_s_clks,
	.num_clks = CLK_IMP_ES_S_NR_CLK,
};

static const struct mtk_gate_regs imp_e_s_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMP_E_S(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imp_e_s_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate imp_e_s_clks[] = {
	GATE_IMP_E_S(CLK_IMP_E_S_I3C0_W1S, "imp_e_s_i3c0_w1s",
			"top_i2c_ck"/* parent */, 0),
	GATE_IMP_E_S(CLK_IMP_E_S_I3C1_W1S, "imp_e_s_i3c1_w1s",
			"top_i2c_ck"/* parent */, 1),
	GATE_IMP_E_S(CLK_IMP_E_S_I3C2_W1S, "imp_e_s_i3c2_w1s",
			"top_i2c_ck"/* parent */, 2),
	GATE_IMP_E_S(CLK_IMP_E_S_I3C4_W1S, "imp_e_s_i3c4_w1s",
			"top_i2c_ck"/* parent */, 3),
	GATE_IMP_E_S(CLK_IMP_E_S_I3C9_W1S, "imp_e_s_i3c9_w1s",
			"top_i2c_ck"/* parent */, 4),
	GATE_IMP_E_S(CLK_IMP_E_S_SEC_EN_W1S, "imp_e_s_sec_w1s",
			"top_i2c_ck"/* parent */, 5),
};

static const struct mtk_clk_desc imp_e_s_mcd = {
	.clks = imp_e_s_clks,
	.num_clks = CLK_IMP_E_S_NR_CLK,
};

static const struct mtk_gate_regs imp_w_s_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMP_W_S(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imp_w_s_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate imp_w_s_clks[] = {
	GATE_IMP_W_S(CLK_IMP_W_S_I3C3_W1S, "imp_w_s_i3c3_w1s",
			"top_i2c_ck"/* parent */, 0),
	GATE_IMP_W_S(CLK_IMP_W_S_I3C6_W1S, "imp_w_s_i3c6_w1s",
			"top_i2c_ck"/* parent */, 1),
	GATE_IMP_W_S(CLK_IMP_W_S_I3C7_W1S, "imp_w_s_i3c7_w1s",
			"top_i2c_ck"/* parent */, 2),
	GATE_IMP_W_S(CLK_IMP_W_S_I3C8_W1S, "imp_w_s_i3c8_w1s",
			"top_i2c_ck"/* parent */, 3),
	GATE_IMP_W_S(CLK_IMP_W_S_SEC_EN_W1S, "imp_w_s_sec_w1s",
			"top_i2c_ck"/* parent */, 4),
};

static const struct mtk_clk_desc imp_w_s_mcd = {
	.clks = imp_w_s_clks,
	.num_clks = CLK_IMP_W_S_NR_CLK,
};

static const struct mtk_gate_regs perao0_cg_regs = {
	.set_ofs = 0x24,
	.clr_ofs = 0x28,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs perao1_cg_regs = {
	.set_ofs = 0x2C,
	.clr_ofs = 0x30,
	.sta_ofs = 0x14,
};

static const struct mtk_gate_regs perao2_cg_regs = {
	.set_ofs = 0x34,
	.clr_ofs = 0x38,
	.sta_ofs = 0x18,
};

#define GATE_PERAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PERAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(CLK_PERAOP_UART0, "peraop_uart0",
			"top_uart_ck"/* parent */, 0),
	GATE_PERAO0(CLK_PERAOP_UART1, "peraop_uart1",
			"top_uart_ck"/* parent */, 1),
	GATE_PERAO0(CLK_PERAOP_UART2, "peraop_uart2",
			"top_uart_ck"/* parent */, 2),
	GATE_PERAO0(CLK_PERAOP_PWM_H, "peraop_pwm_h",
			"top_axip_ck"/* parent */, 4),
	GATE_PERAO0(CLK_PERAOP_PWM_B, "peraop_pwm_b",
			"top_pwm_ck"/* parent */, 5),
	GATE_PERAO0(CLK_PERAOP_PWM_FB1, "peraop_pwm_fb1",
			"top_pwm_ck"/* parent */, 6),
	GATE_PERAO0(CLK_PERAOP_PWM_FB2, "peraop_pwm_fb2",
			"top_pwm_ck"/* parent */, 7),
	GATE_PERAO0(CLK_PERAOP_PWM_FB3, "peraop_pwm_fb3",
			"top_pwm_ck"/* parent */, 8),
	GATE_PERAO0(CLK_PERAOP_PWM_FB4, "peraop_pwm_fb4",
			"top_pwm_ck"/* parent */, 9),
	GATE_PERAO0(CLK_PERAOP_SPI0_B, "peraop_spi0_b",
			"top_spi0_ck"/* parent */, 12),
	GATE_PERAO0(CLK_PERAOP_SPI1_B, "peraop_spi1_b",
			"top_spi1_ck"/* parent */, 13),
	GATE_PERAO0(CLK_PERAOP_SPI2_B, "peraop_spi2_b",
			"top_spi2_ck"/* parent */, 14),
	GATE_PERAO0(CLK_PERAOP_SPI3_B, "peraop_spi3_b",
			"top_spi3_ck"/* parent */, 15),
	GATE_PERAO0(CLK_PERAOP_SPI4_B, "peraop_spi4_b",
			"top_spi4_ck"/* parent */, 16),
	GATE_PERAO0(CLK_PERAOP_SPI5_B, "peraop_spi5_b",
			"top_spi5_ck"/* parent */, 17),
	GATE_PERAO0(CLK_PERAOP_SPI6_B, "peraop_spi6_b",
			"top_spi6_ck"/* parent */, 18),
	GATE_PERAO0(CLK_PERAOP_SPI7_B, "peraop_spi7_b",
			"top_spi7_ck"/* parent */, 19),
	GATE_PERAO0(CLK_PERAOP_DMA_B, "peraop_dma_b",
			"top_axip_ck"/* parent */, 29),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAOP_SSUSB0_FRMCNT, "peraop_ssusb0_frmcnt",
			"da_univ_48m_div_ck"/* parent */, 1),
	GATE_PERAO1(CLK_PERAOP_MSDC0, "peraop_msdc0",
			"top_msdc50_0_ck"/* parent */, 7),
	GATE_PERAO1(CLK_PERAOP_MSDC0_H, "peraop_msdc0_h",
			"top_msdc5hclk_ck"/* parent */, 8),
	GATE_PERAO1(CLK_PERAOP_MSDC0_FAES, "peraop_msdc0_faes",
			"top_aes_msdcfde_ck"/* parent */, 9),
	GATE_PERAO1(CLK_PERAOP_MSDC0_MST_F, "peraop_msdc0_mst_f",
			"top_axip_ck"/* parent */, 10),
	GATE_PERAO1(CLK_PERAOP_MSDC0_SLV_H, "peraop_msdc0_slv_h",
			"top_axip_ck"/* parent */, 11),
	GATE_PERAO1(CLK_PERAOP_MSDC1, "peraop_msdc1",
			"top_msdc30_1_ck"/* parent */, 12),
	GATE_PERAO1(CLK_PERAOP_MSDC1_H, "peraop_msdc1_h",
			"top_msdc30_1_h_ck"/* parent */, 13),
	GATE_PERAO1(CLK_PERAOP_MSDC1_MST_F, "peraop_msdc1_mst_f",
			"top_axip_ck"/* parent */, 14),
	GATE_PERAO1(CLK_PERAOP_MSDC1_SLV_H, "peraop_msdc1_slv_h",
			"top_axip_ck"/* parent */, 15),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAOP_AUDIO0, "peraop_audio0",
			"top_axip_ck"/* parent */, 0),
	GATE_PERAO2(CLK_PERAOP_AUDIO1, "peraop_audio1",
			"top_axip_ck"/* parent */, 1),
	GATE_PERAO2(CLK_PERAOP_AUDIO2, "peraop_audio2",
			"top_aud_intbus_ck"/* parent */, 2),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct mtk_gate_regs ufsao_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSAO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufsao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ufsao_clks[] = {
	GATE_UFSAO(CLK_UFSAO_UNIPRO_TX_SYM, "ufsao_unipro_tx_sym",
			"top_f26m_ck"/* parent */, 0),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_RX_SYM0, "ufsao_unipro_rx_sym0",
			"top_f26m_ck"/* parent */, 1),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_RX_SYM1, "ufsao_unipro_rx_sym1",
			"top_f26m_ck"/* parent */, 2),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_SYS, "ufsao_unipro_sys",
			"top_ufs_ck"/* parent */, 3),
	GATE_UFSAO(CLK_UFSAO_UNIPRO_SAP_CFG, "ufsao_unipro_sap_cfg",
			"top_f26m_ck"/* parent */, 8),
	GATE_UFSAO(CLK_UFSAO_PHY_TOP_AHB_S_BUS, "ufsao_phy_ahb_s_bus",
			"hf_fufs_faxi_ck"/* parent */, 9),
};

static const struct mtk_clk_desc ufsao_mcd = {
	.clks = ufsao_clks,
	.num_clks = CLK_UFSAO_NR_CLK,
};

static const struct mtk_gate_regs ufspdn_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSPDN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufspdn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ufspdn_clks[] = {
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI, "ufspdn_UFSHCI",
			"top_ufs_ck"/* parent */, 0),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_AES, "ufspdn_ufshci_aes",
			"top_aes_ufsfde_ck"/* parent */, 1),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_AHB, "ufspdn_UFSHCI_ahb",
			"hf_fufs_faxi_ck"/* parent */, 3),
	GATE_UFSPDN(CLK_UFSPDN_UFSHCI_AXI, "ufspdn_UFSHCI_axi",
			"hf_fufs_fmem_sub_ck"/* parent */, 5),
};

static const struct mtk_clk_desc ufspdn_mcd = {
	.clks = ufspdn_clks,
	.num_clks = CLK_UFSPDN_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6878_peri[] = {
	{
		.compatible = "mediatek,mt6878-imp_iic_wrap_cen_s",
		.data = &im_c_s_mcd,
	}, {
		.compatible = "mediatek,mt6878-imp_iic_wrap_es_s",
		.data = &imp_es_s_mcd,
	}, {
		.compatible = "mediatek,mt6878-imp_iic_wrap_e_s",
		.data = &imp_e_s_mcd,
	}, {
		.compatible = "mediatek,mt6878-imp_iic_wrap_w_s",
		.data = &imp_w_s_mcd,
	}, {
		.compatible = "mediatek,mt6878-pericfg_ao",
		.data = &perao_mcd,
	}, {
		.compatible = "mediatek,mt6878-ufscfg_ao",
		.data = &ufsao_mcd,
	}, {
		.compatible = "mediatek,mt6878-ufscfg_pdn",
		.data = &ufspdn_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6878_peri_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6878_peri_drv = {
	.probe = clk_mt6878_peri_grp_probe,
	.driver = {
		.name = "clk-mt6878-peri",
		.of_match_table = of_match_clk_mt6878_peri,
	},
};

module_platform_driver(clk_mt6878_peri_drv);
MODULE_LICENSE("GPL");
