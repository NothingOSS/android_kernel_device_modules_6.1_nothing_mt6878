// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chuan-wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6878-clk.h>

/* bringup config */
#define MT_CCF_BRINGUP		1
#define MT_CCF_PLL_DISABLE	0
#define MT_CCF_MUX_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

/* TOPCK MUX SEL REG */
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_UPDATE1				0x0008
#define CLK_CFG_UPDATE2				0x000c
#define VLP_CLK_CFG_UPDATE			0x0004
#define CLK_CFG_0				0x0010
#define CLK_CFG_0_SET				0x0014
#define CLK_CFG_0_CLR				0x0018
#define CLK_CFG_1				0x0020
#define CLK_CFG_1_SET				0x0024
#define CLK_CFG_1_CLR				0x0028
#define CLK_CFG_2				0x0030
#define CLK_CFG_2_SET				0x0034
#define CLK_CFG_2_CLR				0x0038
#define CLK_CFG_3				0x0040
#define CLK_CFG_3_SET				0x0044
#define CLK_CFG_3_CLR				0x0048
#define CLK_CFG_4				0x0050
#define CLK_CFG_4_SET				0x0054
#define CLK_CFG_4_CLR				0x0058
#define CLK_CFG_5				0x0060
#define CLK_CFG_5_SET				0x0064
#define CLK_CFG_5_CLR				0x0068
#define CLK_CFG_6				0x0070
#define CLK_CFG_6_SET				0x0074
#define CLK_CFG_6_CLR				0x0078
#define CLK_CFG_7				0x0080
#define CLK_CFG_7_SET				0x0084
#define CLK_CFG_7_CLR				0x0088
#define CLK_CFG_8				0x0090
#define CLK_CFG_8_SET				0x0094
#define CLK_CFG_8_CLR				0x0098
#define CLK_CFG_9				0x00A0
#define CLK_CFG_9_SET				0x00A4
#define CLK_CFG_9_CLR				0x00A8
#define CLK_CFG_10				0x00B0
#define CLK_CFG_10_SET				0x00B4
#define CLK_CFG_10_CLR				0x00B8
#define CLK_CFG_11				0x00C0
#define CLK_CFG_11_SET				0x00C4
#define CLK_CFG_11_CLR				0x00C8
#define CLK_CFG_12				0x00D0
#define CLK_CFG_12_SET				0x00D4
#define CLK_CFG_12_CLR				0x00D8
#define CLK_CFG_13				0x00E0
#define CLK_CFG_13_SET				0x00E4
#define CLK_CFG_13_CLR				0x00E8
#define CLK_CFG_14				0x00F0
#define CLK_CFG_14_SET				0x00F4
#define CLK_CFG_14_CLR				0x00F8
#define CLK_CFG_15				0x0100
#define CLK_CFG_15_SET				0x0104
#define CLK_CFG_15_CLR				0x0108
#define CLK_CFG_16				0x0110
#define CLK_CFG_16_SET				0x0114
#define CLK_CFG_16_CLR				0x0118
#define CLK_CFG_18				0x0190
#define CLK_CFG_18_SET				0x0194
#define CLK_CFG_18_CLR				0x0198
#define CLK_CFG_20				0x0120
#define CLK_CFG_20_SET				0x0124
#define CLK_CFG_20_CLR				0x0128
#define CLK_AUDDIV_0				0x0320
#define VLP_CLK_CFG_0				0x0008
#define VLP_CLK_CFG_0_SET				0x000C
#define VLP_CLK_CFG_0_CLR				0x0010
#define VLP_CLK_CFG_1				0x0014
#define VLP_CLK_CFG_1_SET				0x0018
#define VLP_CLK_CFG_1_CLR				0x001C
#define VLP_CLK_CFG_2				0x0020
#define VLP_CLK_CFG_2_SET				0x0024
#define VLP_CLK_CFG_2_CLR				0x0028
#define VLP_CLK_CFG_3				0x002C
#define VLP_CLK_CFG_3_SET				0x0030
#define VLP_CLK_CFG_3_CLR				0x0034
#define VLP_CLK_CFG_4				0x0038
#define VLP_CLK_CFG_4_SET				0x003C
#define VLP_CLK_CFG_4_CLR				0x0040

/* TOPCK MUX SHIFT */
#define TOP_MUX_AXI_SHIFT			0
#define TOP_MUX_AXI_PERI_SHIFT			1
#define TOP_MUX_AXI_UFS_SHIFT			2
#define TOP_MUX_BUS_AXIMEM_SHIFT		3
#define TOP_MUX_DISP0_SHIFT			4
#define TOP_MUX_MMINFRA_SHIFT			5
#define TOP_MUX_MMUP_SHIFT			6
#define TOP_MUX_CAMTG_SHIFT			7
#define TOP_MUX_CAMTG2_SHIFT			8
#define TOP_MUX_CAMTG3_SHIFT			9
#define TOP_MUX_CAMTG4_SHIFT			10
#define TOP_MUX_CAMTG5_SHIFT			11
#define TOP_MUX_CAMTG6_SHIFT			12
#define TOP_MUX_UART_SHIFT			13
#define TOP_MUX_SPI0_SHIFT			14
#define TOP_MUX_SPI1_SHIFT			15
#define TOP_MUX_SPI2_SHIFT			16
#define TOP_MUX_SPI3_SHIFT			17
#define TOP_MUX_SPI4_SHIFT			18
#define TOP_MUX_SPI5_SHIFT			19
#define TOP_MUX_SPI6_SHIFT			20
#define TOP_MUX_SPI7_SHIFT			21
#define TOP_MUX_MSDC_MACRO_0P_SHIFT		22
#define TOP_MUX_MSDC50_0_HCLK_SHIFT		23
#define TOP_MUX_MSDC50_0_SHIFT			24
#define TOP_MUX_AES_MSDCFDE_SHIFT		25
#define TOP_MUX_MSDC_MACRO_1P_SHIFT		26
#define TOP_MUX_MSDC30_1_SHIFT			27
#define TOP_MUX_MSDC30_1_HCLK_SHIFT		28
#define TOP_MUX_AUD_INTBUS_SHIFT		29
#define TOP_MUX_ATB_SHIFT			30
#define TOP_MUX_USB_TOP_SHIFT			1
#define TOP_MUX_SSUSB_XHCI_SHIFT		2
#define TOP_MUX_I2C_SHIFT			3
#define TOP_MUX_SENINF_SHIFT			4
#define TOP_MUX_SENINF1_SHIFT			5
#define TOP_MUX_SENINF2_SHIFT			6
#define TOP_MUX_SENINF3_SHIFT			7
#define TOP_MUX_AUD_ENGEN1_SHIFT		8
#define TOP_MUX_AUD_ENGEN2_SHIFT		9
#define TOP_MUX_AES_UFSFDE_SHIFT		10
#define TOP_MUX_UFS_SHIFT			11
#define TOP_MUX_UFS_MBIST_SHIFT			12
#define TOP_MUX_AUD_1_SHIFT			13
#define TOP_MUX_AUD_2_SHIFT			14
#define TOP_MUX_DPMAIF_MAIN_SHIFT		15
#define TOP_MUX_VENC_SHIFT			16
#define TOP_MUX_VDEC_SHIFT			17
#define TOP_MUX_PWM_SHIFT			18
#define TOP_MUX_AUDIO_H_SHIFT			19
#define TOP_MUX_MCUPM_SHIFT			20
#define TOP_MUX_MEM_SUB_SHIFT			21
#define TOP_MUX_MEM_SUB_PERI_SHIFT		22
#define TOP_MUX_MEM_SUB_UFS_SHIFT		23
#define TOP_MUX_EMI_N_SHIFT			24
#define TOP_MUX_DSI_OCC_SHIFT			25
#define TOP_MUX_AP2CONN_HOST_SHIFT		26
#define TOP_MUX_IMG1_SHIFT			27
#define TOP_MUX_IPE_SHIFT			28
#define TOP_MUX_CAM_SHIFT			29
#define TOP_MUX_CCUSYS_SHIFT			30
#define TOP_MUX_CAMTM_SHIFT			0
#define TOP_MUX_CCU_AHB_SHIFT			1
#define TOP_MUX_CCUTM_SHIFT			2
#define TOP_MUX_MSDC_1P_RX_SHIFT		3
#define TOP_MUX_DSP_SHIFT			4
#define TOP_MUX_EMI_INTERFACE_546_SHIFT		5
#define TOP_MUX_MFG_REF_SHIFT			11
#define TOP_MUX_MFGSC_REF_SHIFT			12
#define TOP_MUX_SCP_SHIFT			0
#define TOP_MUX_PWRAP_ULPOSC_SHIFT		1
#define TOP_MUX_SPMI_P_MST_SHIFT		2
#define TOP_MUX_SPMI_M_MST_SHIFT		3
#define TOP_MUX_DVFSRC_SHIFT			4
#define TOP_MUX_PWM_VLP_SHIFT			5
#define TOP_MUX_AXI_VLP_SHIFT			6
#define TOP_MUX_DBGAO_26M_SHIFT			7
#define TOP_MUX_SYSTIMER_26M_SHIFT		8
#define TOP_MUX_SSPM_SHIFT			9
#define TOP_MUX_SSPM_F26M_SHIFT			10
#define TOP_MUX_SRCK_SHIFT			11
#define TOP_MUX_SCP_SPI_SHIFT			12
#define TOP_MUX_SCP_IIC_SHIFT			13
#define TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT		14
#define TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT		15
#define TOP_MUX_SSPM_ULPOSC_SHIFT		16
#define TOP_MUX_APXGPT_26M_SHIFT		18

/* TOPCK CKSTA REG */


/* TOPCK DIVIDER REG */
#define CLK_AUDDIV_5				0x033C

