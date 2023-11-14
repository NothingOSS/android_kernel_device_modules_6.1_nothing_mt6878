// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chuan-wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <dt-bindings/power/mt6878-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6878.h"
#include "clk-fmeter.h"
#include "clk-mt6878-fmeter.h"

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

	if (id >= MT6878_CHK_PD_NUM)
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
				i,
				pwr_event[i],
				pwr_event[i + 1],
				pwr_event[i + 2],
				pwr_event[i + 3],
				pwr_event[i + 4]);

	spin_unlock_irqrestore(&pwr_trace_lock, flags);
}

/*
 * The clk names in Mediatek CCF.
 */

/* ssr_top */
struct pd_check_swcg ssr_top_swcgs[] = {
	SWCG("ssr_rw_ssr_rng"),
	SWCG("ssr_rq_rw_ssr_dma"),
	SWCG("ssr_rq_rw_ssr_kdf"),
	SWCG("ssr_rq_rw_ssr_pka"),
	SWCG("ssr_rw_ssr_apb"),
	SWCG("ssr_rw_sej_b_test"),
	SWCG("ssr_rw_aes_b_test"),
	SWCG("ssr_rw_sej_f13m_test"),
	SWCG(NULL),
};
/* afe */
struct pd_check_swcg afe_swcgs[] = {
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
	SWCG("afe_mtkaifv4"),
	SWCG("afe_ul1_aht"),
	SWCG("afe_ul1_adc_hires"),
	SWCG("afe_ul1_tml"),
	SWCG("afe_ul1_adc"),
	SWCG("afe_ul0_aht"),
	SWCG("afe_ul0_adc_hires"),
	SWCG("afe_ul0_tml"),
	SWCG("afe_ul0_adc"),
	SWCG("afe_etdm_in4"),
	SWCG("afe_etdm_in2"),
	SWCG("afe_etdm_in1"),
	SWCG("afe_etdm_out4"),
	SWCG("afe_etdm_out2"),
	SWCG("afe_etdm_out1"),
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
/* dispsys_config */
struct pd_check_swcg dispsys_config_swcgs[] = {
	SWCG("mm_disp_ovl0_2l"),
	SWCG("mm_disp_ovl1_2l"),
	SWCG("mm_disp_ovl2_2l"),
	SWCG("mm_disp_ovl3_2l"),
	SWCG("mm_disp_ufbc_wdma0"),
	SWCG("mm_disp_rsz1"),
	SWCG("mm_disp_rsz0"),
	SWCG("mm_disp_tdshp0"),
	SWCG("mm_disp_c3d0"),
	SWCG("mm_disp_color0"),
	SWCG("mm_disp_ccorr0"),
	SWCG("mm_disp_ccorr1"),
	SWCG("mm_disp_aal0"),
	SWCG("mm_disp_gamma0"),
	SWCG("mm_disp_postmask0"),
	SWCG("mm_disp_dither0"),
	SWCG("mm_disp_tdshp1"),
	SWCG("mm_disp_c3d1"),
	SWCG("mm_disp_ccorr2"),
	SWCG("mm_disp_ccorr3"),
	SWCG("mm_disp_gamma1"),
	SWCG("mm_disp_dither1"),
	SWCG("mm_disp_splitter0"),
	SWCG("mm_disp_dsc_wrap0"),
	SWCG("mm_CLK0"),
	SWCG("mm_CLK1"),
	SWCG("mm_disp_wdma1"),
	SWCG("mm_disp_apb_bus"),
	SWCG("mm_disp_fake_eng0"),
	SWCG("mm_disp_fake_eng1"),
	SWCG("mm_disp_mutex0"),
	SWCG("mm_smi_common"),
	SWCG("mm_dsi0_ck"),
	SWCG("mm_dsi1_ck"),
	SWCG("mm_26m_ck"),
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
	SWCG("img_sub_common0"),
	SWCG("img_sub_common1"),
	SWCG("img_sub_common3"),
	SWCG("img_sub_common4"),
	SWCG("img_gals_rx_dip0"),
	SWCG("img_gals_rx_dip1"),
	SWCG("img_gals_rx_traw0"),
	SWCG("img_gals_rx_wpe0"),
	SWCG("img_gals_rx_wpe1"),
	SWCG("img_gals_rx_ipe0"),
	SWCG("img_gals_tx_ipe0"),
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
	SWCG("img_vcore_gals_disp"),
	SWCG("img_vcore_main"),
	SWCG("img_vcore_sub0"),
	SWCG("img_vcore_sub1"),
	SWCG(NULL),
};
/* vdec_gcon_base */
struct pd_check_swcg vdec_gcon_base_swcgs[] = {
	SWCG("vde2_larb_cken"),
	SWCG("vde2_vdec_cken"),
	SWCG("vde2_vdec_active"),
	SWCG(NULL),
};
/* venc_gcon */
struct pd_check_swcg venc_gcon_swcgs[] = {
	SWCG("ven1_larb"),
	SWCG("ven1_venc"),
	SWCG("ven1_jpgenc"),
	SWCG("ven1_gals"),
	SWCG(NULL),
};
/* camsys_main */
struct pd_check_swcg camsys_main_swcgs[] = {
	SWCG("cam_m_larb13"),
	SWCG("cam_m_larb14"),
	SWCG("cam_m_larb29"),
	SWCG("cam_m_cam"),
	SWCG("cam_m_cam_suba"),
	SWCG("cam_m_cam_subb"),
	SWCG("cam_m_cam_mraw"),
	SWCG("cam_m_camtg"),
	SWCG("cam_m_seninf"),
	SWCG("cam_m_camsv"),
	SWCG("cam_m_cam2mm0_GCON_0"),
	SWCG("cam_m_cam2mm1_GCON_0"),
	SWCG("cam_m_ccusys"),
	SWCG("cam_m_cam_asg"),
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
	SWCG("cam_ra_yuv2raw2mm"),
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
	SWCG("cam_rb_yuv2raw2mm"),
	SWCG(NULL),
};
/* camsys_yuvb */
struct pd_check_swcg camsys_yuvb_swcgs[] = {
	SWCG("cam_yb_larbx"),
	SWCG("cam_yb_cam"),
	SWCG("cam_yb_camtg"),
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
/* ccu_main */
struct pd_check_swcg ccu_main_swcgs[] = {
	SWCG("ccu_larb30_con"),
	SWCG("ccu_ahb_con"),
	SWCG("ccusys_ccu0_con"),
	SWCG("ccu2mm0_GCON"),
	SWCG(NULL),
};
/* cam_vcore */
struct pd_check_swcg cam_vcore_swcgs[] = {
	SWCG("cam_vcore_c2mm0_dis"),
	SWCG("cam_vcore_mm0_dis"),
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
/* mdpsys_config */
struct pd_check_swcg mdpsys_config_swcgs[] = {
	SWCG("mdp_mutex0"),
	SWCG("mdp_apb_bus"),
	SWCG("mdp_smi0"),
	SWCG("mdp_rdma0"),
	SWCG("mdp_hdr0"),
	SWCG("mdp_aal0"),
	SWCG("mdp_rsz0"),
	SWCG("mdp_tdshp0"),
	SWCG("mdp_wrot0"),
	SWCG("mdp_rdma1"),
	SWCG("mdp_rsz1"),
	SWCG("mdp_wrot1"),
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
	{MT6878_CHK_PD_SSRSYS, PD_NULL, ssr_top_swcgs, ssr_top},
	{MT6878_CHK_PD_AUDIO, PD_NULL, afe_swcgs, afe},
	{MT6878_CHK_PD_DIS0, MT6878_CHK_PD_MM_INFRA, dispsys_config_swcgs, mm},
	{MT6878_CHK_PD_ISP_MAIN, MT6878_CHK_PD_ISP_VCORE, imgsys_main_swcgs, img},
	{MT6878_CHK_PD_ISP_DIP1, MT6878_CHK_PD_ISP_MAIN, dip_top_dip1_swcgs, dip_top_dip1},
	{MT6878_CHK_PD_ISP_DIP1, MT6878_CHK_PD_ISP_MAIN, dip_nr1_dip1_swcgs, dip_nr1_dip1},
	{MT6878_CHK_PD_ISP_DIP1, MT6878_CHK_PD_ISP_MAIN, dip_nr2_dip1_swcgs, dip_nr2_dip1},
	{MT6878_CHK_PD_ISP_DIP1, MT6878_CHK_PD_ISP_MAIN, wpe1_dip1_swcgs, wpe1_dip1},
	{MT6878_CHK_PD_ISP_DIP1, MT6878_CHK_PD_ISP_MAIN, wpe2_dip1_swcgs, wpe2_dip1},
	{MT6878_CHK_PD_ISP_DIP1, MT6878_CHK_PD_ISP_MAIN, traw_dip1_swcgs, traw_dip1},
	{MT6878_CHK_PD_ISP_MAIN, MT6878_CHK_PD_ISP_VCORE, img_vcore_d1a_swcgs, img_v},
	{MT6878_CHK_PD_VDE0, MT6878_CHK_PD_MM_INFRA, vdec_gcon_base_swcgs, vde2},
	{MT6878_CHK_PD_VEN0, MT6878_CHK_PD_MM_INFRA, venc_gcon_swcgs, ven1},
	{MT6878_CHK_PD_CAM_MAIN, MT6878_CHK_PD_CAM_VCORE, camsys_main_swcgs, cam_m},
	{MT6878_CHK_PD_CAM_SUBA, MT6878_CHK_PD_CAM_MAIN, camsys_rawa_swcgs, cam_ra},
	{MT6878_CHK_PD_CAM_SUBA, MT6878_CHK_PD_CAM_MAIN, camsys_yuva_swcgs, cam_ya},
	{MT6878_CHK_PD_CAM_SUBB, MT6878_CHK_PD_CAM_MAIN, camsys_rawb_swcgs, cam_rb},
	{MT6878_CHK_PD_CAM_SUBB, MT6878_CHK_PD_CAM_MAIN, camsys_yuvb_swcgs, cam_yb},
	{MT6878_CHK_PD_CAM_MAIN, MT6878_CHK_PD_CAM_VCORE, camsys_mraw_swcgs, cam_mr},
	{MT6878_CHK_PD_CAM_CCU_AO, MT6878_CHK_PD_CAM_CCU, ccu_main_swcgs, ccu},
	{MT6878_CHK_PD_CAM_VCORE, MT6878_CHK_PD_MM_INFRA, cam_vcore_swcgs, cam_vcore},
	{MT6878_CHK_PD_MM_INFRA, PD_NULL, mminfra_config_swcgs, mminfra_config},
	{MT6878_CHK_PD_DIS0, MT6878_CHK_PD_MM_INFRA, mdpsys_config_swcgs, mdp},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6878_CHK_PD_NUM)
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

