// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/vmalloc.h>
#include "mbraink_gpu.h"

#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)
#include <fpsgo_common.h>
#include <fstb.h>
#endif

#include <ged_dvfs.h>
#include <gpufreq_v2.h>

#define Q2QTIMEOUT 500000000 //500ms
#define Q2QTIMEOUT_HIST 70000000 //70ms
static unsigned long long gq2qTimeoutInNs = Q2QTIMEOUT;
unsigned int TimeoutCounter[10] = {0};
unsigned int TimeoutRange[10] = {70, 120, 170, 220, 270, 320, 370, 420, 470, 520};

static void calculateTimeoutCouter(unsigned long long q2qTimeInNS)
{
	if (q2qTimeInNS < 120000000) //70~120ms
		TimeoutCounter[0]++;
	else if (q2qTimeInNS < 170000000) //120~170ms
		TimeoutCounter[1]++;
	else if (q2qTimeInNS < 220000000) //170~220ms
		TimeoutCounter[2]++;
	else if (q2qTimeInNS < 270000000) //220~270ms
		TimeoutCounter[3]++;
	else if (q2qTimeInNS < 320000000) //270~320ms
		TimeoutCounter[4]++;
	else if (q2qTimeInNS < 370000000) //320~370ms
		TimeoutCounter[5]++;
	else if (q2qTimeInNS < 420000000) //370~420ms
		TimeoutCounter[6]++;
	else if (q2qTimeInNS < 470000000) //420~470ms
		TimeoutCounter[7]++;
	else if (q2qTimeInNS < 520000000) //470~520ms
		TimeoutCounter[8]++;
	else //>520ms
		TimeoutCounter[9]++;
}

ssize_t getTimeoutCouterReport(char *pBuf)
{
	ssize_t size = 0;

	if (pBuf == NULL)
		return size;

	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[0], TimeoutRange[1], TimeoutCounter[0],
		TimeoutRange[1], TimeoutRange[2], TimeoutCounter[1]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[2], TimeoutRange[3], TimeoutCounter[2],
		TimeoutRange[3], TimeoutRange[4], TimeoutCounter[3]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[4], TimeoutRange[5], TimeoutCounter[4],
		TimeoutRange[5], TimeoutRange[6], TimeoutCounter[5]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n%d~%d:%d\n",
		TimeoutRange[6], TimeoutRange[7], TimeoutCounter[6],
		TimeoutRange[7], TimeoutRange[8], TimeoutCounter[7]);
	size += scnprintf(pBuf+size, 1024-size, "%d~%d:%d\n>%d:%d\n",
		TimeoutRange[8], TimeoutRange[9], TimeoutCounter[8],
		TimeoutRange[9], TimeoutCounter[9]);

	return size;
}

void fpsgo2mbrain_hint_frameinfo(int pid, unsigned long long bufID,
	int fps, unsigned long long time)
{
	if (time > Q2QTIMEOUT_HIST)
		calculateTimeoutCouter(time);

	if (time > gq2qTimeoutInNs) {
		pr_info("q2q (%d) (%llu) (%llu) ns limit (%llu) ns\n",
			pid,
			bufID,
			time,
			gq2qTimeoutInNs);
		mbraink_netlink_send_msg(NETLINK_EVENT_Q2QTIMEOUT);
	}
}

int mbraink_gpu_init(void)
{
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)
	fpsgo_other2fstb_register_info_callback(FPSGO_Q2Q_TIME,
		fpsgo2mbrain_hint_frameinfo);
#endif
	return 0;
}

int mbraink_gpu_deinit(void)
{
#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)
	fpsgo_other2fstb_unregister_info_callback(FPSGO_Q2Q_TIME,
		fpsgo2mbrain_hint_frameinfo);
#endif
	return 0;
}

void mbraink_gpu_setQ2QTimeoutInNS(unsigned long long q2qTimeoutInNS)
{
	gq2qTimeoutInNs = q2qTimeoutInNS;
}

unsigned long long mbraink_gpu_getQ2QTimeoutInNS(void)
{
	return gq2qTimeoutInNs;
}

int mbraink_gpu_getOppInfo(struct mbraink_gpu_opp_info *gOppInfo)
{
	int i = 0;
	int ret = 0;
	unsigned int u32Count = 0;
	unsigned int u32Level = 0;
	struct GED_DVFS_OPP_STAT *report = NULL;
	u64 u64ts = 0;

	if (gOppInfo == NULL) {
		pr_info("Null gOppInfo\n");
		return -1;
	}

	u32Count = ged_dvfs_get_real_oppfreq_num();

	if (u32Count)
		report = vmalloc(sizeof(struct GED_DVFS_OPP_STAT) * u32Count);

	if ((report != NULL) &&
		ged_dvfs_query_opp_cost(report, u32Count, false, &u64ts) == 0) {
		gOppInfo->data1 = u64ts;
		for (i = 0; i < u32Count; i++) {
			u32Level = gpufreq_get_freq_by_idx(TARGET_DEFAULT, i)/1000;
			if (i < MAX_GPU_OPP_INFO_SZ) {
				gOppInfo->raw[i].data1 = u32Level;
				gOppInfo->raw[i].data2 = report[i].ui64Active;
				gOppInfo->raw[i].data3 = report[i].ui64Idle;
			}
		}
	} else {
		pr_info("can't allocate ged dvfs opp stat memory\n");
		ret = -1;
	}

	if (report != NULL)
		vfree(report);

	return ret;
}

int mbraink_gpu_getStateInfo(struct mbraink_gpu_state_info *gStateInfo)
{
	int ret = 0;

	if (gStateInfo != NULL) {
		ret = ged_dvfs_query_power_state_time(&gStateInfo->data1,
							&gStateInfo->data2,
							&gStateInfo->data3,
							&gStateInfo->data4);
	} else {
		pr_info("gStateInfo is Null\n");
		ret = -1;
	}
	return ret;
}

int mbraink_gpu_getLoadingInfo(struct mbraink_gpu_loading_info *gLoadingInfo)
{
	int ret = 0;

	if (gLoadingInfo != NULL) {
		ret = ged_dvfs_query_loading(&gLoadingInfo->data1,
						&gLoadingInfo->data2);
	} else {
		pr_info("gLoadingInfo is Null\n");
		ret = -1;
	}
	return ret;
}

