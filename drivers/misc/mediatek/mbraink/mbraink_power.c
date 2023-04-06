// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <swpm_module_ext.h>

#include "mbraink_power.h"

#if IS_ENABLED(CONFIG_MTK_LPM_MT6985) && \
	IS_ENABLED(CONFIG_MTK_LOW_POWER_MODULE) && \
	IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)

#if (MBRAINK_LANDING_PONSOT_CHECK == 1)
void mtk_get_lp_info(struct lpm_dbg_lp_info *info, int type)
{
	pr_info("%s: not support yet...", __func__);
}
#endif

int mbraink_get_power_info(char *buffer, unsigned int size, int datatype)
{
	int idx = 0, n = 0;
	struct mbraink_26m mbraink_26m_stat;
	struct lpm_dbg_lp_info mbraink_lpm_dbg_lp_info;
	struct md_sleep_status mbraink_md_data;
	struct timespec64 tv = { 0 };
	const char * const mbraink_lp_state_name[NUM_SPM_STAT] = {
		"AP",
		"26M",
		"VCORE",
	};

	memset(&mbraink_26m_stat, 0, sizeof(mbraink_26m_stat));
	memset(&mbraink_lpm_dbg_lp_info, 0, sizeof(mbraink_lpm_dbg_lp_info));
	memset(&mbraink_md_data, 0, sizeof(mbraink_md_data));

	ktime_get_real_ts64(&tv);
	n += snprintf(buffer + n, size, "systime:%lld\n", tv.tv_sec);

	n += snprintf(buffer + n, size, "datatype:%d\n", datatype);

	mtk_get_lp_info(&mbraink_lpm_dbg_lp_info, SPM_IDLE_STAT);
	for (idx = 0; idx < NUM_SPM_STAT; idx++) {
		n += snprintf(buffer + n, size, "Idle_count %s:%lld\n",
			mbraink_lp_state_name[idx], mbraink_lpm_dbg_lp_info.record[idx].count);
	}
	for (idx = 0; idx < NUM_SPM_STAT; idx++) {
		n += snprintf(buffer + n, size, "Idle_period %s:%lld.%03lld\n",
			mbraink_lp_state_name[idx],
			PCM_TICK_TO_SEC(mbraink_lpm_dbg_lp_info.record[idx].duration),
			PCM_TICK_TO_SEC((mbraink_lpm_dbg_lp_info.record[idx].duration%
				PCM_32K_TICKS_PER_SEC)*
				1000));
	}

	mtk_get_lp_info(&mbraink_lpm_dbg_lp_info, SPM_SUSPEND_STAT);
	for (idx = 0; idx < NUM_SPM_STAT; idx++) {
		n += snprintf(buffer + n, size, "Suspend_count %s:%lld\n",
			mbraink_lp_state_name[idx], mbraink_lpm_dbg_lp_info.record[idx].count);
	}
	for (idx = 0; idx < NUM_SPM_STAT; idx++) {
		n += snprintf(buffer + n, size, "Suspend_period %s:%lld.%03lld\n",
			mbraink_lp_state_name[idx],
			PCM_TICK_TO_SEC(mbraink_lpm_dbg_lp_info.record[idx].duration),
			PCM_TICK_TO_SEC((mbraink_lpm_dbg_lp_info.record[idx].duration%
				PCM_32K_TICKS_PER_SEC)*
				1000));
	}

	get_md_sleep_time(&mbraink_md_data);
	if (!is_md_sleep_info_valid(&mbraink_md_data))
		pr_notice("mbraink_md_data is not valid!\n");

	n += snprintf(buffer + n, size, "MD:%lld.%03lld\nMD_2G:%lld.%03lld\nMD_3G:%lld.%03lld\n",
		mbraink_md_data.md_sleep_time / 1000000,
		(mbraink_md_data.md_sleep_time % 1000000) / 1000,
		mbraink_md_data.gsm_sleep_time / 1000000,
		(mbraink_md_data.gsm_sleep_time % 1000000) / 1000,
		mbraink_md_data.wcdma_sleep_time / 1000000,
		(mbraink_md_data.wcdma_sleep_time % 1000000) / 1000);

	n += snprintf(buffer + n, size, "MD_4G:%lld.%03lld\nMD_5G:%lld.%03lld\n",
		mbraink_md_data.lte_sleep_time / 1000000,
		(mbraink_md_data.lte_sleep_time % 1000000) / 1000,
		mbraink_md_data.nr_sleep_time / 1000000,
		(mbraink_md_data.nr_sleep_time % 1000000) / 1000);

	mbraink_26m_stat.req_sta_0 = plat_mmio_read(SPM_REQ_STA_0);
	mbraink_26m_stat.req_sta_1 = plat_mmio_read(SPM_REQ_STA_1);
	mbraink_26m_stat.req_sta_2 = plat_mmio_read(SPM_REQ_STA_2);
	mbraink_26m_stat.req_sta_3 = plat_mmio_read(SPM_REQ_STA_3);
	mbraink_26m_stat.req_sta_4 = plat_mmio_read(SPM_REQ_STA_4);
	mbraink_26m_stat.req_sta_5 = plat_mmio_read(SPM_REQ_STA_5);
	mbraink_26m_stat.req_sta_6 = plat_mmio_read(SPM_REQ_STA_6);
	mbraink_26m_stat.req_sta_7 = plat_mmio_read(SPM_REQ_STA_7);
	mbraink_26m_stat.req_sta_8 = plat_mmio_read(SPM_REQ_STA_8);
	mbraink_26m_stat.req_sta_9 = plat_mmio_read(SPM_REQ_STA_9);
	mbraink_26m_stat.req_sta_10 = plat_mmio_read(SPM_REQ_STA_10);
	mbraink_26m_stat.src_req = plat_mmio_read(SPM_SRC_REQ);

	n += snprintf(buffer + n, size, "req_sta_0:%u\nreq_sta_1:%u\nreq_sta_2:%u\nreq_sta_3:%u\n",
		mbraink_26m_stat.req_sta_0, mbraink_26m_stat.req_sta_1,
		mbraink_26m_stat.req_sta_2, mbraink_26m_stat.req_sta_3);
	n += snprintf(buffer + n, size, "req_sta_4:%u\nreq_sta_5:%u\nreq_sta_6:%u\nreq_sta_7:%u\n",
		mbraink_26m_stat.req_sta_4, mbraink_26m_stat.req_sta_5,
		mbraink_26m_stat.req_sta_6, mbraink_26m_stat.req_sta_7);
	n += snprintf(buffer + n, size, "req_sta_8:%u\nreq_sta_9:%u\nreq_sta_10:%u\nsrc_req:%u\n",
		mbraink_26m_stat.req_sta_8, mbraink_26m_stat.req_sta_9,
		mbraink_26m_stat.req_sta_10, mbraink_26m_stat.src_req);
	buffer[n] = '\0';

	return n;
}
#else
int mbraink_get_power_info(char *buffer, unsigned int size, int datatype)
{
	pr_info("%s: Do not support ioctl power query.\n", __func__);
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
int mbraink_power_getVcoreInfo(struct mbraink_power_vcoreInfo *pmbrainkPowerVcoreInfo)
{
	int ret = 0;
	int i, j;
	int32_t core_vol_num, core_ip_num;
	struct ip_stats *core_ip_stats_ptr;
	struct vol_duration *core_duration_ptr;

	core_vol_num = get_vcore_vol_num();
	core_ip_num = get_vcore_ip_num();

	core_duration_ptr = kmalloc_array(core_vol_num, sizeof(struct vol_duration), GFP_KERNEL);
	core_ip_stats_ptr = kmalloc_array(core_ip_num, sizeof(struct ip_stats), GFP_KERNEL);
	for (i = 0; i < core_ip_num; i++)
		core_ip_stats_ptr[i].vol_times =
			kmalloc_array(core_vol_num, sizeof(struct ip_vol_times), GFP_KERNEL);

	sync_latest_data();

	if (!core_duration_ptr) {
		pr_notice("core_duration_idx failure\n");
		ret = -1;
		goto End;
	} else if (!core_ip_stats_ptr) {
		pr_notice("core_ip_stats_idx failure\n");
		ret = -1;
		goto End;
	}

	get_vcore_vol_duration(core_vol_num, core_duration_ptr);
	get_vcore_ip_vol_stats(core_ip_num, core_vol_num, core_ip_stats_ptr);

	pr_info("VCORE_VOL_NUM = %d\n", core_vol_num);
	pr_info("VCORE_IP_NUM = %d\n", core_ip_num);

	if (core_vol_num != MAX_VCORE_NUM) {
		pr_notice("VCORE_VOL_NUM is not expected(%d)\n", MAX_VCORE_NUM);
		ret = -1;
		goto End;
	}

	if (core_ip_num != MBRAINK_VCORE_IP_MAX) {
		pr_notice("VCORE_IP_NUM is not expected(%d)\n", MBRAINK_VCORE_IP_MAX);
		ret = -1;
		goto End;
	}

	for (i = 0; i < core_vol_num; i++) {
		pmbrainkPowerVcoreInfo->vcoreDurationInfo[i].vol =
			core_duration_ptr[i].vol;
		pmbrainkPowerVcoreInfo->vcoreDurationInfo[i].duration =
			core_duration_ptr[i].duration;
		pr_info("VCORE %d mV : %lld ms\n",
			pmbrainkPowerVcoreInfo->vcoreDurationInfo[i].vol,
			pmbrainkPowerVcoreInfo->vcoreDurationInfo[i].duration);
	}

	for (i = 0; i < core_ip_num; i++) {
		strncpy(pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].ip_name,
			core_ip_stats_ptr[i].ip_name,
			MAX_IP_NAME_LENGTH - 1);
		pr_info("VCORE IP %s\n",
			pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].ip_name);
		for (j = 0; j < core_vol_num; j++) {
			pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].vol_times[j].vol =
				core_ip_stats_ptr[i].vol_times[j].vol;
			pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].vol_times[j].active_time =
				core_ip_stats_ptr[i].vol_times[j].active_time;
			pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].vol_times[j].idle_time =
				core_ip_stats_ptr[i].vol_times[j].idle_time;
			pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].vol_times[j].off_time =
				core_ip_stats_ptr[i].vol_times[j].off_time;

			pr_info("%d mV \t active_time : %lld ms \t idle_time : %lld ms \t off_time : %lld ms\n",
			pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].vol_times[j].vol,
			pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].vol_times[j].active_time,
			pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].vol_times[j].idle_time,
			pmbrainkPowerVcoreInfo->vcoreIpDurationInfo[i].vol_times[j].off_time);
		}
	}
End:
	kfree(core_duration_ptr);

	for (i = 0; i < core_ip_num; i++)
		kfree(core_ip_stats_ptr[i].vol_times);
	kfree(core_ip_stats_ptr);

	return ret;
}

#else
int mbraink_power_getVcoreInfo(struct mbraink_audio_idleRatioInfo *pmbrainkAudioIdleRatioInfo)
{
	pr_info("%s: Do not support ioctl vcore info query.\n", __func__);
	return 0;
}
#endif //#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)


