// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/tracepoint.h>

#include "apu_tags.h"
#include "apu_tp.h"
#include "mdw_rv.h"
#include "mdw_rv_tag.h"
#include "mdw_rv_events.h"
#include "mdw_cmn.h"

static struct apu_tags *mdw_rv_tags;

enum mdw_tag_type {
	MDW_TAG_CMD,
	MDW_TAG_SUBCMD,
};

void mdw_cmd_trace(struct mdw_cmd *c, uint32_t status)
{
	trace_mdw_rv_cmd(status,
		c->pid,
		c->uid,
		c->rvid,
		c->num_subcmds,
		c->priority,
		c->softlimit,
		c->power_dtime,
		c->einfos->c.sc_rets,
		c->power_plcy,
		c->tolerance_ms,
		c->start_ts);
}

/* The parameters must aligned with trace_mdw_rv_cmd() */
static void
probe_rv_mdw_cmd(void *data, uint32_t status, pid_t pid,
		uint64_t uid, uint64_t rvid,
		uint32_t num_subcmds,
		uint32_t priority,
		uint32_t softlimit,
		uint32_t pwr_dtime,
		uint64_t sc_rets,
		uint32_t pwr_plcy,
		uint32_t tolerance,
		uint64_t start_ts)
{
	struct mdw_rv_tag t;

	if (!mdw_rv_tags)
		return;

	t.type = MDW_TAG_CMD;
	t.d.cmd.status = status;
	t.d.cmd.pid = pid;
	t.d.cmd.uid = uid;
	t.d.cmd.rvid = rvid;
	t.d.cmd.num_subcmds = num_subcmds;
	t.d.cmd.priority = priority;
	t.d.cmd.softlimit = softlimit;
	t.d.cmd.pwr_dtime = pwr_dtime;
	t.d.cmd.sc_rets = sc_rets;
	t.d.cmd.pwr_plcy = pwr_plcy;
	t.d.cmd.tolerance = tolerance;
	t.d.cmd.start_ts = start_ts;

	apu_tag_add(mdw_rv_tags, &t);
}

void mdw_subcmd_trace(struct mdw_cmd *c, uint32_t sc_idx,
		uint32_t history_iptime, uint32_t status)
{

	struct mdw_subcmd_exec_info *sc_einfo = NULL;

	sc_einfo = &c->einfos->sc;

	trace_mdw_rv_subcmd(status,
		c->rvid,
		c->subcmds[sc_idx].type,
		sc_idx,
		sc_einfo[sc_idx].ip_start_ts,
		sc_einfo[sc_idx].ip_end_ts,
		sc_einfo[sc_idx].was_preempted,
		sc_einfo[sc_idx].executed_core_bitmap,
		sc_einfo[sc_idx].tcm_usage,
		history_iptime);
}

/* The parameters must aligned with trace_mdw_rv_subcmd() */
static void
probe_rv_mdw_subcmd(void *data, uint32_t status,
		uint64_t rvid,
		uint32_t sc_type,
		uint32_t sc_idx,
		uint32_t ipstart_ts,
		uint32_t ipend_ts,
		uint32_t was_preempted,
		uint32_t executed_core_bmp,
		uint32_t tcm_usage,
		uint32_t history_iptime)
{
	struct mdw_rv_tag t;

	if (!mdw_rv_tags)
		return;

	t.type = MDW_TAG_SUBCMD;
	t.d.subcmd.status = status;
	t.d.subcmd.rvid = rvid;
	t.d.subcmd.sc_type = sc_type;
	t.d.subcmd.sc_idx = sc_idx;
	t.d.subcmd.ipstart_ts = ipstart_ts;
	t.d.subcmd.ipend_ts = ipend_ts;
	t.d.subcmd.was_preempted = was_preempted;
	t.d.subcmd.executed_core_bmp = executed_core_bmp;
	t.d.subcmd.tcm_usage = tcm_usage;
	t.d.subcmd.history_iptime = history_iptime;

	apu_tag_add(mdw_rv_tags, &t);
}

