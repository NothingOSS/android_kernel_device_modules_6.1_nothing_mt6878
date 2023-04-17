// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Benjamin Chao <benjamin.chao@mediatek.com>
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

const char * const *get_mt6897_all_clk_names(void)
{
	static const char * const clks[] = {
		/* topckgen */
		"axi_sel",
		"peri_faxi_sel",
		"ufs_faxi_sel",
		"bus_aximem_sel",
		"disp0_sel",
		"disp1_sel",
		"ovl0_sel",
		"ovl1_sel",
		"mdp0_sel",
		"mdp1_sel",
		"mminfra_sel",
		"dsp_sel",
		"mfg_ref_sel",
		"mfgsc_ref_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",
		"camtg5_sel",
		"camtg6_sel",
		"camtg7_sel",
		"camtg8_sel",
		"uart_sel",
		"msdc_macro_1p_sel",
		"msdc_macro_2p_sel",
		"msdc30_1_sel",
		"msdc30_2_sel",
		"aud_intbus_sel",
		"atb_sel",
		"dp_sel",
		"disp_pwm_sel",
		"usb_sel",
		"ssusb_xhci_sel",
		"i2c_sel",
		"seninf_sel",
		"seninf1_sel",
		"seninf2_sel",
		"seninf3_sel",
		"seninf4_sel",
		"seninf5_sel",
		"aud_engen1_sel",
		"aud_engen2_sel",
		"aes_ufsfde_sel",
		"ufs_sel",
		"pextp_mbist_sel",
		"aud_1_sel",
		"aud_2_sel",
		"adsp_sel",
		"audio_local_bus_sel",
		"dpmaif_main_sel",
		"venc_sel",
		"vdec_sel",
		"pwm_sel",
		"audio_h_sel",
		"mcupm_sel",
		"mem_sub_sel",
		"peri_fmem_sub_sel",
		"ufs_fmem_sub_sel",
		"emi_n_sel",
		"emi_s_sel",
		"ccu_ahb_sel",
		"ap2conn_host_sel",
		"img1_sel",
		"ipe_sel",
		"mcu_acp_sel",
		"mcu_l3gic_sel",
		"mcu_infra_sel",
		"tl_sel",
		"pextp_faxi_sel",
		"pextp_fmem_sub_sel",
		"emi_if_546_sel",
		"spi0_sel",
		"spi1_sel",
		"spi2_sel",
		"spi3_sel",
		"spi4_sel",
		"spi5_sel",
		"spi6_sel",
		"spi7_sel",
		"mmup_sel",
		"dbgao_26m_sel",
		"cam_sel",
		"camtm_sel",
		"dpe_sel",
		"mfg_int0_sel",
		"mfg1_int1_sel",
		"apll_i2sin0_mck_sel",
		"apll_i2sin1_mck_sel",
		"apll_i2sin2_mck_sel",
		"apll_i2sin3_mck_sel",
		"apll_i2sin4_mck_sel",
		"apll_i2sin6_mck_sel",
		"apll_i2sout0_mck_sel",
		"apll_i2sout1_mck_sel",
		"apll_i2sout2_mck_sel",
		"apll_i2sout3_mck_sel",
		"apll_i2sout4_mck_sel",
		"apll_i2sout6_mck_sel",
		"apll_fmi2s_mck_sel",
		"apll_tdmout_mck_sel",

		/* topckgen */
		"apll12_div_in0",
		"apll12_div_in1",
		"apll12_div_in2",
		"apll12_div_in3",
		"apll12_div_in4",
		"apll12_div_in6",
		"apll12_div_i2sout0",
		"apll12_div_i2sout1",
		"apll12_div_i2sout2",
		"apll12_div_i2sout3",
		"apll12_div_i2sout4",
		"apll12_div_i2sout6",
		"apll12_div_f2s",
		"apll12_div_tdm",
		"apll12_div_tdb",

		/* infracfg_ao */
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
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"adsppll",
		"tvdpll",
		"apll1",
		"apll2",
		"mpll",
		"emipll",
		"imgpll",

		/* pericfg_ao */
		"peraop_uart0",
		"peraop_uart1",
		"peraop_uart2",
		"peraop_uart3",
		"peraop_pwm_hclk",
		"peraop_pwm_bclk",
		"peraop_pwm_fbclk1",
		"peraop_pwm_fbclk2",
		"peraop_pwm_fbclk3",
		"peraop_pwm_fbclk4",
		"peraop_disp_pwm0",
		"peraop_disp_pwm1",
		"peraop_spi0_bclk",
		"peraop_spi1_bclk",
		"peraop_spi2_bclk",
		"peraop_spi3_bclk",
		"peraop_spi4_bclk",
		"peraop_spi5_bclk",
		"peraop_spi6_bclk",
		"peraop_spi7_bclk",
		"peraop_dma_bclk",
		"peraop_ssusb0_frmcnt",
		"peraop_msdc1",
		"peraop_msdc1_fclk",
		"peraop_msdc1_hclk",
		"peraop_msdc2",
		"peraop_msdc2_fclk",
		"peraop_msdc2_hclk",
		"peraop_audio_slv_ck",
		"peraop_audio_mst_ck",
		"peraop_aud_intbusck",

		/* afe */
		"afe_dl1_dac_tml",
		"afe_dl1_dac_hires",
		"afe_dl1_dac",
		"afe_dl1_predis",
		"afe_dl1_nle",
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
		"afe_dmic1_adc_tml",
		"afe_dmic1_adc_hires",
		"afe_dmic1_tml",
		"afe_dmic1_adc",
		"afe_dmic0_adc_tml",
		"afe_dmic0_adc_hires",
		"afe_dmic0_tml",
		"afe_dmic0_adc",
		"afe_ul1_adc_tml",
		"afe_ul1_adc_hires",
		"afe_ul1_tml",
		"afe_ul1_adc",
		"afe_ul0_adc_tml",
		"afe_ul0_adc_hires",
		"afe_ul0_tml",
		"afe_ul0_adc",
		"afe_dprx_ck",
		"afe_dptx_ck",
		"afe_etdm_in6",
		"afe_etdm_in5",
		"afe_etdm_in4",
		"afe_etdm_in3",
		"afe_etdm_in2",
		"afe_etdm_in1",
		"afe_etdm_in0",
		"afe_etdm_out6",
		"afe_etdm_out5",
		"afe_etdm_out4",
		"afe_etdm_out3",
		"afe_etdm_out2",
		"afe_etdm_out1",
		"afe_etdm_out0",
		"afe_general24_asrc",
		"afe_general23_asrc",
		"afe_general22_asrc",
		"afe_general21_asrc",
		"afe_general20_asrc",
		"afe_general19_asrc",
		"afe_general18_asrc",
		"afe_general17_asrc",
		"afe_general16_asrc",
		"afe_general15_asrc",
		"afe_general14_asrc",
		"afe_general13_asrc",
		"afe_general12_asrc",
		"afe_general11_asrc",
		"afe_general10_asrc",
		"afe_general9_asrc",
		"afe_general8_asrc",
		"afe_general7_asrc",
		"afe_general6_asrc",
		"afe_general5_asrc",
		"afe_general4_asrc",
		"afe_general3_asrc",
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

		/* imp_iic_wrap_c */
		"impc_ap_clock",

		/* ufscfg_ao */
		"ufs_uni_tx_symbolclk",
		"ufscfg_ao_ufx_rx0",
		"ufscfg_ao_ufx_rx1",
		"ufs_uni_sysclk",

		/* ufscfg_pdn */
		"ufscfg_ufshci_u_clk",
		"ufscfg_ufshci_ck",

		/* pextpcfg_ao */
		"pextpcfg_ao_mac_rclk",
		"pextpcfg_ao_mac_pclk",
		"pextpcfg_ao_m_tck",
		"pextpcfg_ao_m_axck",
		"pextpcfg_ao_m_ahck",
		"pextpcfg_ao_m_pck",

		/* imp_iic_wrap_en */
		"impen_api2c2",
		"impen_api2c4",
		"impen_api2c10",
		"impen_api2c11",

		/* imp_iic_wrap_es */
		"impes_api2c8",
		"impes_api2c9",
		"impes_api2c12",
		"impes_api2c13",

		/* imp_iic_wrap_s */
		"imps_api2c0",
		"imps_api2c1",
		"imps_api2c7",

		/* imp_iic_wrap_n */
		"impn_api2c3",
		"impn_api2c5",

		/* mfgpll_pll_ctrl */
		"mfg-ao-mfgpll",

		/* mfgscpll_pll_ctrl */
		"mfgsc-ao-mfgscpll",

		/* dispsys0_config */
		"dispsys0_disp_cfg",
		"dispsys0_disp_mutex0",
		"dispsys0_disp_aal0",
		"dispsys0_disp_c3d0",
		"dispsys0_disp_ccorr0",
		"dispsys0_disp_ccorr1",
		"dispsys0_disp_chist0",
		"dispsys0_disp_chist1",
		"dispsys0_disp_color0",
		"dispsys0_di0",
		"dispsys0_di1",
		"dispsys0_dli0",
		"dispsys0_dli1",
		"dispsys0_dli2",
		"dispsys0_dli3",
		"dispsys0_dli4",
		"dispsys0_dli5",
		"dispsys0_dlo0",
		"dispsys0_dlo1",
		"dispsys0_dp_intclk",
		"dispsys0_dscw0",
		"dispsys0_clk0",
		"dispsys0_disp_gamma0",
		"dispsys0_mdp_aal0",
		"dispsys0_mdp_rdma0",
		"dispsys0_disp_merge0",
		"dispsys0_disp_merge1",
		"dispsys0_disp_oddmr0",
		"dispsys0_palign0",
		"dispsys0_pmask0",
		"dispsys0_disp_relay0",
		"dispsys0_disp_rsz0",
		"dispsys0_disp_spr0",
		"dispsys0_disp_tdshp0",
		"dispsys0_disp_tdshp1",
		"dispsys0_wdma1",
		"dispsys0_disp_vdcm0",
		"dispsys0_disp_wdma1",
		"dispsys0_smi_comm0",
		"dispsys0_disp_y2r0",
		"dispsys0_disp_ccorr2",
		"dispsys0_disp_ccorr3",
		"dispsys0_disp_gamma1",
		"dispsys0_dsi_clk",
		"dispsys0_dp_clk",
		"dispsys0_26m_clk",

		/* dispsys1_config */
		"dispsys1_disp_cfg",
		"dispsys1_disp_mutex0",
		"dispsys1_disp_aal0",
		"dispsys1_disp_c3d0",
		"dispsys1_disp_ccorr0",
		"dispsys1_disp_ccorr1",
		"dispsys1_disp_chist0",
		"dispsys1_disp_chist1",
		"dispsys1_disp_color0",
		"dispsys1_di0",
		"dispsys1_di1",
		"dispsys1_dli0",
		"dispsys1_dli1",
		"dispsys1_dli2",
		"dispsys1_dli3",
		"dispsys1_dli4",
		"dispsys1_dli5",
		"dispsys1_dlo0",
		"dispsys1_dlo1",
		"dispsys1_dp_intclk",
		"dispsys1_dscw0",
		"dispsys1_clk0",
		"dispsys1_disp_gamma0",
		"dispsys1_mdp_aal0",
		"dispsys1_mdp_rdma0",
		"dispsys1_disp_merge0",
		"dispsys1_disp_merge1",
		"dispsys1_disp_oddmr0",
		"dispsys1_palign0",
		"dispsys1_pmask0",
		"dispsys1_disp_relay0",
		"dispsys1_disp_rsz0",
		"dispsys1_disp_spr0",
		"dispsys1_disp_tdshp0",
		"dispsys1_disp_tdshp1",
		"dispsys1_wdma1",
		"dispsys1_disp_vdcm0",
		"dispsys1_disp_wdma1",
		"dispsys1_smi_comm0",
		"dispsys1_disp_y2r0",
		"dispsys1_disp_ccorr2",
		"dispsys1_disp_ccorr3",
		"dispsys1_disp_gamma1",
		"dispsys1_dsi_clk",
		"dispsys1_dp_clk",
		"dispsys1_26m_clk",

		/* ovlsys0_config */
		"ovlsys0_ovl_config",
		"ovlsys0_ovl_fake_e0",
		"ovlsys0_ovl_fake_e1",
		"ovlsys0_ovl_mutex0",
		"ovlsys0_disp_ovl0_2l",
		"ovlsys0_disp_ovl1_2l",
		"ovlsys0_disp_ovl2_2l",
		"ovlsys0_disp_ovl3_2l",
		"ovlsys0_ovl_rsz1",
		"ovlsys0_ovl_mdp",
		"ovlsys0_ovl_wdma0",
		"ovlsys0_wdma0",
		"ovlsys0_ovl_wdma2",
		"ovlsys0_dli0",
		"ovlsys0_dli1",
		"ovlsys0_dli2",
		"ovlsys0_dlo0",
		"ovlsys0_dlo1",
		"ovlsys0_dlo2",
		"ovlsys0_dlo3",
		"ovlsys0_dlo4",
		"ovlsys0_dlo5",
		"ovlsys0_dlo6",
		"ovlsys0_ovl_irot",
		"ovlsys0_cg0_smi_com0",
		"ovlsys0_ovl_y2r0",
		"ovlsys0_ovl_y2r1",

		/* ovlsys1_config */
		"ovlsys1_ovl_config",
		"ovlsys1_ovl_fake_e0",
		"ovlsys1_ovl_fake_e1",
		"ovlsys1_ovl_mutex0",
		"ovlsys1_disp_ovl0_2l",
		"ovlsys1_disp_ovl1_2l",
		"ovlsys1_disp_ovl2_2l",
		"ovlsys1_disp_ovl3_2l",
		"ovlsys1_ovl_rsz1",
		"ovlsys1_ovl_mdp",
		"ovlsys1_ovl_wdma0",
		"ovlsys1_wdma0",
		"ovlsys1_ovl_wdma2",
		"ovlsys1_dli0",
		"ovlsys1_dli1",
		"ovlsys1_dli2",
		"ovlsys1_dlo0",
		"ovlsys1_dlo1",
		"ovlsys1_dlo2",
		"ovlsys1_dlo3",
		"ovlsys1_dlo4",
		"ovlsys1_dlo5",
		"ovlsys1_dlo6",
		"ovlsys1_ovl_irot",
		"ovlsys1_cg0_smi_com0",
		"ovlsys1_ovl_y2r0",
		"ovlsys1_ovl_y2r1",

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
		"img_adl_larb",
		"img_adlrd",
		"img_avs",
		"img_ips",
		"img_sub_common0",
		"img_sub_common1",
		"img_sub_common2",
		"img_sub_common3",
		"img_sub_common4",
		"img_gals_rx_dip0",
		"img_gals_rx_dip1",
		"img_gals_rx_traw0",
		"img_gals_rx_wpe0",
		"img_gals_rx_wpe1",
		"img_gals_rx_wpe2",
		"img_gals_rx_ipe0",
		"img_gals_tx_ipe0",
		"img_gals_rx_ipe1",
		"img_gals_tx_ipe1",
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

		/* wpe3_dip1 */
		"wpe3_dip1_larb11",
		"wpe3_dip1_wpe",
		"wpe3_dip1_gals0",

		/* traw_dip1 */
		"traw_dip1_larb28",
		"traw_dip1_larb40",
		"traw_dip1_traw",
		"traw_dip1_gals",

		/* img_vcore_d1a */
		"imgv_imgv_g_disp_ck",
		"imgv_imgv_main_ck",
		"imgv_imgv_sub0_ck",
		"imgv_imgv_sub1_ck",

		/* vdec_soc_gcon_base */
		"vde1_lat_cken",
		"vde1_lat_active",
		"vde1_lat_cken_eng",
		"vde1_vdec_cken",
		"vde1_vdec_active",
		"vde1_vdec_cken_eng",

		/* vdec_gcon_base */
		"vde2_lat_cken",
		"vde2_lat_active",
		"vde2_lat_cken_eng",
		"vde2_vdec_cken",
		"vde2_vdec_active",
		"vde2_vdec_cken_eng",

		/* venc_gcon */
		"ven1_cke0_larb",
		"ven1_cke1_venc",
		"ven1_cke2_jpgenc",
		"ven1_cke3_jpgdec",
		"ven1_cke4_jpgdec_c1",
		"ven1_cke5_gals",
		"ven1_cke6_gals_sram",

		/* venc_gcon_core1 */
		"ven2_cke0_larb",
		"ven2_cke1_venc",
		"ven2_cke2_jpgenc",
		"ven2_cke3_jpgdec",
		"ven2_cke4_jpgdec_c1",
		"ven2_cke5_gals",
		"ven2_cke6_gals_sram",

		/* vlp_cksys */
		"vlp_scp_sel",
		"vlp_scp_spi_sel",
		"vlp_scp_iic_sel",
		"vlp_scp_spi_hspd_sel",
		"vlp_scp_iic_hspd_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_spmi_m_sel",
		"vlp_spmi_p_sel",
		"vlp_dvfsrc_sel",
		"vlp_pwm_vlp_sel",
		"vlp_axi_vlp_sel",
		"vlp_systimer_26m_sel",
		"vlp_sspm_sel",
		"vlp_srck_sel",
		"vlp_sramrc_sel",
		"vlp_camtg_sel",
		"vlp_ips_sel",
		"vlp_26m_sspm_sel",
		"vlp_ulposc_sspm_sel",
		"vlp_ccusys_sel",
		"vlp_ccutm_sel",

		/* scp */
		"scp_set_spi0",
		"scp_set_spi1",
		"scp_set_spi2",
		"scp_set_spi3",

		/* scp_iic */
		"scp_iic_api2c1",
		"scp_iic_api2c2",
		"scp_iic_api2c3",
		"scp_iic_api2c4",
		"scp_iic_api2c5",
		"scp_iic_api2c6",

		/* scp_fast_iic */
		"scp_fast_iic_api2c0",

		/* camsys_main */
		"cam_m_larb13_ck",
		"cam_m_larb14_ck",
		"cam_m_larb27_ck",
		"cam_m_larb29_ck",
		"cam_m_cam_ck",
		"cam_m_cam_suba_ck",
		"cam_m_cam_subb_ck",
		"cam_m_cam_subc_ck",
		"cam_m_cam_mraw_ck",
		"cam_m_camtg_ck",
		"cam_m_seninf_ck",
		"cam_m_camsv_ck",
		"cam_m_adlrd_ck",
		"cam_m_adlwr_ck",
		"cam_m_uisp_ck",
		"cam_m_fake_eng_ck",
		"cam_m_cam2mm0_gcon_0",
		"cam_m_cam2mm1_gcon_0",
		"cam_m_cam2sys_gcon_0",
		"cam_m_cam2mm2_gcon_0",
		"cam_m_ccusys_ck",
		"cam_m_ips_ck",
		"cam_m_cam_dpe_ck",
		"cam_m_cam_asg_ck",
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
		"cam_ra_yuv2mm_gals",

		/* camsys_yuva */
		"cam_ya_larbx",
		"cam_ya_cam",
		"cam_ya_camtg",

		/* camsys_rawb */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",
		"cam_rb_raw2mm_gals",
		"cam_rb_yuv2mm_gals",

		/* camsys_yuvb */
		"cam_yb_larbx",
		"cam_yb_cam",
		"cam_yb_camtg",

		/* camsys_rawc */
		"cam_rc_larbx",
		"cam_rc_cam",
		"cam_rc_camtg",
		"cam_rc_raw2mm_gals",
		"cam_rc_yuv2mm_gals",

		/* camsys_yuvc */
		"cam_yc_larbx",
		"cam_yc_cam",
		"cam_yc_camtg",

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

		/* camsys_ipe */
		"camsys_ipe_larb19",
		"camsys_ipe_dpe",
		"camsys_ipe_fus",
		"camsys_ipe_dhze",
		"camsys_ipe_gals",

		/* ccu_main */
		"ccu_larb30_con",
		"ccu_ahb_con",
		"ccusys_ccu0_con",
		"ccusys_ccu1_con",
		"ccu2mm0_gcon",

		/* cam_vcore */
		"camv_subc_dis",

		/* mminfra_config */
		"mminfra_gce_d",
		"mminfra_gce_m",
		"mminfra_smi",
		"mminfra_gce_26m",

		/* mdpsys0_config */
		"mdp0_mdp_mutex0",
		"mdp0_apb_bus",
		"mdp0_smi0",
		"mdp0_mdp_rdma0",
		"mdp0_mdp_rdma2",
		"mdp0_mdp_hdr0",
		"mdp0_mdp_aal0",
		"mdp0_mdp_rsz0",
		"mdp0_mdp_tdshp0",
		"mdp0_mdp_color0",
		"mdp0_mdp_wrot0",
		"mdp0_mdp_fake_eng0",
		"mdp0_mdp_dli_async0",
		"mdp0_mdp_dli_async1",
		"mdp0_mdpsys_config",
		"mdp0_mdp_rdma1",
		"mdp0_mdp_rdma3",
		"mdp0_mdp_hdr1",
		"mdp0_mdp_aal1",
		"mdp0_mdp_rsz1",
		"mdp0_mdp_tdshp1",
		"mdp0_mdp_color1",
		"mdp0_mdp_wrot1",
		"mdp0_mdp_fg0",
		"mdp0_mdp_rsz2",
		"mdp0_mdp_wrot2",
		"mdp0_mdp_dlo_async0",
		"mdp0_mdp_fg1",
		"mdp0_mdp_rsz3",
		"mdp0_mdp_wrot3",
		"mdp0_mdp_dlo_async1",
		"mdp0_mdp_dli_async2",
		"mdp0_mdp_dli_async3",
		"mdp0_mdp_dlo_async2",
		"mdp0_mdp_dlo_async3",
		"mdp0_mdp_birsz0",
		"mdp0_mdp_birsz1",
		"mdp0_img_dl_async0",
		"mdp0_img_dl_async1",
		"mdp0_hre_mdpsys",

		/* mdpsys1_config */
		"mdp1_mdp_mutex0",
		"mdp1_apb_bus",
		"mdp1_smi0",
		"mdp1_mdp_rdma0",
		"mdp1_mdp_rdma2",
		"mdp1_mdp_hdr0",
		"mdp1_mdp_aal0",
		"mdp1_mdp_rsz0",
		"mdp1_mdp_tdshp0",
		"mdp1_mdp_color0",
		"mdp1_mdp_wrot0",
		"mdp1_mdp_fake_eng0",
		"mdp1_mdp_dli_async0",
		"mdp1_mdp_dli_async1",
		"mdp1_mdpsys_config",
		"mdp1_mdp_rdma1",
		"mdp1_mdp_rdma3",
		"mdp1_mdp_hdr1",
		"mdp1_mdp_aal1",
		"mdp1_mdp_rsz1",
		"mdp1_mdp_tdshp1",
		"mdp1_mdp_color1",
		"mdp1_mdp_wrot1",
		"mdp1_mdp_fg0",
		"mdp1_mdp_rsz2",
		"mdp1_mdp_wrot2",
		"mdp1_mdp_dlo_async0",
		"mdp1_mdp_fg1",
		"mdp1_mdp_rsz3",
		"mdp1_mdp_wrot3",
		"mdp1_mdp_dlo_async1",
		"mdp1_mdp_dli_async2",
		"mdp1_mdp_dli_async3",
		"mdp1_mdp_dlo_async2",
		"mdp1_mdp_dlo_async3",
		"mdp1_mdp_birsz0",
		"mdp1_mdp_birsz1",
		"mdp1_img_dl_async0",
		"mdp1_img_dl_async1",
		"mdp1_hre_mdpsys",

		/* ccipll_pll_ctrl */
		"ccipll-pll-ctpll",

		/* armpll_ll_pll_ctrl */
		"armpll-ll-pll-ct-ll",

		/* armpll_bl_pll_ctrl */
		"armpll-bl-pll-ct-bl",

		/* armpll_b_pll_ctrl */
		"armpll-b-pll-ct-b",

		/* ptppll_pll_ctrl */
		"ptppll-pll-ctpll",


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

static struct clkdbg_ops clkdbg_mt6897_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6897_all_clk_names,
};

static int clk_dbg_mt6897_probe(struct platform_device *pdev)
{
	set_clkdbg_ops(&clkdbg_mt6897_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6897_drv = {
	.probe = clk_dbg_mt6897_probe,
	.driver = {
		.name = "clk-dbg-mt6897",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6897_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6897_drv, "clk-dbg-mt6897");
}

static void __exit clkdbg_mt6897_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6897_drv);
}

subsys_initcall(clkdbg_mt6897_init);
module_exit(clkdbg_mt6897_exit);
MODULE_LICENSE("GPL");
