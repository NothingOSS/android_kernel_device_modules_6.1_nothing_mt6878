// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chuan-wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <clk-mux.h>
#include "clkdbg.h"
#include "clkchk.h"
#include "clk-fmeter.h"

const char * const *get_mt6878_all_clk_names(void)
{
	static const char * const clks[] = {
		/* topckgen */
		"top_axi_sel",
		"top_axip_sel",
		"top_axi_ufs_sel",
		"top_bus_aximem_sel",
		"top_disp0_sel",
		"top_mminfra_sel",
		"top_mmup_sel",
		"top_camtg_sel",
		"top_camtg2_sel",
		"top_camtg3_sel",
		"top_camtg4_sel",
		"top_camtg5_sel",
		"top_camtg6_sel",
		"top_uart_sel",
		"top_spi0_sel",
		"top_spi1_sel",
		"top_spi2_sel",
		"top_spi3_sel",
		"top_spi4_sel",
		"top_spi5_sel",
		"top_spi6_sel",
		"top_spi7_sel",
		"top_msdc_0p_sel",
		"top_msdc5hclk_sel",
		"top_msdc50_0_sel",
		"top_aes_msdcfde_sel",
		"top_msdc_1p_sel",
		"top_msdc30_1_sel",
		"top_msdc30_1_h_sel",
		"top_aud_intbus_sel",
		"top_atb_sel",
		"top_usb_sel",
		"top_usb_xhci_sel",
		"top_i2c_sel",
		"top_seninf_sel",
		"top_seninf1_sel",
		"top_seninf2_sel",
		"top_seninf3_sel",
		"top_aud_engen1_sel",
		"top_aud_engen2_sel",
		"top_aes_ufsfde_sel",
		"top_ufs_sel",
		"top_ufs_mbist_sel",
		"top_aud_1_sel",
		"top_aud_2_sel",
		"top_dpmaif_main_sel",
		"top_venc_sel",
		"top_vdec_sel",
		"top_pwm_sel",
		"top_audio_h_sel",
		"top_mcupm_sel",
		"top_mem_sub_sel",
		"top_mem_subp_sel",
		"top_mem_sub_ufs_sel",
		"top_emi_n_sel",
		"top_dsi_occ_sel",
		"top_ap2conn_host_sel",
		"top_img1_sel",
		"top_ipe_sel",
		"top_cam_sel",
		"top_ccusys_sel",
		"top_camtm_sel",
		"top_ccu_ahb_sel",
		"top_ccutm_sel",
		"top_msdc_1p_rx_sel",
		"top_dsp_sel",
		"top_md_emi_sel",
		"top_mfg_ref_sel",
		"top_mfgsc_ref_sel",
		"top_mfg_int0_sel",
		"top_mfg1_int1_sel",
		"top_apll_SI0_m_sel",
		"top_apll_SI1_m_sel",
		"top_apll_SI2_m_sel",
		"top_apll_SI3_m_sel",
		"top_apll_SI4_m_sel",
		"top_apll_SI6_m_sel",
		"top_apll_SO0_m_sel",
		"top_apll_SO1_m_sel",
		"top_apll_SO2_m_sel",
		"top_apll_SO3_m_sel",
		"top_apll_SO4_m_sel",
		"top_apll_SO6_m_sel",
		"top_apll_fmi2s_m_sel",
		"top_apll_td_m_sel",

		/* topckgen */
		"top_apll12_div_td_m",

		/* topckgen */
		"top_apll12_div_SI1",
		"top_apll12_div_SI2",
		"top_apll12_div_SI4",
		"top_apll12_div_SO1",
		"top_apll12_div_SO2",
		"top_apll12_div_SO4",
		"top_apll12_div_fmi2s",

		/* infra_ao_reg */
		"ifrao_dma",
		"ifrao_ccif1_ap",
		"ifrao_ccif1_md",
		"ifrao_ccif_ap",
		"ifrao_ccif_md",
		"ifrao_cldmabclk",
		"ifrao_cq_dma",
		"ifrao_ccif5_md",
		"ifrao_ccif2_ap",
		"ifrao_ccif2_md",
		"ifrao_dpmaif_main",
		"ifrao_ccif4_md",
		"ifrao_dpmaif_26m",

		/* apmixedsys */
		"armpll-ll",
		"armpll-bl",
		"ccipll",
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"ufspll",
		"apll1",
		"apll2",

		/* pericfg_ao */
		"peraop_uart0",
		"peraop_uart1",
		"peraop_uart2",
		"peraop_pwm_h",
		"peraop_pwm_b",
		"peraop_pwm_fb1",
		"peraop_pwm_fb2",
		"peraop_pwm_fb3",
		"peraop_pwm_fb4",
		"peraop_spi0_b",
		"peraop_spi1_b",
		"peraop_spi2_b",
		"peraop_spi3_b",
		"peraop_spi4_b",
		"peraop_spi5_b",
		"peraop_spi6_b",
		"peraop_spi7_b",
		"peraop_dma_b",
		"peraop_ssusb0_frmcnt",
		"peraop_msdc0",
		"peraop_msdc0_h",
		"peraop_msdc0_faes",
		"peraop_msdc0_mst_f",
		"peraop_msdc0_slv_h",
		"peraop_msdc1",
		"peraop_msdc1_h",
		"peraop_msdc1_mst_f",
		"peraop_msdc1_slv_h",
		"peraop_audio0",
		"peraop_audio1",
		"peraop_audio2",

		/* afe */
		"afe_dl0_dac_tml",
		"afe_dl0_dac_hires",
		"afe_dl0_dac",
		"afe_dl0_predis",
		"afe_dl0_nle",
		"afe_pcm1",
		"afe_pcm0",
		"afe_cm1",
		"afe_cm0",
		"afe_stf",
		"afe_hw_gain23",
		"afe_hw_gain01",
		"afe_fm_i2s",
		"afe_mtkaifv4",
		"afe_ul1_aht",
		"afe_ul1_adc_hires",
		"afe_ul1_tml",
		"afe_ul1_adc",
		"afe_ul0_aht",
		"afe_ul0_adc_hires",
		"afe_ul0_tml",
		"afe_ul0_adc",
		"afe_etdm_in4",
		"afe_etdm_in2",
		"afe_etdm_in1",
		"afe_etdm_out4",
		"afe_etdm_out2",
		"afe_etdm_out1",
		"afe_general2_asrc",
		"afe_general1_asrc",
		"afe_general0_asrc",
		"afe_connsys_i2s_asrc",
		"afe_audio_hopping_ck",
		"afe_audio_f26m_ck",
		"afe_apll1_ck",
		"afe_apll2_ck",
		"afe_h208m_ck",
		"afe_apll_tuner2",
		"afe_apll_tuner1",

		/* imp_iic_wrap_cen_s */
		"im_c_s_i3c5_w1s",
		"im_c_s_sec_w1s",

		/* ufscfg_ao */
		"ufsao_unipro_tx_sym",
		"ufsao_unipro_rx_sym0",
		"ufsao_unipro_rx_sym1",
		"ufsao_unipro_sys",
		"ufsao_unipro_sap_cfg",
		"ufsao_phy_ahb_s_bus",

		/* ufscfg_pdn */
		"ufspdn_UFSHCI",
		"ufspdn_ufshci_aes",
		"ufspdn_UFSHCI_ahb",
		"ufspdn_UFSHCI_axi",

		/* imp_iic_wrap_e_s */
		"imp_e_s_i3c0_w1s",
		"imp_e_s_i3c1_w1s",
		"imp_e_s_i3c2_w1s",
		"imp_e_s_i3c4_w1s",
		"imp_e_s_i3c9_w1s",
		"imp_e_s_sec_w1s",

		/* imp_iic_wrap_es_s */
		"imp_es_s_i3c10_w1s",
		"imp_es_s_i3c11_w1s",
		"imp_es_s_i3c12_w1s",
		"imp_es_s_sec_w1s",

		/* imp_iic_wrap_w_s */
		"imp_w_s_i3c3_w1s",
		"imp_w_s_i3c6_w1s",
		"imp_w_s_i3c7_w1s",
		"imp_w_s_i3c8_w1s",
		"imp_w_s_sec_w1s",

		/* mfgpll_pll_ctrl */
		"mfg-ao-mfgpll",

		/* mfgscpll_pll_ctrl */
		"mfgsc-ao-mfgscpll",

		/* dispsys_config */
		"mm_disp_ovl0_2l",
		"mm_disp_ovl1_2l",
		"mm_disp_ovl2_2l",
		"mm_disp_ovl3_2l",
		"mm_disp_ufbc_wdma0",
		"mm_disp_rsz1",
		"mm_disp_rsz0",
		"mm_disp_tdshp0",
		"mm_disp_c3d0",
		"mm_disp_color0",
		"mm_disp_ccorr0",
		"mm_disp_ccorr1",
		"mm_disp_aal0",
		"mm_disp_gamma0",
		"mm_disp_postmask0",
		"mm_disp_dither0",
		"mm_disp_tdshp1",
		"mm_disp_c3d1",
		"mm_disp_ccorr2",
		"mm_disp_ccorr3",
		"mm_disp_gamma1",
		"mm_disp_dither1",
		"mm_disp_splitter0",
		"mm_disp_dsc_wrap0",
		"mm_CLK0",
		"mm_CLK1",
		"mm_disp_wdma1",
		"mm_disp_apb_bus",
		"mm_disp_fake_eng0",
		"mm_disp_fake_eng1",
		"mm_disp_mutex0",
		"mm_smi_common",
		"mm_dsi0_ck",
		"mm_dsi1_ck",
		"mm_26m_ck",

		/* imgsys_main */
		"img_fdvt",
		"img_me",
		"img_mmg",
		"img_larb12",
		"img_larb9",
		"img_traw0",
		"img_traw1",
		"img_dip0",
		"img_wpe0",
		"img_ipe",
		"img_wpe1",
		"img_wpe2",
		"img_sub_common0",
		"img_sub_common1",
		"img_sub_common3",
		"img_sub_common4",
		"img_gals_rx_dip0",
		"img_gals_rx_dip1",
		"img_gals_rx_traw0",
		"img_gals_rx_wpe0",
		"img_gals_rx_wpe1",
		"img_gals_rx_ipe0",
		"img_gals_tx_ipe0",
		"img_gals",

		/* dip_top_dip1 */
		"dip_dip1_dip_top",
		"dip_dip1_dip_gals0",
		"dip_dip1_dip_gals1",
		"dip_dip1_dip_gals2",
		"dip_dip1_dip_gals3",
		"dip_dip1_larb10",
		"dip_dip1_larb15",
		"dip_dip1_larb38",
		"dip_dip1_larb39",

		/* dip_nr1_dip1 */
		"dip_nr1_dip1_larb",
		"dip_nr1_dip1_dip_nr1",

		/* dip_nr2_dip1 */
		"dip_nr2_dip1_larb15",
		"dip_nr2_dip1_dip_nr",

		/* wpe1_dip1 */
		"wpe1_dip1_larb11",
		"wpe1_dip1_wpe",
		"wpe1_dip1_gals0",

		/* wpe2_dip1 */
		"wpe2_dip1_larb11",
		"wpe2_dip1_wpe",
		"wpe2_dip1_gals0",

		/* traw_dip1 */
		"traw_dip1_larb28",
		"traw_dip1_larb40",
		"traw_dip1_traw",
		"traw_dip1_gals",

		/* img_vcore_d1a */
		"img_vcore_gals_disp",
		"img_vcore_main",
		"img_vcore_sub0",
		"img_vcore_sub1",

		/* vdec_gcon_base */
		"vde2_larb_cken",
		"vde2_vdec_cken",
		"vde2_vdec_active",

		/* venc_gcon */
		"ven1_larb",
		"ven1_venc",
		"ven1_jpgenc",
		"ven1_gals",

		/* vlp_cksys */
		"vlp_scp_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_spmi_p_sel",
		"vlp_spmi_m_sel",
		"vlp_dvfsrc_sel",
		"vlp_pwm_vlp_sel",
		"vlp_axi_vlp_sel",
		"vlp_dbgao_26m_sel",
		"vlp_systimer_26m_sel",
		"vlp_sspm_sel",
		"vlp_sspm_f26m_sel",
		"vlp_srck_sel",
		"vlp_scp_spi_sel",
		"vlp_scp_iic_sel",
		"vlp_scp_spi_hs_sel",
		"vlp_scp_iic_hs_sel",
		"vlp_sspm_ulposc_sel",
		"vlp_apxgpt_26m_sel",

		/* scp */
		"scp_set_spi0",
		"scp_set_spi1",
		"scp_set_spi2",
		"scp_set_spi3",

		/* camsys_main */
		"cam_m_larb13",
		"cam_m_larb14",
		"cam_m_larb29",
		"cam_m_cam",
		"cam_m_cam_suba",
		"cam_m_cam_subb",
		"cam_m_cam_mraw",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_camsv",
		"cam_m_cam2mm0_GCON_0",
		"cam_m_cam2mm1_GCON_0",
		"cam_m_ccusys",
		"cam_m_cam_asg",
		"cam_m_camsv_a_con_1",
		"cam_m_camsv_b_con_1",
		"cam_m_camsv_c_con_1",
		"cam_m_camsv_d_con_1",
		"cam_m_camsv_e_con_1",
		"cam_m_camsv_con_1",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",
		"cam_ra_raw2mm_gals",
		"cam_ra_yuv2raw2mm",

		/* camsys_yuva */
		"cam_ya_larbx",
		"cam_ya_cam",
		"cam_ya_camtg",

		/* camsys_rawb */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",
		"cam_rb_raw2mm_gals",
		"cam_rb_yuv2raw2mm",

		/* camsys_yuvb */
		"cam_yb_larbx",
		"cam_yb_cam",
		"cam_yb_camtg",

		/* camsys_mraw */
		"cam_mr_larbx",
		"cam_mr_gals",
		"cam_mr_camtg",
		"cam_mr_mraw0",
		"cam_mr_mraw1",
		"cam_mr_mraw2",
		"cam_mr_mraw3",
		"cam_mr_pda0",
		"cam_mr_pda1",

		/* ccu_main */
		"ccu_larb30_con",
		"ccu_ahb_con",
		"ccusys_ccu0_con",
		"ccu2mm0_GCON",

		/* cam_vcore */
		"cam_vcore_c2mm0_dis",
		"cam_vcore_mm0_dis",

		/* mminfra_config */
		"mminfra_gce_d",
		"mminfra_gce_m",
		"mminfra_gce_26m",

		/* mdpsys_config */
		"mdp_mutex0",
		"mdp_apb_bus",
		"mdp_smi0",
		"mdp_rdma0",
		"mdp_hdr0",
		"mdp_aal0",
		"mdp_rsz0",
		"mdp_tdshp0",
		"mdp_wrot0",
		"mdp_rdma1",
		"mdp_rsz1",
		"mdp_wrot1",


	};

	return clks;
}


/*
 * clkdbg dump all fmeter clks
 */
static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return mt_get_fmeter_clks();
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	return mt_get_fmeter_freq(fclk->id, fclk->type);
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6878_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6878_all_clk_names,
};

static int clk_dbg_mt6878_probe(struct platform_device *pdev)
{
	set_clkdbg_ops(&clkdbg_mt6878_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6878_drv = {
	.probe = clk_dbg_mt6878_probe,
	.driver = {
		.name = "clk-dbg-mt6878",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6878_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6878_drv, "clk-dbg-mt6878");
}

static void __exit clkdbg_mt6878_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6878_drv);
}

subsys_initcall(clkdbg_mt6878_init);
module_exit(clkdbg_mt6878_exit);
MODULE_LICENSE("GPL");