/* APMIXED PLL REG */
#define AP_PLL_CON3				0x00C
#define APLL1_TUNER_CON0			0x040
#define APLL2_TUNER_CON0			0x044
#define ARMPLL_LL_CON0				0x204
#define ARMPLL_LL_CON1				0x208
#define ARMPLL_LL_CON2				0x20C
#define ARMPLL_LL_CON3				0x210
#define ARMPLL_BL_CON0				0x214
#define ARMPLL_BL_CON1				0x218
#define ARMPLL_BL_CON2				0x21C
#define ARMPLL_BL_CON3				0x220
#define CCIPLL_CON0				0x224
#define CCIPLL_CON1				0x228
#define CCIPLL_CON2				0x22C
#define CCIPLL_CON3				0x230
#define MAINPLL_CON0				0x304
#define MAINPLL_CON1				0x308
#define MAINPLL_CON2				0x30C
#define MAINPLL_CON3				0x310
#define UNIVPLL_CON0				0x314
#define UNIVPLL_CON1				0x318
#define UNIVPLL_CON2				0x31C
#define UNIVPLL_CON3				0x320
#define MSDCPLL_CON0				0x35C
#define MSDCPLL_CON1				0x360
#define MSDCPLL_CON2				0x364
#define MSDCPLL_CON3				0x368
#define MMPLL_CON0				0x324
#define MMPLL_CON1				0x328
#define MMPLL_CON2				0x32C
#define MMPLL_CON3				0x330
#define UFSPLL_CON0				0x36C
#define UFSPLL_CON1				0x370
#define UFSPLL_CON2				0x374
#define UFSPLL_CON3				0x378
#define APLL1_CON0				0x334
#define APLL1_CON1				0x338
#define APLL1_CON2				0x33C
#define APLL1_CON3				0x340
#define APLL1_CON4				0x344
#define APLL2_CON0				0x348
#define APLL2_CON1				0x34C
#define APLL2_CON2				0x350
#define APLL2_CON3				0x354
#define APLL2_CON4				0x358
#define MFGPLL_CON0				0x008
#define MFGPLL_CON1				0x00C
#define MFGPLL_CON2				0x010
#define MFGPLL_CON3				0x014
#define MFGSCPLL_CON0				0x008
#define MFGSCPLL_CON1				0x00C
#define MFGSCPLL_CON2				0x010
#define MFGSCPLL_CON3				0x014

/* HW Voter REG */
#define HWV_CG_0_SET				0x0000
#define HWV_CG_0_CLR				0x0004
#define HWV_CG_0_DONE				0x1C00
#define HWV_CG_1_SET				0x0008
#define HWV_CG_1_CLR				0x000C
#define HWV_CG_1_DONE				0x1C04
#define HWV_CG_2_SET				0x0010
#define HWV_CG_2_CLR				0x0014
#define HWV_CG_2_DONE				0x1C08
#define HWV_CG_3_SET				0x0018
#define HWV_CG_3_CLR				0x001C
#define HWV_CG_3_DONE				0x1C0C
#define HWV_CG_4_SET				0x0020
#define HWV_CG_4_CLR				0x0024
#define HWV_CG_4_DONE				0x1C10
#define HWV_CG_5_SET				0x0028
#define HWV_CG_5_CLR				0x002C
#define HWV_CG_5_DONE				0x1C14
#define HWV_CG_6_SET				0x0030
#define HWV_CG_6_CLR				0x0034
#define HWV_CG_6_DONE				0x1C18
#define HWV_CG_7_SET				0x0038
#define HWV_CG_7_CLR				0x003C
#define HWV_CG_7_DONE				0x1C1C
#define HWV_CG_8_SET				0x0040
#define HWV_CG_8_CLR				0x0044
#define HWV_CG_8_DONE				0x1C20

static DEFINE_SPINLOCK(mt6878_clk_lock);