	if (id >= MT6878_CHK_PD_NUM)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6878(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6878_CHK_PD_MD1,
	MT6878_CHK_PD_CONN,
	MT6878_CHK_PD_AUDIO,
	MT6878_CHK_PD_ISP_MAIN,
	MT6878_CHK_PD_ISP_DIP1,
	MT6878_CHK_PD_ISP_VCORE,
	MT6878_CHK_PD_VDE0,
	MT6878_CHK_PD_VEN0,
	MT6878_CHK_PD_CAM_MAIN,
	MT6878_CHK_PD_CAM_SUBA,
	MT6878_CHK_PD_CAM_SUBB,
	MT6878_CHK_PD_CAM_VCORE,
	MT6878_CHK_PD_CAM_CCU,
	MT6878_CHK_PD_CAM_CCU_AO,
	MT6878_CHK_PD_DIS0,
	MT6878_CHK_PD_MM_INFRA,
	MT6878_CHK_PD_MM_PROC,
	MT6878_CHK_PD_CSI_RX,
	MT6878_CHK_PD_SSRSYS,
	MT6878_CHK_PD_SSUSB,
	MT6878_CHK_PD_MFG0,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6878_CHK_PD_NUM)
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
	mfg_ao,
	mfgsc_ao,
	vlp,
	hwv,
	hwv_ext,
	chk_sys_num,
};

static void debug_dump(unsigned int id, unsigned int pwr_sta)
{
	const struct fmeter_clk *fclks;
	int i, parent_id = PD_NULL;

	if (id >= MT6878_CHK_PD_NUM)
		return;

	fclks = mt_get_fmeter_clks();

	set_subsys_reg_dump_mt6878(debug_dump_id);

	get_subsys_reg_dump_mt6878();

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id) {
			print_subsys_reg_mt6878(mtk_subsys_check[i].chk_id);
			parent_id = mtk_subsys_check[i].pd_parent;
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (parent_id == PD_NULL)
			break;

		if (mtk_subsys_check[i].pd_id == parent_id)
			print_subsys_reg_mt6878(mtk_subsys_check[i].chk_id);
	}

