// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>

#include "mbraink_suspend_info.h"

/*spinlock for mbraink mbraink_suspend_info_list*/
static DEFINE_SPINLOCK(suspend_info_list_lock);
struct mbraink_suspend_info_list mbraink_suspend_info_list_data[SUSPEND_INFO_SZ];
struct mbraink_suspend_info_list_p mbraink_suspend_info_list_p_data;

void mbraink_set_suspend_info_list_record(unsigned short datatype)
{
	struct timespec64 tv = { 0 };
	unsigned long flags;
	unsigned short w_idx = 0;

	spin_lock_irqsave(&suspend_info_list_lock, flags);

	w_idx = (unsigned short)(mbraink_suspend_info_list_p_data.w_idx % SUSPEND_INFO_SZ);

	if (mbraink_suspend_info_list_data[w_idx].dirty == false) {
		ktime_get_real_ts64(&tv);
		mbraink_suspend_info_list_data[w_idx].timestamp =
			(tv.tv_sec*1000)+(tv.tv_nsec/1000000);
		mbraink_suspend_info_list_data[w_idx].datatype =
			datatype;
		if (datatype == 1) {
			char buf[64] = {'\0'};

			last_resume_reason_show(NULL, NULL, buf);
			if (strstr(buf, "Abort:"))
				mbraink_suspend_info_list_data[w_idx].reason = -2;
			else if (strstr(buf, "-1"))
				mbraink_suspend_info_list_data[w_idx].reason = -1;
			else {
				int irq = -3, size = 0;
				char sub_buf[64] = {'\0'};

				size = sscanf(buf, "%d %s\n", &irq, sub_buf);
				if (size < 0)
					pr_info("%s, sscanf failed\n", __func__);
				mbraink_suspend_info_list_data[w_idx].reason = irq;
			}
		} else
			mbraink_suspend_info_list_data[w_idx].reason = 0;
		pr_info("%s: w_idx = %u, timestamp=%lld, datatype=%u, reason=%d\n",
			__func__,
			w_idx,
			mbraink_suspend_info_list_data[w_idx].timestamp,
			mbraink_suspend_info_list_data[w_idx].datatype,
			mbraink_suspend_info_list_data[w_idx].reason);
		mbraink_suspend_info_list_data[w_idx].dirty = true;
		mbraink_suspend_info_list_p_data.w_idx =
			(mbraink_suspend_info_list_p_data.w_idx + 1) % SUSPEND_INFO_SZ;
	} else {
		pr_notice("buffer is full,  w_idx = %u !!!\n",
			w_idx);
	}

	spin_unlock_irqrestore(&suspend_info_list_lock, flags);
}

void mbraink_get_suspend_info_list_record(struct mbraink_suspend_info_struct_data *buffer, int max)
{
	unsigned int buf_idx = 0;
	unsigned short r_idx = 0;
	unsigned short init_r_idx = 0;
	unsigned long flags;

	spin_lock_irqsave(&suspend_info_list_lock, flags);

	memset(buffer, 0, sizeof(struct mbraink_suspend_info_struct_data));
	init_r_idx = r_idx = mbraink_suspend_info_list_p_data.r_idx % SUSPEND_INFO_SZ;

	do {
		if (buf_idx >= max) {
			buffer->is_continue = true;
			break;
		} else if (mbraink_suspend_info_list_data[r_idx].dirty == true) {
			buffer->drv_data[buf_idx].timestamp =
				mbraink_suspend_info_list_data[r_idx].timestamp;
			buffer->drv_data[buf_idx].datatype =
				mbraink_suspend_info_list_data[r_idx].datatype;
			buffer->drv_data[buf_idx].reason =
				mbraink_suspend_info_list_data[r_idx].reason;
			pr_info("%s: r_idx = %u, buf_idx=%u, timestamp=%lld, datatype=%u\n",
				__func__,
				r_idx, buf_idx,
				buffer->drv_data[buf_idx].timestamp,
				buffer->drv_data[buf_idx].datatype);
			mbraink_suspend_info_list_data[r_idx].dirty = false;
			buf_idx++;
			buffer->count++;
		} else
			break;

		r_idx = (r_idx + 1) % SUSPEND_INFO_SZ;
	} while (r_idx != init_r_idx);
	mbraink_suspend_info_list_p_data.r_idx = r_idx;
	spin_unlock_irqrestore(&suspend_info_list_lock, flags);
}

void mbraink_suspend_info_list_init(void)
{
	memset(mbraink_suspend_info_list_data,
		0,
		SUSPEND_INFO_SZ * sizeof(struct mbraink_suspend_info_list));
	mbraink_suspend_info_list_p_data.r_idx = 0;
	mbraink_suspend_info_list_p_data.w_idx = 0;
}