static const struct mtk_fixed_factor vlp_divs[] = {
	FACTOR(CLK_VLP_SCP, "vlp_scp_ck",
			"vlp_scp_sel", 1, 1),
	FACTOR(CLK_VLP_PWRAP_ULPOSC, "vlp_pwrap_ulposc_ck",
			"vlp_pwrap_ulposc_sel", 1, 1),
	FACTOR(CLK_VLP_SPMI_P_MST, "vlp_spmi_p_ck",
			"vlp_spmi_p_sel", 1, 1),
	FACTOR(CLK_VLP_SPMI_M_MST, "vlp_spmi_m_ck",
			"vlp_spmi_m_sel", 1, 1),
	FACTOR(CLK_VLP_DVFSRC, "vlp_dvfsrc_ck",
			"vlp_dvfsrc_sel", 1, 1),
	FACTOR(CLK_VLP_PWM_VLP, "vlp_pwm_vlp_ck",
			"vlp_pwm_vlp_sel", 1, 1),
	FACTOR(CLK_VLP_AXI_VLP, "vlp_axi_vlp_ck",
			"vlp_axi_vlp_sel", 1, 1),
	FACTOR(CLK_VLP_DBGAO_26M, "vlp_dbgao_26m_ck",
			"vlp_dbgao_26m_sel", 1, 1),
	FACTOR(CLK_VLP_SYSTIMER_26M, "vlp_systimer_26m_ck",
			"vlp_systimer_26m_sel", 1, 1),
	FACTOR(CLK_VLP_SSPM, "vlp_sspm_ck",
			"vlp_sspm_sel", 1, 1),
	FACTOR(CLK_VLP_SSPM_F26M, "vlp_sspm_f26m_ck",
			"vlp_sspm_f26m_sel", 1, 1),
	FACTOR(CLK_VLP_SRCK, "vlp_srck_ck",
			"vlp_srck_sel", 1, 1),
	FACTOR(CLK_VLP_SCP_SPI, "vlp_scp_spi_ck",
			"vlp_scp_spi_sel", 1, 1),
	FACTOR(CLK_VLP_SCP_IIC, "vlp_scp_iic_ck",
			"vlp_scp_iic_sel", 1, 1),
	FACTOR(CLK_VLP_SCP_SPI_HS, "vlp_scp_spi_hs_ck",
			"vlp_scp_spi_hs_sel", 1, 1),
	FACTOR(CLK_VLP_SCP_IIC_HS, "vlp_scp_iic_hs_ck",
			"vlp_scp_iic_hs_sel", 1, 1),
	FACTOR(CLK_VLP_SSPM_ULPOSC, "vlp_sspm_ulposc_ck",
			"vlp_sspm_ulposc_sel", 1, 1),
	FACTOR(CLK_VLP_TIA_ULPOSC, "vlp_tia_ulposc_ck",
			"tia_ulposc_sel", 1, 1),
	FACTOR(CLK_VLP_APXGPT_26M, "vlp_apxgpt_26m_ck",
			"vlp_apxgpt_26m_sel", 1, 1),
	FACTOR(CLK_VLP_SPM, "vlp_spm_ck",
			"top_mainpll_d7_d4", 1, 1),
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_ARMPLL_BL_CK_VRPOC, "top_armpll_bl_vrpoc",
			"armpll_bl", 1, 1),
	FACTOR(CLK_TOP_ARMPLL_LL_CK_VRPOC, "top_armpll_ll_vrpoc",
			"armpll_ll", 1, 1),
	FACTOR(CLK_TOP_CCIPLL, "top_ccipll_ck",
			"ccipll", 1, 1),
	FACTOR(CLK_TOP_MFGPLL, "top_mfgpll_ck",
			"mfg-ao-mfgpll", 1, 1),
	FACTOR(CLK_TOP_MFGSCPLL, "top_mfgscpll_ck",
			"mfgsc-ao-mfgscpll", 1, 1),
	FACTOR(CLK_TOP_MAINPLL, "top_mainpll_ck",
			"mainpll", 1, 1),
	FACTOR(CLK_TOP_MAINPLL_D3, "top_mainpll_d3",
			"mainpll", 1, 3),
	FACTOR(CLK_TOP_MAINPLL_D4, "top_mainpll_d4",
			"mainpll", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D4_D2, "top_mainpll_d4_d2",
			"mainpll", 1, 8),
	FACTOR(CLK_TOP_MAINPLL_D4_D4, "top_mainpll_d4_d4",
			"mainpll", 1, 16),
	FACTOR(CLK_TOP_MAINPLL_D4_D8, "top_mainpll_d4_d8",
			"mainpll", 43, 1375),
	FACTOR(CLK_TOP_MAINPLL_D4_D16, "top_mainpll_d4_d16",
			"mainpll", 64, 4099),
	FACTOR(CLK_TOP_MAINPLL_D5, "top_mainpll_d5",
			"mainpll", 1, 5),
	FACTOR(CLK_TOP_MAINPLL_D5_D2, "top_mainpll_d5_d2",
			"mainpll", 1, 10),
	FACTOR(CLK_TOP_MAINPLL_D5_D4, "top_mainpll_d5_d4",
			"mainpll", 1, 20),
	FACTOR(CLK_TOP_MAINPLL_D5_D8, "top_mainpll_d5_d8",
			"mainpll", 1, 40),
	FACTOR(CLK_TOP_MAINPLL_D6, "top_mainpll_d6",
			"mainpll", 1, 6),
	FACTOR(CLK_TOP_MAINPLL_D6_D2, "top_mainpll_d6_d2",
			"mainpll", 1, 12),
	FACTOR(CLK_TOP_MAINPLL_D6_D4, "top_mainpll_d6_d4",
			"mainpll", 1, 24),
	FACTOR(CLK_TOP_MAINPLL_D6_D8, "top_mainpll_d6_d8",
			"mainpll", 1, 48),
	FACTOR(CLK_TOP_MAINPLL_D7, "top_mainpll_d7",
			"mainpll", 1, 7),
	FACTOR(CLK_TOP_MAINPLL_D7_D2, "top_mainpll_d7_d2",
			"mainpll", 1, 14),
	FACTOR(CLK_TOP_MAINPLL_D7_D4, "top_mainpll_d7_d4",
			"mainpll", 1, 28),
	FACTOR(CLK_TOP_MAINPLL_D7_D8, "top_mainpll_d7_d8",
			"mainpll", 1, 56),
	FACTOR(CLK_TOP_MAINPLL_D9, "top_mainpll_d9",
			"mainpll", 1, 9),
	FACTOR(CLK_TOP_UNIVPLL, "top_univpll_ck",
			"univpll", 1, 1),
	FACTOR(CLK_TOP_UNIVPLL_D2, "top_univpll_d2",
			"univpll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D3, "top_univpll_d3",
			"univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL_D4, "top_univpll_d4",
			"univpll", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D4_D2, "top_univpll_d4_d2",
			"univpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_D4_D4, "top_univpll_d4_d4",
			"univpll", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_D4_D8, "top_univpll_d4_d8",
			"univpll", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "top_univpll_d5",
			"univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL_D5_D2, "top_univpll_d5_d2",
			"univpll", 1, 10),
	FACTOR(CLK_TOP_UNIVPLL_D5_D4, "top_univpll_d5_d4",
			"univpll", 1, 20),
	FACTOR(CLK_TOP_UNIVPLL_D5_D8, "top_univpll_d5_d8",
			"univpll", 1, 40),
	FACTOR(CLK_TOP_UNIVPLL_D5_D16, "top_univpll_d5_d16",
			"univpll", 1, 80),
	FACTOR(CLK_TOP_UNIVPLL_D6, "top_univpll_d6",
			"univpll", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL_D6_D2, "top_univpll_d6_d2",
			"univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL_D6_D4, "top_univpll_d6_d4",
			"univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL_D6_D8, "top_univpll_d6_d8",
			"univpll", 1, 48),
	FACTOR(CLK_TOP_UNIVPLL_D6_D16, "top_univpll_d6_d16",
			"univpll", 1, 96),
	FACTOR(CLK_TOP_UNIVPLL_D7, "top_univpll_d7",
			"univpll", 1, 7),
	FACTOR(CLK_TOP_UNIVPLL_D7_D2, "top_univpll_d7_d2",
			"univpll", 1, 14),
	FACTOR(CLK_TOP_UNIVPLL_192M, "top_uvpll192m_ck",
			"univpll", 1, 13),
	FACTOR(CLK_TOP_UVPLL192M_D2, "top_uvpll192m_d2",
			"univpll", 1, 26),
	FACTOR(CLK_TOP_UVPLL192M_D4, "top_uvpll192m_d4",
			"univpll", 1, 52),
	FACTOR(CLK_TOP_UVPLL192M_D8, "top_uvpll192m_d8",
			"univpll", 1, 104),
	FACTOR(CLK_TOP_UVPLL192M_D10, "top_uvpll192m_d10",
			"univpll", 1, 130),
	FACTOR(CLK_TOP_UVPLL192M_D16, "top_uvpll192m_d16",
			"univpll", 1, 208),
	FACTOR(CLK_TOP_UVPLL192M_D32, "top_uvpll192m_d32",
			"univpll", 1, 416),
	FACTOR(CLK_TOP_USB20_192M, "top_usb20_192m_ck",
			"univpll", 1, 13),
	FACTOR(CLK_TOP_USB20_PLL_D2, "top_usb20_pll_d2",
			"univpll", 1, 26),
	FACTOR(CLK_TOP_USB20_PLL_D4, "top_usb20_pll_d4",
			"univpll", 1, 52),
	FACTOR(CLK_TOP_APLL1, "top_apll1_ck",
			"apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "top_apll1_d2",
			"apll1", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "top_apll1_d4",
			"apll1", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "top_apll1_d8",
			"apll1", 1, 8),
	FACTOR(CLK_TOP_APLL2, "top_apll2_ck",
			"apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D2, "top_apll2_d2",
			"apll2", 1, 2),
	FACTOR(CLK_TOP_APLL2_D4, "top_apll2_d4",
			"apll2", 1, 4),
	FACTOR(CLK_TOP_APLL2_D8, "top_apll2_d8",
			"apll2", 1, 8),
	FACTOR(CLK_TOP_CLK26M_BYP, "top_clk26m_byp",
			"apll2", 57, 431),
	FACTOR(CLK_TOP_MMPLL, "top_mmpll_ck",
			"mmpll", 1, 1),
	FACTOR(CLK_TOP_MMPLL_D3, "top_mmpll_d3",
			"mmpll", 1, 3),
	FACTOR(CLK_TOP_MMPLL_D4, "top_mmpll_d4",
			"mmpll", 1, 4),
	FACTOR(CLK_TOP_MMPLL_D4_D2, "top_mmpll_d4_d2",
			"mmpll", 1, 8),
	FACTOR(CLK_TOP_MMPLL_D4_D4, "top_mmpll_d4_d4",
			"mmpll", 1, 16),
	FACTOR(CLK_TOP_MMPLL_D5, "top_mmpll_d5",
			"mmpll", 1, 5),
	FACTOR(CLK_TOP_MMPLL_D5_D2, "top_mmpll_d5_d2",
			"mmpll", 1, 10),
	FACTOR(CLK_TOP_MMPLL_D5_D4, "top_mmpll_d5_d4",
			"mmpll", 1, 20),
	FACTOR(CLK_TOP_MMPLL_D6, "top_mmpll_d6",
			"mmpll", 1, 6),
	FACTOR(CLK_TOP_MMPLL_D6_D2, "top_mmpll_d6_d2",
			"mmpll", 1, 12),
	FACTOR(CLK_TOP_MMPLL_D7, "top_mmpll_d7",
			"mmpll", 1, 7),
	FACTOR(CLK_TOP_MMPLL_D9, "top_mmpll_d9",
			"mmpll", 1, 9),
	FACTOR(CLK_TOP_UFSPLL, "top_ufspll_ck",
			"ufspll", 1, 1),
	FACTOR(CLK_TOP_UFSPLL_D2, "top_ufspll_d2",
			"ufspll", 1, 2),
	FACTOR(CLK_TOP_UFSPLL_D4, "top_ufspll_d4",
			"ufspll", 1, 4),
	FACTOR(CLK_TOP_UFSPLL_D8, "top_ufspll_d8",
			"ufspll", 1, 8),
	FACTOR(CLK_TOP_UFSPLL_D16, "top_ufspll_d16",
			"ufspll", 92, 1473),
	FACTOR(CLK_TOP_MSDCPLL, "top_msdcpll_ck",
			"msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "top_msdcpll_d2",
			"msdcpll", 1, 2),
	FACTOR(CLK_TOP_MSDCPLL_D4, "top_msdcpll_d4",
			"msdcpll", 1, 4),
	FACTOR(CLK_TOP_MSDCPLL_D8, "top_msdcpll_d8",
			"msdcpll", 1, 8),
	FACTOR(CLK_TOP_MSDCPLL_D16, "top_msdcpll_d16",
			"msdcpll", 1, 16),
	FACTOR(CLK_TOP_ARMPLL_26M, "top_armpll_26m_ck",
			"None", 1, 1),
	FACTOR(CLK_TOP_CLKRTC, "top_clkrtc",
			"clk32k", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX8, "top_tck_26m_mx8_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX9, "top_tck_26m_mx9_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX10, "top_tck_26m_mx10_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX11, "top_tck_26m_mx11_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX12, "top_tck_26m_mx12_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_CSW_FAXI, "top_csw_faxi_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_F26M_CK_D52, "top_f26m_d52",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_F26M_CK_D2, "top_f26m_d2",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_OSC, "top_osc_ck",
			"ulposc", 1, 1),
	FACTOR(CLK_TOP_OSC_D2, "top_osc_d2",
			"ulposc", 1, 2),
	FACTOR(CLK_TOP_OSC_D3, "top_osc_d3",
			"ulposc", 1, 3),
	FACTOR(CLK_TOP_OSC_D4, "top_osc_d4",
			"ulposc", 1, 4),
	FACTOR(CLK_TOP_OSC_D7, "top_osc_d7",
			"ulposc", 1, 7),
	FACTOR(CLK_TOP_OSC_D8, "top_osc_d8",
			"ulposc", 1, 8),
	FACTOR(CLK_TOP_OSC_D16, "top_osc_d16",
			"ulposc", 61, 973),
	FACTOR(CLK_TOP_OSC_D10, "top_osc_d10",
			"ulposc", 1, 10),
	FACTOR(CLK_TOP_OSC_D20, "top_osc_d20",
			"ulposc", 1, 20),
	FACTOR(CLK_TOP_OSC_CK_2, "top_osc_2",
			"ulposc", 1, 1),
	FACTOR(CLK_TOP_OSC2_D2, "top_osc2_d2",
			"ulposc", 1, 2),
	FACTOR(CLK_TOP_OSC2_D3, "top_osc2_d3",
			"ulposc", 1, 3),
	FACTOR(CLK_TOP_OSC2_D5, "top_osc2_d5",
			"ulposc", 8, 13),
	FACTOR(CLK_TOP_F26M, "top_f26m_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_RTC, "top_rtc_ck",
			"clk32k", 1, 1),
	FACTOR(CLK_TOP_AXI, "top_axi_ck",
			"top_axi_sel", 1, 1),
	FACTOR(CLK_TOP_AXI_P, "top_axip_ck",
			"top_axip_sel", 1, 1),
	FACTOR(CLK_TOP_AXI_UFS, "top_axi_ufs_ck",
			"top_axi_ufs_sel", 1, 1),
	FACTOR(CLK_TOP_BUS, "top_b",
			"top_bus_aximem_sel", 1, 1),
	FACTOR(CLK_TOP_DISP0, "top_disp0_ck",
			"top_disp0_sel", 1, 1),
	FACTOR(CLK_TOP_MMINFRA, "top_mminfra_ck",
			"top_mminfra_sel", 1, 1),
	FACTOR(CLK_TOP_MMUP, "top_mmup_ck",
			"top_mmup_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG, "top_camtg_ck",
			"top_camtg_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG2, "top_camtg2_ck",
			"top_camtg2_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG3, "top_camtg3_ck",
			"top_camtg3_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG4, "top_camtg4_ck",
			"top_camtg4_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG5, "top_camtg5_ck",
			"top_camtg5_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG6, "top_camtg6_ck",
			"top_camtg6_sel", 1, 1),
	FACTOR(CLK_TOP_UART, "top_uart_ck",
			"top_uart_sel", 1, 1),
	FACTOR(CLK_TOP_SPI0, "top_spi0_ck",
			"top_spi0_sel", 1, 1),
	FACTOR(CLK_TOP_SPI1, "top_spi1_ck",
			"top_spi1_sel", 1, 1),
	FACTOR(CLK_TOP_SPI2, "top_spi2_ck",
			"top_spi2_sel", 1, 1),
	FACTOR(CLK_TOP_SPI3, "top_spi3_ck",
			"top_spi3_sel", 1, 1),
	FACTOR(CLK_TOP_SPI4, "top_spi4_ck",
			"top_spi4_sel", 1, 1),
	FACTOR(CLK_TOP_SPI5, "top_spi5_ck",
			"top_spi5_sel", 1, 1),
	FACTOR(CLK_TOP_SPI6, "top_spi6_ck",
			"top_spi6_sel", 1, 1),
	FACTOR(CLK_TOP_SPI7, "top_spi7_ck",
			"top_spi7_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC_0P, "top_msdc_0p_ck",
			"top_msdc_0p_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC50_0_HCLK, "top_msdc5hclk_ck",
			"top_msdc5hclk_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC50_0, "top_msdc50_0_ck",
			"top_msdc50_0_sel", 1, 1),
	FACTOR(CLK_TOP_AES_MSDCFDE, "top_aes_msdcfde_ck",
			"top_aes_msdcfde_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC_1P, "top_msdc_1p_ck",
			"top_msdc_1p_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1, "top_msdc30_1_ck",
			"top_msdc30_1_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1_HCLK, "top_msdc30_1_h_ck",
			"top_msdc30_1_h_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_INTBUS, "top_aud_intbus_ck",
			"top_aud_intbus_sel", 1, 1),
	FACTOR(CLK_TOP_ATB, "top_atb_ck",
			"top_atb_sel", 1, 1),
	FACTOR(CLK_TOP_DISP_PWM, "top_disp_pwm_ck",
			"disp_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_USB_TOP, "top_usb_ck",
			"top_usb_sel", 1, 1),
	FACTOR(CLK_TOP_USB_XHCI, "top_usb_xhci_ck",
			"top_usb_xhci_sel", 1, 1),
	FACTOR(CLK_TOP_I2C, "top_i2c_ck",
			"top_i2c_sel", 1, 1),
	FACTOR(CLK_TOP_SENINF, "top_seninf_ck",
			"top_seninf_sel", 1, 1),
	FACTOR(CLK_TOP_SENINF1, "top_seninf1_ck",
			"top_seninf1_sel", 1, 1),
	FACTOR(CLK_TOP_SENINF2, "top_seninf2_ck",
			"top_seninf2_sel", 1, 1),
	FACTOR(CLK_TOP_SENINF3, "top_seninf3_ck",
			"top_seninf3_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN1, "top_aud_engen1_ck",
			"top_aud_engen1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN2, "top_aud_engen2_ck",
			"top_aud_engen2_sel", 1, 1),
	FACTOR(CLK_TOP_AES_UFSFDE, "top_aes_ufsfde_ck",
			"top_aes_ufsfde_sel", 1, 1),
	FACTOR(CLK_TOP_UFS, "top_ufs_ck",
			"top_ufs_sel", 1, 1),
	FACTOR(CLK_TOP_UFS_MBIST, "top_ufs_mbist_ck",
			"top_ufs_mbist_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_1, "top_aud_1_ck",
			"top_aud_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_2, "top_aud_2_ck",
			"top_aud_2_sel", 1, 1),
	FACTOR(CLK_TOP_DPMAIF_MAIN, "top_dpmaif_main_ck",
			"top_dpmaif_main_sel", 1, 1),
	FACTOR(CLK_TOP_VENC, "top_venc_ck",
			"top_venc_sel", 1, 1),
	FACTOR(CLK_TOP_VDEC, "top_vdec_ck",
			"top_vdec_sel", 1, 1),
	FACTOR(CLK_TOP_PWM, "top_pwm_ck",
			"top_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_AUDIO_H, "top_audio_h_ck",
			"top_audio_h_sel", 1, 1),
	FACTOR(CLK_TOP_MCUPM, "top_mcupm_ck",
			"top_mcupm_sel", 1, 1),
	FACTOR(CLK_TOP_MEM_SUB, "top_mem_sub_ck",
			"top_mem_sub_sel", 1, 1),
	FACTOR(CLK_TOP_MEM_SUB_P, "top_mem_subp_ck",
			"top_mem_subp_sel", 1, 1),
	FACTOR(CLK_TOP_MEM_SUB_UFS, "top_mem_sub_ufs_ck",
			"top_mem_sub_ufs_sel", 1, 1),
	FACTOR(CLK_TOP_EMI_N, "top_emi_n_ck",
			"top_emi_n_sel", 1, 1),
	FACTOR(CLK_TOP_DSI_OCC, "top_dsi_occ_ck",
			"top_dsi_occ_sel", 1, 1),
	FACTOR(CLK_TOP_AP2CONN_HOST, "top_ap2conn_host_ck",
			"top_ap2conn_host_sel", 1, 1),
	FACTOR(CLK_TOP_IMG1, "top_img1_ck",
			"top_img1_sel", 1, 1),
	FACTOR(CLK_TOP_IPE, "top_ipe_ck",
			"top_ipe_sel", 1, 1),
	FACTOR(CLK_TOP_CAM, "top_cam_ck",
			"top_cam_sel", 1, 1),
	FACTOR(CLK_TOP_CCUSYS, "top_ccusys_ck",
			"top_ccusys_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTM, "top_camtm_ck",
			"top_camtm_sel", 1, 1),
	FACTOR(CLK_TOP_CCU_AHB, "top_ccu_ahb_ck",
			"top_ccu_ahb_sel", 1, 1),
	FACTOR(CLK_TOP_CCUTM, "top_ccutm_ck",
			"top_ccutm_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC_1P_RX, "top_msdc_1p_rx_ck",
			"top_msdc_1p_rx_sel", 1, 1),
	FACTOR(CLK_TOP_DSP, "top_dsp_ck",
			"top_dsp_sel", 1, 1),
	FACTOR(CLK_TOP_EMI_INF_546, "top_emi_inf_546_ck",
			"top_md_emi_sel", 1, 1),
	FACTOR(CLK_TOP_SR_PKA, "top_sr_pka_ck",
			"sr_pka_sel", 1, 1),
	FACTOR(CLK_TOP_SR_DMA, "top_sr_dma_ck",
			"sr_dma_sel", 1, 1),
	FACTOR(CLK_TOP_SR_KDF, "top_sr_kdf_ck",
			"sr_kdf_sel", 1, 1),
	FACTOR(CLK_TOP_SR_RNG, "top_sr_rng_ck",
			"sr_rng_sel", 1, 1),
	FACTOR(CLK_TOP_DXCC, "top_dxcc_ck",
			"dxcc_sel", 1, 1),
	FACTOR(CLK_TOP_MFG_REF, "top_mfg_ref_ck",
			"top_mfg_ref_sel", 1, 1),
	FACTOR(CLK_TOP_MFG_INT0, "top_mfg_int0_ck",
			"top_mfg_int0_sel", 1, 1),
	FACTOR(CLK_TOP_MFG1_INT1, "top_mfg1_int1_ck",
			"top_mfg1_int1_sel", 1, 1),
	FACTOR(CLK_TOP_MFGSC_REF, "top_mfgsc_ref_ck",
			"top_mfgsc_ref_sel", 1, 1),
	FACTOR(CLK_TOP_ULPOSC, "top_ulposc_ck",
			"top_osc_ck", 1, 1),
	FACTOR(CLK_TOP_ULPOSC_CORE, "top_ulposc_core_ck",
			"top_osc_2", 1, 1),
};

