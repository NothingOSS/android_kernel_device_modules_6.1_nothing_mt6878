// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Benjamin Chao <benjamin.chao@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <dt-bindings/power/mt6897-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6897.h"
#include "clk-fmeter.h"
#include "clk-mt6897-fmeter.h"

#define TAG				"[pdchk] "
#define BUG_ON_CHK_ENABLE		0
#define EVT_LEN				40
#define PWR_ID_SHIFT			0
#define PWR_STA_SHIFT			8
#define HWV_INT_MTCMOS_TRIGGER		0x0008

#define HWV_IRQ_STATUS			0x0500

static DEFINE_SPINLOCK(pwr_trace_lock);
static unsigned int pwr_event[EVT_LEN];
static unsigned int evt_cnt, suspend_cnt;

static void trace_power_event(unsigned int id, unsigned int pwr_sta)
{
	unsigned long flags = 0;

	if (id >= MT6897_CHK_PD_NUM)
		return;

	spin_lock_irqsave(&pwr_trace_lock, flags);

	pwr_event[evt_cnt] = (id << PWR_ID_SHIFT) | (pwr_sta << PWR_STA_SHIFT);
	evt_cnt++;
	if (evt_cnt >= EVT_LEN)
		evt_cnt = 0;

	spin_unlock_irqrestore(&pwr_trace_lock, flags);
}

static void dump_power_event(void)
{
	unsigned long flags = 0;
	int i;

	spin_lock_irqsave(&pwr_trace_lock, flags);

	pr_notice("first idx: %d\n", evt_cnt);
	for (i = 0; i < EVT_LEN; i += 5)
		pr_notice("pwr_evt[%d] = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			  i, pwr_event[i], pwr_event[i + 1], pwr_event[i + 2],
			  pwr_event[i + 3], pwr_event[i + 4]);

	spin_unlock_irqrestore(&pwr_trace_lock, flags);
}

/*
 * The clk names in Mediatek CCF.
 */

