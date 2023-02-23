// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Perry Hsu <perry.hsu@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[blocktag][ufs]" fmt

#define DEBUG 1
#define BTAG_UFS_TRACE_LATENCY ((unsigned long long)(1000000000))

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/tick.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_proto.h>
#include "mtk_blocktag.h"
#include "blocktag-ufs.h"

/* ring trace for debugfs */
struct mtk_blocktag *ufs_mtk_btag;
struct workqueue_struct *ufs_mtk_btag_wq;
struct work_struct ufs_mtk_btag_worker;

static inline __u16 chbe16_to_u16(const char *str)
{
	__u16 ret;

	ret = str[0];
	ret = ret << 8 | str[1];
	return ret;
}

static inline __u32 chbe32_to_u32(const char *str)
{
	__u32 ret;

	ret = str[0];
	ret = ret << 8 | str[1];
	ret = ret << 8 | str[2];
	ret = ret << 8 | str[3];
	return ret;
}

#define scsi_cmnd_cmd(cmd)  (cmd->cmnd[0])

static enum mtk_btag_io_type cmd_to_io_type(__u16 cmd)
{
	switch (cmd) {
	case READ_6:
	case READ_10:
	case READ_16:
#if IS_ENABLED(CONFIG_SCSI_UFS_HPB)
	case UFSHPB_READ:
#endif
		return BTAG_IO_READ;

	case WRITE_6:
	case WRITE_10:
	case WRITE_16:
		return BTAG_IO_WRITE;

	default:
		return BTAG_IO_UNKNOWN;
	}
}

static __u32 scsi_cmnd_len(struct scsi_cmnd *cmd)
{
	__u32 len;

	switch (scsi_cmnd_cmd(cmd)) {
	case READ_6:
	case WRITE_6:
		len = cmd->cmnd[4];
		break;

	case READ_10:
	case WRITE_10:
		len = chbe16_to_u16(&cmd->cmnd[7]);
		break;

	case READ_16:
	case WRITE_16:
		len = chbe32_to_u32(&cmd->cmnd[10]);
		break;

#if IS_ENABLED(CONFIG_SCSI_UFS_HPB)
	case UFSHPB_READ:
		len = cmd->cmnd[14];
		break;
#endif

	default:
		return 0;
	}
	return len << UFS_LOGBLK_SHIFT;
}

static struct btag_ufs_ctx *btag_ufs_ctx(__u16 qid)
{
	struct btag_ufs_ctx *ctx = BTAG_CTX(ufs_mtk_btag);

	if (!ctx)
		return NULL;

	if (qid >= ufs_mtk_btag->ctx.count) {
		pr_notice("invalid queue id %d\n", qid);
		return NULL;
	}
	return &ctx[qid];
}

static struct btag_ufs_ctx *btag_ufs_tid_to_ctx(__u16 tid)
{
	if (tid >= BTAG_UFS_TAGS) {
		pr_notice("%s: invalid tag id %d\n", __func__, tid);
		return NULL;
	}

	return btag_ufs_ctx(tid_to_qid(tid));
}

static struct btag_ufs_tag *btag_ufs_tag(struct btag_ufs_ctx *ctx,
					 __u16 tid)
{
	if (!ctx)
		return NULL;

	if (tid >= BTAG_UFS_TAGS) {
		pr_notice("%s: invalid tag id %d\n", __func__, tid);
		return NULL;
	}

	return &ctx->tags[tid % BTAG_UFS_QUEUE_TAGS];
}

static void btag_ufs_pidlog_insert(struct mtk_btag_proc_pidlogger *pidlog,
				   struct scsi_cmnd *cmd, __u32 *top_len)
{
	struct req_iterator rq_iter;
	struct bio_vec bvec;
	struct request *rq;
	__u16 insert_pid[BTAG_PIDLOG_ENTRIES] = {0};
	__u32 insert_len[BTAG_PIDLOG_ENTRIES] = {0};
	__u32 insert_cnt = 0;
	enum mtk_btag_io_type io_type;

	*top_len = 0;
	rq = scsi_cmd_to_rq(cmd);
	if (!rq)
		return;

	io_type = cmd_to_io_type(scsi_cmnd_cmd(cmd));
	if (io_type == BTAG_IO_UNKNOWN)
		return;

	rq_for_each_segment(bvec, rq, rq_iter) {
		short pid;
		int idx;

		if (!bvec.bv_page)
			continue;

		pid = mtk_btag_page_pidlog_get(bvec.bv_page);
		mtk_btag_page_pidlog_set(bvec.bv_page, 0);

		if (!pid)
			continue;

		if (pid < 0) {
			*top_len += bvec.bv_len;
			pid = -pid;
		}

		if (!pidlog)
			continue;

		for (idx = 0; idx < BTAG_PIDLOG_ENTRIES; idx++) {
			if (insert_pid[idx] == 0) {
				insert_pid[idx] = pid;
				insert_len[idx] = bvec.bv_len;
				insert_cnt++;
				break;
			} else if (insert_pid[idx] == pid) {
				insert_len[idx] += bvec.bv_len;
				break;
			}
		}
	}