static const char * const vlp_scp_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4",
	"top_univpll_d3",
	"top_mainpll_d3",
	"top_univpll_d6",
	"top_apll1_ck",
	"top_mainpll_d4",
	"top_mainpll_d7",
	"top_osc_d10"
};

static const char * const vlp_pwrap_ulposc_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10",
	"top_osc_d7",
	"top_osc_d8",
	"top_osc_d16",
	"top_mainpll_d7_d8"
};

static const char * const vlp_spmi_p_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_f26m_d2",
	"top_osc_d8",
	"top_osc_d10",
	"top_osc_d16",
	"top_osc_d7",
	"top_clkrtc",
	"top_mainpll_d7_d8",
	"top_mainpll_d6_d8",
	"top_mainpll_d5_d8"
};

static const char * const vlp_spmi_m_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_f26m_d2",
	"top_osc_d8",
	"top_osc_d10",
	"top_osc_d16",
	"top_osc_d7",
	"top_clkrtc",
	"top_mainpll_d7_d8",
	"top_mainpll_d6_d8",
	"top_mainpll_d5_d8"
};

static const char * const vlp_dvfsrc_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const char * const vlp_pwm_vlp_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d4",
	"top_clkrtc",
	"top_osc_d10",
	"top_mainpll_d4_d8"
};

static const char * const vlp_axi_vlp_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10",
	"top_osc_d2",
	"top_mainpll_d7_d4",
	"top_mainpll_d7_d2"
};

static const char * const vlp_dbgao_26m_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const char * const vlp_systimer_26m_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const char * const vlp_sspm_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10",
	"top_mainpll_d5_d2",
	"top_osc_ck",
	"top_mainpll_d6"
};

static const char * const vlp_sspm_f26m_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const char * const vlp_srck_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const char * const vlp_scp_spi_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d5_d4",
	"top_mainpll_d7_d2",
	"top_osc_d10"
};

static const char * const vlp_scp_iic_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d5_d4",
	"top_mainpll_d7_d2",
	"top_osc_d10"
};

static const char * const vlp_scp_spi_hs_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d5_d4",
	"top_mainpll_d7_d2",
	"top_osc_d10"
};

static const char * const vlp_scp_iic_hs_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d5_d4",
	"top_mainpll_d7_d2",
	"top_osc_d10"
};

static const char * const vlp_sspm_ulposc_parents[] = {
	"top_osc_ck",
	"top_univpll_d5_d2"
};

static const char * const vlp_apxgpt_26m_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d10"
};

static const struct mtk_mux vlp_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* VLP_CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_VLP_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_DBGAO_26M_SEL/* dts */, "vlp_dbgao_26m_sel",
		vlp_dbgao_26m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DBGAO_26M_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SSPM_F26M_SEL/* dts */, "vlp_sspm_f26m_sel",
		vlp_sspm_f26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_F26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SCP_SPI_HS_SEL/* dts */, "vlp_scp_spi_hs_sel",
		vlp_scp_spi_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SCP_IIC_HS_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_VLP_SSPM_ULPOSC_SEL/* dts */, "vlp_sspm_ulposc_sel",
		vlp_sspm_ulposc_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_APXGPT_26M_SEL/* dts */, "vlp_apxgpt_26m_sel",
		vlp_apxgpt_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APXGPT_26M_SHIFT/* upd shift */),
#else
	/* VLP_CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD(CLK_VLP_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_DBGAO_26M_SEL/* dts */, "vlp_dbgao_26m_sel",
		vlp_dbgao_26m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DBGAO_26M_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SSPM_F26M_SEL/* dts */, "vlp_sspm_f26m_sel",
		vlp_sspm_f26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_F26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SCP_SPI_HS_SEL/* dts */, "vlp_scp_spi_hs_sel",
		vlp_scp_spi_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_SCP_IIC_HS_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_VLP_SSPM_ULPOSC_SEL/* dts */, "vlp_sspm_ulposc_sel",
		vlp_sspm_ulposc_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_APXGPT_26M_SEL/* dts */, "vlp_apxgpt_26m_sel",
		vlp_apxgpt_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APXGPT_26M_SHIFT/* upd shift */),
#endif
};

static const char * const top_axi_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d4",
	"top_mainpll_d7_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d5_d2",
	"top_mainpll_d6_d2",
	"top_osc_d4"
};

static const char * const top_axip_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d4",
	"top_mainpll_d7_d2",
	"top_osc_d4"
};

static const char * const top_axi_ufs_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d8",
	"top_mainpll_d7_d4",
	"top_osc_d8"
};

static const char * const top_bus_aximem_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d7_d2",
	"top_mainpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d6"
};

static const char * const top_disp0_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d6",
	"top_univpll_d6",
	"top_mmpll_d6",
	"top_univpll_d4"
};

static const char * const top_mminfra_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_mainpll_d5_d2",
	"top_mmpll_d6_d2",
	"top_mainpll_d4_d2",
	"top_mmpll_d4_d2",
	"top_mainpll_d6",
	"top_mmpll_d7",
	"top_univpll_d6",
	"top_mainpll_d5",
	"top_mmpll_d6",
	"top_univpll_d5",
	"top_mainpll_d4",
	"top_univpll_d4",
	"top_mmpll_d4",
	"top_mmpll_d5_d2"
};

static const char * const top_mmup_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d5_d2",
	"top_mmpll_d4_d2",
	"top_mainpll_d4",
	"top_univpll_d4",
	"top_mmpll_d4",
	"top_mainpll_d3"
};

