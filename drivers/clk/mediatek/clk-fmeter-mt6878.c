// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chuan-wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-fmeter.h"
#include "clk-mt6878-fmeter.h"

#define FM_TIMEOUT			30
#define SUBSYS_PLL_NUM			4
#define VLP_FM_WAIT_TIME		40	/* ~= 38.64ns * 1023 */

#define FM_PLL_CK			0
#define FM_PLL_CKDIV_CK			1
#define FM_CKDIV_SHIFT			(7)
#define FM_CKDIV_MASK			GENMASK(10, 7)
#define FM_POSTDIV_SHIFT		(24)
#define FM_POSTDIV_MASK			GENMASK(26, 24)
#define FM_TEST_CLK_EN			(1 << 15)

static DEFINE_SPINLOCK(meter_lock);
#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)

static DEFINE_SPINLOCK(subsys_meter_lock);
#define subsys_fmeter_lock(flags)   spin_lock_irqsave(&subsys_meter_lock, flags)
#define subsys_fmeter_unlock(flags) spin_unlock_irqrestore(&subsys_meter_lock, flags)

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

/* check from topckgen&vlpcksys CODA */
#define CLK26CALI_0					(0x220)
#define CLK26CALI_1					(0x224)
#define CLK_MISC_CFG_0				(0x140)
#define CLK_DBG_CFG					(0x17C)
#define VLP_FQMTR_CON0				(0x230)
#define VLP_FQMTR_CON1				(0x234)

/* MFGPLL_PLL_CTRL Register */
#define MFGPLL_CON0					(0x0008)
#define MFGPLL_CON1					(0x000C)
#define MFGPLL_FQMTR_CON0				(0x0040)
#define MFGPLL_FQMTR_CON1				(0x0044)

/* MFGSCPLL_PLL_CTRL Register */
#define MFGSCPLL_CON0					(0x0008)
#define MFGSCPLL_CON1					(0x000C)
#define MFGSCPLL_FQMTR_CON0				(0x0040)
#define MFGSCPLL_FQMTR_CON1				(0x0044)


static void __iomem *fm_base[FM_SYS_NUM];

struct fmeter_data {
	enum fm_sys_id type;
	const char *name;
	unsigned int pll_con0;
	unsigned int pll_con1;
	unsigned int con0;
	unsigned int con1;
};

static struct fmeter_data subsys_fm[] = {
	[FM_VLP_CKSYS] = {FM_VLP_CKSYS, "fm_vlp_cksys",
		0, 0, VLP_FQMTR_CON0, VLP_FQMTR_CON1},
	[FM_MFGPLL] = {FM_MFGPLL, "fm_mfgpll",
		MFGPLL_CON0, MFGPLL_CON1, MFGPLL_FQMTR_CON0, MFGPLL_FQMTR_CON1},
	[FM_MFGSCPLL] = {FM_MFGSCPLL, "fm_mfgscpll",
		MFGSCPLL_CON0, MFGSCPLL_CON1, MFGSCPLL_FQMTR_CON0, MFGSCPLL_FQMTR_CON1},
};

const char *comp_list[] = {
	[FM_APMIXEDSYS] = "mediatek,mt6878-apmixedsys",
	[FM_TOPCKGEN] = "mediatek,mt6878-topckgen",
	[FM_VLP_CKSYS] = "mediatek,mt6878-vlp_cksys",
	[FM_MFGPLL] = "mediatek,mt6878-mfgpll_pll_ctrl",
	[FM_MFGSCPLL] = "mediatek,mt6878-mfgscpll_pll_ctrl",
};

/*
 * clk fmeter
 */

#define FMCLK3(_t, _i, _n, _o, _g, _c) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .grp = _g, .ck_div = _c}
#define FMCLK2(_t, _i, _n, _o, _p, _c) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .pdn = _p, .ck_div = _c}
#define FMCLK(_t, _i, _n, _c) { .type = _t, .id = _i, .name = _n, .ck_div = _c}

