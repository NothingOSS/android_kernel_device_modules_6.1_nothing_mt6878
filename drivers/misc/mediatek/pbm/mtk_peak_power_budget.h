/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef __MTK_PEAK_POWER_BUDGETING_H__
#define __MTK_PEAK_POWER_BUDGETING_H__


enum ppb_kicker {
	KR_BUDGET,
	KR_FLASH,
	KR_AUDIO,
	KR_CAMERA,
	KR_DISPLAY,
	KR_APU,
	KR_NUM
};

enum ppb_sram_offset {
	PPB_MODE,
	PPB_CG_PWR,
	PPB_VSYS_PWR,
	PPB_VSYS_ACK,
	PPB_FLASH_PWR,
	PPB_AUDIO_PWR,
	PPB_CAMERA_PWR,
	PPB_APU_PWR,
	PPB_DISPLAY_PWR,
	PPB_DRAM_PWR,
	PPB_RESERVE2,
	PPB_RESERVE3,
	PPB_RESERVE4,
	PPB_RESERVE5,
	PPB_OFFSET_NUM
};

struct ppb_ctrl {
	u8 ppb_stop;
	u8 ppb_drv_done;
	u8 manual_mode;
	u8 ppb_mode;
};

struct ppb {
	unsigned int loading_flash;
	unsigned int loading_audio;
	unsigned int loading_camera;
	unsigned int loading_display;
	unsigned int loading_apu;
	unsigned int vsys_budget;
	unsigned int remain_budget;
};

struct power_budget_t {
	unsigned int temp_cur_stage;
	unsigned int temp_max_stage;
	int temp_thd[2];
	unsigned int rdc[3];
	unsigned int rac[3];
	unsigned int uvlo;
	unsigned int ocp;
	unsigned int cur_rdc;
	unsigned int cur_rac;
	unsigned int sys_power;
	unsigned int bat_power;
	unsigned int imax;
	unsigned int ocv;
	struct work_struct bat_work;
	struct power_supply *psy;
};

struct ocv_table_t {
	unsigned int mah;
	unsigned int dod;
	unsigned int voltage;
};

struct fg_info_t {
	int temp;
	int qmax;
	int ocv_table_size;
	struct ocv_table_t ocv_table[100];
};

struct fg_cus_data {
	unsigned int fg_info_size;
	unsigned int bat_type;
	struct fg_info_t fg_info[10];
};

extern void kicker_pbm_request_power(enum ppb_kicker kicker, unsigned int power);


#endif /* __MTK_PEAK_POWER_BUDGETING_H__ */
