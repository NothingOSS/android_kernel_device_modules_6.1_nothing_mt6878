// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Perry Hsu <perry.hsu@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
 */

#define DEBUG 1
#define SECTOR_SHIFT 12
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

#define scsi_cmnd_lba(cmd)  chbe32_to_u32(&cmd->cmnd[2])
#define scsi_cmnd_len(cmd)  chbe16_to_u16(&cmd->cmnd[7])
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

static struct mtk_btag_ufs_ctx *mtk_btag_ufs_curr_ctx(__u16 task_id)
{
	struct mtk_btag_ufs_ctx *ctx = BTAG_CTX(ufs_mtk_btag);

	if (!ctx)
		return NULL;

	if (BTAG_UFS_QUEUE_ID(task_id) >= ufs_mtk_btag->ctx.count) {
		pr_notice("[BLOCK_TAG] %s: invalid task id %d\n",
			__func__, task_id);
		return NULL;
	}

	return &ctx[BTAG_UFS_QUEUE_ID(task_id)];
}

static struct mtk_btag_ufs_task *mtk_btag_ufs_curr_task(__u16 task_id,
		struct mtk_btag_ufs_ctx **curr_ctx)
{
	struct mtk_btag_ufs_ctx *ctx;
	struct mtk_btag_ufs_task *tsk;

	ctx = mtk_btag_ufs_curr_ctx(task_id);
	if (!ctx)
		return NULL;

	*curr_ctx = ctx;
	tsk = &ctx->task[BTAG_UFS_TAG_ID(task_id)];
	return tsk;
}

static void btag_ufs_pidlog_insert(struct mtk_btag_proc_pidlogger *pidlog,
				   struct scsi_cmnd *cmd, __u32 *top_len,
				   enum mtk_btag_io_type io_type)
{
	struct req_iterator rq_iter;
	struct bio_vec bvec;
	struct request *rq;
	__u16 insert_pid[BTAG_PIDLOG_ENTRIES] = {0};
	__u32 insert_len[BTAG_PIDLOG_ENTRIES] = {0};
	__u32 insert_cnt = 0;