static const struct fmeter_clk fclks[] = {
	/* CKGEN Part */
	FMCLK2(CKGEN, FM_AXI_CK, "fm_axi_ck", 0x0010, 7, 1),
	FMCLK2(CKGEN, FM_AXIP_CK, "fm_axip_ck", 0x0010, 15, 1),
	FMCLK2(CKGEN, FM_AXI_UFS_CK, "fm_axi_ufs_ck", 0x0010, 23, 1),
	FMCLK2(CKGEN, FM_BUS_AXIMEM_CK, "fm_bus_aximem_ck", 0x0010, 31, 1),
	FMCLK2(CKGEN, FM_DISP0_CK, "fm_disp0_ck", 0x0020, 7, 1),
	FMCLK2(CKGEN, FM_MMINFRA_CK, "fm_mminfra_ck", 0x0020, 15, 1),
	FMCLK2(CKGEN, FM_MMUP_CK, "fm_mmup_ck", 0x0020, 23, 1),
	FMCLK2(CKGEN, FM_CAMTG_CK, "fm_camtg_ck", 0x0020, 31, 1),
	FMCLK2(CKGEN, FM_CAMTG2_CK, "fm_camtg2_ck", 0x0030, 7, 1),
	FMCLK2(CKGEN, FM_CAMTG3_CK, "fm_camtg3_ck", 0x0030, 15, 1),
	FMCLK2(CKGEN, FM_CAMTG4_CK, "fm_camtg4_ck", 0x0030, 23, 1),
	FMCLK2(CKGEN, FM_CAMTG5_CK, "fm_camtg5_ck", 0x0030, 31, 1),
	FMCLK2(CKGEN, FM_CAMTG6_CK, "fm_camtg6_ck", 0x0040, 7, 1),
	FMCLK2(CKGEN, FM_UART_CK, "fm_uart_ck", 0x0040, 15, 1),
	FMCLK2(CKGEN, FM_SPI0_CK, "fm_spi0_ck", 0x0040, 23, 1),
	FMCLK2(CKGEN, FM_SPI1_CK, "fm_spi1_ck", 0x0040, 31, 1),
	FMCLK2(CKGEN, FM_SPI2_CK, "fm_spi2_ck", 0x0050, 7, 1),
	FMCLK2(CKGEN, FM_SPI3_CK, "fm_spi3_ck", 0x0050, 15, 1),
	FMCLK2(CKGEN, FM_SPI4_CK, "fm_spi4_ck", 0x0050, 23, 1),
	FMCLK2(CKGEN, FM_SPI5_CK, "fm_spi5_ck", 0x0050, 31, 1),
	FMCLK2(CKGEN, FM_SPI6_CK, "fm_spi6_ck", 0x0060, 7, 1),
	FMCLK2(CKGEN, FM_SPI7_CK, "fm_spi7_ck", 0x0060, 15, 1),
	FMCLK2(CKGEN, FM_MSDC_0P_CK, "fm_msdc_0p_ck", 0x0060, 23, 1),
	FMCLK2(CKGEN, FM_MSDC5HCLK_CK, "fm_msdc5hclk_ck", 0x0060, 31, 1),
	FMCLK2(CKGEN, FM_MSDC50_0_CK, "fm_msdc50_0_ck", 0x0070, 7, 1),
	FMCLK2(CKGEN, FM_AES_MSDCFDE_CK, "fm_aes_msdcfde_ck", 0x0070, 15, 1),
	FMCLK2(CKGEN, FM_MSDC_1P_CK, "fm_msdc_1p_ck", 0x0070, 23, 1),
	FMCLK2(CKGEN, FM_MSDC30_1_CK, "fm_msdc30_1_ck", 0x0070, 31, 1),
	FMCLK2(CKGEN, FM_MSDC30_1_H_CK, "fm_msdc30_1_h_ck", 0x0080, 7, 1),
	FMCLK2(CKGEN, FM_AUD_INTBUS_CK, "fm_aud_intbus_ck", 0x0080, 15, 1),
	FMCLK2(CKGEN, FM_ATB_CK, "fm_atb_ck", 0x0080, 23, 1),
	FMCLK2(CKGEN, FM_DISP_PWM_CK, "fm_disp_pwm_ck", 0x0080, 31, 1),
	FMCLK2(CKGEN, FM_USB_CK, "fm_usb_ck", 0x0090, 7, 1),
	FMCLK2(CKGEN, FM_USB_XHCI_CK, "fm_usb_xhci_ck", 0x0090, 15, 1),
	FMCLK2(CKGEN, FM_I2C_CK, "fm_i2c_ck", 0x0090, 23, 1),
	FMCLK2(CKGEN, FM_SENINF_CK, "fm_seninf_ck", 0x0090, 31, 1),
	FMCLK2(CKGEN, FM_SENINF1_CK, "fm_seninf1_ck", 0x00A0, 7, 1),
	FMCLK2(CKGEN, FM_SENINF2_CK, "fm_seninf2_ck", 0x00A0, 15, 1),
	FMCLK2(CKGEN, FM_SENINF3_CK, "fm_seninf3_ck", 0x00A0, 23, 1),
	FMCLK2(CKGEN, FM_AUD_ENGEN1_CK, "fm_aud_engen1_ck", 0x00A0, 31, 1),
	FMCLK2(CKGEN, FM_AUD_ENGEN2_CK, "fm_aud_engen2_ck", 0x00B0, 7, 1),
	FMCLK2(CKGEN, FM_AES_UFSFDE_CK, "fm_aes_ufsfde_ck", 0x00B0, 15, 1),
	FMCLK2(CKGEN, FM_UFS_CK, "fm_ufs_ck", 0x00B0, 23, 1),
	FMCLK2(CKGEN, FM_UFS_MBIST_CK, "fm_ufs_mbist_ck", 0x00B0, 31, 1),
	FMCLK2(CKGEN, FM_AUD_1_CK, "fm_aud_1_ck", 0x00C0, 7, 1),
	FMCLK2(CKGEN, FM_AUD_2_CK, "fm_aud_2_ck", 0x00C0, 15, 1),
	FMCLK2(CKGEN, FM_DPMAIF_MAIN_CK, "fm_dpmaif_main_ck", 0x00C0, 23, 1),
	FMCLK2(CKGEN, FM_VENC_CK, "fm_venc_ck", 0x00C0, 31, 1),
	FMCLK2(CKGEN, FM_VDEC_CK, "fm_vdec_ck", 0x00D0, 7, 1),
	FMCLK2(CKGEN, FM_PWM_CK, "fm_pwm_ck", 0x00D0, 15, 1),
	FMCLK2(CKGEN, FM_AUDIO_H_CK, "fm_audio_h_ck", 0x00D0, 23, 1),
	FMCLK2(CKGEN, FM_MCUPM_CK, "fm_mcupm_ck", 0x00D0, 31, 1),
	FMCLK2(CKGEN, FM_MEM_SUB_CK, "fm_mem_sub_ck", 0x00E0, 7, 1),
	FMCLK2(CKGEN, FM_MEM_SUBP_CK, "fm_mem_subp_ck", 0x00E0, 15, 1),
	FMCLK2(CKGEN, FM_MEM_SUB_UFS_CK, "fm_mem_sub_ufs_ck", 0x00E0, 23, 1),
	FMCLK2(CKGEN, FM_EMI_N_CK, "fm_emi_n_ck", 0x00E0, 31, 1),
	FMCLK2(CKGEN, FM_DSI_OCC_CK, "fm_dsi_occ_ck", 0x00F0, 7, 1),
	FMCLK2(CKGEN, FM_AP2CONN_HOST_CK, "fm_ap2conn_host_ck", 0x00F0, 15, 1),
	FMCLK2(CKGEN, FM_IMG1_CK, "fm_img1_ck", 0x00F0, 23, 1),
	FMCLK2(CKGEN, FM_IPE_CK, "fm_ipe_ck", 0x00F0, 31, 1),
	FMCLK2(CKGEN, FM_CAM_CK, "fm_cam_ck", 0x0100, 7, 1),
	FMCLK2(CKGEN, FM_CCUSYS_CK, "fm_ccusys_ck", 0x0100, 15, 1),
	FMCLK2(CKGEN, FM_CAMTM_CK, "fm_camtm_ck", 0x0100, 23, 1),
	FMCLK2(CKGEN, FM_CCU_AHB_CK, "fm_ccu_ahb_ck", 0x0100, 31, 1),
	FMCLK2(CKGEN, FM_CCUTM_CK, "fm_ccutm_ck", 0x0110, 7, 1),
	FMCLK2(CKGEN, FM_DSP_CK, "fm_dsp_ck", 0x0110, 23, 1),
	FMCLK2(CKGEN, FM_EMI_INF_546_CK, "fm_emi_inf_546_ck", 0x0110, 31, 1),
	FMCLK2(CKGEN, FM_SR_PKA_CK, "fm_sr_pka_ck", 0x0180, 7, 1),
	FMCLK2(CKGEN, FM_SR_DMA_CK, "fm_sr_dma_ck", 0x0180, 15, 1),
	FMCLK2(CKGEN, FM_SR_KDF_CK, "fm_sr_kdf_ck", 0x0180, 23, 1),
	FMCLK2(CKGEN, FM_SR_RNG_CK, "fm_sr_rng_ck", 0x0180, 31, 1),
	FMCLK2(CKGEN, FM_DXCC_CK, "fm_dxcc_ck", 0x0190, 7, 1),
	FMCLK2(CKGEN, FM_MFG_REF_CK, "fm_mfg_ref_ck", 0x0190, 15, 1),
	FMCLK2(CKGEN, FM_MFGSC_REF_CK, "fm_mfgsc_ref_ck", 0x0190, 23, 1),
	/* ABIST Part */
	FMCLK(ABIST, FM_APLL1_CK, "fm_apll1_ck", 1),
	FMCLK(ABIST, FM_APLL2_CK, "fm_apll2_ck", 1),
	FMCLK(ABIST, FM_PLLGP_MIN_FM_CK, "fm_pllgp_min_fm_ck", 1),
	FMCLK(ABIST, FM_ARMPLL_BL_CK, "fm_armpll_bl_ck", 1),
	FMCLK3(ABIST, FM_ARMPLL_BL_CKDIV_CK, "fm_armpll_bl_ckdiv_ck", 0x0218, 3, 13),
	FMCLK(ABIST, FM_ARMPLL_LL_CK, "fm_armpll_ll_ck", 1),
	FMCLK3(ABIST, FM_ARMPLL_LL_CKDIV_CK, "fm_armpll_ll_ckdiv_ck", 0x0208, 3, 13),
	FMCLK(ABIST, FM_CCIPLL_CK, "fm_ccipll_ck", 1),
	FMCLK3(ABIST, FM_CCIPLL_CKDIV_CK, "fm_ccipll_ckdiv_ck", 0x0228, 3, 13),
	FMCLK(ABIST, FM_CSI0B_DPHY_DELAYCAL_CK, "fm_csi0b_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI0A_DPHY_DELAYCAL_CK, "fm_csi0a_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_APU, "fm_lvts_ckmon_apu", 1),
	FMCLK(ABIST, FM_DSI0_LNTC_DSICLK, "fm_dsi0_lntc_dsiclk", 1),
	FMCLK(ABIST, FM_DSI0_MPLL_TST_CK, "fm_dsi0_mpll_tst_ck", 1),
	FMCLK3(ABIST, FM_MAINPLL_CKDIV_CK, "fm_mainpll_ckdiv_ck", 0x0308, 3, 13),
	FMCLK(ABIST, FM_MAINPLL_CK, "fm_mainpll_ck", 1),
	FMCLK(ABIST, FM_MDPLL1_FS26M_GUIDE, "fm_mdpll1_fs26m_guide", 1),
	FMCLK3(ABIST, FM_MMPLL_CKDIV_CK, "fm_mmpll_ckdiv_ck", 0x0328, 3, 13),
	FMCLK(ABIST, FM_MMPLL_CK, "fm_mmpll_ck", 1),
	FMCLK(ABIST, FM_MMPLL_D3_CK, "fm_mmpll_d3_ck", 1),
	FMCLK(ABIST, FM_MSDCPLL_CK, "fm_msdcpll_ck", 1),
	FMCLK(ABIST, FM_UFSPLL_CK, "fm_ufspll_ck", 1),
	FMCLK(ABIST, FM_ULPOSC2_MON_V_VCORE_CK, "fm_ulposc2_mon_v_vcore_ck", 1),
	FMCLK(ABIST, FM_ULPOSC_MON_V_VCORE_CK, "fm_ulposc_mon_v_vcore_ck", 1),
	FMCLK(ABIST, FM_UNIVPLL_CK, "fm_univpll_ck", 1),
	FMCLK(ABIST, FM_UVPLL192M_CK, "fm_uvpll192m_ck", 1),
	FMCLK(ABIST, FM_UFS_CLK2FREQ_CK, "fm_ufs_clk2freq_ck", 1),
	FMCLK(ABIST, FM_WBG_DIG_BPLL_CK, "fm_wbg_dig_bpll_ck", 1),
	FMCLK(ABIST, FM_WBG_DIG_WPLL_CK960, "fm_wbg_dig_wpll_ck960", 1),
	FMCLK(ABIST, FM_466M_FMEM_INFRASYS, "fm_466m_fmem_infrasys", 1),
	FMCLK(ABIST, FM_MCUSYS_ARM_OUT_ALL, "fm_mcusys_arm_out_all", 1),
	FMCLK(ABIST, FM_MSDC11_IN_CK, "fm_msdc11_in_ck", 1),
	FMCLK(ABIST, FM_MSDC12_IN_CK, "fm_msdc12_in_ck", 1),
	FMCLK(ABIST, FM_MSDC21_IN_CK, "fm_msdc21_in_ck", 1),
	FMCLK(ABIST, FM_MSDC22_IN_CK, "fm_msdc22_in_ck", 1),
	FMCLK(ABIST, FM_F32K_VCORE_CK, "fm_f32k_vcore_ck", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L7, "fm_lvts_ckmon_l7", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L6, "fm_lvts_ckmon_l6", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L5, "fm_lvts_ckmon_l5", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L4, "fm_lvts_ckmon_l4", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L3, "fm_lvts_ckmon_l3", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L2, "fm_lvts_ckmon_l2", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L1, "fm_lvts_ckmon_l1", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_LM, "fm_lvts_ckmon_lm", 1),
	FMCLK3(ABIST, FM_APLL1_CKDIV_CK, "fm_apll1_ckdiv_ck", 0x0338, 3, 13),
	FMCLK3(ABIST, FM_APLL2_CKDIV_CK, "fm_apll2_ckdiv_ck", 0x034C, 3, 13),
	FMCLK3(ABIST, FM_UFSPLL_CKDIV_CK, "fm_ufspll_ckdiv_ck", 0x0370, 3, 13),
	FMCLK3(ABIST, FM_MSDCPLL_CKDIV_CK, "fm_msdcpll_ckdiv_ck", 0x0360, 3, 13),
	/* VLPCK Part */
	FMCLK2(VLPCK, FM_SCP_CK, "fm_scp_ck", 0x0008, 7, 1),
	FMCLK2(VLPCK, FM_PWRAP_ULPOSC_CK, "fm_pwrap_ulposc_ck", 0x0008, 15, 1),
	FMCLK2(VLPCK, FM_SPMI_P_CK, "fm_spmi_p_ck", 0x0008, 23, 1),
	FMCLK2(VLPCK, FM_SPMI_M_CK, "fm_spmi_m_ck", 0x0008, 31, 1),
	FMCLK2(VLPCK, FM_DVFSRC_CK, "fm_dvfsrc_ck", 0x0014, 7, 1),
	FMCLK2(VLPCK, FM_PWM_VLP_CK, "fm_pwm_vlp_ck", 0x0014, 15, 1),
	FMCLK2(VLPCK, FM_AXI_VLP_CK, "fm_axi_vlp_ck", 0x0014, 23, 1),
	FMCLK2(VLPCK, FM_DBGAO_26M_CK, "fm_dbgao_26m_ck", 0x0014, 31, 1),
	FMCLK2(VLPCK, FM_SYSTIMER_26M_CK, "fm_systimer_26m_ck", 0x0020, 7, 1),
	FMCLK2(VLPCK, FM_SSPM_CK, "fm_sspm_ck", 0x0020, 15, 1),
	FMCLK2(VLPCK, FM_SSPM_F26M_CK, "fm_sspm_f26m_ck", 0x0020, 23, 1),
	FMCLK2(VLPCK, FM_SRCK_CK, "fm_srck_ck", 0x0020, 31, 1),
	FMCLK2(VLPCK, FM_SCP_SPI_CK, "fm_scp_spi_ck", 0x002C, 7, 1),
	FMCLK2(VLPCK, FM_SCP_IIC_CK, "fm_scp_iic_ck", 0x002C, 15, 1),
	FMCLK2(VLPCK, FM_SCP_SPI_HS_CK, "fm_scp_spi_hs_ck", 0x002C, 23, 1),
	FMCLK2(VLPCK, FM_SCP_IIC_HS_CK, "fm_scp_iic_hs_ck", 0x002C, 31, 1),
	FMCLK2(VLPCK, FM_SSPM_ULPOSC_CK, "fm_sspm_ulposc_ck", 0x0038, 7, 1),
	FMCLK2(VLPCK, FM_TIA_ULPOSC_CK, "fm_tia_ulposc_ck", 0x0038, 15, 1),
	FMCLK2(VLPCK, FM_APXGPT_26M_CK, "fm_apxgpt_26m_ck", 0x0038, 23, 1),
	FMCLK(VLPCK, FM_SPM_CK, "fm_spm_ck", 1),
	FMCLK(VLPCK, FM_DGAO_66M_CK, "fm_dgao_66m_ck", 1),
	FMCLK(VLPCK, FM_ULPOSC_CORE_CK, "fm_ulposc_core_ck", 1),
	FMCLK(VLPCK, FM_ULPOSC_CK, "fm_ulposc_ck", 1),
	FMCLK(VLPCK, FM_OSC_CK, "fm_osc_ck", 1),
	FMCLK(VLPCK, FM_OSC_2, "fm_osc_2", 1),
	/* SUBSYS Part */
	FMCLK(SUBSYS, FM_MFGPLL, "fm_mfgpll", 1),
	FMCLK(SUBSYS, FM_MFGSCPLL, "fm_mfgscpll", 1),
	{},
};

