/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chuan-wen Chen <chuan-wen.chen@mediatek.com>
 */

#ifndef __DRV_CLKCHK_MT6878_H
#define __DRV_CLKCHK_MT6878_H

enum chk_sys_id {
	top = 0,
	ifrao = 1,
	apmixed = 2,
	ifr = 3,
	emi_reg = 4,
	nemicfg_ao_mem_reg_bus = 5,
	ssr_top = 6,
	perao = 7,
	afe = 8,
	im_c_s = 9,
	ufsao = 10,
	ufspdn = 11,
	imp_e_s = 12,
	imp_es_s = 13,
	imp_w_s = 14,
	mfg_ao = 15,
	gpueb_ao = 16,
	mfgsc_ao = 17,
	mm = 18,
	img = 19,
	dip_top_dip1 = 20,
	dip_nr1_dip1 = 21,
	dip_nr2_dip1 = 22,
	wpe1_dip1 = 23,
	wpe2_dip1 = 24,
	traw_dip1 = 25,
	img_v = 26,
	vde2 = 27,
	ven1 = 28,
	spm = 29,
	vlpcfg_reg = 30,
	vlp = 31,
	scp = 32,
	hfrp = 33,
	cam_m = 34,
	cam_ra = 35,
	cam_ya = 36,
	cam_rb = 37,
	cam_yb = 38,
	cam_mr = 39,
	ccu = 40,
	cam_vcore = 41,
	dvfsrc_top = 42,
	mminfra_config = 43,
	mdp = 44,
	hwv = 45,
	hwv_ext = 46,
	hwv_wrt = 47,
	chk_sys_num = 48,
};

enum chk_pd_id {
	MT6878_CHK_PD_MD1 = 0,
	MT6878_CHK_PD_CONN = 1,
	MT6878_CHK_PD_AUDIO = 2,
	MT6878_CHK_PD_CSI_RX = 3,
	MT6878_CHK_PD_SSRSYS = 4,
	MT6878_CHK_PD_SSUSB = 5,
	MT6878_CHK_PD_MFG0 = 6,
	MT6878_CHK_PD_MM_INFRA = 7,
	MT6878_CHK_PD_DIS0 = 8,
	MT6878_CHK_PD_VEN0 = 9,
	MT6878_CHK_PD_CAM_VCORE = 10,
	MT6878_CHK_PD_CAM_CCU = 11,
	MT6878_CHK_PD_CAM_CCU_AO = 12,
	MT6878_CHK_PD_CAM_MAIN = 13,
	MT6878_CHK_PD_CAM_SUBA = 14,
	MT6878_CHK_PD_CAM_SUBB = 15,
	MT6878_CHK_PD_ISP_VCORE = 16,
	MT6878_CHK_PD_ISP_MAIN = 17,
	MT6878_CHK_PD_ISP_DIP1 = 18,
	MT6878_CHK_PD_VDE0 = 19,
	MT6878_CHK_PD_MM_PROC = 20,
	MT6878_CHK_PD_APU = 21,
	MT6878_CHK_PD_NUM,
};

#ifdef CONFIG_MTK_DVFSRC_HELPER
extern int get_sw_req_vcore_opp(void);
#endif

extern void print_subsys_reg_mt6878(enum chk_sys_id id);
extern void set_subsys_reg_dump_mt6878(enum chk_sys_id id[]);
extern void get_subsys_reg_dump_mt6878(void);
extern u32 get_mt6878_reg_value(u32 id, u32 ofs);
extern void release_mt6878_hwv_secure(void);
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
extern void dump_clk_event(void);
#endif

#endif	/* __DRV_CLKCHK_MT6878_H */