static const char * const top_camtg_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_uvpll192m_d8",
	"top_univpll_d6_d8",
	"top_uvpll192m_d4",
	"top_osc_d16",
	"top_osc_d20",
	"top_osc_d10",
	"top_univpll_d6_d16",
	"top_ufspll_d16",
	"top_f26m_d2",
	"top_uvpll192m_d10",
	"top_uvpll192m_d16",
	"top_uvpll192m_d32"
};

static const char * const top_camtg2_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_uvpll192m_d8",
	"top_univpll_d6_d8",
	"top_uvpll192m_d4",
	"top_osc_d16",
	"top_osc_d20",
	"top_osc_d10",
	"top_univpll_d6_d16",
	"top_ufspll_d16",
	"top_f26m_d2",
	"top_uvpll192m_d10",
	"top_uvpll192m_d16",
	"top_uvpll192m_d32"
};

static const char * const top_camtg3_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_uvpll192m_d8",
	"top_univpll_d6_d8",
	"top_uvpll192m_d4",
	"top_osc_d16",
	"top_osc_d20",
	"top_osc_d10",
	"top_univpll_d6_d16",
	"top_ufspll_d16",
	"top_f26m_d2",
	"top_uvpll192m_d10",
	"top_uvpll192m_d16",
	"top_uvpll192m_d32"
};

static const char * const top_camtg4_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_uvpll192m_d8",
	"top_univpll_d6_d8",
	"top_uvpll192m_d4",
	"top_osc_d16",
	"top_osc_d20",
	"top_osc_d10",
	"top_univpll_d6_d16",
	"top_ufspll_d16",
	"top_f26m_d2",
	"top_uvpll192m_d10",
	"top_uvpll192m_d16",
	"top_uvpll192m_d32"
};

static const char * const top_camtg5_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_uvpll192m_d8",
	"top_univpll_d6_d8",
	"top_uvpll192m_d4",
	"top_osc_d16",
	"top_osc_d20",
	"top_osc_d10",
	"top_univpll_d6_d16",
	"top_ufspll_d16",
	"top_f26m_d2",
	"top_uvpll192m_d10",
	"top_uvpll192m_d16",
	"top_uvpll192m_d32"
};

static const char * const top_camtg6_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_uvpll192m_d8",
	"top_univpll_d6_d8",
	"top_uvpll192m_d4",
	"top_osc_d16",
	"top_osc_d20",
	"top_osc_d10",
	"top_univpll_d6_d16",
	"top_ufspll_d16",
	"top_f26m_d2",
	"top_uvpll192m_d10",
	"top_uvpll192m_d16",
	"top_uvpll192m_d32"
};

static const char * const top_uart_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d8"
};

static const char * const top_spi0_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_uvpll192m_ck",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_uvpll192m_ck",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi2_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_uvpll192m_ck",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi3_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_uvpll192m_ck",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi4_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_uvpll192m_ck",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi5_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_uvpll192m_ck",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi6_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_uvpll192m_ck",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_spi7_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_uvpll192m_ck",
	"top_mainpll_d6_d2",
	"top_univpll_d4_d4",
	"top_mainpll_d4_d4",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_msdc_0p_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_msdcpll_ck",
	"top_univpll_d4_d2"
};

static const char * const top_msdc5hclk_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d2",
	"top_mainpll_d6_d2"
};

static const char * const top_msdc50_0_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_msdcpll_ck",
	"top_msdcpll_d2",
	"top_mainpll_d6_d2",
	"top_mainpll_d6",
	"top_univpll_d4_d4"
};

static const char * const top_aes_msdcfde_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d2",
	"top_mainpll_d6",
	"top_mainpll_d4_d4",
	"top_msdcpll_ck"
};

static const char * const top_msdc_1p_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_msdcpll_ck",
	"top_univpll_d4_d2"
};

static const char * const top_msdc30_1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_mainpll_d6_d2",
	"top_mainpll_d7_d2",
	"top_msdcpll_d2"
};

static const char * const top_msdc30_1_h_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d4",
	"top_mainpll_d6_d4"
};

static const char * const top_aud_intbus_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d4",
	"top_mainpll_d7_d4"
};

static const char * const top_atb_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d2",
	"top_mainpll_d5_d2"
};

static const char * const top_usb_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_usb_xhci_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d5_d4",
	"top_univpll_d6_d4"
};

static const char * const top_i2c_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d8",
	"top_univpll_d5_d4",
	"top_mainpll_d4_d4"
};

static const char * const top_seninf_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_osc_ck",
	"top_univpll_d4_d2",
	"top_mmpll_d4_d2",
	"top_mmpll_d7",
	"top_univpll_d6",
	"top_univpll_d5"
};

static const char * const top_seninf1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_osc_ck",
	"top_univpll_d4_d2",
	"top_mmpll_d4_d2",
	"top_mmpll_d7",
	"top_univpll_d6",
	"top_univpll_d5"
};

static const char * const top_seninf2_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_osc_ck",
	"top_univpll_d4_d2",
	"top_mmpll_d4_d2",
	"top_mmpll_d7",
	"top_univpll_d6",
	"top_univpll_d5"
};

static const char * const top_seninf3_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_osc_ck",
	"top_univpll_d4_d2",
	"top_mmpll_d4_d2",
	"top_mmpll_d7",
	"top_univpll_d6",
	"top_univpll_d5"
};

static const char * const top_aud_engen1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_apll1_d2",
	"top_apll1_d4",
	"top_apll1_d8"
};

static const char * const top_aud_engen2_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_apll2_d2",
	"top_apll2_d4",
	"top_apll2_d8"
};

static const char * const top_aes_ufsfde_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4",
	"top_mainpll_d4_d2",
	"top_mainpll_d6",
	"top_mainpll_d4_d4",
	"top_univpll_d4_d2",
	"top_univpll_d6"
};

static const char * const top_ufs_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d8",
	"top_mainpll_d4_d4",
	"top_mainpll_d5_d2",
	"top_mainpll_d6_d2",
	"top_univpll_d6_d2",
	"top_msdcpll_d2"
};

static const char * const top_ufs_mbist_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4_d2",
	"top_univpll_d4_d2",
	"top_ufspll_d2"
};

static const char * const top_aud_1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_apll1_ck"
};

static const char * const top_aud_2_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_apll2_ck"
};

static const char * const top_dpmaif_main_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6",
	"top_mainpll_d5",
	"top_mainpll_d6",
	"top_mainpll_d4_d2",
	"top_univpll_d4_d2"
};

static const char * const top_venc_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mmpll_d4_d2",
	"top_mainpll_d6",
	"top_univpll_d4_d2",
	"top_mainpll_d4_d2",
	"top_univpll_d6",
	"top_mmpll_d6",
	"top_mainpll_d5_d2",
	"top_mainpll_d6_d2",
	"top_mmpll_d9",
	"top_mmpll_d4",
	"top_mainpll_d4",
	"top_univpll_d4",
	"top_univpll_d5",
	"top_univpll_d5_d2",
	"top_mainpll_d5"
};

static const char * const top_vdec_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_uvpll192m_d2",
	"top_univpll_d5_d4",
	"top_mainpll_d5",
	"top_mainpll_d5_d2",
	"top_mmpll_d6_d2",
	"top_univpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_univpll_d4_d2",
	"top_univpll_d7",
	"top_mmpll_d7",
	"top_mmpll_d6",
	"top_univpll_d6",
	"top_mainpll_d4",
	"top_mmpll_d4_d2",
	"top_mmpll_d5_d2"
};

static const char * const top_pwm_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4_d8"
};

static const char * const top_audio_h_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d7_d2",
	"top_apll1_ck",
	"top_apll2_ck"
};

static const char * const top_mcupm_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_mainpll_d5_d2",
	"top_mainpll_d6_d2"
};

static const char * const top_mem_sub_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4_d4",
	"top_mainpll_d6_d2",
	"top_mainpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d6",
	"top_mmpll_d7",
	"top_mainpll_d5",
	"top_univpll_d5",
	"top_mainpll_d4",
	"top_univpll_d4"
};

static const char * const top_mem_subp_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4_d4",
	"top_mainpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d6",
	"top_mainpll_d5",
	"top_univpll_d5",
	"top_mainpll_d4"
};

static const char * const top_mem_sub_ufs_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4_d4",
	"top_mainpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d6",
	"top_mainpll_d5",
	"top_univpll_d5",
	"top_mainpll_d4"
};

static const char * const top_emi_n_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_ck",
	"top_univpll_d5",
	"top_mainpll_d3"
};

static const char * const top_dsi_occ_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_univpll_d5_d2",
	"top_univpll_d4_d2"
};

static const char * const top_ap2conn_host_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d7_d4"
};

static const char * const top_img1_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4",
	"top_mmpll_d5",
	"top_mmpll_d6",
	"top_univpll_d6",
	"top_mmpll_d7",
	"top_mmpll_d4_d2",
	"top_univpll_d4_d2",
	"top_mainpll_d4_d2",
	"top_mmpll_d6_d2",
	"top_mmpll_d5_d2"
};

static const char * const top_ipe_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d4",
	"top_mmpll_d5",
	"top_mmpll_d6",
	"top_univpll_d6",
	"top_mainpll_d6",
	"top_mmpll_d4_d2",
	"top_univpll_d4_d2",
	"top_mainpll_d4_d2",
	"top_mmpll_d6_d2",
	"top_mmpll_d5_d2"
};

static const char * const top_cam_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4",
	"top_mmpll_d4",
	"top_univpll_d4",
	"top_univpll_d5",
	"top_mmpll_d7",
	"top_mmpll_d6",
	"top_univpll_d6",
	"top_univpll_d4_d2",
	"top_mmpll_d9",
	"top_mmpll_d5_d2",
	"top_osc_d2"
};

static const char * const top_ccusys_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_univpll_d7_d2",
	"top_mmpll_d5_d2",
	"top_univpll_d4_d2",
	"top_univpll_d6",
	"top_mmpll_d6",
	"top_mainpll_d4",
	"top_univpll_d4",
	"top_mmpll_d4",
	"top_mainpll_d3",
	"top_univpll_d3"
};

static const char * const top_camtm_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_univpll_d6_d2",
	"top_univpll_d6_d4"
};

static const char * const top_ccu_ahb_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_osc_d2",
	"top_mmpll_d5_d2"
};

static const char * const top_ccutm_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d4",
	"top_osc_d2",
	"top_univpll_d7_d2",
	"top_univpll_d6_d2"
};

static const char * const top_msdc_1p_rx_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6_d2",
	"top_mainpll_d6_d2",
	"top_mainpll_d7_d2",
	"top_msdcpll_d2"
};

static const char * const top_dsp_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d7_d2",
	"top_univpll_d6_d2",
	"top_univpll_d5_d2",
	"top_mainpll_d4_d2",
	"top_mainpll_d7",
	"top_mainpll_d6",
	"top_univpll_d5"
};

static const char * const top_md_emi_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_mainpll_d4"
};

static const char * const top_mfg_ref_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6",
	"top_mainpll_d5_d2"
};