const struct fmeter_clk *mt6878_get_fmeter_clks(void)
{
	return fclks;
}

static unsigned int check_pdn(void __iomem *base,
		unsigned int type, unsigned int ID)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		if (fclks[i].type == type && fclks[i].id == ID)
			break;
	}

	if (i >= ARRAY_SIZE(fclks) - 1)
		return 1;

	if (!fclks[i].ofs)
		return 0;

	if (type == SUBSYS) {
		if ((clk_readl(base + fclks[i].ofs) & fclks[i].pdn)
				!= fclks[i].pdn) {
			return 1;
		}
	} else if (type != SUBSYS && ((clk_readl(base + fclks[i].ofs)
			& BIT(fclks[i].pdn)) == BIT(fclks[i].pdn)))
		return 1;

	return 0;
}

static void set_test_clk_en(unsigned int type, unsigned int ID, bool onoff)
{
	void __iomem *pll_con0 = NULL;
	int i;

	if (type != ABIST && type != ABIST_CK2)
		return;

	if ((ID <= 0) || (type == ABIST && ID >= FM_ABIST_NUM))
		return;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		if (fclks[i].type == type && fclks[i].id == ID
				&& fclks[i].grp != 0) {
			pll_con0 =  fm_base[FM_APMIXEDSYS] + fclks[i].ofs - 0x4;
			break;
		}
	}

	if ((i == (ARRAY_SIZE(fclks) - 1)) || pll_con0 == NULL)
		return;

	if (onoff) {
		// pll con0[15] = 1 (enable test clk)
		clk_writel(pll_con0, (clk_readl(pll_con0) | FM_TEST_CLK_EN));
	} else {
		clk_writel(pll_con0, (clk_readl(pll_con0) & ~(FM_TEST_CLK_EN)));
	}
}