	if (pidlog)
		mtk_btag_pidlog_insert(pidlog, insert_pid, insert_len,
				       insert_cnt, io_type);
}

void mtk_btag_ufs_send_command(__u16 tid, struct scsi_cmnd *cmd)
{
	struct btag_ufs_ctx *ctx;
	struct btag_ufs_tag *tag;
	unsigned long flags;
	__u64 cur_time = sched_clock();
	__u64 window_t = 0;
	__u32 top_len;

	if (!cmd)
		return;

	ctx = btag_ufs_tid_to_ctx(tid);
	if (!ctx)
		return;

	/* tag */
	tag = btag_ufs_tag(ctx, tid);
	if (!tag)
		return;
	tag->len = scsi_cmnd_len(cmd);
	tag->cmd = scsi_cmnd_cmd(cmd);
	tag->start_t = cur_time;

	/* workload */
	spin_lock_irqsave(&ctx->wl.lock, flags);
	if (!ctx->wl.depth++) {
		ctx->wl.idle_total += cur_time - ctx->wl.idle_begin;
		ctx->wl.idle_begin = 0;
	}
	window_t = cur_time - ctx->wl.window_begin;
	spin_unlock_irqrestore(&ctx->wl.lock, flags);

	/* pidlog */
	btag_ufs_pidlog_insert(&ctx->pidlog, cmd, &top_len);

	if (window_t > BTAG_UFS_TRACE_LATENCY)
		queue_work(ufs_mtk_btag_wq, &ufs_mtk_btag_worker);
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_send_command);

void mtk_btag_ufs_transfer_req_compl(__u16 tid)
{
	struct btag_ufs_ctx *ctx;
	struct btag_ufs_tag *tag;
	unsigned long flags;
	enum mtk_btag_io_type io_type;
	__u64 cur_time = sched_clock();
	__u64 window_t = 0;

	ctx = btag_ufs_tid_to_ctx(tid);
	if (!ctx)
		return;

	tag = btag_ufs_tag(ctx, tid);
	if (!tag)
		return;

	/* throughput */
	io_type = cmd_to_io_type(tag->cmd);
	if (io_type < BTAG_IO_TYPE_NR) {
		spin_lock_irqsave(&ctx->tp.lock, flags);
		ctx->tp.usage[io_type] += (cur_time - tag->start_t);
		ctx->tp.size[io_type] += tag->len;
		spin_unlock_irqrestore(&ctx->tp.lock, flags);
	}

	/* workload */
	spin_lock_irqsave(&ctx->wl.lock, flags);
	ctx->wl.req_cnt++;
	if (!tag->start_t) {
		ctx->wl.idle_total = 0;
		ctx->wl.idle_begin = ctx->wl.depth ? 0 : cur_time;
	} else if (!--ctx->wl.depth) {
		ctx->wl.idle_begin = cur_time;
	}
	window_t = cur_time - ctx->wl.window_begin;
	spin_unlock_irqrestore(&ctx->wl.lock, flags);

	/* clear tag */
	tag->start_t = 0;
	tag->cmd = 0;
	tag->len = 0;

	if (window_t > BTAG_UFS_TRACE_LATENCY)
		queue_work(ufs_mtk_btag_wq, &ufs_mtk_btag_worker);
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_transfer_req_compl);

/* evaluate throughput, workload and pidlog of given context */
static void btag_ufs_ctx_eval(struct btag_ufs_ctx *ctx,
			      struct mtk_btag_trace *tr)
{
	struct mtk_btag_throughput *tp = tr->throughput;
	struct mtk_btag_workload *wl = &tr->workload;
	enum mtk_btag_io_type io_type;
	__u64 cur_time, idle_total, window_begin;

	/* throughput */
	spin_lock(&ctx->tp.lock);
	for (io_type = 0; io_type < BTAG_IO_TYPE_NR; io_type++) {
		tp[io_type].usage = ctx->tp.usage[io_type];
		tp[io_type].size = ctx->tp.size[io_type];
		ctx->tp.usage[io_type] = 0;
		ctx->tp.size[io_type] = 0;
	}
	spin_unlock(&ctx->tp.lock);
	mtk_btag_throughput_eval(tp);

	/* workload */
	spin_lock(&ctx->wl.lock);
	cur_time = sched_clock();
	idle_total = ctx->wl.idle_total;
	wl->count = ctx->wl.req_cnt;
	window_begin = ctx->wl.window_begin;
	if (ctx->wl.idle_begin) {
		idle_total += cur_time - ctx->wl.idle_begin;
		ctx->wl.idle_begin = cur_time;
	}
	ctx->wl.idle_total = 0;
	ctx->wl.req_cnt = 0;
	ctx->wl.window_begin = cur_time;
	spin_unlock(&ctx->wl.lock);