/* afe */
struct pd_check_swcg afe_swcgs[] = {
	SWCG("afe_dl1_dac_tml"),
	SWCG("afe_dl1_dac_hires"),
	SWCG("afe_dl1_dac"),
	SWCG("afe_dl1_predis"),
	SWCG("afe_dl1_nle"),
	SWCG("afe_dl0_dac_tml"),
	SWCG("afe_dl0_dac_hires"),
	SWCG("afe_dl0_dac"),
	SWCG("afe_dl0_predis"),
	SWCG("afe_dl0_nle"),
	SWCG("afe_pcm1"),
	SWCG("afe_pcm0"),
	SWCG("afe_cm1"),
	SWCG("afe_cm0"),
	SWCG("afe_stf"),
	SWCG("afe_hw_gain23"),
	SWCG("afe_hw_gain01"),
	SWCG("afe_fm_i2s"),
	SWCG("afe_dmic1_adc_tml"),
	SWCG("afe_dmic1_adc_hires"),
	SWCG("afe_dmic1_tml"),
	SWCG("afe_dmic1_adc"),
	SWCG("afe_dmic0_adc_tml"),
	SWCG("afe_dmic0_adc_hires"),
	SWCG("afe_dmic0_tml"),
	SWCG("afe_dmic0_adc"),
	SWCG("afe_ul1_adc_tml"),
	SWCG("afe_ul1_adc_hires"),
	SWCG("afe_ul1_tml"),
	SWCG("afe_ul1_adc"),
	SWCG("afe_ul0_adc_tml"),
	SWCG("afe_ul0_adc_hires"),
	SWCG("afe_ul0_tml"),
	SWCG("afe_ul0_adc"),
	SWCG("afe_dprx_ck"),
	SWCG("afe_dptx_ck"),
	SWCG("afe_etdm_in6"),
	SWCG("afe_etdm_in5"),
	SWCG("afe_etdm_in4"),
	SWCG("afe_etdm_in3"),
	SWCG("afe_etdm_in2"),
	SWCG("afe_etdm_in1"),
	SWCG("afe_etdm_in0"),
	SWCG("afe_etdm_out6"),
	SWCG("afe_etdm_out5"),
	SWCG("afe_etdm_out4"),
	SWCG("afe_etdm_out3"),
	SWCG("afe_etdm_out2"),
	SWCG("afe_etdm_out1"),
	SWCG("afe_etdm_out0"),
	SWCG("afe_general24_asrc"),
	SWCG("afe_general23_asrc"),
	SWCG("afe_general22_asrc"),
	SWCG("afe_general21_asrc"),
	SWCG("afe_general20_asrc"),
	SWCG("afe_general19_asrc"),
	SWCG("afe_general18_asrc"),
	SWCG("afe_general17_asrc"),
	SWCG("afe_general16_asrc"),
	SWCG("afe_general15_asrc"),
	SWCG("afe_general14_asrc"),
	SWCG("afe_general13_asrc"),
	SWCG("afe_general12_asrc"),
	SWCG("afe_general11_asrc"),
	SWCG("afe_general10_asrc"),
	SWCG("afe_general9_asrc"),
	SWCG("afe_general8_asrc"),
	SWCG("afe_general7_asrc"),
	SWCG("afe_general6_asrc"),
	SWCG("afe_general5_asrc"),
	SWCG("afe_general4_asrc"),
	SWCG("afe_general3_asrc"),
	SWCG("afe_general2_asrc"),
	SWCG("afe_general1_asrc"),
	SWCG("afe_general0_asrc"),
	SWCG("afe_connsys_i2s_asrc"),
	SWCG("afe_audio_hopping_ck"),
	SWCG("afe_audio_f26m_ck"),
	SWCG("afe_apll1_ck"),
	SWCG("afe_apll2_ck"),
	SWCG("afe_h208m_ck"),
	SWCG("afe_apll_tuner2"),
	SWCG("afe_apll_tuner1"),
	SWCG(NULL),
};
/* dispsys0_config */
struct pd_check_swcg dispsys0_config_swcgs[] = {
	SWCG("dispsys0_disp_cfg"),
	SWCG("dispsys0_disp_mutex0"),
	SWCG("dispsys0_disp_aal0"),
	SWCG("dispsys0_disp_c3d0"),
	SWCG("dispsys0_disp_ccorr0"),
	SWCG("dispsys0_disp_ccorr1"),
	SWCG("dispsys0_disp_chist0"),
	SWCG("dispsys0_disp_chist1"),
	SWCG("dispsys0_disp_color0"),
	SWCG("dispsys0_di0"),
	SWCG("dispsys0_di1"),
	SWCG("dispsys0_dli0"),
	SWCG("dispsys0_dli1"),
	SWCG("dispsys0_dli2"),
	SWCG("dispsys0_dli3"),
	SWCG("dispsys0_dli4"),
	SWCG("dispsys0_dli5"),
	SWCG("dispsys0_dlo0"),
	SWCG("dispsys0_dlo1"),
	SWCG("dispsys0_dp_intclk"),
	SWCG("dispsys0_dscw0"),
	SWCG("dispsys0_clk0"),
	SWCG("dispsys0_disp_gamma0"),
	SWCG("dispsys0_mdp_aal0"),
	SWCG("dispsys0_mdp_rdma0"),
	SWCG("dispsys0_disp_merge0"),
	SWCG("dispsys0_disp_merge1"),
	SWCG("dispsys0_disp_oddmr0"),
	SWCG("dispsys0_palign0"),
	SWCG("dispsys0_pmask0"),
	SWCG("dispsys0_disp_relay0"),
	SWCG("dispsys0_disp_rsz0"),
	SWCG("dispsys0_disp_spr0"),
	SWCG("dispsys0_disp_tdshp0"),
	SWCG("dispsys0_disp_tdshp1"),
	SWCG("dispsys0_wdma1"),
	SWCG("dispsys0_disp_vdcm0"),
	SWCG("dispsys0_disp_wdma1"),
	SWCG("dispsys0_smi_comm0"),
	SWCG("dispsys0_disp_y2r0"),
	SWCG("dispsys0_disp_ccorr2"),
	SWCG("dispsys0_disp_ccorr3"),
	SWCG("dispsys0_disp_gamma1"),
	SWCG("dispsys0_dsi_clk"),
	SWCG("dispsys0_dp_clk"),
	SWCG("dispsys0_26m_clk"),
	SWCG(NULL),
};
/* dispsys1_config */
struct pd_check_swcg dispsys1_config_swcgs[] = {
	SWCG("dispsys1_disp_cfg"),
	SWCG("dispsys1_disp_mutex0"),
	SWCG("dispsys1_disp_aal0"),
	SWCG("dispsys1_disp_c3d0"),
	SWCG("dispsys1_disp_ccorr0"),
	SWCG("dispsys1_disp_ccorr1"),
	SWCG("dispsys1_disp_chist0"),
	SWCG("dispsys1_disp_chist1"),
	SWCG("dispsys1_disp_color0"),
	SWCG("dispsys1_di0"),
	SWCG("dispsys1_di1"),
	SWCG("dispsys1_dli0"),
	SWCG("dispsys1_dli1"),
	SWCG("dispsys1_dli2"),
	SWCG("dispsys1_dli3"),
	SWCG("dispsys1_dli4"),
	SWCG("dispsys1_dli5"),
	SWCG("dispsys1_dlo0"),
	SWCG("dispsys1_dlo1"),
	SWCG("dispsys1_dp_intclk"),
	SWCG("dispsys1_dscw0"),
	SWCG("dispsys1_clk0"),
	SWCG("dispsys1_disp_gamma0"),
	SWCG("dispsys1_mdp_aal0"),
	SWCG("dispsys1_mdp_rdma0"),
	SWCG("dispsys1_disp_merge0"),
	SWCG("dispsys1_disp_merge1"),
	SWCG("dispsys1_disp_oddmr0"),
	SWCG("dispsys1_palign0"),
	SWCG("dispsys1_pmask0"),
	SWCG("dispsys1_disp_relay0"),
	SWCG("dispsys1_disp_rsz0"),
	SWCG("dispsys1_disp_spr0"),
	SWCG("dispsys1_disp_tdshp0"),
	SWCG("dispsys1_disp_tdshp1"),
	SWCG("dispsys1_wdma1"),
	SWCG("dispsys1_disp_vdcm0"),
	SWCG("dispsys1_disp_wdma1"),
	SWCG("dispsys1_smi_comm0"),
	SWCG("dispsys1_disp_y2r0"),
	SWCG("dispsys1_disp_ccorr2"),
	SWCG("dispsys1_disp_ccorr3"),
	SWCG("dispsys1_disp_gamma1"),
	SWCG("dispsys1_dsi_clk"),
	SWCG("dispsys1_dp_clk"),
	SWCG("dispsys1_26m_clk"),
	SWCG(NULL),
};
/* ovlsys0_config */
struct pd_check_swcg ovlsys0_config_swcgs[] = {
	SWCG("ovlsys0_ovl_config"),
	SWCG("ovlsys0_ovl_fake_e0"),
	SWCG("ovlsys0_ovl_fake_e1"),
	SWCG("ovlsys0_ovl_mutex0"),
	SWCG("ovlsys0_disp_ovl0_2l"),
	SWCG("ovlsys0_disp_ovl1_2l"),
	SWCG("ovlsys0_disp_ovl2_2l"),
	SWCG("ovlsys0_disp_ovl3_2l"),
	SWCG("ovlsys0_ovl_rsz1"),
	SWCG("ovlsys0_ovl_mdp"),
	SWCG("ovlsys0_ovl_wdma0"),
	SWCG("ovlsys0_wdma0"),
	SWCG("ovlsys0_ovl_wdma2"),
	SWCG("ovlsys0_dli0"),
	SWCG("ovlsys0_dli1"),
	SWCG("ovlsys0_dli2"),
	SWCG("ovlsys0_dlo0"),
	SWCG("ovlsys0_dlo1"),
	SWCG("ovlsys0_dlo2"),
	SWCG("ovlsys0_dlo3"),
	SWCG("ovlsys0_dlo4"),
	SWCG("ovlsys0_dlo5"),
	SWCG("ovlsys0_dlo6"),
	SWCG("ovlsys0_ovl_irot"),
	SWCG("ovlsys0_cg0_smi_com0"),
	SWCG("ovlsys0_ovl_y2r0"),
	SWCG("ovlsys0_ovl_y2r1"),
	SWCG(NULL),
};
/* ovlsys1_config */
struct pd_check_swcg ovlsys1_config_swcgs[] = {
	SWCG("ovlsys1_ovl_config"),
	SWCG("ovlsys1_ovl_fake_e0"),
	SWCG("ovlsys1_ovl_fake_e1"),
	SWCG("ovlsys1_ovl_mutex0"),
	SWCG("ovlsys1_disp_ovl0_2l"),
	SWCG("ovlsys1_disp_ovl1_2l"),
	SWCG("ovlsys1_disp_ovl2_2l"),
	SWCG("ovlsys1_disp_ovl3_2l"),
	SWCG("ovlsys1_ovl_rsz1"),
	SWCG("ovlsys1_ovl_mdp"),
	SWCG("ovlsys1_ovl_wdma0"),
	SWCG("ovlsys1_wdma0"),
	SWCG("ovlsys1_ovl_wdma2"),
	SWCG("ovlsys1_dli0"),
	SWCG("ovlsys1_dli1"),
	SWCG("ovlsys1_dli2"),
	SWCG("ovlsys1_dlo0"),
	SWCG("ovlsys1_dlo1"),
	SWCG("ovlsys1_dlo2"),
	SWCG("ovlsys1_dlo3"),
	SWCG("ovlsys1_dlo4"),
	SWCG("ovlsys1_dlo5"),
	SWCG("ovlsys1_dlo6"),
	SWCG("ovlsys1_ovl_irot"),
	SWCG("ovlsys1_cg0_smi_com0"),
	SWCG("ovlsys1_ovl_y2r0"),
	SWCG("ovlsys1_ovl_y2r1"),
	SWCG(NULL),
};
/* imgsys_main */
struct pd_check_swcg imgsys_main_swcgs[] = {
	SWCG("img_fdvt"),
	SWCG("img_me"),
	SWCG("img_mmg"),
	SWCG("img_larb12"),
	SWCG("img_larb9"),
	SWCG("img_traw0"),
	SWCG("img_traw1"),
	SWCG("img_dip0"),
	SWCG("img_wpe0"),
	SWCG("img_ipe"),
	SWCG("img_wpe1"),
	SWCG("img_wpe2"),
	SWCG("img_adl_larb"),
	SWCG("img_adlrd"),
	SWCG("img_avs"),
	SWCG("img_ips"),
	SWCG("img_sub_common0"),
	SWCG("img_sub_common1"),
	SWCG("img_sub_common2"),
	SWCG("img_sub_common3"),
	SWCG("img_sub_common4"),
	SWCG("img_gals_rx_dip0"),
	SWCG("img_gals_rx_dip1"),
	SWCG("img_gals_rx_traw0"),
	SWCG("img_gals_rx_wpe0"),
	SWCG("img_gals_rx_wpe1"),
	SWCG("img_gals_rx_wpe2"),
	SWCG("img_gals_rx_ipe0"),
	SWCG("img_gals_tx_ipe0"),
	SWCG("img_gals_rx_ipe1"),
	SWCG("img_gals_tx_ipe1"),
	SWCG("img_gals"),
	SWCG(NULL),
};
/* dip_top_dip1 */
struct pd_check_swcg dip_top_dip1_swcgs[] = {
	SWCG("dip_dip1_dip_top"),
	SWCG("dip_dip1_dip_gals0"),
	SWCG("dip_dip1_dip_gals1"),
	SWCG("dip_dip1_dip_gals2"),
	SWCG("dip_dip1_dip_gals3"),
	SWCG("dip_dip1_larb10"),
	SWCG("dip_dip1_larb15"),
	SWCG("dip_dip1_larb38"),
	SWCG("dip_dip1_larb39"),
	SWCG(NULL),
};
/* dip_nr1_dip1 */
struct pd_check_swcg dip_nr1_dip1_swcgs[] = {
	SWCG("dip_nr1_dip1_larb"),
	SWCG("dip_nr1_dip1_dip_nr1"),
	SWCG(NULL),
};
/* dip_nr2_dip1 */
struct pd_check_swcg dip_nr2_dip1_swcgs[] = {
	SWCG("dip_nr2_dip1_larb15"),
	SWCG("dip_nr2_dip1_dip_nr"),
	SWCG(NULL),
};
/* wpe1_dip1 */
struct pd_check_swcg wpe1_dip1_swcgs[] = {
	SWCG("wpe1_dip1_larb11"),
	SWCG("wpe1_dip1_wpe"),
	SWCG("wpe1_dip1_gals0"),
	SWCG(NULL),
};
/* wpe2_dip1 */
struct pd_check_swcg wpe2_dip1_swcgs[] = {
	SWCG("wpe2_dip1_larb11"),
	SWCG("wpe2_dip1_wpe"),
	SWCG("wpe2_dip1_gals0"),
	SWCG(NULL),
};
/* wpe3_dip1 */
struct pd_check_swcg wpe3_dip1_swcgs[] = {
	SWCG("wpe3_dip1_larb11"),
	SWCG("wpe3_dip1_wpe"),
	SWCG("wpe3_dip1_gals0"),
	SWCG(NULL),
};
/* traw_dip1 */
struct pd_check_swcg traw_dip1_swcgs[] = {
	SWCG("traw_dip1_larb28"),
	SWCG("traw_dip1_larb40"),
	SWCG("traw_dip1_traw"),
	SWCG("traw_dip1_gals"),
	SWCG(NULL),
};
/* img_vcore_d1a */
struct pd_check_swcg img_vcore_d1a_swcgs[] = {
	SWCG("imgv_imgv_g_disp_ck"),
	SWCG("imgv_imgv_main_ck"),
	SWCG("imgv_imgv_sub0_ck"),
	SWCG("imgv_imgv_sub1_ck"),
	SWCG(NULL),
};
/* vdec_soc_gcon_base */
struct pd_check_swcg vdec_soc_gcon_base_swcgs[] = {
	SWCG("vde1_lat_cken"),
	SWCG("vde1_lat_active"),
	SWCG("vde1_lat_cken_eng"),
	SWCG("vde1_vdec_cken"),
	SWCG("vde1_vdec_active"),
	SWCG("vde1_vdec_cken_eng"),
	SWCG(NULL),
};
/* vdec_gcon_base */
struct pd_check_swcg vdec_gcon_base_swcgs[] = {
	SWCG("vde2_lat_cken"),
	SWCG("vde2_lat_active"),
	SWCG("vde2_lat_cken_eng"),
	SWCG("vde2_vdec_cken"),
	SWCG("vde2_vdec_active"),
	SWCG("vde2_vdec_cken_eng"),
	SWCG(NULL),
};
/* venc_gcon */
struct pd_check_swcg venc_gcon_swcgs[] = {
	SWCG("ven1_cke0_larb"),
	SWCG("ven1_cke1_venc"),
	SWCG("ven1_cke2_jpgenc"),
	SWCG("ven1_cke3_jpgdec"),
	SWCG("ven1_cke4_jpgdec_c1"),
	SWCG("ven1_cke5_gals"),
	SWCG("ven1_cke6_gals_sram"),
	SWCG(NULL),
};
/* venc_gcon_core1 */
struct pd_check_swcg venc_gcon_core1_swcgs[] = {
	SWCG("ven2_cke0_larb"),
	SWCG("ven2_cke1_venc"),
	SWCG("ven2_cke2_jpgenc"),
	SWCG("ven2_cke3_jpgdec"),
	SWCG("ven2_cke4_jpgdec_c1"),
	SWCG("ven2_cke5_gals"),
	SWCG("ven2_cke6_gals_sram"),
	SWCG(NULL),
};
/* camsys_main */
struct pd_check_swcg camsys_main_swcgs[] = {
	SWCG("cam_m_larb13_ck"),
	SWCG("cam_m_larb14_ck"),
	SWCG("cam_m_larb27_ck"),
	SWCG("cam_m_larb29_ck"),
	SWCG("cam_m_cam_ck"),
	SWCG("cam_m_cam_suba_ck"),
	SWCG("cam_m_cam_subb_ck"),
	SWCG("cam_m_cam_subc_ck"),
	SWCG("cam_m_cam_mraw_ck"),
	SWCG("cam_m_camtg_ck"),
	SWCG("cam_m_seninf_ck"),
	SWCG("cam_m_camsv_ck"),
	SWCG("cam_m_adlrd_ck"),
	SWCG("cam_m_adlwr_ck"),
	SWCG("cam_m_uisp_ck"),
	SWCG("cam_m_fake_eng_ck"),
	SWCG("cam_m_cam2mm0_gcon_0"),
	SWCG("cam_m_cam2mm1_gcon_0"),
	SWCG("cam_m_cam2sys_gcon_0"),
	SWCG("cam_m_cam2mm2_gcon_0"),
	SWCG("cam_m_ccusys_ck"),
	SWCG("cam_m_ips_ck"),
	SWCG("cam_m_cam_dpe_ck"),
	SWCG("cam_m_cam_asg_ck"),
	SWCG("cam_m_camsv_a_con_1"),
	SWCG("cam_m_camsv_b_con_1"),
	SWCG("cam_m_camsv_c_con_1"),
	SWCG("cam_m_camsv_d_con_1"),
	SWCG("cam_m_camsv_e_con_1"),
	SWCG("cam_m_camsv_con_1"),
	SWCG(NULL),
};
/* camsys_rawa */
struct pd_check_swcg camsys_rawa_swcgs[] = {
	SWCG("cam_ra_larbx"),
	SWCG("cam_ra_cam"),
	SWCG("cam_ra_camtg"),
	SWCG("cam_ra_raw2mm_gals"),
	SWCG("cam_ra_yuv2mm_gals"),
	SWCG(NULL),
};
/* camsys_yuva */
struct pd_check_swcg camsys_yuva_swcgs[] = {
	SWCG("cam_ya_larbx"),
	SWCG("cam_ya_cam"),
	SWCG("cam_ya_camtg"),
	SWCG(NULL),
};
/* camsys_rawb */
struct pd_check_swcg camsys_rawb_swcgs[] = {
	SWCG("cam_rb_larbx"),
	SWCG("cam_rb_cam"),
	SWCG("cam_rb_camtg"),
	SWCG("cam_rb_raw2mm_gals"),
	SWCG("cam_rb_yuv2mm_gals"),
	SWCG(NULL),
};
/* camsys_yuvb */
struct pd_check_swcg camsys_yuvb_swcgs[] = {
	SWCG("cam_yb_larbx"),
	SWCG("cam_yb_cam"),
	SWCG("cam_yb_camtg"),
	SWCG(NULL),
};
/* camsys_rawc */
struct pd_check_swcg camsys_rawc_swcgs[] = {
	SWCG("cam_rc_larbx"),
	SWCG("cam_rc_cam"),
	SWCG("cam_rc_camtg"),
	SWCG("cam_rc_raw2mm_gals"),
	SWCG("cam_rc_yuv2mm_gals"),
	SWCG(NULL),
};
/* camsys_yuvc */
struct pd_check_swcg camsys_yuvc_swcgs[] = {
	SWCG("cam_yc_larbx"),
	SWCG("cam_yc_cam"),
	SWCG("cam_yc_camtg"),
	SWCG(NULL),
};
/* camsys_mraw */
struct pd_check_swcg camsys_mraw_swcgs[] = {
	SWCG("cam_mr_larbx"),
	SWCG("cam_mr_gals"),
	SWCG("cam_mr_camtg"),
	SWCG("cam_mr_mraw0"),
	SWCG("cam_mr_mraw1"),
	SWCG("cam_mr_mraw2"),
	SWCG("cam_mr_mraw3"),
	SWCG("cam_mr_pda0"),
	SWCG("cam_mr_pda1"),
	SWCG(NULL),
};
/* camsys_ipe */
struct pd_check_swcg camsys_ipe_swcgs[] = {
	SWCG("camsys_ipe_larb19"),
	SWCG("camsys_ipe_dpe"),
	SWCG("camsys_ipe_fus"),
	SWCG("camsys_ipe_dhze"),
	SWCG("camsys_ipe_gals"),
	SWCG(NULL),
};
/* ccu_main */
struct pd_check_swcg ccu_main_swcgs[] = {
	SWCG("ccu_larb30_con"),
	SWCG("ccu_ahb_con"),
	SWCG("ccusys_ccu0_con"),
	SWCG("ccusys_ccu1_con"),
	SWCG("ccu2mm0_gcon"),
	SWCG(NULL),
};
/* cam_vcore */
struct pd_check_swcg cam_vcore_swcgs[] = {
	SWCG("camv_subc_dis"),
	SWCG(NULL),
};
/* mminfra_config */
struct pd_check_swcg mminfra_config_swcgs[] = {
	SWCG("mminfra_gce_d"),
	SWCG("mminfra_gce_m"),
	SWCG("mminfra_smi"),
	SWCG("mminfra_gce_26m"),
	SWCG(NULL),
};
/* mdpsys0_config */
struct pd_check_swcg mdpsys0_config_swcgs[] = {
	SWCG("mdp0_mdp_mutex0"),
	SWCG("mdp0_apb_bus"),
	SWCG("mdp0_smi0"),
	SWCG("mdp0_mdp_rdma0"),
	SWCG("mdp0_mdp_rdma2"),
	SWCG("mdp0_mdp_hdr0"),
	SWCG("mdp0_mdp_aal0"),
	SWCG("mdp0_mdp_rsz0"),
	SWCG("mdp0_mdp_tdshp0"),
	SWCG("mdp0_mdp_color0"),
	SWCG("mdp0_mdp_wrot0"),
	SWCG("mdp0_mdp_fake_eng0"),
	SWCG("mdp0_mdp_dli_async0"),
	SWCG("mdp0_mdp_dli_async1"),
	SWCG("mdp0_mdpsys_config"),
	SWCG("mdp0_mdp_rdma1"),
	SWCG("mdp0_mdp_rdma3"),
	SWCG("mdp0_mdp_hdr1"),
	SWCG("mdp0_mdp_aal1"),
	SWCG("mdp0_mdp_rsz1"),
	SWCG("mdp0_mdp_tdshp1"),
	SWCG("mdp0_mdp_color1"),
	SWCG("mdp0_mdp_wrot1"),
	SWCG("mdp0_mdp_fg0"),
	SWCG("mdp0_mdp_rsz2"),
	SWCG("mdp0_mdp_wrot2"),
	SWCG("mdp0_mdp_dlo_async0"),
	SWCG("mdp0_mdp_fg1"),
	SWCG("mdp0_mdp_rsz3"),
	SWCG("mdp0_mdp_wrot3"),
	SWCG("mdp0_mdp_dlo_async1"),
	SWCG("mdp0_mdp_dli_async2"),
	SWCG("mdp0_mdp_dli_async3"),
	SWCG("mdp0_mdp_dlo_async2"),
	SWCG("mdp0_mdp_dlo_async3"),
	SWCG("mdp0_mdp_birsz0"),
	SWCG("mdp0_mdp_birsz1"),
	SWCG("mdp0_img_dl_async0"),
	SWCG("mdp0_img_dl_async1"),
	SWCG("mdp0_hre_mdpsys"),
	SWCG(NULL),
};
/* mdpsys1_config */
struct pd_check_swcg mdpsys1_config_swcgs[] = {
	SWCG("mdp1_mdp_mutex0"),
	SWCG("mdp1_apb_bus"),
	SWCG("mdp1_smi0"),
	SWCG("mdp1_mdp_rdma0"),
	SWCG("mdp1_mdp_rdma2"),
	SWCG("mdp1_mdp_hdr0"),
	SWCG("mdp1_mdp_aal0"),
	SWCG("mdp1_mdp_rsz0"),
	SWCG("mdp1_mdp_tdshp0"),
	SWCG("mdp1_mdp_color0"),
	SWCG("mdp1_mdp_wrot0"),
	SWCG("mdp1_mdp_fake_eng0"),
	SWCG("mdp1_mdp_dli_async0"),
	SWCG("mdp1_mdp_dli_async1"),
	SWCG("mdp1_mdpsys_config"),
	SWCG("mdp1_mdp_rdma1"),
	SWCG("mdp1_mdp_rdma3"),
	SWCG("mdp1_mdp_hdr1"),
	SWCG("mdp1_mdp_aal1"),
	SWCG("mdp1_mdp_rsz1"),
	SWCG("mdp1_mdp_tdshp1"),
	SWCG("mdp1_mdp_color1"),
	SWCG("mdp1_mdp_wrot1"),
	SWCG("mdp1_mdp_fg0"),
	SWCG("mdp1_mdp_rsz2"),
	SWCG("mdp1_mdp_wrot2"),
	SWCG("mdp1_mdp_dlo_async0"),
	SWCG("mdp1_mdp_fg1"),
	SWCG("mdp1_mdp_rsz3"),
	SWCG("mdp1_mdp_wrot3"),
	SWCG("mdp1_mdp_dlo_async1"),
	SWCG("mdp1_mdp_dli_async2"),
	SWCG("mdp1_mdp_dli_async3"),
	SWCG("mdp1_mdp_dlo_async2"),
	SWCG("mdp1_mdp_dlo_async3"),
	SWCG("mdp1_mdp_birsz0"),
	SWCG("mdp1_mdp_birsz1"),
	SWCG("mdp1_img_dl_async0"),
	SWCG("mdp1_img_dl_async1"),
	SWCG("mdp1_hre_mdpsys"),
	SWCG(NULL),
};