/* implement ckgen&abist api (example as below) */

static int __mt_get_freq(unsigned int ID, int type)
{
	void __iomem *dbg_addr = fm_base[FM_TOPCKGEN] + CLK_DBG_CFG;
	void __iomem *misc_addr = fm_base[FM_TOPCKGEN] + CLK_MISC_CFG_0;
	void __iomem *cali0_addr = fm_base[FM_TOPCKGEN] + CLK26CALI_0;
	void __iomem *cali1_addr = fm_base[FM_TOPCKGEN] + CLK26CALI_1;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);

	set_test_clk_en(type, ID, true);

	if (type == CKGEN && check_pdn(fm_base[FM_TOPCKGEN], CKGEN, ID)) {
		pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		fmeter_unlock(flags);
		return 0;
	}

	while (clk_readl(cali0_addr) & 0x10) {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	}

	/* CLK26CALI_0[15]: rst 1 -> 0 */
	clk_writel(cali0_addr, (clk_readl(cali0_addr) & 0xFFFF7FFF));
	/* CLK26CALI_0[15]: rst 0 -> 1 */
	clk_writel(cali0_addr, (clk_readl(cali0_addr) | 0x00008000));

	if (type == CKGEN) {
		clk_dbg_cfg = clk_readl(dbg_addr);
		clk_writel(dbg_addr,
			(clk_dbg_cfg & 0xFFFF80FC) | (ID << 8) | (0x1));
	} else if (type == ABIST) {
		clk_dbg_cfg = clk_readl(dbg_addr);
		clk_writel(dbg_addr,
			(clk_dbg_cfg & 0xFF80FFFC) | (ID << 16));
	} else {
		fmeter_unlock(flags);
		return 0;
	}

	/* sel fqmtr_cksel and set ckgen_k1 to 0(DIV4) */
	clk_misc_cfg_0 = clk_readl(misc_addr);
	clk_writel(misc_addr, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(cali1_addr);
	clk_writel(cali0_addr, 0x9000);
	clk_writel(cali0_addr, 0x9010);

	/* wait frequency meter finish */
	i = 0;
	do {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	} while (clk_readl(cali0_addr) & 0x10);

	temp = clk_readl(cali1_addr) & 0xFFFF;

	output = (temp * 26000) / 1024;

	set_test_clk_en(type, ID, false);

	clk_writel(dbg_addr, clk_dbg_cfg);
	clk_writel(misc_addr, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/

	clk_writel(cali0_addr, 0x8000);
	fmeter_unlock(flags);

	if (i > FM_TIMEOUT)
		return 0;

	if ((output * 4) < 1000) {
		pr_notice("%s(%d): CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			ID,
			clk_readl(dbg_addr),
			clk_readl(misc_addr),
			clk_readl(cali0_addr),
			clk_readl(cali1_addr));
	}

	/* Fmeter is div by 4 */
	return (output * 4);
}

/* implement ckgen&abist api (example as below) */

static int __mt_get_freq2(unsigned int  type, unsigned int id)
{
	void __iomem *con0 = fm_base[type] + subsys_fm[type].con0;
	void __iomem *con1 = fm_base[type] + subsys_fm[type].con1;
	unsigned int temp;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);

	/* PLL4H_FQMTR_CON1[15]: rst 1 -> 0 */
	clk_writel(con0, clk_readl(con0) & 0xFFFF7FFF);
	/* PLL4H_FQMTR_CON1[15]: rst 0 -> 1 */
	clk_writel(con0, clk_readl(con0) | 0x8000);

	/* sel fqmtr_cksel */
	if (type == FM_VLP_CKSYS)
		clk_writel(con0, (clk_readl(con0) & 0xFFE0FFFF) | (id << 16));
	else
		clk_writel(con0, (clk_readl(con0) & 0x00FFFFF8) | (id << 0));
	/* set ckgen_load_cnt to 1024 */
	clk_writel(con1, (clk_readl(con1) & 0xFC00FFFF) | (0x1FF << 16));

	/* sel fqmtr_cksel and set ckgen_k1 to 0(DIV4) */
	clk_writel(con0, (clk_readl(con0) & 0x00FFFFFF) | (3 << 24));

	/* fqmtr_en set to 1, fqmtr_exc set to 0, fqmtr_start set to 0 */
	clk_writel(con0, (clk_readl(con0) & 0xFFFF8007) | 0x1000);
	/*fqmtr_start set to 1 */
	clk_writel(con0, clk_readl(con0) | 0x10);

	/* wait frequency meter finish */
	if (type == FM_VLP_CKSYS) {
		udelay(VLP_FM_WAIT_TIME);
	} else {
		while (clk_readl(con0) & 0x10) {
			udelay(10);
			i++;
			if (i > 30) {
				pr_notice("[%d]con0: 0x%x, con1: 0x%x\n",
					id, clk_readl(con0), clk_readl(con1));
				break;
			}
		}
	}

	temp = clk_readl(con1) & 0xFFFF;
	output = ((temp * 26000)) / 512; // Khz

	clk_writel(con0, 0x8000);

	fmeter_unlock(flags);

	/* Fmeter is div by 4 */
	return (output * 4);
}