	dump_power_event();

	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}

	mdelay(5000);
	BUG_ON(1);
}

static enum chk_sys_id log_dump_id[] = {
	spm,
	chk_sys_num,
};

static void log_dump(unsigned int id, unsigned int pwr_sta)
{
	if (id >= MT6878_CHK_PD_NUM)
		return;

	if (id == MT6878_CHK_PD_MD1) {
		set_subsys_reg_dump_mt6878(log_dump_id);
		get_subsys_reg_dump_mt6878();
	}
}

static void external_dump(void)
{
	const struct fmeter_clk *fclks;

	fclks = mt_get_fmeter_clks();

	set_subsys_reg_dump_mt6878(debug_dump_id);

	get_subsys_reg_dump_mt6878();

	dump_power_event();

	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}
}

static struct pd_sta pd_pwr_sta[] = {
	{MT6878_CHK_PD_MD1, spm, 0x0E00, GENMASK(31, 30)},
	{MT6878_CHK_PD_CONN, spm, 0x0E04, GENMASK(31, 30)},
	{MT6878_CHK_PD_AUDIO, spm, 0x0E18, GENMASK(31, 30)},
	{MT6878_CHK_PD_ISP_MAIN, spm, 0x0E28, GENMASK(31, 30)},
	{MT6878_CHK_PD_ISP_DIP1, spm, 0x0E2C, GENMASK(31, 30)},
	{MT6878_CHK_PD_ISP_VCORE, spm, 0x0E34, GENMASK(31, 30)},
	{MT6878_CHK_PD_VDE0, spm, 0x0E38, GENMASK(31, 30)},
	{MT6878_CHK_PD_VEN0, spm, 0x0E40, GENMASK(31, 30)},
	{MT6878_CHK_PD_CAM_MAIN, spm, 0x0E48, GENMASK(31, 30)},
	{MT6878_CHK_PD_CAM_SUBA, spm, 0x0E50, GENMASK(31, 30)},
	{MT6878_CHK_PD_CAM_SUBB, spm, 0x0E54, GENMASK(31, 30)},
	{MT6878_CHK_PD_CAM_VCORE, spm, 0x0E5C, GENMASK(31, 30)},
	{MT6878_CHK_PD_CAM_CCU, spm, 0x0E60, GENMASK(31, 30)},
	{MT6878_CHK_PD_CAM_CCU_AO, spm, 0x0E64, GENMASK(31, 30)},
	{MT6878_CHK_PD_DIS0, spm, 0x0E70, GENMASK(31, 30)},
	{MT6878_CHK_PD_MM_INFRA, spm, 0x0E78, GENMASK(31, 30)},
	{MT6878_CHK_PD_MM_PROC, spm, 0x0E7C, GENMASK(31, 30)},
	{MT6878_CHK_PD_CSI_RX, spm, 0x0E9C, GENMASK(31, 30)},
	{MT6878_CHK_PD_SSRSYS, spm, 0x0EA0, GENMASK(31, 30)},
	{MT6878_CHK_PD_SSUSB, spm, 0x0EA8, GENMASK(31, 30)},
	{MT6878_CHK_PD_MFG0, spm, 0x0EB4, GENMASK(31, 30)},
};