static const char * const top_mfgsc_ref_parents[] = {
	"top_tck_26m_mx9_ck",
	"top_univpll_d6",
	"top_mainpll_d5_d2"
};

static const char * const top_mfg_int0_parents[] = {
	"top_mfg_ref_ck",
	"top_mfgpll_ck"
};

static const char * const top_mfg1_int1_parents[] = {
	"top_mfgsc_ref_ck",
	"top_mfgscpll_ck"
};

static const char * const top_apll_SI0_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_SI1_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_SI2_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_SI3_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_SI4_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_SI6_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_SO0_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_SO1_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_SO2_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_SO3_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_SO4_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_SO6_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_fmi2s_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const char * const top_apll_td_m_parents[] = {
	"top_aud_1_sel",
	"top_aud_2_sel"
};

static const struct mtk_mux top_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_TOP_AXI_SEL/* dts */, "top_axi_sel",
		top_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AXIP_SEL/* dts */, "top_axip_sel",
		top_axip_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_PERI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AXI_UFS_SEL/* dts */, "top_axi_ufs_sel",
		top_axi_ufs_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "top_bus_aximem_sel",
		top_bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_TOP_DISP0_SEL/* dts */, "top_disp0_sel",
		top_disp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MMINFRA_SEL/* dts */, "top_mminfra_sel",
		top_mminfra_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MMUP_SEL/* dts */, "top_mmup_sel",
		top_mmup_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMUP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG_SEL/* dts */, "top_camtg_sel",
		top_camtg_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG_SHIFT/* upd shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL/* dts */, "top_camtg2_sel",
		top_camtg2_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL/* dts */, "top_camtg3_sel",
		top_camtg3_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG4_SEL/* dts */, "top_camtg4_sel",
		top_camtg4_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG4_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG5_SEL/* dts */, "top_camtg5_sel",
		top_camtg5_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG5_SHIFT/* upd shift */),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG6_SEL/* dts */, "top_camtg6_sel",
		top_camtg6_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG6_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_UART_SEL/* dts */, "top_uart_sel",
		top_uart_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UART_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI0_SEL/* dts */, "top_spi0_sel",
		top_spi0_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI1_SEL/* dts */, "top_spi1_sel",
		top_spi1_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI1_SHIFT/* upd shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_TOP_SPI2_SEL/* dts */, "top_spi2_sel",
		top_spi2_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI3_SEL/* dts */, "top_spi3_sel",
		top_spi3_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI4_SEL/* dts */, "top_spi4_sel",
		top_spi4_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI4_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI5_SEL/* dts */, "top_spi5_sel",
		top_spi5_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI5_SHIFT/* upd shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_TOP_SPI6_SEL/* dts */, "top_spi6_sel",
		top_spi6_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI6_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI7_SEL/* dts */, "top_spi7_sel",
		top_spi7_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI7_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_0P_SEL/* dts */, "top_msdc_0p_sel",
		top_msdc_0p_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC_MACRO_0P_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL/* dts */, "top_msdc5hclk_sel",
		top_msdc5hclk_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC50_0_HCLK_SHIFT/* upd shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL/* dts */, "top_msdc50_0_sel",
		top_msdc50_0_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC50_0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AES_MSDCFDE_SEL/* dts */, "top_aes_msdcfde_sel",
		top_aes_msdcfde_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AES_MSDCFDE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_1P_SEL/* dts */, "top_msdc_1p_sel",
		top_msdc_1p_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL/* dts */, "top_msdc30_1_sel",
		top_msdc30_1_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_1_SHIFT/* upd shift */),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_1_HCLK_SEL/* dts */, "top_msdc30_1_h_sel",
		top_msdc30_1_h_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_1_HCLK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL/* dts */, "top_aud_intbus_sel",
		top_aud_intbus_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_ATB_SEL/* dts */, "top_atb_sel",
		top_atb_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL/* dts */, "top_usb_sel",
		top_usb_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_TOP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_XHCI_SEL/* dts */, "top_usb_xhci_sel",
		top_usb_xhci_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_I2C_SEL/* dts */, "top_i2c_sel",
		top_i2c_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF_SEL/* dts */, "top_seninf_sel",
		top_seninf_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF_SHIFT/* upd shift */),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD(CLK_TOP_SENINF1_SEL/* dts */, "top_seninf1_sel",
		top_seninf1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF2_SEL/* dts */, "top_seninf2_sel",
		top_seninf2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF3_SEL/* dts */, "top_seninf3_sel",
		top_seninf3_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "top_aud_engen1_sel",
		top_aud_engen1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "top_aud_engen2_sel",
		top_aud_engen2_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AES_UFSFDE_SEL/* dts */, "top_aes_ufsfde_sel",
		top_aes_ufsfde_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_UFS_SEL/* dts */, "top_ufs_sel",
		top_ufs_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_UFS_MBIST_SEL/* dts */, "top_ufs_mbist_sel",
		top_ufs_mbist_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_MBIST_SHIFT/* upd shift */),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD(CLK_TOP_AUD_1_SEL/* dts */, "top_aud_1_sel",
		top_aud_1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_2_SEL/* dts */, "top_aud_2_sel",
		top_aud_2_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DPMAIF_MAIN_SEL/* dts */, "top_dpmaif_main_sel",
		top_dpmaif_main_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_VENC_SEL/* dts */, "top_venc_sel",
		top_venc_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_VENC_SHIFT/* upd shift */),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD(CLK_TOP_VDEC_SEL/* dts */, "top_vdec_sel",
		top_vdec_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_VDEC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_PWM_SEL/* dts */, "top_pwm_sel",
		top_pwm_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PWM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUDIO_H_SEL/* dts */, "top_audio_h_sel",
		top_audio_h_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUDIO_H_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCUPM_SEL/* dts */, "top_mcupm_sel",
		top_mcupm_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_SEL/* dts */, "top_mem_sub_sel",
		top_mem_sub_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUBP_SEL/* dts */, "top_mem_subp_sel",
		top_mem_subp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_PERI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_UFS_SEL/* dts */, "top_mem_sub_ufs_sel",
		top_mem_sub_ufs_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_N_SEL/* dts */, "top_emi_n_sel",
		top_emi_n_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD(CLK_TOP_DSI_OCC_SEL/* dts */, "top_dsi_occ_sel",
		top_dsi_occ_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DSI_OCC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "top_ap2conn_host_sel",
		top_ap2conn_host_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_IMG1_SEL/* dts */, "top_img1_sel",
		top_img1_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IMG1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_IPE_SEL/* dts */, "top_ipe_sel",
		top_ipe_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPE_SHIFT/* upd shift */),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD(CLK_TOP_CAM_SEL/* dts */, "top_cam_sel",
		top_cam_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_CAM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CCUSYS_SEL/* dts */, "top_ccusys_sel",
		top_ccusys_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_CCUSYS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTM_SEL/* dts */, "top_camtm_sel",
		top_camtm_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CAMTM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CCU_AHB_SEL/* dts */, "top_ccu_ahb_sel",
		top_ccu_ahb_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CCU_AHB_SHIFT/* upd shift */),
	/* CLK_CFG_16 */
	MUX_CLR_SET_UPD(CLK_TOP_CCUTM_SEL/* dts */, "top_ccutm_sel",
		top_ccutm_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CCUTM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_1P_RX_SEL/* dts */, "top_msdc_1p_rx_sel",
		top_msdc_1p_rx_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MSDC_1P_RX_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSP_SEL/* dts */, "top_dsp_sel",
		top_dsp_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_INF_546_SEL/* dts */, "top_md_emi_sel",
		top_md_emi_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */),
	/* CLK_CFG_18 */
	MUX_CLR_SET_UPD(CLK_TOP_MFG_REF_SEL/* dts */, "top_mfg_ref_sel",
		top_mfg_ref_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MFGSC_REF_SEL/* dts */, "top_mfgsc_ref_sel",
		top_mfgsc_ref_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MFGSC_REF_SHIFT/* upd shift */),
	/* CLK_CFG_20 */
	MUX_CLR_SET(CLK_TOP_MFG_INT0_SEL/* dts */, "top_mfg_int0_sel",
		top_mfg_int0_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 16/* lsb */, 1/* width */),
	MUX_CLR_SET(CLK_TOP_MFG1_INT1_SEL/* dts */, "top_mfg1_int1_sel",
		top_mfg1_int1_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 17/* lsb */, 1/* width */),
#else
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_TOP_AXI_SEL/* dts */, "top_axi_sel",
		top_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AXIP_SEL/* dts */, "top_axip_sel",
		top_axip_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_PERI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AXI_UFS_SEL/* dts */, "top_axi_ufs_sel",
		top_axi_ufs_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "top_bus_aximem_sel",
		top_bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */),
	/* CLK_CFG_1 */
	MUX_HWV(CLK_TOP_DISP0_SEL/* dts */, "top_disp0_sel", top_disp0_parents/* parent */,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR/* set parent */,
		HWV_CG_2_DONE, HWV_CG_2_SET, HWV_CG_2_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP0_SHIFT/* upd shift */),
	MUX_HWV(CLK_TOP_MMINFRA_SEL/* dts */, "top_mminfra_sel", top_mminfra_parents/* parent */,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR/* set parent */,
		HWV_CG_2_DONE, HWV_CG_2_SET, HWV_CG_2_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MMUP_SEL/* dts */, "top_mmup_sel",
		top_mmup_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMUP_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG_SEL/* dts */, "top_camtg_sel",
		top_camtg_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG_SHIFT/* upd shift */),
	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL/* dts */, "top_camtg2_sel",
		top_camtg2_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL/* dts */, "top_camtg3_sel",
		top_camtg3_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG3_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG4_SEL/* dts */, "top_camtg4_sel",
		top_camtg4_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG4_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG5_SEL/* dts */, "top_camtg5_sel",
		top_camtg5_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG5_SHIFT/* upd shift */),
	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG6_SEL/* dts */, "top_camtg6_sel",
		top_camtg6_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG6_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL/* dts */, "top_uart_sel",
		top_uart_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_UART_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI0_SEL/* dts */, "top_spi0_sel",
		top_spi0_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI0_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI1_SEL/* dts */, "top_spi1_sel",
		top_spi1_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI1_SHIFT/* upd shift */),
	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI2_SEL/* dts */, "top_spi2_sel",
		top_spi2_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI3_SEL/* dts */, "top_spi3_sel",
		top_spi3_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI3_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI4_SEL/* dts */, "top_spi4_sel",
		top_spi4_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI4_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI5_SEL/* dts */, "top_spi5_sel",
		top_spi5_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI5_SHIFT/* upd shift */),
	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI6_SEL/* dts */, "top_spi6_sel",
		top_spi6_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI6_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI7_SEL/* dts */, "top_spi7_sel",
		top_spi7_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI7_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_0P_SEL/* dts */, "top_msdc_0p_sel",
		top_msdc_0p_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC_MACRO_0P_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL/* dts */, "top_msdc5hclk_sel",
		top_msdc5hclk_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC50_0_HCLK_SHIFT/* upd shift */),
	/* CLK_CFG_6 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL/* dts */, "top_msdc50_0_sel",
		top_msdc50_0_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC50_0_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_MSDCFDE_SEL/* dts */, "top_aes_msdcfde_sel",
		top_aes_msdcfde_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AES_MSDCFDE_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_1P_SEL/* dts */, "top_msdc_1p_sel",
		top_msdc_1p_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL/* dts */, "top_msdc30_1_sel",
		top_msdc30_1_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */),
	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_HCLK_SEL/* dts */, "top_msdc30_1_h_sel",
		top_msdc30_1_h_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_1_HCLK_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL/* dts */, "top_aud_intbus_sel",
		top_aud_intbus_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_ATB_SEL/* dts */, "top_atb_sel",
		top_atb_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */),
	/* CLK_CFG_8 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL/* dts */, "top_usb_sel",
		top_usb_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_XHCI_SEL/* dts */, "top_usb_xhci_sel",
		top_usb_xhci_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_I2C_SEL/* dts */, "top_i2c_sel",
		top_i2c_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF_SEL/* dts */, "top_seninf_sel",
		top_seninf_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF_SHIFT/* upd shift */),
	/* CLK_CFG_9 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF1_SEL/* dts */, "top_seninf1_sel",
		top_seninf1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF2_SEL/* dts */, "top_seninf2_sel",
		top_seninf2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF3_SEL/* dts */, "top_seninf3_sel",
		top_seninf3_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF3_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "top_aud_engen1_sel",
		top_aud_engen1_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */),
	/* CLK_CFG_10 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "top_aud_engen2_sel",
		top_aud_engen2_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_UFSFDE_SEL/* dts */, "top_aes_ufsfde_sel",
		top_aes_ufsfde_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UFS_SEL/* dts */, "top_ufs_sel",
		top_ufs_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UFS_MBIST_SEL/* dts */, "top_ufs_mbist_sel",
		top_ufs_mbist_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_MBIST_SHIFT/* upd shift */),
	/* CLK_CFG_11 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL/* dts */, "top_aud_1_sel",
		top_aud_1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_2_SEL/* dts */, "top_aud_2_sel",
		top_aud_2_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DPMAIF_MAIN_SEL/* dts */, "top_dpmaif_main_sel",
		top_dpmaif_main_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */),
	MUX_HWV(CLK_TOP_VENC_SEL/* dts */, "top_venc_sel", top_venc_parents/* parent */,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR/* set parent */,
		HWV_CG_3_DONE, HWV_CG_3_SET, HWV_CG_3_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VENC_SHIFT/* upd shift */),
	/* CLK_CFG_12 */
	MUX_IPI(CLK_TOP_VDEC_SEL/* dts */, "top_vdec_sel", top_vdec_parents/* parent */,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR/* set parent */,
		HWV_CG_4_DONE, HWV_CG_4_SET, HWV_CG_4_CLR, /* hwv */
		3/* ipi */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VDEC_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWM_SEL/* dts */, "top_pwm_sel",
		top_pwm_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_H_SEL/* dts */, "top_audio_h_sel",
		top_audio_h_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUDIO_H_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCUPM_SEL/* dts */, "top_mcupm_sel",
		top_mcupm_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_SEL/* dts */, "top_mem_sub_sel",
		top_mem_sub_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUBP_SEL/* dts */, "top_mem_subp_sel",
		top_mem_subp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_PERI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_UFS_SEL/* dts */, "top_mem_sub_ufs_sel",
		top_mem_sub_ufs_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_N_SEL/* dts */, "top_emi_n_sel",
		top_emi_n_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */),
	/* CLK_CFG_14 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSI_OCC_SEL/* dts */, "top_dsi_occ_sel",
		top_dsi_occ_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DSI_OCC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "top_ap2conn_host_sel",
		top_ap2conn_host_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */),
	MUX_IPI(CLK_TOP_IMG1_SEL/* dts */, "top_img1_sel", top_img1_parents/* parent */,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR/* set parent */,
		HWV_CG_5_DONE, HWV_CG_5_SET, HWV_CG_5_CLR, /* hwv */
		5/* ipi */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IMG1_SHIFT/* upd shift */),
	MUX_IPI(CLK_TOP_IPE_SEL/* dts */, "top_ipe_sel", top_ipe_parents/* parent */,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR/* set parent */,
		HWV_CG_5_DONE, HWV_CG_5_SET, HWV_CG_5_CLR, /* hwv */
		6/* ipi */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IPE_SHIFT/* upd shift */),
	/* CLK_CFG_15 */
	MUX_IPI(CLK_TOP_CAM_SEL/* dts */, "top_cam_sel", top_cam_parents/* parent */,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR/* set parent */,
		HWV_CG_6_DONE, HWV_CG_6_SET, HWV_CG_6_CLR, /* hwv */
		4/* ipi */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_CAM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CCUSYS_SEL/* dts */, "top_ccusys_sel",
		top_ccusys_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_CCUSYS_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTM_SEL/* dts */, "top_camtm_sel",
		top_camtm_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CAMTM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CCU_AHB_SEL/* dts */, "top_ccu_ahb_sel",
		top_ccu_ahb_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CCU_AHB_SHIFT/* upd shift */),
	/* CLK_CFG_16 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CCUTM_SEL/* dts */, "top_ccutm_sel",
		top_ccutm_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CCUTM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_1P_RX_SEL/* dts */, "top_msdc_1p_rx_sel",
		top_msdc_1p_rx_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MSDC_1P_RX_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSP_SEL/* dts */, "top_dsp_sel",
		top_dsp_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_INF_546_SEL/* dts */, "top_md_emi_sel",
		top_md_emi_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */),
	/* CLK_CFG_18 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_REF_SEL/* dts */, "top_mfg_ref_sel",
		top_mfg_ref_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MFG_REF_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFGSC_REF_SEL/* dts */, "top_mfgsc_ref_sel",
		top_mfgsc_ref_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MFGSC_REF_SHIFT/* upd shift */),
	/* CLK_CFG_20 */
	MUX_CLR_SET(CLK_TOP_MFG_INT0_SEL/* dts */, "top_mfg_int0_sel",
		top_mfg_int0_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 16/* lsb */, 1/* width */),
	MUX_CLR_SET(CLK_TOP_MFG1_INT1_SEL/* dts */, "top_mfg1_int1_sel",
		top_mfg1_int1_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 17/* lsb */, 1/* width */),