static unsigned int mt6878_get_ckgen_freq(unsigned int ID)
{
	return __mt_get_freq(ID, CKGEN);
}

static unsigned int mt6878_get_abist_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST);
}

static unsigned int mt6878_get_vlpck_freq(unsigned int ID)
{
	return __mt_get_freq2(FM_VLP_CKSYS, ID);
}

static unsigned int mt6878_get_subsys_freq(unsigned int ID)
{
	int output = 0;
	unsigned long flags;

	subsys_fmeter_lock(flags);

	pr_notice("subsys ID: %d\n", ID);
	if (ID >= FM_SYS_NUM)
		return 0;

	output = __mt_get_freq2(ID, FM_PLL_CK);

	subsys_fmeter_unlock(flags);

	return output;
}

static unsigned int mt6878_get_fmeter_freq(unsigned int id,
		enum FMETER_TYPE type)
{
	if (type == CKGEN)
		return mt6878_get_ckgen_freq(id);
	else if (type == ABIST)
		return mt6878_get_abist_freq(id);
	else if (type == SUBSYS)
		return mt6878_get_subsys_freq(id);
	else if (type == VLPCK)
		return mt6878_get_vlpck_freq(id);

	return FT_NULL;
}

// implement fmeter id for ulposc1/2

static int mt6878_get_fmeter_id(enum FMETER_ID fid)
{
	if (fid == FID_DISP_PWM)
		return FM_DISP_PWM_CK;
	else if (fid == FID_ULPOSC1)
		return FM_OSC_CK;
	else if (fid == FID_ULPOSC2)
		return FM_OSC_2;

	return FID_NULL;
}

