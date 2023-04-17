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

static const struct mtk_gate_regs impc_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impc_clks[] = {
	GATE_IMPC(CLK_IMPC_AP_CLOCK, "impc_ap_clock",
			"i2c_ck"/* parent */, 0),
};

static const struct mtk_clk_desc impc_mcd = {
	.clks = impc_clks,
	.num_clks = CLK_IMPC_NR_CLK,
};

static const struct mtk_gate_regs impn_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impn_clks[] = {
	GATE_IMPN(CLK_IMPN_AP_CLOCK_I2C3, "impn_api2c3",
			"i2c_ck"/* parent */, 0),
	GATE_IMPN(CLK_IMPN_AP_CLOCK_I2C5, "impn_api2c5",
			"i2c_ck"/* parent */, 1),
};

static const struct mtk_clk_desc impn_mcd = {
	.clks = impn_clks,
	.num_clks = CLK_IMPN_NR_CLK,
};

static const struct mtk_gate_regs imps_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPS(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imps_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate imps_clks[] = {
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C0, "imps_api2c0",
			"i2c_ck"/* parent */, 0),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C1, "imps_api2c1",
			"i2c_ck"/* parent */, 1),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C7, "imps_api2c7",
			"i2c_ck"/* parent */, 2),
};

static const struct mtk_clk_desc imps_mcd = {
	.clks = imps_clks,
	.num_clks = CLK_IMPS_NR_CLK,
};

static const struct mtk_gate_regs perao0_cg_regs = {
	.set_ofs = 0x24,
	.clr_ofs = 0x28,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs perao0_hwv_regs = {
	.set_ofs = 0x0078,
	.clr_ofs = 0x007C,
	.sta_ofs = 0x1C3C,
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

#define GATE_HWV_PERAO0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &perao0_cg_regs,			\
		.hwv_regs = &perao0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
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
			"uart_ck"/* parent */, 0),
	GATE_PERAO0(CLK_PERAOP_UART1, "peraop_uart1",
			"uart_ck"/* parent */, 1),
	GATE_PERAO0(CLK_PERAOP_UART2, "peraop_uart2",
			"uart_ck"/* parent */, 2),
	GATE_PERAO0(CLK_PERAOP_UART3, "peraop_uart3",
			"uart_ck"/* parent */, 3),
	GATE_PERAO0(CLK_PERAOP_PWM_HCLK, "peraop_pwm_hclk",
			"peri_faxi_ck"/* parent */, 4),
	GATE_PERAO0(CLK_PERAOP_PWM_BCLK, "peraop_pwm_bclk",
			"pwm_ck"/* parent */, 5),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK1, "peraop_pwm_fbclk1",
			"pwm_ck"/* parent */, 6),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK2, "peraop_pwm_fbclk2",
			"pwm_ck"/* parent */, 7),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK3, "peraop_pwm_fbclk3",
			"pwm_ck"/* parent */, 8),
	GATE_PERAO0(CLK_PERAOP_PWM_FBCLK4, "peraop_pwm_fbclk4",
			"pwm_ck"/* parent */, 9),
	GATE_HWV_PERAO0(CLK_PERAOP_DISP_PWM0, "peraop_disp_pwm0",
			"disp_pwm_ck"/* parent */, 10),
	GATE_HWV_PERAO0(CLK_PERAOP_DISP_PWM1, "peraop_disp_pwm1",
			"disp_pwm_ck"/* parent */, 11),
	GATE_HWV_PERAO0(CLK_PERAOP_SPI0_BCLK, "peraop_spi0_bclk",
			"spi0_ck"/* parent */, 12),
	GATE_PERAO0(CLK_PERAOP_SPI1_BCLK, "peraop_spi1_bclk",
			"spi1_ck"/* parent */, 13),
	GATE_PERAO0(CLK_PERAOP_SPI2_BCLK, "peraop_spi2_bclk",
			"spi2_ck"/* parent */, 14),
	GATE_PERAO0(CLK_PERAOP_SPI3_BCLK, "peraop_spi3_bclk",
			"spi3_ck"/* parent */, 15),
	GATE_PERAO0(CLK_PERAOP_SPI4_BCLK, "peraop_spi4_bclk",
			"spi4_ck"/* parent */, 16),
	GATE_PERAO0(CLK_PERAOP_SPI5_BCLK, "peraop_spi5_bclk",
			"spi5_ck"/* parent */, 17),
	GATE_PERAO0(CLK_PERAOP_SPI6_BCLK, "peraop_spi6_bclk",
			"spi6_ck"/* parent */, 18),
	GATE_PERAO0(CLK_PERAOP_SPI7_BCLK, "peraop_spi7_bclk",
			"spi7_ck"/* parent */, 19),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAOP_DMA_BCLK, "peraop_dma_bclk",
			"peri_faxi_ck"/* parent */, 1),
	GATE_PERAO1(CLK_PERAOP_SSUSB0_FRMCNT, "peraop_ssusb0_frmcnt",
			"ssusb_fmcnt_ck"/* parent */, 4),
	GATE_PERAO1(CLK_PERAOP_MSDC1, "peraop_msdc1",
			"msdc30_1_ck"/* parent */, 10),
	GATE_PERAO1(CLK_PERAOP_MSDC1_FCLK, "peraop_msdc1_fclk",
			"peri_faxi_ck"/* parent */, 11),
	GATE_PERAO1(CLK_PERAOP_MSDC1_HCLK, "peraop_msdc1_hclk",
			"peri_faxi_ck"/* parent */, 12),
	GATE_PERAO1(CLK_PERAOP_MSDC2, "peraop_msdc2",
			"msdc30_2_ck"/* parent */, 13),
	GATE_PERAO1(CLK_PERAOP_MSDC2_FCLK, "peraop_msdc2_fclk",
			"peri_faxi_ck"/* parent */, 14),
	GATE_PERAO1(CLK_PERAOP_MSDC2_HCLK, "peraop_msdc2_hclk",
			"peri_faxi_ck"/* parent */, 15),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAOP_AUDIO_SLV_CK, "peraop_audio_slv_ck",
			"peri_faxi_ck"/* parent */, 0),
	GATE_PERAO2(CLK_PERAOP_AUDIO_MST_CK, "peraop_audio_mst_ck",
			"peri_faxi_ck"/* parent */, 1),
	GATE_PERAO2(CLK_PERAOP_AUDIO_INTBUS_CK, "peraop_aud_intbusck",
			"aud_intbus_ck"/* parent */, 2),
};

