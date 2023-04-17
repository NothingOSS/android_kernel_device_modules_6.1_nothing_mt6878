/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Benjamin Chao <benjamin.chao@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6897_H
#define __DRV_CLKCHK_MT6897_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	apmixed = 2,
	ifrbus_ao_reg_bus = 3,
	emi_reg = 4,
	semi_reg = 5,
	nemicfg_ao_mem_reg_bus = 6,
	semicfg_ao_mem_reg_bus = 7,
	perao = 8,
	afe = 9,
	impc = 10,
	ufscfg_ao_bus = 11,
	ufscfg_ao = 12,
	ufscfg_pdn = 13,
	pextpcfg_ao = 14,
	impen = 15,
	impes = 16,
	imps = 17,
	impn = 18,
	gpu_eb_rpc = 19,
	mfg_ao = 20,
	mfgsc_ao = 21,
	dispsys0_config = 22,
	dispsys1_config = 23,
	ovlsys0_config = 24,
	ovlsys1_config = 25,
	img = 26,
	dip_top_dip1 = 27,
	dip_nr1_dip1 = 28,
	dip_nr2_dip1 = 29,
	wpe1_dip1 = 30,
	wpe2_dip1 = 31,
	wpe3_dip1 = 32,
	traw_dip1 = 33,
	imgv = 34,
	vde1 = 35,
	vde2 = 36,
	ven1 = 37,
	ven2 = 38,
	spm = 39,
	vlpcfg = 40,
	vlp_ck = 41,
	scp = 42,
	scp_iic = 43,
	scp_fast_iic = 44,
	cam_m = 45,
	cam_ra = 46,
	cam_ya = 47,
	cam_rb = 48,
	cam_yb = 49,
	cam_rc = 50,
	cam_yc = 51,
	cam_mr = 52,
	camsys_ipe = 53,
	ccu = 54,
	cam_vcore = 55,
	dvfsrc_apb = 56,
	mminfra_config = 57,
	mdp0 = 58,
	mdp1 = 59,
	ccipll_pll_ctrl = 60,
	armpll_ll_pll_ctrl = 61,
	armpll_bl_pll_ctrl = 62,
	armpll_b_pll_ctrl = 63,
	ptppll_pll_ctrl = 64,
	hwv_ext = 65,
	hwv = 66,
	hwv_wrt = 67,
	chk_sys_num = 68,
};

enum chk_pd_id {
	MT6897_CHK_PD_MFG1 = 0,
	MT6897_CHK_PD_MFG2 = 1,
	MT6897_CHK_PD_MFG3 = 2,
	MT6897_CHK_PD_MFG4 = 3,
	MT6897_CHK_PD_MFG6 = 4,
	MT6897_CHK_PD_MFG7 = 5,
	MT6897_CHK_PD_MFG9 = 6,
	MT6897_CHK_PD_MFG10 = 7,
	MT6897_CHK_PD_MFG11 = 8,
	MT6897_CHK_PD_MFG12 = 9,
	MT6897_CHK_PD_MFG13 = 10,
	MT6897_CHK_PD_MFG14 = 11,
	MT6897_CHK_PD_MD1 = 12,
	MT6897_CHK_PD_CONN = 13,
	MT6897_CHK_PD_UFS0 = 14,
	MT6897_CHK_PD_UFS0_PHY = 15,
	MT6897_CHK_PD_PEXTP_MAC0 = 16,
	MT6897_CHK_PD_PEXTP_PHY0 = 17,
	MT6897_CHK_PD_AUDIO = 18,
	MT6897_CHK_PD_ADSP_TOP = 19,
	MT6897_CHK_PD_ADSP_AO = 20,
	MT6897_CHK_PD_CSI_RX = 21,
	MT6897_CHK_PD_MM_INFRA = 22,
	MT6897_CHK_PD_OVL1 = 23,
	MT6897_CHK_PD_DP_TX = 24,
	MT6897_CHK_PD_VDE1 = 25,
	MT6897_CHK_PD_VDE0 = 26,
	MT6897_CHK_PD_CAM_VCORE = 27,
	MT6897_CHK_PD_CAM_MAIN = 28,
	MT6897_CHK_PD_CAM_MRAW = 29,
	MT6897_CHK_PD_CAM_SUBB = 30,
	MT6897_CHK_PD_CAM_SUBC = 31,
	MT6897_CHK_PD_CAM_SUBA = 32,
	MT6897_CHK_PD_CAM_CCU = 33,
	MT6897_CHK_PD_CAM_CCU_AO = 34,
	MT6897_CHK_PD_MM_PROC = 35,
	MT6897_CHK_PD_DIS0 = 36,
	MT6897_CHK_PD_MDP0 = 37,
	MT6897_CHK_PD_DIS1 = 38,
	MT6897_CHK_PD_MDP1 = 39,
	MT6897_CHK_PD_VEN1 = 40,
	MT6897_CHK_PD_VEN0 = 41,
	MT6897_CHK_PD_ISP_VCORE = 42,
	MT6897_CHK_PD_ISP_MAIN = 43,
	MT6897_CHK_PD_ISP_DIP1 = 44,
	MT6897_CHK_PD_OVL0 = 45,
	MT6897_CHK_PD_APU = 46,
	MT6897_CHK_PD_NUM,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6897(enum chk_sys_id id);
extern void set_subsys_reg_dump_mt6897(enum chk_sys_id id[]);
extern void get_subsys_reg_dump_mt6897(void);
extern u32 get_mt6897_reg_value(u32 id, u32 ofs);
extern void release_mt6897_hwv_secure(void);
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
extern void dump_clk_event(void);
#endif

#endif	/* __DRV_CLKCHK_MT6897_H */