static void __iomem *get_base_from_comp(const char *comp)
{
	struct device_node *node;
	static void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, comp);
	if (node) {
		base = of_iomap(node, 0);
		if (!base) {
			pr_err("%s() can't find iomem for %s\n",
					__func__, comp);
			return ERR_PTR(-EINVAL);
		}

		return base;
	}

	pr_err("%s can't find compatible node\n", __func__);

	return ERR_PTR(-EINVAL);
}

/*
 * init functions
 */

static struct fmeter_ops fm_ops = {
	.get_fmeter_clks = mt6878_get_fmeter_clks,
	.get_fmeter_freq = mt6878_get_fmeter_freq,
	.get_fmeter_id = mt6878_get_fmeter_id,
};

static int clk_fmeter_mt6878_probe(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < FM_SYS_NUM; i++) {
		fm_base[i] = get_base_from_comp(comp_list[i]);
		if (IS_ERR(fm_base[i]))
			goto ERR;

	}

	fmeter_set_ops(&fm_ops);

	return 0;
ERR:
	pr_err("%s(%s) can't find base\n", __func__, comp_list[i]);

	return -EINVAL;
}

static struct platform_driver clk_fmeter_mt6878_drv = {
	.probe = clk_fmeter_mt6878_probe,
	.driver = {
		.name = "clk-fmeter-mt6878",
		.owner = THIS_MODULE,
	},
};

static int __init clk_fmeter_init(void)
{
	static struct platform_device *clk_fmeter_dev;

	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6878", -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev))
		pr_warn("unable to register clk-fmeter device");

	return platform_driver_register(&clk_fmeter_mt6878_drv);
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6878_drv);
}

subsys_initcall(clk_fmeter_init);
module_exit(clk_fmeter_exit);
MODULE_LICENSE("GPL");