#endif
};

static const struct mtk_composite top_composites[] = {
	/* CLK_AUDDIV_0 */
	MUX(CLK_TOP_APLL_SI0_MCK_SEL/* dts */, "top_apll_SI0_m_sel",
		top_apll_SI0_m_parents/* parent */, 0x0320/* ofs */,
		16/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_SI1_MCK_SEL/* dts */, "top_apll_SI1_m_sel",
		top_apll_SI1_m_parents/* parent */, 0x0320/* ofs */,
		17/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_SI2_MCK_SEL/* dts */, "top_apll_SI2_m_sel",
		top_apll_SI2_m_parents/* parent */, 0x0320/* ofs */,
		18/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_SI3_MCK_SEL/* dts */, "top_apll_SI3_m_sel",
		top_apll_SI3_m_parents/* parent */, 0x0320/* ofs */,
		19/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_SI4_MCK_SEL/* dts */, "top_apll_SI4_m_sel",
		top_apll_SI4_m_parents/* parent */, 0x0320/* ofs */,
		20/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_SI6_MCK_SEL/* dts */, "top_apll_SI6_m_sel",
		top_apll_SI6_m_parents/* parent */, 0x0320/* ofs */,
		21/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_SO0_MCK_SEL/* dts */, "top_apll_SO0_m_sel",
		top_apll_SO0_m_parents/* parent */, 0x0320/* ofs */,
		22/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_SO1_MCK_SEL/* dts */, "top_apll_SO1_m_sel",
		top_apll_SO1_m_parents/* parent */, 0x0320/* ofs */,
		23/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_SO2_MCK_SEL/* dts */, "top_apll_SO2_m_sel",
		top_apll_SO2_m_parents/* parent */, 0x0320/* ofs */,
		24/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_SO3_MCK_SEL/* dts */, "top_apll_SO3_m_sel",
		top_apll_SO3_m_parents/* parent */, 0x0320/* ofs */,
		25/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_SO4_MCK_SEL/* dts */, "top_apll_SO4_m_sel",
		top_apll_SO4_m_parents/* parent */, 0x0320/* ofs */,
		26/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_SO6_MCK_SEL/* dts */, "top_apll_SO6_m_sel",
		top_apll_SO6_m_parents/* parent */, 0x0320/* ofs */,
		27/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_FMI2S_MCK_SEL/* dts */, "top_apll_fmi2s_m_sel",
		top_apll_fmi2s_m_parents/* parent */, 0x0320/* ofs */,
		28/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_TD_MCK_SEL/* dts */, "top_apll_td_m_sel",
		top_apll_td_m_parents/* parent */, 0x0320/* ofs */,
		29/* lsb */, 1/* width */),
	/* CLK_AUDDIV_5 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_TD_M/* dts */, "top_apll12_div_td_m"/* ccf */,
		"top_apll_td_m_sel"/* parent */, 0x0320/* pdn ofs */,
		13/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		8/* lsb */),
};

static const struct mtk_gate_regs top_cg_regs = {
	.set_ofs = 0x320,
	.clr_ofs = 0x320,
	.sta_ofs = 0x320,
};

#define GATE_TOP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate top_clks[] = {
	GATE_TOP(CLK_TOP_APLL12_DIV_SI1, "top_apll12_div_SI1",
			"top_aud_1_ck"/* parent */, 1),
	GATE_TOP(CLK_TOP_APLL12_DIV_SI2, "top_apll12_div_SI2",
			"top_aud_1_ck"/* parent */, 2),
	GATE_TOP(CLK_TOP_APLL12_DIV_SI4, "top_apll12_div_SI4",
			"top_aud_1_ck"/* parent */, 4),
	GATE_TOP(CLK_TOP_APLL12_DIV_SO1, "top_apll12_div_SO1",
			"top_aud_1_ck"/* parent */, 7),
	GATE_TOP(CLK_TOP_APLL12_DIV_SO2, "top_apll12_div_SO2",
			"top_aud_1_ck"/* parent */, 8),
	GATE_TOP(CLK_TOP_APLL12_DIV_SO4, "top_apll12_div_SO4",
			"top_aud_1_ck"/* parent */, 10),
	GATE_TOP(CLK_TOP_APLL12_DIV_FMI2S, "top_apll12_div_fmi2s",
			"top_aud_1_ck"/* parent */, 12),
};

