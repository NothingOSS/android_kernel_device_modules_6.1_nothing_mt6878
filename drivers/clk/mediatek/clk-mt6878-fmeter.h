/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chuan-wen Chen <chuan-wen.chen@mediatek.com>
 */

#ifndef _CLK_MT6878_FMETER_H
#define _CLK_MT6878_FMETER_H

/* generate from clock_table.xlsx from TOPCKGEN DE */

/* CKGEN Part */
#define FM_AXI_CK				1
#define FM_AXIP_CK				2
#define FM_AXI_UFS_CK				3
#define FM_BUS_AXIMEM_CK		4
#define FM_DISP0_CK				5
#define FM_MMINFRA_CK				6
#define FM_MMUP_CK				7
#define FM_CAMTG_CK				8
#define FM_CAMTG2_CK				9
#define FM_CAMTG3_CK				10
#define FM_CAMTG4_CK				11
#define FM_CAMTG5_CK				12
#define FM_CAMTG6_CK				13
#define FM_UART_CK				14
#define FM_SPI0_CK				15
#define FM_SPI1_CK				16
#define FM_SPI2_CK				17
#define FM_SPI3_CK				18
#define FM_SPI4_CK				19
#define FM_SPI5_CK				20
#define FM_SPI6_CK				21
#define FM_SPI7_CK				22
#define FM_MSDC_0P_CK				23
#define FM_MSDC5HCLK_CK				24
#define FM_MSDC50_0_CK				25
#define FM_AES_MSDCFDE_CK			26
#define FM_MSDC_1P_CK				27
#define FM_MSDC30_1_CK				28
#define FM_MSDC30_1_H_CK			29
#define FM_AUD_INTBUS_CK			30
#define FM_ATB_CK				31
#define FM_DISP_PWM_CK				32
#define FM_USB_CK				33
#define FM_USB_XHCI_CK				34
#define FM_I2C_CK				35
#define FM_SENINF_CK				36
#define FM_SENINF1_CK				37
#define FM_SENINF2_CK				38
#define FM_SENINF3_CK				39
#define FM_AUD_ENGEN1_CK			40
#define FM_AUD_ENGEN2_CK			41
#define FM_AES_UFSFDE_CK			42
#define FM_UFS_CK				43
#define FM_UFS_MBIST_CK				44
#define FM_AUD_1_CK				45
#define FM_AUD_2_CK				46
#define FM_DPMAIF_MAIN_CK			47
#define FM_VENC_CK				48
#define FM_VDEC_CK				49
#define FM_PWM_CK				50
#define FM_AUDIO_H_CK				51
#define FM_MCUPM_CK				52
#define FM_MEM_SUB_CK				53
#define FM_MEM_SUBP_CK				54
#define FM_MEM_SUB_UFS_CK			55
#define FM_EMI_N_CK				56
#define FM_DSI_OCC_CK				57
#define FM_AP2CONN_HOST_CK			58
#define FM_IMG1_CK				59
#define FM_IPE_CK				60
#define FM_CAM_CK				61
#define FM_CCUSYS_CK				62
#define FM_CAMTM_CK				63
#define FM_CCU_AHB_CK				64
#define FM_CCUTM_CK				65
#define FM_DSP_CK				67
#define FM_EMI_INF_546_CK			68
#define FM_SR_PKA_CK				69
#define FM_SR_DMA_CK				70
#define FM_SR_KDF_CK				71
#define FM_SR_RNG_CK				72
#define FM_DXCC_CK				73
#define FM_MFG_REF_CK				74
#define FM_MFGSC_REF_CK				75
#define FM_CKGEN_NUM				76
/* ABIST Part */
#define FM_APLL1_CK				2
#define FM_APLL2_CK				3
#define FM_PLLGP_MIN_FM_CK			4
#define FM_ARMPLL_BL_CK				6
#define FM_ARMPLL_BL_CKDIV_CK			7
#define FM_ARMPLL_LL_CK				8
#define FM_ARMPLL_LL_CKDIV_CK			9
#define FM_CCIPLL_CK				10
#define FM_CCIPLL_CKDIV_CK			11
#define FM_CSI0B_DPHY_DELAYCAL_CK		12
#define FM_CSI0A_DPHY_DELAYCAL_CK		13
#define FM_LVTS_CKMON_APU			16
#define FM_DSI0_LNTC_DSICLK			20
#define FM_DSI0_MPLL_TST_CK			21
#define FM_MAINPLL_CKDIV_CK			23
#define FM_MAINPLL_CK				24
#define FM_MDPLL1_FS26M_GUIDE			25
#define FM_MMPLL_CKDIV_CK			26
#define FM_MMPLL_CK				27
#define FM_MMPLL_D3_CK				28
#define FM_MSDCPLL_CK				30
#define FM_UFSPLL_CK				35
#define FM_ULPOSC2_MON_V_VCORE_CK		36
#define FM_ULPOSC_MON_V_VCORE_CK		37
#define FM_UNIVPLL_CK				38
#define FM_UVPLL192M_CK				40
#define FM_UFS_CLK2FREQ_CK			41
#define FM_WBG_DIG_BPLL_CK			42
#define FM_WBG_DIG_WPLL_CK960			43
#define FM_466M_FMEM_INFRASYS			50
#define FM_MCUSYS_ARM_OUT_ALL			51
#define FM_MSDC11_IN_CK				54
#define FM_MSDC12_IN_CK				55
#define FM_MSDC21_IN_CK				56
#define FM_MSDC22_IN_CK				57
#define FM_F32K_VCORE_CK			58
#define FM_LVTS_CKMON_L7			63
#define FM_LVTS_CKMON_L6			64
#define FM_LVTS_CKMON_L5			65
#define FM_LVTS_CKMON_L4			66
#define FM_LVTS_CKMON_L3			67
#define FM_LVTS_CKMON_L2			68
#define FM_LVTS_CKMON_L1			69
#define FM_LVTS_CKMON_LM			70
#define FM_APLL1_CKDIV_CK			71
#define FM_APLL2_CKDIV_CK			72
#define FM_UFSPLL_CKDIV_CK			74
#define FM_MSDCPLL_CKDIV_CK			77
#define FM_ABIST_NUM				78
/* VLPCK Part */
#define FM_SCP_CK				1
#define FM_PWRAP_ULPOSC_CK			2
#define FM_SPMI_P_CK				3
#define FM_SPMI_M_CK				4
#define FM_DVFSRC_CK				5
#define FM_PWM_VLP_CK				6
#define FM_AXI_VLP_CK				7
#define FM_DBGAO_26M_CK				8
#define FM_SYSTIMER_26M_CK			9
#define FM_SSPM_CK				10
#define FM_SSPM_F26M_CK				11
#define FM_SRCK_CK				12
#define FM_SCP_SPI_CK				13
#define FM_SCP_IIC_CK				14
#define FM_SCP_SPI_HS_CK			15
#define FM_SCP_IIC_HS_CK			16
#define FM_SSPM_ULPOSC_CK			17
#define FM_TIA_ULPOSC_CK			18
#define FM_APXGPT_26M_CK			19
#define FM_SPM_CK				20
#define FM_DGAO_66M_CK				21
#define FM_ULPOSC_CORE_CK			22
#define FM_ULPOSC_CK				23
#define FM_OSC_CK				24
#define FM_OSC_2				25
#define FM_VLPCK_NUM				26

enum fm_sys_id {
	FM_APMIXEDSYS = 0,
	FM_TOPCKGEN = 1,
	FM_VLP_CKSYS = 2,
	FM_MFGPLL = 3,
	FM_MFGSCPLL = 4,
	FM_SYS_NUM = 5,
};

#endif /* _CLK_MT6878_FMETER_H */