static const struct mtk_clk_desc perao_mcd = {
	.clks = perao_clks,
	.num_clks = CLK_PERAO_NR_CLK,
};

static const struct mtk_gate_regs pextpcfg_ao_cg_regs = {
	.set_ofs = 0x18,
	.clr_ofs = 0x1C,
	.sta_ofs = 0x14,
};

#define GATE_PEXTPCFG_AO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &pextpcfg_ao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate pextpcfg_ao_clks[] = {
	GATE_PEXTPCFG_AO(CLK_PEXTPCFG_AO_PEXTP_P0_MAC_REF, "pextpcfg_ao_mac_rclk",
			"f26m_ck"/* parent */, 0),
	GATE_PEXTPCFG_AO(CLK_PEXTPCFG_AO_PEXTP_P0_MAC_PL_PCLK, "pextpcfg_ao_mac_pclk",
			"f26m_ck"/* parent */, 1),
	GATE_PEXTPCFG_AO(CLK_PEXTPCFG_AO_PEXTP_P0_MAC_TL, "pextpcfg_ao_m_tck",
			"tl_ck"/* parent */, 2),
	GATE_PEXTPCFG_AO(CLK_PEXTPCFG_AO_PEXTP_P0_MAC_AXI, "pextpcfg_ao_m_axck",
			"pextp_fmem_sub_ck"/* parent */, 3),
	GATE_PEXTPCFG_AO(CLK_PEXTPCFG_AO_PEXTP_P0_MAC_AHB_APB, "pextpcfg_ao_m_ahck",
			"pextp_faxi_ck"/* parent */, 4),
	GATE_PEXTPCFG_AO(CLK_PEXTPCFG_AO_PEXTP_P0_PHY_REF_CK, "pextpcfg_ao_m_pck",
			"f26m_ck"/* parent */, 5),
};

static const struct mtk_clk_desc pextpcfg_ao_mcd = {
	.clks = pextpcfg_ao_clks,
	.num_clks = CLK_PEXTPCFG_AO_NR_CLK,
};