static const struct mtk_clk_desc top_mcd = {
	.clks = top_clks,
	.num_clks = CLK_TOP_NR_CLK,
};


enum subsys_id {
	APMIXEDSYS = 0,
	MFGPLL_PLL_CTRL = 1,
	MFGSCPLL_PLL_CTRL = 2,
	PLL_SYS_NUM,
};

static const struct mtk_pll_data *plls_data[PLL_SYS_NUM];
static void __iomem *plls_base[PLL_SYS_NUM];

#define MT6878_PLL_FMAX		(3800UL * MHZ)
#define MT6878_PLL_FMIN		(1500UL * MHZ)
#define MT6878_INTEGER_BITS	8

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL(_id, _name, _reg, _en_reg, _en_mask, _pll_en_bit,		\
			_flags, _rst_bar_mask,				\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _en_mask,					\
		.pll_en_bit = _pll_en_bit,				\
		.flags = (_flags | PLL_CFLAGS),				\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT6878_PLL_FMAX,				\
		.fmin = MT6878_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6878_INTEGER_BITS,			\
	}

#define PLL_SETCLR(_id, _name, _pll_setclr, _en_setclr_bit,		\
			_rstb_setclr_bit, _flags, _pd_reg,		\
			_pd_shift, _tuner_reg, _tuner_en_reg,		\
			_tuner_en_bit, _pcw_reg, _pcw_shift,		\
			_pcwbits) {					\
		.id = _id,						\
		.name = _name,						\
		.reg = 0,						\
		.pll_setclr = &(_pll_setclr),				\
		.en_setclr_bit = _en_setclr_bit,			\
		.rstb_setclr_bit = _rstb_setclr_bit,			\
		.flags = (_flags | PLL_CFLAGS),				\
		.fmax = MT6878_PLL_FMAX,				\
		.fmin = MT6878_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6878_INTEGER_BITS,			\
	}

#define PLL_HWV_SETCLR(_id, _name, _pll_setclr, _hwv_comp,		\
			_hwv_sta_ofs, _hwv_set_ofs, _hwv_clr_ofs,	\
			_en_setclr_bit, _rstb_setclr_bit, _flags,	\
			_pd_reg, _pd_shift, _tuner_reg, _tuner_en_reg,	\
			_tuner_en_bit, _pcw_reg, _pcw_shift,		\
			_pcwbits) {					\
		.id = _id,						\
		.name = _name,						\
		.pll_setclr = &(_pll_setclr),				\
		.hwv_comp = _hwv_comp,					\
		.hwv_set_ofs = _hwv_set_ofs,				\
		.hwv_clr_ofs = _hwv_clr_ofs,				\
		.hwv_sta_ofs = _hwv_sta_ofs,				\
		.en_setclr_bit = _en_setclr_bit,			\
		.rstb_setclr_bit = _rstb_setclr_bit,			\
		.flags = (_flags | PLL_CFLAGS | CLK_USE_HW_VOTER | HWV_CHK_FULL_STA),	\
		.fmax = MT6878_PLL_FMAX,				\
		.fmin = MT6878_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6878_INTEGER_BITS,			\
	}

static struct mtk_pll_setclr_data setclr_data = {
	.en_ofs = 0x0070,
	.en_set_ofs = 0x0074,
	.en_clr_ofs = 0x0078,
	.rstb_ofs = 0x0080,
	.rstb_set_ofs = 0x0084,
	.rstb_clr_ofs = 0x0088,
};

static const struct mtk_pll_data apmixed_plls[] = {
	PLL_SETCLR(CLK_APMIXED_ARMPLL_LL, "armpll-ll", setclr_data/*base*/,
		9, 0, PLL_AO,
		ARMPLL_LL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_LL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_ARMPLL_BL, "armpll-bl", setclr_data/*base*/,
		8, 0, PLL_AO,
		ARMPLL_BL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_BL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_CCIPLL, "ccipll", setclr_data/*base*/,
		7, 0, PLL_AO,
		CCIPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		CCIPLL_CON1, 0, 22/*pcw*/),
	PLL_HWV_SETCLR(CLK_APMIXED_MAINPLL, "mainpll", setclr_data/*base*/,
		"hw-voter-regmap"/*comp*/, HWV_CG_7_DONE,
		HWV_CG_7_SET, HWV_CG_7_CLR, /* hwv */
		6, 2, HAVE_RST_BAR | PLL_AO | CLK_NO_RES,
		MAINPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MAINPLL_CON1, 0, 22/*pcw*/),
	PLL_HWV_SETCLR(CLK_APMIXED_UNIVPLL, "univpll", setclr_data/*base*/,
		"hw-voter-regmap"/*comp*/, HWV_CG_7_DONE,
		HWV_CG_7_SET, HWV_CG_7_CLR, /* hwv */
		5, 1, HAVE_RST_BAR | CLK_NO_RES,
		UNIVPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		UNIVPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_MSDCPLL, "msdcpll", setclr_data/*base*/,
		4, 0, 0,
		MSDCPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MSDCPLL_CON1, 0, 22/*pcw*/),
	PLL_HWV_SETCLR(CLK_APMIXED_MMPLL, "mmpll", setclr_data/*base*/,
		"hw-voter-regmap"/*comp*/, HWV_CG_7_DONE,
		HWV_CG_7_SET, HWV_CG_7_CLR, /* hwv */
		3, 0, HAVE_RST_BAR | CLK_NO_RES,
		MMPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MMPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_UFSPLL, "ufspll", setclr_data/*base*/,
		2, 0, 0,
		UFSPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		UFSPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_APLL1, "apll1", setclr_data/*base*/,
		1, 0, 0,
		APLL1_CON1, 24/*pd*/,
		APLL1_TUNER_CON0, AP_PLL_CON3, 0/*tuner*/,
		APLL1_CON2, 0, 32/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_APLL2, "apll2", setclr_data/*base*/,
		0, 0, 0,
		APLL2_CON1, 24/*pd*/,
		APLL2_TUNER_CON0, AP_PLL_CON3, 1/*tuner*/,
		APLL2_CON2, 0, 32/*pcw*/),
};

static const struct mtk_pll_data mfg_ao_plls[] = {
	PLL(CLK_MFG_AO_MFGPLL, "mfg-ao-mfgpll", MFGPLL_CON0/*base*/,
		MFGPLL_CON0, 0, 0/*en*/,
		0, BIT(0)/*rstb*/,
		MFGPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data mfgsc_ao_plls[] = {
	PLL(CLK_MFGSC_AO_MFGSCPLL, "mfgsc-ao-mfgscpll", MFGSCPLL_CON0/*base*/,
		MFGSCPLL_CON0, 0, 0/*en*/,
		0, BIT(0)/*rstb*/,
		MFGSCPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGSCPLL_CON1, 0, 22/*pcw*/),
};

static int clk_mt6878_pll_registration(enum subsys_id id,
		const struct mtk_pll_data *plls,
		struct platform_device *pdev,
		int num_plls)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	if (id >= PLL_SYS_NUM) {
		pr_notice("%s init invalid id(%d)\n", __func__, id);
		return 0;
	}

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(num_plls);

	mtk_clk_register_plls(node, plls, num_plls,
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	plls_data[id] = plls;
	plls_base[id] = base;

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6878_apmixed_probe(struct platform_device *pdev)
{
	return clk_mt6878_pll_registration(APMIXEDSYS, apmixed_plls,
			pdev, ARRAY_SIZE(apmixed_plls));
}

static int clk_mt6878_mfg_ao_probe(struct platform_device *pdev)
{
	return clk_mt6878_pll_registration(MFGPLL_PLL_CTRL, mfg_ao_plls,
			pdev, ARRAY_SIZE(mfg_ao_plls));
}

static int clk_mt6878_mfgsc_ao_probe(struct platform_device *pdev)
{
	return clk_mt6878_pll_registration(MFGSCPLL_PLL_CTRL, mfgsc_ao_plls,
			pdev, ARRAY_SIZE(mfgsc_ao_plls));
}

static int clk_mt6878_top_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);

	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs),
			clk_data);

	mtk_clk_register_muxes(top_muxes, ARRAY_SIZE(top_muxes), node,
			&mt6878_clk_lock, clk_data);

	mtk_clk_register_gates(node, top_clks, ARRAY_SIZE(top_clks), clk_data);

	mtk_clk_register_composites(top_composites, ARRAY_SIZE(top_composites),
			base, &mt6878_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6878_vlp_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_VLP_NR_CLK);

	mtk_clk_register_factors(vlp_divs, ARRAY_SIZE(vlp_divs),
			clk_data);

	mtk_clk_register_muxes(vlp_muxes, ARRAY_SIZE(vlp_muxes), node,
			&mt6878_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

/* for suspend LDVT only */
static void pll_force_off_internal(const struct mtk_pll_data *plls,
		void __iomem *base)
{
	void __iomem *rst_reg, *en_reg, *pwr_reg;

	for (; plls->name; plls++) {
		/* do not pwrdn the AO PLLs */
		if ((plls->flags & PLL_AO) == PLL_AO)
			continue;

		if ((plls->flags & HAVE_RST_BAR) == HAVE_RST_BAR) {
			rst_reg = base + plls->en_reg;
			writel(readl(rst_reg) & ~plls->rst_bar_mask,
				rst_reg);
		}

		en_reg = base + plls->en_reg;

		pwr_reg = base + plls->pwr_reg;

		writel(readl(en_reg) & ~plls->en_mask,
				en_reg);
		writel(readl(pwr_reg) | (0x2),
				pwr_reg);
		writel(readl(pwr_reg) & ~(0x1),
				pwr_reg);
	}
}

void mt6878_pll_force_off(void)
{
	int i;

	for (i = 0; i < PLL_SYS_NUM; i++)
		pll_force_off_internal(plls_data[i], plls_base[i]);
}
EXPORT_SYMBOL_GPL(mt6878_pll_force_off);

static const struct of_device_id of_match_clk_mt6878[] = {
	{
		.compatible = "mediatek,mt6878-apmixedsys",
		.data = clk_mt6878_apmixed_probe,
	}, {
		.compatible = "mediatek,mt6878-mfgpll_pll_ctrl",
		.data = clk_mt6878_mfg_ao_probe,
	}, {
		.compatible = "mediatek,mt6878-mfgscpll_pll_ctrl",
		.data = clk_mt6878_mfgsc_ao_probe,
	}, {
		.compatible = "mediatek,mt6878-topckgen",
		.data = clk_mt6878_top_probe,
	}, {
		.compatible = "mediatek,mt6878-vlp_cksys",
		.data = clk_mt6878_vlp_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6878_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt6878_drv = {
	.probe = clk_mt6878_probe,
	.driver = {
		.name = "clk-mt6878",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6878,
	},
};

module_platform_driver(clk_mt6878_drv);
MODULE_LICENSE("GPL");