static u32 get_pd_pwr_status(int pd_id)
{
	u32 val;
	int i;

	if (pd_id == PD_NULL || pd_id > ARRAY_SIZE(pd_pwr_sta))
		return 0;

	for (i = 0; i < ARRAY_SIZE(pd_pwr_sta); i++) {
		if (pd_id == pd_pwr_sta[i].pd_id) {
			val = get_mt6878_reg_value(pd_pwr_sta[i].base, pd_pwr_sta[i].ofs);
			if ((val & pd_pwr_sta[i].msk) == pd_pwr_sta[i].msk)
				return 1;
			else
				return 0;
		}
	}

	return 0;
}

static int off_mtcmos_id[] = {
	MT6878_CHK_PD_ISP_MAIN,
	MT6878_CHK_PD_ISP_DIP1,
	MT6878_CHK_PD_ISP_VCORE,
	MT6878_CHK_PD_VDE0,
	MT6878_CHK_PD_VEN0,
	MT6878_CHK_PD_CAM_MAIN,
	MT6878_CHK_PD_CAM_SUBA,
	MT6878_CHK_PD_CAM_SUBB,
	MT6878_CHK_PD_CAM_VCORE,
	MT6878_CHK_PD_CAM_CCU,
	MT6878_CHK_PD_CAM_CCU_AO,
	MT6878_CHK_PD_DIS0,
	MT6878_CHK_PD_MM_INFRA,
	MT6878_CHK_PD_MM_PROC,
	MT6878_CHK_PD_CSI_RX,
	MT6878_CHK_PD_SSRSYS,
	MT6878_CHK_PD_MFG0,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6878_CHK_PD_MD1,
	MT6878_CHK_PD_CONN,
	MT6878_CHK_PD_AUDIO,
	MT6878_CHK_PD_SSUSB,
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

	irq_sta = get_mt6878_reg_value(hwv_ext, HWV_IRQ_STATUS);

	if ((irq_sta & HWV_INT_MTCMOS_TRIGGER) == HWV_INT_MTCMOS_TRIGGER)
		debug_dump(MT6878_CHK_PD_NUM, 0);
}

/*
 * init functions
 */

static struct pdchk_ops pdchk_mt6878_ops = {
	.get_subsys_cg = get_subsys_cg,
	.dump_subsys_reg = dump_subsys_reg,
	.is_in_pd_list = is_in_pd_list,
	.debug_dump = debug_dump,
	.log_dump = log_dump,
	.external_dump = external_dump,
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

static int pd_chk_mt6878_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;

	pdchk_common_init(&pdchk_mt6878_ops);
	pdchk_hwv_irq_init(pdev);

	return 0;
}

static const struct of_device_id of_match_pdchk_mt6878[] = {
	{
		.compatible = "mediatek,mt6878-pdchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver pd_chk_mt6878_drv = {
	.probe = pd_chk_mt6878_probe,
	.driver = {
		.name = "pd-chk-mt6878",
		.owner = THIS_MODULE,
		.pm = &pdchk_dev_pm_ops,
		.of_match_table = of_match_pdchk_mt6878,
	},
};

/*
 * init functions
 */

static int __init pd_chk_init(void)
{
	return platform_driver_register(&pd_chk_mt6878_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6878_drv);
}

subsys_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