	*top_len = 0;
	rq = scsi_cmd_to_rq(cmd);
	if (!rq)
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

void mtk_btag_ufs_clk_gating(bool clk_on)
{
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_clk_gating);

void mtk_btag_ufs_send_command(__u16 task_id, struct scsi_cmnd *cmd)
{
	struct mtk_btag_ufs_ctx *ctx;
	struct mtk_btag_ufs_task *tsk;
	enum mtk_btag_io_type io_type;
	__u32 top_len;
	unsigned long flags;

	if (!cmd)
		return;

	io_type = cmd_to_io_type(scsi_cmnd_cmd(cmd));
	if (io_type >= BTAG_IO_TYPE_NR)
		return;

	tsk = mtk_btag_ufs_curr_task(task_id, &ctx);
	if (!tsk || !ctx)
		return;

	tsk->lba = scsi_cmnd_lba(cmd);
	tsk->len = scsi_cmnd_len(cmd);
	tsk->cmd = scsi_cmnd_cmd(cmd);

	btag_ufs_pidlog_insert(&ctx->pidlog, cmd, &top_len, io_type);
	mtk_btag_mictx_eval_req(ufs_mtk_btag, BTAG_UFS_QUEUE_ID(task_id),
				io_type, tsk->len << SECTOR_SHIFT, top_len);

	spin_lock_irqsave(&ctx->lock, flags);

	tsk->t[tsk_send_cmd] = sched_clock();
	tsk->t[tsk_req_compl] = 0;

	ctx->sum_of_inflight_start += tsk->t[tsk_send_cmd];
	if (!ctx->period_start_t)
		ctx->period_start_t = tsk->t[tsk_send_cmd];

	ctx->q_depth++;
	mtk_btag_mictx_update(ufs_mtk_btag, BTAG_UFS_QUEUE_ID(task_id),
			      ctx->q_depth, ctx->sum_of_inflight_start);

	spin_unlock_irqrestore(&ctx->lock, flags);
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_send_command);

__u16 mtk_btag_ufs_mictx_eval_wqd(struct mtk_btag_mictx_data *data,
				  u64 t_cur)
{
	__u64 compl = data->weighted_qd;
	__u64 inflight = t_cur * data->q_depth - data->sum_of_inflight_start;
	__u64 dur = t_cur - data->window_begin;

	return DIV64_U64_ROUND_UP(compl + inflight, dur);
}

void mtk_btag_ufs_transfer_req_compl(__u16 task_id, unsigned long req_mask)
{
	struct mtk_btag_ufs_ctx *ctx;
	struct mtk_btag_ufs_task *tsk;
	unsigned long flags;
	enum mtk_btag_io_type io_type;
	__u64 busy_time;
	__u32 size;

	tsk = mtk_btag_ufs_curr_task(task_id, &ctx);
	if (!tsk || !ctx)
		return;

	/* return if there's no on-going request  */
	if (!tsk->t[tsk_send_cmd])
		return;

	spin_lock_irqsave(&ctx->lock, flags);

	tsk->t[tsk_req_compl] = sched_clock();

	io_type = cmd_to_io_type(tsk->cmd);

	/* throughput usage := duration of handling this request */
	busy_time = tsk->t[tsk_req_compl] - tsk->t[tsk_send_cmd];

	/* workload statistics */
	ctx->workload.count++;

	if (io_type < BTAG_IO_TYPE_NR) {
		size = tsk->len << SECTOR_SHIFT;
		ctx->throughput[io_type].usage += busy_time;
		ctx->throughput[io_type].size += size;
		mtk_btag_mictx_eval_tp(ufs_mtk_btag, BTAG_UFS_QUEUE_ID(task_id),
				       io_type, busy_time, size);
	}

	ctx->sum_of_inflight_start -= tsk->t[tsk_send_cmd];
	if (!req_mask)
		ctx->q_depth = 0;
	else
		ctx->q_depth--;
	mtk_btag_mictx_update(ufs_mtk_btag, BTAG_UFS_QUEUE_ID(task_id),
			      ctx->q_depth, ctx->sum_of_inflight_start);
	mtk_btag_mictx_accumulate_weight_qd(ufs_mtk_btag,
					    BTAG_UFS_QUEUE_ID(task_id),
					    tsk->t[tsk_send_cmd],
					    tsk->t[tsk_req_compl]);

	/* clear this task */
	tsk->t[tsk_send_cmd] = tsk->t[tsk_req_compl] = 0;

	spin_unlock_irqrestore(&ctx->lock, flags);
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_transfer_req_compl);

/* print context to trace ring buffer */
static void mtk_btag_ufs_work(struct work_struct *work)
{
	struct mtk_btag_ringtrace *rt = BTAG_RT(ufs_mtk_btag);
	struct mtk_btag_ufs_ctx *ctx;
	struct mtk_btag_trace *tr;
	unsigned long flags;
	__u64 time;
	__u32 idx;

	if (!rt)
		return;

	for (idx = 0; idx < ufs_mtk_btag->ctx.count; idx++) {
		spin_lock_irqsave(&rt->lock, flags);
		tr = mtk_btag_curr_trace(rt);
		if (!tr) {
			spin_unlock_irqrestore(&rt->lock, flags);
			break;
		}

		memset(tr, 0, sizeof(struct mtk_btag_trace));
		tr->pid = 0;
		tr->qid = idx;

		ctx = mtk_btag_ufs_curr_ctx(idx);
		if (!ctx) {
			spin_unlock_irqrestore(&rt->lock, flags);
			break;
		}

		spin_lock(&ctx->lock);
		time = sched_clock();
		if (time - ctx->period_start_t < BTAG_UFS_TRACE_LATENCY) {
			spin_unlock(&ctx->lock);
			spin_unlock_irqrestore(&rt->lock, flags);
			continue;
		}

		tr->time = time;
		mtk_btag_pidlog_eval(&tr->pidlog, &ctx->pidlog);
		mtk_btag_vmstat_eval(&tr->vmstat);
		mtk_btag_cpu_eval(&tr->cpu);
		memcpy(tr->throughput, ctx->throughput,
		       sizeof(struct mtk_btag_throughput) * BTAG_IO_TYPE_NR);
		memcpy(&tr->workload, &ctx->workload, sizeof(struct mtk_btag_workload));

		ctx->period_start_t = tr->time;
		ctx->period_end_t = 0;
		ctx->period_usage = 0;
		memset(ctx->throughput, 0,
		       sizeof(struct mtk_btag_throughput) * BTAG_IO_TYPE_NR);
		memset(&ctx->workload, 0, sizeof(struct mtk_btag_workload));
		spin_unlock(&ctx->lock);

		mtk_btag_next_trace(rt);
		spin_unlock_irqrestore(&rt->lock, flags);
	}
}

/* evaluate throughput and workload of given context */
static void mtk_btag_ufs_ctx_eval(struct mtk_btag_ufs_ctx *ctx)
{
	__u64 period;

	ctx->workload.usage = ctx->period_usage;

	if (ctx->workload.period > (ctx->workload.usage * 100)) {
		ctx->workload.percent = 1;
	} else {
		period = ctx->workload.period;
		do_div(period, 100);
		ctx->workload.percent =
			(__u32)ctx->workload.usage / (__u32)period;
	}
	mtk_btag_throughput_eval(ctx->throughput);
}

static void mtk_btag_ufs_ctx_count_usage(struct mtk_btag_ufs_ctx *ctx,
					 __u64 start, __u64 end)
{
	__u64 busy_in_period;

	if (start < ctx->period_start_t)
		busy_in_period = end - ctx->period_start_t;
	else
		busy_in_period = end - start;

	ctx->period_usage += busy_in_period;
}

/* Check requests after set/clear mask. */
void mtk_btag_ufs_check(__u16 task_id, unsigned long req_mask)
{
	struct mtk_btag_ufs_ctx *ctx;
	__u64 end_time, period_time;
	unsigned long flags;

	ctx = mtk_btag_ufs_curr_ctx(task_id);
	if (!ctx)
		return;

	end_time = sched_clock();

	spin_lock_irqsave(&ctx->lock, flags);

	if (ctx->busy_start_t)
		mtk_btag_ufs_ctx_count_usage(ctx, ctx->busy_start_t, end_time);

	ctx->busy_start_t = (req_mask) ? end_time : 0;

	period_time = end_time - ctx->period_start_t;

	if (period_time >= BTAG_UFS_TRACE_LATENCY) {
		ctx->period_end_t = end_time;
		ctx->workload.period = period_time;
		mtk_btag_ufs_ctx_eval(ctx);
		queue_work(ufs_mtk_btag_wq, &ufs_mtk_btag_worker);
	}
	spin_unlock_irqrestore(&ctx->lock, flags);
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_check);

/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
	if (!buff && !evt) { \
		pr_info(fmt, ##args); \
	} \
} while (0)

static size_t mtk_btag_ufs_seq_debug_show_info(char **buff, unsigned long *size,
					       struct seq_file *seq)
{
	return 0;
}

static void mtk_btag_ufs_init_ctx(struct mtk_blocktag *btag)
{
	struct mtk_btag_ufs_ctx *ctx = BTAG_CTX(btag);
	__u64 time = sched_clock();
	int i;

	if (!ctx)
		return;

	memset(ctx, 0, sizeof(struct mtk_btag_ufs_ctx) * btag->ctx.count);
	for (i = 0; i < btag->ctx.count; i++) {
		spin_lock_init(&ctx[i].lock);
		spin_lock_init(&ctx[i].pidlog.lock);
		ctx[i].period_start_t = time;
	}
}

static struct mtk_btag_vops mtk_btag_ufs_vops = {
	.seq_show = mtk_btag_ufs_seq_debug_show_info,
	.mictx_eval_wqd = mtk_btag_ufs_mictx_eval_wqd,
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
		mtk_btag_ufs_vops.earaio_enabled = true;

	if (host->boot_device)
		mtk_btag_ufs_vops.boot_device = true;

#if IS_ENABLED(CONFIG_UFS_MEDIATEK_MCQ)
	hba_priv = (struct ufs_hba_private *)host->hba->android_vendor_data1;
	if (hba_priv->is_mcq_enabled)
		max_queue = hba_priv->mcq_nr_hw_queue;
#endif

	ufs_mtk_btag_wq = alloc_workqueue("ufs_mtk_btag", WQ_FREEZABLE, 1);
	INIT_WORK(&ufs_mtk_btag_worker, mtk_btag_ufs_work);

	btag = mtk_btag_alloc("ufs",
			      BTAG_STORAGE_UFS,
			      BTAG_UFS_RINGBUF_MAX,
			      sizeof(struct mtk_btag_ufs_ctx),
			      max_queue, &mtk_btag_ufs_vops);

	if (btag) {
		mtk_btag_ufs_init_ctx(btag);
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

