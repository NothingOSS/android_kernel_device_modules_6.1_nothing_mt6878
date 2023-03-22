// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <swpm_module_ext.h>
#include "mbraink_memory.h"

#if IS_ENABLED(CONFIG_MTK_SWPM_MODULE)
int mbraink_memory_getDdrInfo(struct mbraink_memory_ddrInfo *pMemoryDdrInfo)
{
	int i;
	int32_t ddr_freq_num, ddr_bc_ip_num;

	struct ddr_act_times *ddr_act_times_ptr;
	struct ddr_sr_pd_times *ddr_sr_pd_times_ptr;
	struct ddr_ip_bc_stats *ddr_ip_stats_ptr;

	ddr_freq_num = get_ddr_freq_num();
	ddr_bc_ip_num = get_ddr_data_ip_num();

	ddr_act_times_ptr = kmalloc_array(ddr_freq_num, sizeof(struct ddr_act_times), GFP_KERNEL);
	ddr_sr_pd_times_ptr = kmalloc(sizeof(struct ddr_sr_pd_times), GFP_KERNEL);
	ddr_ip_stats_ptr = kmalloc_array(ddr_bc_ip_num,
								sizeof(struct ddr_ip_bc_stats),
								GFP_KERNEL);
	for (i = 0; i < ddr_bc_ip_num; i++)
		ddr_ip_stats_ptr[i].bc_stats = kmalloc_array(ddr_freq_num,
								sizeof(struct ddr_bc_stats),
								GFP_KERNEL);

	sync_latest_data();

	if (!ddr_act_times_ptr) {
		pr_notice("ddr_act_times_idx failure\n");
		goto End;
	} else if (!ddr_sr_pd_times_ptr) {
		pr_notice("ddr_sr_pd_times_idx failure\n");
		goto End;
	} else if (!ddr_ip_stats_ptr) {
		pr_notice("ddr_ip_idx failure\n");
		goto End;
	}

	get_ddr_act_times(ddr_freq_num, ddr_act_times_ptr);
	get_ddr_sr_pd_times(ddr_sr_pd_times_ptr);
	get_ddr_freq_data_ip_stats(ddr_bc_ip_num, ddr_freq_num, ddr_ip_stats_ptr);

	pMemoryDdrInfo->srTimeInMs = ddr_sr_pd_times_ptr->sr_time;
	pMemoryDdrInfo->pdTimeInMs = ddr_sr_pd_times_ptr->pd_time;

	if (ddr_bc_ip_num < 6) {
		pr_notice("ddr_bc_ip_num less than 5.\n");
		goto End;
	}

	pMemoryDdrInfo->totalDdrFreqNum = ddr_freq_num;

	for (i = 0; i < ddr_freq_num; i++) {
		pMemoryDdrInfo->ddrActiveInfo[i].freqInMhz =
			ddr_act_times_ptr[i].freq;
		pMemoryDdrInfo->ddrActiveInfo[i].totalActiveTimeInMs =
			ddr_act_times_ptr[i].active_time;
		pMemoryDdrInfo->ddrActiveInfo[i].totalReadActiveTimeInMs =
			ddr_ip_stats_ptr[0].bc_stats[i].value;
		pMemoryDdrInfo->ddrActiveInfo[i].totalWriteActiveTimeInMs =
			ddr_ip_stats_ptr[1].bc_stats[i].value;
		pMemoryDdrInfo->ddrActiveInfo[i].totalCpuActiveTimeInMs =
			ddr_ip_stats_ptr[2].bc_stats[i].value;
		pMemoryDdrInfo->ddrActiveInfo[i].totalGpuActiveTimeInMs =
			ddr_ip_stats_ptr[3].bc_stats[i].value;
		pMemoryDdrInfo->ddrActiveInfo[i].totalMmActiveTimeInMs =
			ddr_ip_stats_ptr[4].bc_stats[i].value;
		pMemoryDdrInfo->ddrActiveInfo[i].totalMdActiveTimeInMs =
			ddr_ip_stats_ptr[5].bc_stats[i].value;
		/*
		 *pr_info("%s: Freq %dMhz: Time(msec):%lld %llu/%llu/%llu/%llu/%llu/%llu/\n",
		 *__func__,
		 *pMemoryDdrInfo->ddrActiveInfo[i].freqInMhz,
		 *pMemoryDdrInfo->ddrActiveInfo[i].totalActiveTimeInMs,
		 *pMemoryDdrInfo->ddrActiveInfo[i].totalReadActiveTimeInMs,
		 *pMemoryDdrInfo->ddrActiveInfo[i].totalWriteActiveTimeInMs,
		 *pMemoryDdrInfo->ddrActiveInfo[i].totalCpuActiveTimeInMs,
		 *pMemoryDdrInfo->ddrActiveInfo[i].totalGpuActiveTimeInMs,
		 *pMemoryDdrInfo->ddrActiveInfo[i].totalMmActiveTimeInMs,
		 *pMemoryDdrInfo->ddrActiveInfo[i].totalMdActiveTimeInMs);
		 */
	}

	/*
	 *pr_info("SR time(msec): %lld\n",
	 *ddr_sr_pd_times_ptr->sr_time);
	 *pr_info("PD time(msec): %lld\n",
	 *ddr_sr_pd_times_ptr->pd_time);
	 *for (i = 0; i < ddr_freq_num; i++) {
	 *	pr_info("Freq %dMhz: ",
	 *		ddr_act_times_ptr[i].freq);
	 *		pr_info("Time(msec):%lld ",
	 *		ddr_act_times_ptr[i].active_time);
	 *	for (j = 0; j < ddr_bc_ip_num; j++) {
	 *		pr_info("%llu/",
	 *		ddr_ip_stats_ptr[j].bc_stats[i].value);
	 *	}
	 *	pr_info("\n");
	 *}
	 */

End:
	kfree(ddr_act_times_ptr);
	kfree(ddr_sr_pd_times_ptr);

	for (i = 0; i < ddr_bc_ip_num; i++)
		kfree(ddr_ip_stats_ptr[i].bc_stats);
	kfree(ddr_ip_stats_ptr);

	return 0;
}

#else
int mbraink_memory_getDdrInfo(struct mbraink_memory_ddrInfo *pMemoryDdrInfo)
{
	pr_info("%s: Do not support ioctl getDdrInfo query.\n", __func__);
	pMemoryDdrInfo->totalDdrFreqNum = 0;

	return 0;
}
#endif