static void mdw_rv_tag_seq_cmd(struct seq_file *s, struct mdw_rv_tag *t)
{
	char status[8] = "";

	if (t->d.cmd.status == MDW_CMD_DONE) {
		if (snprintf(status, sizeof(status)-1, "%s", "done") < 0)
			return;
	} else if (t->d.cmd.status == MDW_CMD_START) {
		if (snprintf(status, sizeof(status)-1, "%s", "start") < 0)
			return;
	} else if (t->d.cmd.status == MDW_CMD_ENQUE) {
		if (snprintf(status, sizeof(status)-1, "%s", "enque") < 0)
			return;
	}

	seq_printf(s, "%s,", status);
	seq_printf(s, "pid=%d,uid=0x%llx,rvid=0x%llx,",
		t->d.cmd.pid, t->d.cmd.uid, t->d.cmd.rvid);
	seq_printf(s, "num_subcmds=%u,", t->d.cmd.num_subcmds);
	seq_printf(s, "priority=%u,softlimit=%u,",
		t->d.cmd.priority, t->d.cmd.softlimit);
	seq_printf(s, "pwr_dtime=%u,sc_rets=0x%llx,",
		t->d.cmd.pwr_dtime, t->d.cmd.sc_rets);
	seq_printf(s, "pwr_plcy=%x,tolerance=%x,start_ts=0x%llx\n",
		t->d.cmd.pwr_plcy, t->d.cmd.tolerance, t->d.cmd.start_ts);
}

static void mdw_rv_tag_seq_subcmd(struct seq_file *s, struct mdw_rv_tag *t)
{
	char status[8] = "";
	char sc_idx[8] = "";

	if (snprintf(status, sizeof(status)-1, "%s", "sched") < 0)
		return;

	if (snprintf(sc_idx, sizeof(sc_idx)-1, "sc#%d:", t->d.subcmd.sc_idx) < 0)
		return;

	seq_printf(s, "%s,", status);
	seq_printf(s, "rvid=0x%llx,%s", t->d.subcmd.rvid, sc_idx);
	seq_printf(s, "type=%u,ipstart_ts=0x%x,ipend_ts=0x%x,",
		t->d.subcmd.sc_type ,t->d.subcmd.ipstart_ts,
		t->d.subcmd.ipend_ts);
	seq_printf(s, "was_preempted=0x%x,exc_core_bmp=0x%x,",
		t->d.subcmd.was_preempted,
		t->d.subcmd.executed_core_bmp);
	seq_printf(s, "tcm_usage=0x%x,h_iptime=%u\n",
		t->d.subcmd.tcm_usage, t->d.subcmd.history_iptime);
}

static int mdw_rv_tag_seq(struct seq_file *s, void *tag, void *priv)
{
	struct mdw_rv_tag *t = (struct mdw_rv_tag *)tag;

	if (!t)
		return -ENOENT;

	if (t->type == MDW_TAG_CMD)
		mdw_rv_tag_seq_cmd(s, t);
	else if (t->type == MDW_TAG_SUBCMD)
		mdw_rv_tag_seq_subcmd(s, t);

	return 0;
}

static int mdw_rv_tag_seq_info(struct seq_file *s, void *tag, void *priv)
{
	return 0;
}

static struct apu_tp_tbl mdw_rv_tp_tbl[] = {
	{.name = "mdw_rv_cmd", .func = probe_rv_mdw_cmd},
	{.name = "mdw_rv_subcmd", .func = probe_rv_mdw_subcmd},
	APU_TP_TBL_END
};

void mdw_rv_tag_show(struct seq_file *s)
{
	apu_tags_seq(mdw_rv_tags, s);
}

int mdw_rv_tag_init(void)
{
	int ret;

	mdw_rv_tags = apu_tags_alloc("mdw", sizeof(struct mdw_rv_tag),
		MDW_TAGS_CNT, mdw_rv_tag_seq, mdw_rv_tag_seq_info, NULL);

	if (!mdw_rv_tags)
		return -ENOMEM;

	ret = apu_tp_init(mdw_rv_tp_tbl);
	if (ret)
		pr_info("%s: unable to register\n", __func__);

	return ret;
}

void mdw_rv_tag_deinit(void)
{
	apu_tp_exit(mdw_rv_tp_tbl);
	apu_tags_free(mdw_rv_tags);
}