	tr->time = cur_time;
	wl->period = cur_time - window_begin;
	wl->usage = wl->period - idle_total;
	if (!wl->usage)
		wl->percent = 0;
	else if (wl->period > wl->usage * 100)
		wl->percent = 1;
	else
		wl->percent = (__u32)div64_u64(wl->usage * 100, wl->period);

	/* pidlog */
	spin_lock(&ctx->pidlog.lock);
	memcpy(&tr->pidlog.info, &ctx->pidlog.info,
	       sizeof(struct mtk_btag_proc_pidlogger_entry) *
	       BTAG_PIDLOG_ENTRIES);
	memset(&ctx->pidlog.info, 0,
	       sizeof(struct mtk_btag_proc_pidlogger_entry) *
	       BTAG_PIDLOG_ENTRIES);
	spin_unlock(&ctx->pidlog.lock);
}

/* evaluate context to trace ring buffer */
static void btag_ufs_work(struct work_struct *work)
{
	struct mtk_btag_ringtrace *rt = BTAG_RT(ufs_mtk_btag);
	struct mtk_btag_trace *tr;
	struct btag_ufs_ctx *ctx;
	unsigned long flags;
	__u32 qid;

	if (!rt)
		return;

	for (qid = 0; qid < ufs_mtk_btag->ctx.count; qid++) {
		ctx = btag_ufs_ctx(qid);
		if (!ctx)
			break;

		spin_lock_irqsave(&ctx->wl.lock, flags);
		if (sched_clock() - ctx->wl.window_begin <=
		    BTAG_UFS_TRACE_LATENCY) {
			spin_unlock_irqrestore(&ctx->wl.lock, flags);
			continue;
		}
		spin_unlock_irqrestore(&ctx->wl.lock, flags);

		spin_lock_irqsave(&rt->lock, flags);
		tr = mtk_btag_curr_trace(rt);
		if (!tr) {
			spin_unlock_irqrestore(&rt->lock, flags);
			break;
		}

		tr->time = sched_clock();
		tr->pid = 0;
		tr->qid = qid;
		btag_ufs_ctx_eval(ctx, tr);
		mtk_btag_vmstat_eval(&tr->vmstat);
		mtk_btag_cpu_eval(&tr->cpu);

		mtk_btag_next_trace(rt);
		spin_unlock_irqrestore(&rt->lock, flags);
	}
}

static size_t btag_ufs_seq_debug_show_info(char **buff, unsigned long *size,
					   struct seq_file *seq)
{
	return 0;
}

static void btag_ufs_init_ctx(struct mtk_blocktag *btag)
{
	struct btag_ufs_ctx *ctx = BTAG_CTX(btag);
	__u64 time = sched_clock();
	int i;

	if (!ctx)
		return;

	memset(ctx, 0, sizeof(struct btag_ufs_ctx) * btag->ctx.count);
	for (i = 0; i < btag->ctx.count; i++) {
		spin_lock_init(&ctx[i].tp.lock);
		spin_lock_init(&ctx[i].wl.lock);
		spin_lock_init(&ctx[i].pidlog.lock);
		ctx[i].wl.window_begin = time;
		ctx[i].wl.idle_begin = time;
	}
}

static struct mtk_btag_vops btag_ufs_vops = {
	.seq_show = btag_ufs_seq_debug_show_info,
};

int mtk_btag_ufs_init(struct ufs_mtk_host *host)
{
#if IS_ENABLED(CONFIG_UFS_MEDIATEK_MCQ)
	struct ufs_hba_private *hba_priv;
#endif
	struct mtk_blocktag *btag;
	int max_queue = 1;

	if (!host)
		return -1;

	if (host->qos_allowed)
		btag_ufs_vops.earaio_enabled = true;

	if (host->boot_device)
		btag_ufs_vops.boot_device = true;

#if IS_ENABLED(CONFIG_UFS_MEDIATEK_MCQ)
	hba_priv = (struct ufs_hba_private *)host->hba->android_vendor_data1;
	if (hba_priv->is_mcq_enabled)
		max_queue = hba_priv->mcq_nr_hw_queue;
#endif

	ufs_mtk_btag_wq = alloc_workqueue("ufs_mtk_btag",
					  WQ_FREEZABLE | WQ_UNBOUND, 1);
	INIT_WORK(&ufs_mtk_btag_worker, btag_ufs_work);

	btag = mtk_btag_alloc("ufs",
			      BTAG_STORAGE_UFS,
			      BTAG_UFS_RINGBUF_MAX,
			      sizeof(struct btag_ufs_ctx),
			      max_queue, &btag_ufs_vops);

	if (btag) {
		btag_ufs_init_ctx(btag);
		ufs_mtk_btag = btag;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_init);

int mtk_btag_ufs_exit(void)
{
	mtk_btag_free(ufs_mtk_btag);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek UFS Block IO Tracer");
MODULE_AUTHOR("Perry Hsu <perry.hsu@mediatek.com>");
MODULE_AUTHOR("Stanley Chu <stanley chu@mediatek.com>");