static const struct mtk_gate_regs ufscfg_ao_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSCFG_AO(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufscfg_ao_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ufscfg_ao_clks[] = {
	GATE_UFSCFG_AO(CLK_UFSCFG_AO_UNIPRO_TX_SYMBOLCLK, "ufs_uni_tx_symbolclk",
			"f26m_ck"/* parent */, 0),
	GATE_UFSCFG_AO(CLK_UFSCFG_AO_UNIPRO_RX_SYMBOLCLK0, "ufscfg_ao_ufx_rx0",
			"f26m_ck"/* parent */, 1),
	GATE_UFSCFG_AO(CLK_UFSCFG_AO_UNIPRO_RX_SYMBOLCLK1, "ufscfg_ao_ufx_rx1",
			"f26m_ck"/* parent */, 2),
	GATE_UFSCFG_AO(CLK_UFSCFG_AO_UNIPRO_SYSCLK, "ufs_uni_sysclk",
			"ufs_ck"/* parent */, 3),
};

static const struct mtk_clk_desc ufscfg_ao_mcd = {
	.clks = ufscfg_ao_clks,
	.num_clks = CLK_UFSCFG_AO_NR_CLK,
};

static const struct mtk_gate_regs ufscfg_pdn_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x4,
};

#define GATE_UFSCFG_PDN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ufscfg_pdn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ufscfg_pdn_clks[] = {
	GATE_UFSCFG_PDN(CLK_UFSCFG_UFSHCI_UFS, "ufscfg_ufshci_u_clk",
			"ufs_ck"/* parent */, 0),
	GATE_UFSCFG_PDN(CLK_UFSCFG_UFSHCI_AES, "ufscfg_ufshci_ck",
			"aes_ufsfde_ck"/* parent */, 1),
};

static const struct mtk_clk_desc ufscfg_pdn_mcd = {
	.clks = ufscfg_pdn_clks,
	.num_clks = CLK_UFSCFG_PDN_NR_CLK,
};

static const struct mtk_gate_regs impen_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPEN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impen_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impen_clks[] = {
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C2, "impen_api2c2",
			"i2c_ck"/* parent */, 0),
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C4, "impen_api2c4",
			"i2c_ck"/* parent */, 1),
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C10, "impen_api2c10",
			"i2c_ck"/* parent */, 2),
	GATE_IMPEN(CLK_IMPEN_AP_CLOCK_I2C11, "impen_api2c11",
			"i2c_ck"/* parent */, 3),
};

static const struct mtk_clk_desc impen_mcd = {
	.clks = impen_clks,
	.num_clks = CLK_IMPEN_NR_CLK,
};

static const struct mtk_gate_regs impes_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMPES(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impes_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impes_clks[] = {
	GATE_IMPES(CLK_IMPES_AP_CLOCK_I2C8, "impes_api2c8",
			"i2c_ck"/* parent */, 0),
	GATE_IMPES(CLK_IMPES_AP_CLOCK_I2C9, "impes_api2c9",
			"i2c_ck"/* parent */, 1),
	GATE_IMPES(CLK_IMPES_AP_CLOCK_I2C12, "impes_api2c12",
			"i2c_ck"/* parent */, 2),
	GATE_IMPES(CLK_IMPES_AP_CLOCK_I2C13, "impes_api2c13",
			"i2c_ck"/* parent */, 3),
};

static const struct mtk_clk_desc impes_mcd = {
	.clks = impes_clks,
	.num_clks = CLK_IMPES_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6897_peri[] = {
	{
		.compatible = "mediatek,mt6897-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6897-imp_iic_wrap_n",
		.data = &impn_mcd,
	}, {
		.compatible = "mediatek,mt6897-imp_iic_wrap_s",
		.data = &imps_mcd,
	}, {
		.compatible = "mediatek,mt6897-imp_iic_wrap_en",
		.data = &impen_mcd,
	}, {
		.compatible = "mediatek,mt6897-imp_iic_wrap_es",
		.data = &impes_mcd,
	}, {
		.compatible = "mediatek,mt6897-pericfg_ao",
		.data = &perao_mcd,
	}, {
		.compatible = "mediatek,mt6897-pextpcfg_ao",
		.data = &pextpcfg_ao_mcd,
	}, {
		.compatible = "mediatek,mt6897-ufscfg_ao",
		.data = &ufscfg_ao_mcd,
	}, {
		.compatible = "mediatek,mt6897-ufscfg_pdn",
		.data = &ufscfg_pdn_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6897_peri_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6897_peri_drv = {
	.probe = clk_mt6897_peri_grp_probe,
	.driver = {
		.name = "clk-mt6897-peri",
		.of_match_table = of_match_clk_mt6897_peri,
	},
};

module_platform_driver(clk_mt6897_peri_drv);
MODULE_LICENSE("GPL");