struct subsys_cgs_check {
	unsigned int pd_id;		/* power domain id */
	int pd_parent;			/* power domain parent id */
	struct pd_check_swcg *swcgs;	/* those CGs that would be checked */
	enum chk_sys_id chk_id;		/*
					 * chk_id is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};

struct subsys_cgs_check mtk_subsys_check[] = {
	{MT6897_CHK_PD_AUDIO, PD_NULL, afe_swcgs, afe},
	{MT6897_CHK_PD_DIS0, MT6897_CHK_PD_MM_INFRA, dispsys0_config_swcgs, dispsys0_config},
	{MT6897_CHK_PD_DIS1, MT6897_CHK_PD_MM_INFRA, dispsys1_config_swcgs, dispsys1_config},
	{MT6897_CHK_PD_OVL0, MT6897_CHK_PD_MM_INFRA, ovlsys0_config_swcgs, ovlsys0_config},
	{MT6897_CHK_PD_OVL1, MT6897_CHK_PD_MM_INFRA, ovlsys1_config_swcgs, ovlsys1_config},
	{MT6897_CHK_PD_ISP_MAIN, MT6897_CHK_PD_ISP_VCORE, imgsys_main_swcgs, img},
	{MT6897_CHK_PD_ISP_DIP1, MT6897_CHK_PD_ISP_MAIN, dip_top_dip1_swcgs, dip_top_dip1},
	{MT6897_CHK_PD_ISP_DIP1, MT6897_CHK_PD_ISP_MAIN, dip_nr1_dip1_swcgs, dip_nr1_dip1},
	{MT6897_CHK_PD_ISP_DIP1, MT6897_CHK_PD_ISP_MAIN, dip_nr2_dip1_swcgs, dip_nr2_dip1},
	{MT6897_CHK_PD_ISP_DIP1, MT6897_CHK_PD_ISP_MAIN, wpe1_dip1_swcgs, wpe1_dip1},
	{MT6897_CHK_PD_ISP_DIP1, MT6897_CHK_PD_ISP_MAIN, wpe2_dip1_swcgs, wpe2_dip1},
	{MT6897_CHK_PD_ISP_DIP1, MT6897_CHK_PD_ISP_MAIN, wpe3_dip1_swcgs, wpe3_dip1},
	{MT6897_CHK_PD_ISP_DIP1, MT6897_CHK_PD_ISP_MAIN, traw_dip1_swcgs, traw_dip1},
	{MT6897_CHK_PD_ISP_VCORE, MT6897_CHK_PD_MM_INFRA, img_vcore_d1a_swcgs, imgv},
	{MT6897_CHK_PD_VDE0, MT6897_CHK_PD_MM_INFRA, vdec_soc_gcon_base_swcgs, vde1},
	{MT6897_CHK_PD_VDE1, MT6897_CHK_PD_MM_INFRA, vdec_gcon_base_swcgs, vde2},
	{MT6897_CHK_PD_VEN0, MT6897_CHK_PD_MM_INFRA, venc_gcon_swcgs, ven1},
	{MT6897_CHK_PD_VEN1, MT6897_CHK_PD_MM_INFRA, venc_gcon_core1_swcgs, ven2},
	{MT6897_CHK_PD_CAM_MAIN, MT6897_CHK_PD_CAM_VCORE, camsys_main_swcgs, cam_m},
	{MT6897_CHK_PD_CAM_SUBA, MT6897_CHK_PD_CAM_MAIN, camsys_rawa_swcgs, cam_ra},
	{MT6897_CHK_PD_CAM_SUBA, MT6897_CHK_PD_CAM_MAIN, camsys_yuva_swcgs, cam_ya},
	{MT6897_CHK_PD_CAM_SUBB, MT6897_CHK_PD_CAM_MAIN, camsys_rawb_swcgs, cam_rb},
	{MT6897_CHK_PD_CAM_SUBB, MT6897_CHK_PD_CAM_MAIN, camsys_yuvb_swcgs, cam_yb},
	{MT6897_CHK_PD_CAM_SUBC, MT6897_CHK_PD_CAM_MAIN, camsys_rawc_swcgs, cam_rc},
	{MT6897_CHK_PD_CAM_SUBC, MT6897_CHK_PD_CAM_MAIN, camsys_yuvc_swcgs, cam_yc},
	{MT6897_CHK_PD_CAM_MRAW, MT6897_CHK_PD_CAM_MAIN, camsys_mraw_swcgs, cam_mr},
	{MT6897_CHK_PD_CAM_MRAW, MT6897_CHK_PD_CAM_MAIN, camsys_ipe_swcgs, camsys_ipe},
	{MT6897_CHK_PD_CAM_CCU, MT6897_CHK_PD_CAM_VCORE, ccu_main_swcgs, ccu},
	{MT6897_CHK_PD_CAM_VCORE, MT6897_CHK_PD_MM_INFRA, cam_vcore_swcgs, cam_vcore},
	{MT6897_CHK_PD_MM_INFRA, PD_NULL, mminfra_config_swcgs, mminfra_config},
	{MT6897_CHK_PD_MDP0, MT6897_CHK_PD_DIS0, mdpsys0_config_swcgs, mdp0},
	{MT6897_CHK_PD_MDP1, MT6897_CHK_PD_DIS1, mdpsys1_config_swcgs, mdp1},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6897_CHK_PD_NUM)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			return mtk_subsys_check[i].swcgs;
	}

	return NULL;
}

static void dump_subsys_reg(unsigned int id)
{
	int i;

	if (id >= MT6897_CHK_PD_NUM)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6897(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6897_CHK_PD_MFG1,
	MT6897_CHK_PD_MFG2,
	MT6897_CHK_PD_MFG3,
	MT6897_CHK_PD_MFG4,
	MT6897_CHK_PD_MFG6,
	MT6897_CHK_PD_MFG7,
	MT6897_CHK_PD_MFG9,
	MT6897_CHK_PD_MFG10,
	MT6897_CHK_PD_MFG11,
	MT6897_CHK_PD_MFG12,
	MT6897_CHK_PD_MFG13,
	MT6897_CHK_PD_MFG14,
	MT6897_CHK_PD_MD1,
	MT6897_CHK_PD_CONN,
	MT6897_CHK_PD_UFS0,
	MT6897_CHK_PD_UFS0_PHY,
	MT6897_CHK_PD_PEXTP_MAC0,
	MT6897_CHK_PD_PEXTP_PHY0,
	MT6897_CHK_PD_AUDIO,
	MT6897_CHK_PD_ADSP_TOP,
	MT6897_CHK_PD_ADSP_AO,
	MT6897_CHK_PD_ISP_MAIN,
	MT6897_CHK_PD_ISP_DIP1,
	MT6897_CHK_PD_ISP_VCORE,
	MT6897_CHK_PD_VDE0,
	MT6897_CHK_PD_VDE1,
	MT6897_CHK_PD_VEN0,
	MT6897_CHK_PD_VEN1,
	MT6897_CHK_PD_CAM_MAIN,
	MT6897_CHK_PD_CAM_MRAW,
	MT6897_CHK_PD_CAM_SUBA,
	MT6897_CHK_PD_CAM_SUBB,
	MT6897_CHK_PD_CAM_SUBC,
	MT6897_CHK_PD_CAM_VCORE,
	MT6897_CHK_PD_CAM_CCU,
	MT6897_CHK_PD_CAM_CCU_AO,
	MT6897_CHK_PD_MDP0,
	MT6897_CHK_PD_MDP1,
	MT6897_CHK_PD_DIS0,
	MT6897_CHK_PD_DIS1,
	MT6897_CHK_PD_OVL0,
	MT6897_CHK_PD_OVL1,
	MT6897_CHK_PD_MM_INFRA,
	MT6897_CHK_PD_MM_PROC,
	MT6897_CHK_PD_DP_TX,
	MT6897_CHK_PD_CSI_RX,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6897_CHK_PD_NUM)
		return false;

	for (i = 0; i < ARRAY_SIZE(pd_list); i++) {
		if (id == pd_list[i])
			return true;
	}

	return false;
}

static enum chk_sys_id debug_dump_id[] = {
	spm,
	top,
	apmixed,
	ifrbus_ao_reg_bus,
	ufscfg_ao_bus,
	gpu_eb_rpc,
	mfg_ao,
	mfgsc_ao,
	vlpcfg,
	vlp_ck,
	ccipll_pll_ctrl,
	armpll_ll_pll_ctrl,
	armpll_bl_pll_ctrl,
	armpll_b_pll_ctrl,
	ptppll_pll_ctrl,
	chk_sys_num,
};

static void debug_dump(unsigned int id, unsigned int pwr_sta)
{
	int i, parent_id = PD_NULL;

	if (id >= MT6897_CHK_PD_NUM)
		return;

	set_subsys_reg_dump_mt6897(debug_dump_id);

	get_subsys_reg_dump_mt6897();

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id) {
			print_subsys_reg_mt6897(mtk_subsys_check[i].chk_id);
			parent_id = mtk_subsys_check[i].pd_parent;
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (parent_id == PD_NULL)
			break;

		if (mtk_subsys_check[i].pd_id == parent_id)
			print_subsys_reg_mt6897(mtk_subsys_check[i].chk_id);
	}

	dump_power_event();
	dump_clk_event();

	BUG_ON(1);
}

static enum chk_sys_id log_dump_id[] = {
	ifrbus_ao_reg_bus,
	ufscfg_ao_bus,
	gpu_eb_rpc,
	spm,
	vlpcfg,
	chk_sys_num,
};

static void log_dump(unsigned int id, unsigned int pwr_sta)
{
	if (id >= MT6897_CHK_PD_NUM)
		return;

	if (id == MT6897_CHK_PD_MD1) {
		set_subsys_reg_dump_mt6897(log_dump_id);
		get_subsys_reg_dump_mt6897();
	}
}

static struct pd_sta pd_pwr_sta[] = {
	{MT6897_CHK_PD_MFG1, gpu_eb_rpc, 0x0070, GENMASK(31, 30)},
	{MT6897_CHK_PD_MFG2, gpu_eb_rpc, 0x00A0, GENMASK(31, 30)},
	{MT6897_CHK_PD_MFG3, gpu_eb_rpc, 0x00A4, GENMASK(31, 30)},
	{MT6897_CHK_PD_MFG4, gpu_eb_rpc, 0x00A8, GENMASK(31, 30)},
	{MT6897_CHK_PD_MFG6, gpu_eb_rpc, 0x00B0, GENMASK(31, 30)},
	{MT6897_CHK_PD_MFG7, gpu_eb_rpc, 0x00B4, GENMASK(31, 30)},
	{MT6897_CHK_PD_MFG9, gpu_eb_rpc, 0x00BC, GENMASK(31, 30)},
	{MT6897_CHK_PD_MFG10, gpu_eb_rpc, 0x00C0, GENMASK(31, 30)},
	{MT6897_CHK_PD_MFG11, gpu_eb_rpc, 0x00C4, GENMASK(31, 30)},
	{MT6897_CHK_PD_MFG12, gpu_eb_rpc, 0x00C8, GENMASK(31, 30)},
	{MT6897_CHK_PD_MFG13, gpu_eb_rpc, 0x00CC, GENMASK(31, 30)},
	{MT6897_CHK_PD_MFG14, gpu_eb_rpc, 0x00D0, GENMASK(31, 30)},
	{MT6897_CHK_PD_MD1, spm, 0x0E00, GENMASK(31, 30)},
	{MT6897_CHK_PD_CONN, spm, 0x0E04, GENMASK(31, 30)},
	{MT6897_CHK_PD_UFS0, spm, 0x0E10, GENMASK(31, 30)},
	{MT6897_CHK_PD_UFS0_PHY, spm, 0x0E14, GENMASK(31, 30)},
	{MT6897_CHK_PD_PEXTP_MAC0, spm, 0x0E18, GENMASK(31, 30)},
	{MT6897_CHK_PD_PEXTP_PHY0, spm, 0x0E20, GENMASK(31, 30)},
	{MT6897_CHK_PD_AUDIO, spm, 0x0E2C, GENMASK(31, 30)},
	{MT6897_CHK_PD_ADSP_TOP, spm, 0x0E30, GENMASK(31, 30)},
	{MT6897_CHK_PD_ADSP_AO, spm, 0x0E38, GENMASK(31, 30)},
	{MT6897_CHK_PD_ISP_MAIN, spm, 0x0E3C, GENMASK(31, 30)},
	{MT6897_CHK_PD_ISP_DIP1, spm, 0x0E40, GENMASK(31, 30)},
	{MT6897_CHK_PD_ISP_VCORE, spm, 0x0E48, GENMASK(31, 30)},
	{MT6897_CHK_PD_VDE0, spm, 0x0E4C, GENMASK(31, 30)},
	{MT6897_CHK_PD_VDE1, spm, 0x0E50, GENMASK(31, 30)},
	{MT6897_CHK_PD_VEN0, spm, 0x0E5C, GENMASK(31, 30)},
	{MT6897_CHK_PD_VEN1, spm, 0x0E60, GENMASK(31, 30)},
	{MT6897_CHK_PD_CAM_MAIN, spm, 0x0E68, GENMASK(31, 30)},
	{MT6897_CHK_PD_CAM_MRAW, spm, 0x0E6C, GENMASK(31, 30)},
	{MT6897_CHK_PD_CAM_SUBA, spm, 0x0E70, GENMASK(31, 30)},
	{MT6897_CHK_PD_CAM_SUBB, spm, 0x0E74, GENMASK(31, 30)},
	{MT6897_CHK_PD_CAM_SUBC, spm, 0x0E78, GENMASK(31, 30)},
	{MT6897_CHK_PD_CAM_VCORE, spm, 0x0E84, GENMASK(31, 30)},
	{MT6897_CHK_PD_CAM_CCU, spm, 0x0E88, GENMASK(31, 30)},
	{MT6897_CHK_PD_CAM_CCU_AO, spm, 0x0E8C, GENMASK(31, 30)},
	{MT6897_CHK_PD_MDP0, spm, 0x0E94, GENMASK(31, 30)},
	{MT6897_CHK_PD_MDP1, spm, 0x0E98, GENMASK(31, 30)},
	{MT6897_CHK_PD_DIS0, spm, 0x0E9C, GENMASK(31, 30)},
	{MT6897_CHK_PD_DIS1, spm, 0x0EA0, GENMASK(31, 30)},
	{MT6897_CHK_PD_OVL0, spm, 0x0EA4, GENMASK(31, 30)},
	{MT6897_CHK_PD_OVL1, spm, 0x0EA8, GENMASK(31, 30)},
	{MT6897_CHK_PD_MM_INFRA, spm, 0x0EAC, GENMASK(31, 30)},
	{MT6897_CHK_PD_MM_PROC, spm, 0x0EB0, GENMASK(31, 30)},
	{MT6897_CHK_PD_DP_TX, spm, 0x0EB4, GENMASK(31, 30)},
	{MT6897_CHK_PD_CSI_RX, spm, 0x0EF4, GENMASK(31, 30)},
};

static u32 get_pd_pwr_status(int pd_id)
{
	u32 val;
	int i;

	if (pd_id == PD_NULL || pd_id > ARRAY_SIZE(pd_pwr_sta))
		return 0;

	for (i = 0; i < ARRAY_SIZE(pd_pwr_sta); i++) {
		if (pd_id == pd_pwr_sta[i].pd_id) {
			val = get_mt6897_reg_value(pd_pwr_sta[i].base, pd_pwr_sta[i].ofs);
			if ((val & pd_pwr_sta[i].msk) == pd_pwr_sta[i].msk)
				return 1;
			else
				return 0;
		}
	}

	return 0;
}

static int off_mtcmos_id[] = {
	MT6897_CHK_PD_MFG1,
	MT6897_CHK_PD_MFG2,
	MT6897_CHK_PD_MFG3,
	MT6897_CHK_PD_MFG4,
	MT6897_CHK_PD_MFG6,
	MT6897_CHK_PD_MFG7,
	MT6897_CHK_PD_MFG9,
	MT6897_CHK_PD_MFG10,
	MT6897_CHK_PD_MFG11,
	MT6897_CHK_PD_MFG12,
	MT6897_CHK_PD_MFG13,
	MT6897_CHK_PD_MFG14,
	MT6897_CHK_PD_UFS0,
	MT6897_CHK_PD_PEXTP_MAC0,
	MT6897_CHK_PD_PEXTP_PHY0,
	MT6897_CHK_PD_ADSP_AO,
	MT6897_CHK_PD_ISP_MAIN,
	MT6897_CHK_PD_ISP_DIP1,
	MT6897_CHK_PD_ISP_VCORE,
	MT6897_CHK_PD_VDE0,
	MT6897_CHK_PD_VDE1,
	MT6897_CHK_PD_VEN0,
	MT6897_CHK_PD_VEN1,
	MT6897_CHK_PD_CAM_MAIN,
	MT6897_CHK_PD_CAM_MRAW,
	MT6897_CHK_PD_CAM_SUBA,
	MT6897_CHK_PD_CAM_SUBB,
	MT6897_CHK_PD_CAM_SUBC,
	MT6897_CHK_PD_CAM_VCORE,
	MT6897_CHK_PD_CAM_CCU,
	MT6897_CHK_PD_CAM_CCU_AO,
	MT6897_CHK_PD_MDP0,
	MT6897_CHK_PD_MDP1,
	MT6897_CHK_PD_DIS0,
	MT6897_CHK_PD_DIS1,
	MT6897_CHK_PD_OVL0,
	MT6897_CHK_PD_OVL1,
	MT6897_CHK_PD_MM_INFRA,
	MT6897_CHK_PD_MM_PROC,
	MT6897_CHK_PD_DP_TX,
	MT6897_CHK_PD_CSI_RX,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6897_CHK_PD_MD1,
	MT6897_CHK_PD_CONN,
	MT6897_CHK_PD_AUDIO,
	MT6897_CHK_PD_ADSP_TOP,
	MT6897_CHK_PD_UFS0_PHY,
	PD_NULL,
};

static int *get_off_mtcmos_id(void)
{
	return off_mtcmos_id;
}

static int *get_notice_mtcmos_id(void)
{
	return notice_mtcmos_id;
}

static bool is_mtcmos_chk_bug_on(void)
{
#if (BUG_ON_CHK_ENABLE) || (IS_ENABLED(CONFIG_MTK_CLKMGR_DEBUG))
	return true;
#endif
	return false;
}

static int suspend_allow_id[] = {
	MT6897_CHK_PD_UFS0,
	PD_NULL,
};

static int *get_suspend_allow_id(void)
{
	return suspend_allow_id;
}

static bool pdchk_is_suspend_retry_stop(bool reset_cnt)
{
	if (reset_cnt == true) {
		suspend_cnt = 0;
		return true;
	}

	suspend_cnt++;
	pr_notice("%s: suspend cnt: %d\n", __func__, suspend_cnt);

	if (suspend_cnt < 2)
		return false;

	return true;
}

static void check_hwv_irq_sta(void)
{
	u32 irq_sta;

	irq_sta = get_mt6897_reg_value(hwv_ext, HWV_IRQ_STATUS);

	if ((irq_sta & HWV_INT_MTCMOS_TRIGGER) == HWV_INT_MTCMOS_TRIGGER)
		debug_dump(MT6897_CHK_PD_NUM, 0);
}

/*
 * init functions
 */

static struct pdchk_ops pdchk_mt6897_ops = {
	.get_subsys_cg = get_subsys_cg,
	.dump_subsys_reg = dump_subsys_reg,
	.is_in_pd_list = is_in_pd_list,
	.debug_dump = debug_dump,
	.log_dump = log_dump,
	.get_pd_pwr_status = get_pd_pwr_status,
	.get_off_mtcmos_id = get_off_mtcmos_id,
	.get_notice_mtcmos_id = get_notice_mtcmos_id,
	.is_mtcmos_chk_bug_on = is_mtcmos_chk_bug_on,
	.get_suspend_allow_id = get_suspend_allow_id,
	.trace_power_event = trace_power_event,
	.dump_power_event = dump_power_event,
	.check_hwv_irq_sta = check_hwv_irq_sta,
	.is_suspend_retry_stop = pdchk_is_suspend_retry_stop,
};

static int pd_chk_mt6897_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;

	pdchk_common_init(&pdchk_mt6897_ops);
	pdchk_hwv_irq_init(pdev);

	return 0;
}

static const struct of_device_id of_match_pdchk_mt6897[] = {
{
	.compatible = "mediatek,mt6897-pdchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver pd_chk_mt6897_drv = {
	.probe = pd_chk_mt6897_probe,
	.driver = {
		.name = "pd-chk-mt6897",
		.owner = THIS_MODULE,
		.pm = &pdchk_dev_pm_ops,
		.of_match_table = of_match_pdchk_mt6897,
	},
};

/*
 * init functions
 */

static int __init pd_chk_init(void)
{
	return platform_driver_register(&pd_chk_mt6897_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6897_drv);
}

subsys_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
