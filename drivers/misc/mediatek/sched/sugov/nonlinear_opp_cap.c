// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/sort.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <sched/autogroup.h>
#include <linux/sched/clock.h>
#include <linux/energy_model.h>
#include "common.h"
#include "cpufreq.h"
#include "sugov_trace.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v2/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif
#include "mediatek-cpufreq-hw_fdvfs.h"
#include "dsu_interface.h"
#include <mt-plat/mtk_irq_mon.h>
#include "eas/group.h"

DEFINE_PER_CPU(unsigned int, gear_id) = -1;
EXPORT_SYMBOL(gear_id);

DEFINE_PER_CPU(struct sbb_cpu_data *, sbb);
EXPORT_SYMBOL(sbb);

static void __iomem *l3ctl_sram_base_addr;
#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
static struct resource *csram_res;
static void __iomem *sram_base_addr;
static struct pd_capacity_info *pd_capacity_tbl;
static struct pd_capacity_info **pd_wl_type;
static struct cpu_weighting **cpu_wt;
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
static void __iomem *sram_base_addr_freq_scaling;
static struct mtk_em_perf_domain *mtk_em_pd_ptr;
static bool freq_scaling_disabled = true;
#endif
static int pd_count;
static int entry_count;
static int busy_tick_boost_all;
static int sbb_active_ratio[MAX_NR_CPUS] = {
	[0 ... MAX_NR_CPUS - 1] = 100 };
static unsigned int wl_type_delay_update_tick = 2;

static int fpsgo_boosting; //0 : disable, 1 : enable
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
void (*flt_get_fpsgo_boosting)(int fpsgo_flag);
EXPORT_SYMBOL(flt_get_fpsgo_boosting);
#endif

/* group aware dvfs */
int grp_dvfs_support_mode;
int grp_dvfs_ctrl_mode;
void init_grp_dvfs(void)
{
	if (grp_dvfs_support_mode)
		pr_info("grp_dvfs enable\n");
}

int get_grp_dvfs_ctrl(void)
{
	return grp_dvfs_ctrl_mode;
}
EXPORT_SYMBOL_GPL(get_grp_dvfs_ctrl);

void set_grp_dvfs_ctrl(int set)
{
	grp_dvfs_ctrl_mode = (set && grp_dvfs_support_mode) ? set : 0;
}
EXPORT_SYMBOL_GPL(set_grp_dvfs_ctrl);

/* adaptive margin */
int am_support;
int am_ctrl; /* 0: disable, 1: calculate adaptive ratio, 2: adaptive margin */
void init_adaptive_margin(void)
{
	if (am_support) {
		am_ctrl = 1;
		pr_info("adaptive-margin enable\n");
	} else {
		am_ctrl = 0;
	}
}
int get_am_ctrl(void)
{
	return am_ctrl;
}
EXPORT_SYMBOL_GPL(get_am_ctrl);

void set_am_ctrl(int set)
{
	am_ctrl = (set && am_support) ? set : 0;
}
EXPORT_SYMBOL_GPL(set_am_ctrl);

/* eas dsu ctrl */
enum {
	REG_FREQ_LUT_TABLE,
	REG_FREQ_ENABLE,
	REG_FREQ_PERF_STATE,
	REG_FREQ_HW_STATE,
	REG_EM_POWER_TBL,
	REG_FREQ_LATENCY,
	REG_ARRAY_SIZE,
};

struct cpufreq_mtk {
	struct cpufreq_frequency_table *table;
	void __iomem *reg_bases[REG_ARRAY_SIZE];
	int nr_opp;
	unsigned int last_index;
	cpumask_t related_cpus;
	long sb_ch;
};
static struct cpu_dsu_freq_state freq_state;
void init_eas_dsu_ctrl(void)
{
	freq_state.is_eas_dsu_support = true;
	freq_state.is_eas_dsu_ctrl = true;
	freq_state.pd_count = pd_count;
	freq_state.cpu_freq = kcalloc(pd_count, sizeof(unsigned int), GFP_KERNEL);
	freq_state.dsu_freq_vote = kcalloc(pd_count, sizeof(unsigned int), GFP_KERNEL);
	pr_info("eas_dsu_sup.=%d\n", freq_state.is_eas_dsu_support);
}

bool get_eas_dsu_ctrl(void)
{
	return freq_state.is_eas_dsu_ctrl;
}
EXPORT_SYMBOL_GPL(get_eas_dsu_ctrl);

static int wl_type_manual = -1;
void set_eas_dsu_ctrl(bool set)
{
	int i;

	if (freq_state.is_eas_dsu_support) {
		iowrite32(set ? 1 : 0, l3ctl_sram_base_addr + 0x1C);
		freq_state.is_eas_dsu_ctrl = set;
		if (freq_state.is_eas_dsu_ctrl == false) {
			wl_type_manual = 0;
			freq_state.dsu_target_freq = 0;
			for (i = 0; i < freq_state.pd_count; i++)
				freq_state.dsu_freq_vote[i] = 0;
		} else
			wl_type_manual = -1;
	}
}
EXPORT_SYMBOL_GPL(set_eas_dsu_ctrl);

struct cpu_dsu_freq_state *get_dsu_freq_state(void)
{
	return &freq_state;
}
EXPORT_SYMBOL_GPL(get_dsu_freq_state);

void set_dsu_target_freq(struct cpufreq_policy *policy)
{
	int i, cpu = policy->cpu, opp, dsu_target_freq = 0;
	unsigned int wl_type = get_em_wl();
	struct pd_capacity_info *pd_info;
	struct mtk_em_perf_state *ps;
	struct cpufreq_mtk *c = policy->driver_data;

	freq_state.cpu_freq[per_cpu(gear_id, cpu)] = policy->cached_target_freq;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];
		cpu = cpumask_first(&pd_info->cpus);
		if (pd_info->nr_cpus == 1)
			if (available_idle_cpu(cpu)) {
				freq_state.dsu_freq_vote[i] = 0;
				goto skip_single_idle_cpu;
			}

		ps = pd_get_freq_ps(wl_type, cpu, freq_state.cpu_freq[i], &opp);
		freq_state.dsu_freq_vote[i] = ps->dsu_freq;

		if (dsu_target_freq < ps->dsu_freq)
			dsu_target_freq = ps->dsu_freq;

skip_single_idle_cpu:
		if (trace_sugov_ext_dsu_freq_vote_enabled())
			trace_sugov_ext_dsu_freq_vote(wl_type, i,
				 freq_state.cpu_freq[i], freq_state.dsu_freq_vote[i]);
	}

	freq_state.dsu_target_freq = dsu_target_freq;
	c->sb_ch = dsu_target_freq;
	return;
}

int wl_type_delay_ch_cnt = 1; // change counter
static struct dsu_table dsu_tbl;
static int nr_wl_type = 1;
static int wl_type_curr;
static int wl_type_delay;
static int wl_type_delay_cnt;
static int last_wl_type;
static unsigned long last_jiffies;
static DEFINE_SPINLOCK(update_wl_tbl_lock);

int get_nr_wl_type(void)
{
	return nr_wl_type;
}
EXPORT_SYMBOL_GPL(get_nr_wl_type);

int get_nr_cpu_type(void)
{
	return mtk_mapping.nr_cpu_type;
}
EXPORT_SYMBOL_GPL(get_nr_cpu_type);

int get_cpu_type(int type)
{
	if (type < mtk_mapping.total_type)
		return mtk_mapping.cpu_to_dsu[type].cpu_type;
	else
		return -1;
}
EXPORT_SYMBOL_GPL(get_cpu_type);

void set_wl_type_manual(int val)
{
	if (val >= 0 && val < nr_wl_type && is_wl_support())
		wl_type_manual = val;
	else
		wl_type_manual = -1;
}
EXPORT_SYMBOL_GPL(set_wl_type_manual);

int get_wl_type_manual(void)
{
	return wl_type_manual;
}
EXPORT_SYMBOL_GPL(get_wl_type_manual);

void update_wl_tbl(int cpu)
{
	int tmp = 0;

	if (spin_trylock(&update_wl_tbl_lock)) {
		unsigned long tmp_jiffies = jiffies;

		if (last_jiffies !=  tmp_jiffies) {
			last_jiffies =  tmp_jiffies;
			spin_unlock(&update_wl_tbl_lock);
			if (wl_type_manual == -1)
				tmp = get_wl(0);
			else
				tmp = wl_type_manual;
			if (tmp >= 0 && tmp < nr_wl_type)
				wl_type_curr = tmp;
			if (last_wl_type != wl_type_curr && wl_type_curr >= 0
					&& wl_type_curr < nr_wl_type) {
				int i, j;
				struct pd_capacity_info *pd_info;

				pd_capacity_tbl = pd_wl_type[wl_type_curr];
				for (i = 0; i < pd_count; i++) {
					mtk_update_wl_table(i, wl_type_curr);
					pd_info = &pd_capacity_tbl[i];
					for_each_cpu(j, &pd_info->cpus)
						per_cpu(cpu_scale, j) = pd_info->table[0].capacity;
				}
			}
			last_wl_type = wl_type_curr;
			if (trace_sugov_ext_wl_type_enabled())
				trace_sugov_ext_wl_type(per_cpu(gear_id, cpu), cpu, wl_type_curr);
			if (wl_type_delay != wl_type_curr) {
				wl_type_delay_cnt++;
				if (wl_type_delay_cnt > wl_type_delay_update_tick) {
					wl_type_delay_cnt = 0;
					wl_type_delay = wl_type_curr;
					wl_type_delay_ch_cnt++;
				}
			} else
				wl_type_delay_cnt = 0;
		} else
			spin_unlock(&update_wl_tbl_lock);
	}
}
EXPORT_SYMBOL_GPL(update_wl_tbl);

struct dsu_state *dsu_get_opp_ps(int wl_type, int opp)
{
	if (wl_type < 0)
		wl_type = wl_type_curr;
	opp = clamp_val(opp, 0, mtk_dsu_em.nr_perf_states - 1);
	return &dsu_tbl.tbl[wl_type][opp];
}
EXPORT_SYMBOL_GPL(dsu_get_opp_ps);

unsigned int dsu_get_freq_opp(unsigned int freq)
{
	unsigned int idx;

	freq = clamp(freq, dsu_tbl.freq_min, dsu_tbl.freq_max);
	idx = (dsu_tbl.freq_max - freq) >> dsu_tbl.min_gap_log2;
	idx = dsu_tbl.opp_map[idx];
	if (dsu_tbl.tbl[0][idx].freq < freq)
		idx--;
	return idx;
}
EXPORT_SYMBOL_GPL(dsu_get_freq_opp);

int get_curr_wl(void)
{
	return clamp_val(wl_type_curr, 0, nr_wl_type - 1);
}
EXPORT_SYMBOL_GPL(get_curr_wl);

int get_classify_wl(void)
{
	return get_wl(0);
}
EXPORT_SYMBOL_GPL(get_classify_wl);

int get_em_wl(void)
{
	return clamp_val(wl_type_delay, 0, nr_wl_type - 1);
}
EXPORT_SYMBOL_GPL(get_em_wl);

int init_dsu(void)
{
	unsigned int i, t, need_alloc;
	unsigned int min_gap = UINT_MAX;
	int k, next_k;

	dsu_tbl.tbl = kcalloc(nr_wl_type, sizeof(struct dsu_state *),
			GFP_KERNEL);

	for (t = 0; t < nr_wl_type; t++) {
		need_alloc = 1;
		for (k = t - 1; k >= 0; k--) {
			if (mtk_mapping.cpu_to_dsu[t].dsu_type
					== mtk_mapping.cpu_to_dsu[k].dsu_type) {
				dsu_tbl.tbl[t] = dsu_tbl.tbl[k];
				need_alloc = 0;
				break;
			}
		}

		if (need_alloc == 0)
			continue;
		else {
			dsu_tbl.tbl[t] = kcalloc(mtk_dsu_em.nr_perf_states,
				sizeof(struct dsu_state), GFP_KERNEL);
			if (!dsu_tbl.tbl[t])
				return -ENOMEM;
		}

		mtk_update_wl_table(0, t);
		for (i = 0; i < mtk_dsu_em.nr_perf_states; i++) {
			dsu_tbl.tbl[t][i].freq = mtk_dsu_em.dsu_table[i].dsu_frequency;
			dsu_tbl.tbl[t][i].volt = mtk_dsu_em.dsu_table[i].dsu_volt;
			dsu_tbl.tbl[t][i].dyn_pwr = mtk_dsu_em.dsu_table[i].dynamic_power;
			dsu_tbl.tbl[t][i].BW = mtk_dsu_em.dsu_table[i].dsu_bandwidth;
			dsu_tbl.tbl[t][i].EMI_BW = mtk_dsu_em.dsu_table[i].emi_bandwidth;
			if (i > 0 && t == 0)
				min_gap = min(min_gap, dsu_tbl.tbl[t][i - 1].freq
					- dsu_tbl.tbl[t][i].freq);
		}

		if (t != 0) /* the O1 mapping table only need init once */
			continue;
		dsu_tbl.nr_opp = mtk_dsu_em.nr_perf_states;
		dsu_tbl.freq_max = mtk_dsu_em.dsu_table[0].dsu_frequency;
		dsu_tbl.freq_min = mtk_dsu_em.dsu_table[dsu_tbl.nr_opp - 1].dsu_frequency;
		dsu_tbl.min_gap_log2 =
			min_t(unsigned int, ilog2(min_gap), sizeof(unsigned int) * 8);
		dsu_tbl.nr_opp_map = (dsu_tbl.freq_max - dsu_tbl.freq_min) >> dsu_tbl.min_gap_log2;
		dsu_tbl.opp_map = kcalloc(dsu_tbl.nr_opp_map + 1, sizeof(unsigned int), GFP_KERNEL);
		for (i = 0; i < dsu_tbl.nr_opp; i++) {
			k = (dsu_tbl.freq_max - dsu_tbl.tbl[t][i].freq) >> dsu_tbl.min_gap_log2;
			next_k = (dsu_tbl.freq_max -
				dsu_tbl.tbl[t][min(dsu_tbl.nr_opp - 1, i + 1)].freq)
				>> dsu_tbl.min_gap_log2;
			for (; k <= next_k; k++)
				dsu_tbl.opp_map[k] = i;
		}
	}
	mtk_update_wl_table(0, 0);
	return 0;
}

void init_sbb_cpu_data(void)
{
	int cpu;
	struct sbb_cpu_data *data;

	for_each_possible_cpu(cpu) {
		data = kcalloc(1, sizeof(struct sbb_cpu_data), GFP_KERNEL);
		per_cpu(sbb, cpu) = data;
	}
}
EXPORT_SYMBOL_GPL(init_sbb_cpu_data);

void set_system_sbb(bool set)
{
	busy_tick_boost_all = set;
}
EXPORT_SYMBOL_GPL(set_system_sbb);

bool get_system_sbb(void)
{
	return busy_tick_boost_all;
}
EXPORT_SYMBOL_GPL(get_system_sbb);

void set_sbb(int flag, int pid, bool set)
{
	struct task_struct *p, *group_leader;

	switch (flag) {
	case SBB_ALL:
		set_system_sbb(set);
		break;
	case SBB_GROUP:
		rcu_read_lock();
		p = find_task_by_vpid(pid);
		if (p && p->exit_state == 0) {
			get_task_struct(p);
			group_leader = p->group_leader;
			if (group_leader && group_leader->exit_state == 0) {
				struct sbb_task_struct *sts;

				get_task_struct(group_leader);
				sts = &((struct mtk_task *)
					group_leader->android_vendor_data1)->sbb_task;
				sts->set_group = set;
				put_task_struct(group_leader);
			}
			put_task_struct(p);
		}
		rcu_read_unlock();
		break;
	case SBB_TASK:
		rcu_read_lock();
		p = find_task_by_vpid(pid);
		if (p) {
			struct sbb_task_struct *sts;

			get_task_struct(p);
			sts = &((struct mtk_task *) p->android_vendor_data1)->sbb_task;
			sts->set_task = set;
			put_task_struct(p);
		}
		rcu_read_unlock();
	}
}
EXPORT_SYMBOL_GPL(set_sbb);

bool is_sbb_trigger(struct rq *rq)
{
	bool sbb_trigger = false;
	struct task_struct *curr, *group_leader;

	rcu_read_lock();
	curr = rcu_dereference(rq->curr);
	if (curr && curr->exit_state == 0) {
		struct sbb_task_struct *sts;

		sts = &((struct mtk_task *) curr->android_vendor_data1)->sbb_task;
		sbb_trigger |= sts->set_task;
		group_leader = curr->group_leader;
		if (group_leader && group_leader->exit_state == 0) {
			get_task_struct(group_leader);
			sts = &((struct mtk_task *) group_leader->android_vendor_data1)->sbb_task;
			sbb_trigger |= sts->set_group;
			put_task_struct(group_leader);
		}
	}
	rcu_read_unlock();
	sbb_trigger |= busy_tick_boost_all;

	return sbb_trigger;
}
EXPORT_SYMBOL_GPL(is_sbb_trigger);

void set_sbb_active_ratio(int val)
{
	int i;

	for (i = 0; i < pd_count; i++)
		sbb_active_ratio[i] = val;
}
EXPORT_SYMBOL_GPL(set_sbb_active_ratio);

void set_sbb_active_ratio_gear(int gear_id, int val)
{
	sbb_active_ratio[gear_id] = val;
}
EXPORT_SYMBOL_GPL(set_sbb_active_ratio_gear);

int get_sbb_active_ratio_gear(int gear_id)
{
	return sbb_active_ratio[gear_id];
}
EXPORT_SYMBOL_GPL(get_sbb_active_ratio_gear);

unsigned int pd_get_dsu_weighting(int wl_type, int cpu)
{
	int i;

	i = per_cpu(gear_id, cpu);
	if (wl_type < 0)
		wl_type = wl_type_curr;
	return cpu_wt[i][wl_type].dsu_weighting;
}
EXPORT_SYMBOL_GPL(pd_get_dsu_weighting);

unsigned int pd_get_emi_weighting(int wl_type, int cpu)
{
	int i;

	i = per_cpu(gear_id, cpu);
	if (wl_type < 0)
		wl_type = wl_type_curr;
	return cpu_wt[i][wl_type].emi_weighting;
}
EXPORT_SYMBOL_GPL(pd_get_emi_weighting);

bool is_gearless_support(void)
{
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	return !freq_scaling_disabled;
#else
	return false;
#endif
}
EXPORT_SYMBOL_GPL(is_gearless_support);

unsigned int get_nr_gears(void)
{
	return pd_count;
}
EXPORT_SYMBOL_GPL(get_nr_gears);

struct cpumask *get_gear_cpumask(unsigned int gear)
{
	struct pd_capacity_info *pd_info;

	pd_info = &pd_capacity_tbl[gear];
	return &pd_info->cpus;
}
EXPORT_SYMBOL_GPL(get_gear_cpumask);

static inline int map_freq_idx_by_tbl(struct pd_capacity_info *pd_info, unsigned long freq)
{
	int idx;

	freq = min_t(unsigned int, freq, pd_info->table[0].freq);
	if (freq <= pd_info->freq_min)
		return pd_info->nr_freq_opp_map - 1;

	idx = mul_u64_u32_shr((u32) pd_info->table[0].freq - freq, pd_info->inv_DFreq, 32);
	return idx;
}

static inline int map_util_idx_by_tbl(struct pd_capacity_info *pd_info, unsigned long util)
{
	int idx;

	util = clamp_val(util, pd_info->table[pd_info->nr_caps - 1].capacity,
		pd_info->table[0].capacity);
	idx = pd_info->table[0].capacity - util;
	return idx;
}

struct mtk_em_perf_state *pd_get_util_ps(int wl_type, int cpu, unsigned long util, int *opp)
{
	int i, idx;
	struct pd_capacity_info *pd_info;
	struct mtk_em_perf_state *ps;

	i = per_cpu(gear_id, cpu);
	if (wl_type < 0 || wl_type >= nr_wl_type)
		wl_type = wl_type_curr;
	pd_info = &pd_wl_type[wl_type][i];

	idx = map_util_idx_by_tbl(pd_info, util);
	idx = pd_info->util_opp_map[idx];
	ps = &pd_info->table[idx];
	*opp = idx;
	return ps;
}
EXPORT_SYMBOL_GPL(pd_get_util_ps);

unsigned long pd_get_util_opp(int cpu, unsigned long util)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_util_idx_by_tbl(pd_info, util);
	return pd_info->util_opp_map[idx];
}
EXPORT_SYMBOL_GPL(pd_get_util_opp);

unsigned long pd_get_util_opp_legacy(int cpu, unsigned long util)
{
	int i, idx, wl_type = get_em_wl();
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_wl_type[wl_type][i];
	idx = map_util_idx_by_tbl(pd_info, util);
	return pd_info->util_opp_map_legacy[idx];
}
EXPORT_SYMBOL_GPL(pd_get_util_opp_legacy);

unsigned long pd_get_util_freq(int cpu, unsigned long util)
{
	int i, idx, wl_type = get_em_wl();
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_wl_type[wl_type][i];
	idx = map_util_idx_by_tbl(pd_info, util);
	idx = pd_info->util_opp_map[idx];
	return max(pd_info->table[idx].freq, pd_info->freq_min);
}
EXPORT_SYMBOL_GPL(pd_get_util_freq);

unsigned long pd_get_util_pwr_eff(int cpu, unsigned long util)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_util_idx_by_tbl(pd_info, util);
	idx = pd_info->util_opp_map[idx];
	return pd_info->table[idx].pwr_eff;
}
EXPORT_SYMBOL_GPL(pd_get_util_pwr_eff);

struct mtk_em_perf_state *pd_get_freq_ps(int wl_type, int cpu, unsigned long freq,
		int *opp)
{
	int i, idx;
	struct pd_capacity_info *pd_info;
	struct mtk_em_perf_state *ps;

	i = per_cpu(gear_id, cpu);
	if (wl_type < 0)
		wl_type = wl_type_curr;
	pd_info = &pd_wl_type[wl_type][i];
	idx = map_freq_idx_by_tbl(pd_info, freq);
	idx = pd_info->freq_opp_map[idx];
	ps = &pd_info->table[idx];
	*opp = idx;
	return ps;
}
EXPORT_SYMBOL_GPL(pd_get_freq_ps);

struct mtk_em_perf_state *pd_get_opp_ps(int wl_type, int cpu, int opp, bool quant)
{
	int i;
	struct pd_capacity_info *pd_info;
	struct mtk_em_perf_state *ps;

	i = per_cpu(gear_id, cpu);
	if (wl_type < 0 || wl_type >= nr_wl_type)
		wl_type = wl_type_curr;
	pd_info = &pd_wl_type[wl_type][i];

	opp = clamp_val(opp, 0, (quant ? pd_info->nr_caps_legacy : pd_info->nr_caps) - 1);
	if (quant)
		ps = &pd_info->table_legacy[opp];
	else
		ps = &pd_info->table[opp];

	return ps;
}
EXPORT_SYMBOL_GPL(pd_get_opp_ps);

unsigned long pd_get_freq_opp(int cpu, unsigned long freq)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_freq_idx_by_tbl(pd_info, freq);
	return pd_info->freq_opp_map[idx];
}
EXPORT_SYMBOL_GPL(pd_get_freq_opp);

unsigned long pd_get_freq_opp_legacy(int cpu, unsigned long freq)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];

	if (freq <= pd_info->freq_min)
		return pd_info->freq_opp_map_legacy[pd_info->nr_freq_opp_map - 1];

	idx = map_freq_idx_by_tbl(pd_info, freq);
	if (freq > pd_get_opp_freq_legacy(cpu, pd_info->freq_opp_map_legacy[idx] + 1))
		return pd_info->freq_opp_map_legacy[idx];
	else
		return pd_info->freq_opp_map_legacy[idx] + 1;
}
EXPORT_SYMBOL_GPL(pd_get_freq_opp_legacy);

unsigned long pd_get_freq_util(int cpu, unsigned long freq)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_freq_idx_by_tbl(pd_info, freq);
	idx = pd_info->freq_opp_map[idx];
	return pd_info->table[idx].capacity;
}
EXPORT_SYMBOL_GPL(pd_get_freq_util);

unsigned long pd_get_freq_pwr_eff(int cpu, unsigned long freq)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_freq_idx_by_tbl(pd_info, freq);
	idx = pd_info->freq_opp_map[idx];
	return pd_info->table[idx].pwr_eff;
}
EXPORT_SYMBOL_GPL(pd_get_freq_pwr_eff);

unsigned long pd_get_opp_freq(int cpu, int opp)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = clamp_val(opp, 0, pd_info->nr_caps - 1);
	return max(pd_info->table[opp].freq, pd_info->freq_min);
}
EXPORT_SYMBOL_GPL(pd_get_opp_freq);

unsigned long pd_get_opp_capacity(int cpu, int opp)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = clamp_val(opp, 0, pd_info->nr_caps - 1);
	return pd_info->table[opp].capacity;
}
EXPORT_SYMBOL_GPL(pd_get_opp_capacity);

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
unsigned long pd_get_opp_capacity_legacy(int cpu, int opp)
{
	int i, wl_type = get_em_wl();
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_wl_type[wl_type][i];
	opp = clamp_val(opp, 0,
		mtk_em_pd_ptr_public[i].nr_perf_states - 1);
	return pd_info->table_legacy[opp].capacity;
}
#else
unsigned long pd_get_opp_capacity_legacy(int cpu, int opp)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = clamp_val(opp, 0, pd_info->nr_caps - 1);
	return pd_info->table[opp].capacity;
}
#endif
EXPORT_SYMBOL_GPL(pd_get_opp_capacity_legacy);


#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
unsigned long pd_get_opp_freq_legacy(int cpu, int opp)
{
	int i;

	i = per_cpu(gear_id, cpu);
	opp = clamp_val(opp, 0,
		mtk_em_pd_ptr_public[i].nr_perf_states - 1);
	return mtk_em_pd_ptr_public[i].table[opp].freq;
}
#else
unsigned long pd_get_opp_freq_legacy(int cpu, int opp)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = clamp_val(opp, 0, pd_info->nr_caps - 1);
	return pd_info->table[opp].freq;
}
#endif
EXPORT_SYMBOL_GPL(pd_get_opp_freq_legacy);

unsigned long pd_get_opp_pwr_eff(int cpu, int opp)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = clamp_val(opp, 0, pd_info->nr_caps - 1);
	return pd_info->table[opp].pwr_eff;
}
EXPORT_SYMBOL_GPL(pd_get_opp_pwr_eff);

unsigned long pd_get_opp_to(int cpu, unsigned long input, enum sugov_type out_type, bool quant)
{
	switch (out_type) {
	case CAP:
		return quant ? pd_get_opp_capacity_legacy(cpu, input) :
			pd_get_opp_capacity(cpu, input);
	case FREQ:
		return quant ? pd_get_opp_freq_legacy(cpu, input) : pd_get_opp_freq(cpu, input);
	case PWR_EFF:
		return pd_get_opp_pwr_eff(cpu, input);
	default:
		return -EINVAL;
	}
}

unsigned long pd_get_util_to(int cpu, unsigned long input, enum sugov_type out_type, bool quant)
{
	switch (out_type) {
	case OPP:
		return quant ? pd_get_util_opp_legacy(cpu, input) : pd_get_util_opp(cpu, input);
	case FREQ:
		return pd_get_util_freq(cpu, input);
	case PWR_EFF:
		return pd_get_util_pwr_eff(cpu, input);
	default:
		return -EINVAL;
	}
}

unsigned long pd_get_freq_to(int cpu, unsigned long input, enum sugov_type out_type, bool quant)
{
	switch (out_type) {
	case OPP:
		return quant ? pd_get_freq_opp_legacy(cpu, input) : pd_get_freq_opp(cpu, input);
	case CAP:
		return pd_get_freq_util(cpu, input);
	case PWR_EFF:
		return pd_get_freq_pwr_eff(cpu, input);
	default:
		return -EINVAL;
	}
}

unsigned long pd_X2Y(int cpu, unsigned long input, enum sugov_type in_type,
		enum sugov_type out_type, bool quant)
{
	switch (in_type) {
	case OPP:
		return pd_get_opp_to(cpu, input, out_type, quant);
	case CAP:
		return pd_get_util_to(cpu, input, out_type, quant);
	case FREQ:
		return pd_get_freq_to(cpu, input, out_type, quant);
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(pd_X2Y);

unsigned int pd_get_cpu_opp(int cpu)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	return pd_info->nr_caps;
}
EXPORT_SYMBOL_GPL(pd_get_cpu_opp);

unsigned int pd_get_cpu_gear_id(int cpu)
{
	return per_cpu(gear_id, cpu);
}
EXPORT_SYMBOL_GPL(pd_get_cpu_gear_id);

unsigned int pd_get_opp_leakage(unsigned int cpu, unsigned int opp, unsigned int temperature)
{
	return mtk_get_leakage(cpu, opp, temperature);
}
EXPORT_SYMBOL_GPL(pd_get_opp_leakage);

void Adaptive_module_bypass(int fpsgo_flag)
{
	fpsgo_boosting = fpsgo_flag;
	if (flt_get_fpsgo_boosting)
		flt_get_fpsgo_boosting(!fpsgo_boosting);
}
EXPORT_SYMBOL_GPL(Adaptive_module_bypass);

int get_fpsgo_bypass_flag(void)
{
	return fpsgo_boosting;
}
EXPORT_SYMBOL_GPL(get_fpsgo_bypass_flag);

static void register_fpsgo_sugov_hooks(void)
{
	fpsgo_notify_fbt_is_boost_fp = Adaptive_module_bypass;
}

static inline int cmpulong_dec(const void *a, const void *b)
{
	return -(*(unsigned long *)a - *(unsigned long *)b);
}

static void free_capacity_table(void)
{
	int i;

	if (!pd_capacity_tbl)
		return;

	for (i = 0; i < pd_count; i++) {
		kfree(pd_capacity_tbl[i].table);
		pd_capacity_tbl[i].table = NULL;
		kfree(pd_capacity_tbl[i].util_opp_map);
		pd_capacity_tbl[i].util_opp_map = NULL;
		kfree(pd_capacity_tbl[i].util_opp_map_legacy);
		pd_capacity_tbl[i].util_opp_map_legacy = NULL;
		kfree(pd_capacity_tbl[i].freq_opp_map);
		pd_capacity_tbl[i].freq_opp_map = NULL;
		kfree(pd_capacity_tbl[i].freq_opp_map_legacy);
		pd_capacity_tbl[i].freq_opp_map_legacy = NULL;
	}
	kfree(pd_capacity_tbl);
	pd_capacity_tbl = NULL;
}

inline int init_util_freq_opp_mapping_table_type(int t)
{
	int i, j, k, nr_opp, next_k, opp;
	unsigned int min_gap;
	unsigned long min_cap, max_cap;
	unsigned long min_freq, max_freq, curr_freq;
	struct pd_capacity_info *pd_info;
	struct cpufreq_policy *policy;

	pd_capacity_tbl = pd_wl_type[t];
	for (i = 0; i < pd_count; i++) {
		if (is_wl_support())
			mtk_update_wl_table(i, t);
		pd_info = &pd_capacity_tbl[i];
		nr_opp = pd_info->nr_caps;

		/* init util_opp_map */
		max_cap = pd_info->table[0].capacity;
		min_cap = pd_info->table[nr_opp - 1].capacity;
		pd_info->nr_util_opp_map = max_cap - min_cap + 1;

		pd_info->util_opp_map = kcalloc(pd_info->nr_util_opp_map, sizeof(int),
									GFP_KERNEL);
		if (!pd_info->util_opp_map)
			goto nomem;

		pd_info->util_opp_map_legacy = kcalloc(pd_info->nr_util_opp_map,
			sizeof(int), GFP_KERNEL);
		if (!pd_info->util_opp_map_legacy)
			goto nomem;

		for (j = 0; j < nr_opp; j++) {
			k = max_cap - pd_info->table[j].capacity;
			next_k = max_cap - pd_info->table[min(nr_opp - 1, j + 1)].capacity;
			for (; k <= next_k; k++) {
				pd_info->util_opp_map[k] = j;
				for (opp = mtk_em_pd_ptr_public[i].nr_perf_states - 1;
					opp >= 0; opp--)
					if (mtk_em_pd_ptr_public[i].table[opp].capacity >=
						(max_cap - k))
						break;
				pd_info->util_opp_map_legacy[k] = opp;
			}
		}

		/* init freq_opp_map */
		min_gap = UINT_MAX;
		/* skip opp=0, nr_opp-1 because potential irregular freq delta*/
		for (j = 0; j < nr_opp - 2; j++)
			min_gap = min(min_gap,
				pd_info->table[j].freq - pd_info->table[j + 1].freq);
		pd_info->DFreq = min_gap;
		pd_info->inv_DFreq = (u32) DIV_ROUND_UP((u64) UINT_MAX, pd_info->DFreq);
		max_freq = pd_info->table[0].freq;
		min_freq = rounddown(pd_info->table[nr_opp - 1].freq, pd_info->DFreq);

		pd_info->nr_freq_opp_map = ((max_freq - min_freq) / pd_info->DFreq) + 1;

		pd_info->freq_opp_map = kcalloc(pd_info->nr_freq_opp_map, sizeof(int),
			GFP_KERNEL);
		if (!pd_info->freq_opp_map)
			goto nomem;

		pd_info->freq_opp_map_legacy = kcalloc(pd_info->nr_freq_opp_map,
			sizeof(int), GFP_KERNEL);
		if (!pd_info->freq_opp_map_legacy)
			goto nomem;

		opp = 0;
		curr_freq = pd_info->table[opp].freq;

		policy = cpufreq_cpu_get(cpumask_first(&pd_info->cpus));
		if (!policy)
			pr_info("%s: %d: policy NULL in pd=%d\n", __func__, __LINE__, i);

		for (j = 0; j < pd_info->nr_freq_opp_map - 1; j++) {
			if (curr_freq <= pd_info->table[opp + 1].freq)
				opp++;
			pd_info->freq_opp_map[j] = opp;

			if (policy)
				pd_info->freq_opp_map_legacy[j] =
					cpufreq_table_find_index_dl(policy,
					pd_info->table[opp].freq, false);

			curr_freq -= pd_info->DFreq;
		}
		/* fill last element with min_freq opp */
		pd_info->freq_opp_map[pd_info->nr_freq_opp_map - 1] = opp + 1;
		if (policy) {
			pd_info->freq_opp_map_legacy[pd_info->nr_freq_opp_map - 1] =
				cpufreq_table_find_index_dl(policy,
					pd_info->table[opp + 1].freq, false);
			cpufreq_cpu_put(policy);
		}
	}
	return 0;
nomem:
	pr_info("allocate util mapping table failed\n");
	free_capacity_table();
	return -ENOENT;
}

static int init_util_freq_opp_mapping_table(void)
{
	int t, k, ret, need_alloc;

	for (t = nr_wl_type - 1; t >= 0 ; t--) {
		need_alloc = 1;
		for (k = t + 1; k < nr_wl_type; k++) {
			if (mtk_mapping.cpu_to_dsu[t].cpu_type
					== mtk_mapping.cpu_to_dsu[k].cpu_type) {
				need_alloc = 0;
				break;
			}
		}

		if (need_alloc == 0)
			continue;
		else {
			ret = init_util_freq_opp_mapping_table_type(t);
			if (ret)
				goto nomem;
		}
	}
	return 0;
nomem:
	return -ENOENT;
}

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
static int init_capacity_table(void)
{
	int i, j, t;
	struct pd_capacity_info *pd_info;

	for (t = nr_wl_type - 1; t >= 0 ; t--) {
		pd_capacity_tbl = pd_wl_type[t];
		for (i = 0; i < pd_count; i++) {
			if (is_wl_support())
				mtk_update_wl_table(i, t);
			pd_info = &pd_capacity_tbl[i];
			if (!cpumask_equal(&pd_info->cpus, mtk_em_pd_ptr[i].cpumask)) {
				pr_info("cpumask mismatch, pd=%*pb, em=%*pb\n",
					cpumask_pr_args(&pd_info->cpus),
					cpumask_pr_args(mtk_em_pd_ptr[i].cpumask));
					return -1;
			}
			pd_info->table = mtk_em_pd_ptr[i].table;
			pd_info->table_legacy = mtk_em_pd_ptr_public[i].table;

			/* cpu weighting init*/
			cpu_wt[i][t].dsu_weighting =
				mtk_em_pd_ptr_public[i].cur_weighting.dsu_weighting;
			cpu_wt[i][t].emi_weighting =
				mtk_em_pd_ptr_public[i].cur_weighting.emi_weighting;

			for_each_cpu(j, &pd_info->cpus) {
				per_cpu(gear_id, j) = i;
				if (per_cpu(cpu_scale, j) != pd_info->table[0].capacity) {
					pr_info("capacity err: cpu=%d, cpu_scale=%lu, pd_info_cap=%u\n",
						j, per_cpu(cpu_scale, j),
						pd_info->table[0].capacity);
					per_cpu(cpu_scale, j) = pd_info->table[0].capacity;
				} else {
					pr_info("capacity match: cpu=%d, cpu_scale=%lu, pd_info_cap=%u\n",
						j, per_cpu(cpu_scale, j),
						pd_info->table[0].capacity);
				}
			}
		}
	}
	return 0;
}
#else
#if IS_ENABLED(CONFIG_64BIT)
#define em_scale_power(p) ((p) * 1000)
#else
#define em_scale_power(p) (p)
#endif
static int init_capacity_table(void)
{
	int i, j, cpu;
	void __iomem *base = sram_base_addr;
	int count = 0;
	unsigned long offset = 0;
	unsigned long cap;
	unsigned long end_cap;
	unsigned long *caps, *freqs, *powers;
	struct pd_capacity_info *pd_info;
	struct em_perf_domain *pd;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];
		cpu = cpumask_first(&pd_info->cpus);
		pd = em_cpu_get(cpu);
		if (!pd)
			goto err;
		caps = kcalloc(pd_info->nr_caps, sizeof(unsigned long), GFP_KERNEL);
		freqs = kcalloc(pd_info->nr_caps, sizeof(unsigned long), GFP_KERNEL);
		powers = kcalloc(pd_info->nr_caps, sizeof(unsigned long), GFP_KERNEL);

		for (j = 0; j < pd_info->nr_caps; j++) {
			/* for init caps */
			cap = ioread16(base + offset);
			if (cap == 0)
				goto err;
			caps[j] = cap;

			/* for init freqs */
			freqs[j] = pd->table[j].frequency;

			/* for init pwr_eff */
			powers[j] = pd->table[j].power;

			count += 1;
			offset += CAPACITY_ENTRY_SIZE;
		}

		/* decreasing sorting */
		sort(caps, pd_info->nr_caps, sizeof(unsigned long), cmpulong_dec, NULL);
		sort(freqs, pd_info->nr_caps, sizeof(unsigned long), cmpulong_dec, NULL);
		sort(powers, pd_info->nr_caps, sizeof(unsigned long), cmpulong_dec, NULL);

		/* for init pwr_eff */
		for (j = 0; j < pd_info->nr_caps; j++) {
			pd_info->table[j].capacity = caps[j];
			pd_info->table[j].freq = freqs[j];
			pd_info->table[j].pwr_eff =
				em_scale_power(powers[j]) / pd_info->table[j].capacity;
		}
		kfree(caps);
		caps = NULL;
		kfree(freqs);
		freqs = NULL;
		kfree(powers);
		powers = NULL;

		/* repeated last cap 0 between each cluster */
		end_cap = ioread16(base + offset);
		if (end_cap != cap)
			goto err;
		offset += CAPACITY_ENTRY_SIZE;

		for_each_cpu(j, &pd_info->cpus) {
			per_cpu(gear_id, j) = i;
			if (per_cpu(cpu_scale, j) != pd_info->table[0].capacity) {
				pr_info("capacity err: cpu=%d, cpu_scale=%lu, pd_info_cap=%u\n",
					j, per_cpu(cpu_scale, j),
					pd_info->table[0].capacity);
				per_cpu(cpu_scale, j) = pd_info->table[0].capacity;
			} else {
				pr_info("capacity match: cpu=%d, cpu_scale=%lu, pd_info_cap=%u\n",
					j, per_cpu(cpu_scale, j),
					pd_info->table[0].capacity);
			}
		}
	}

	if (entry_count != count)
		goto err;

	return 0;

err:
	pr_info("count %d does not match entry_count %d\n", count, entry_count);

	free_capacity_table();
	return -ENOENT;
}
#endif

static int alloc_capacity_table(void)
{
	int cpu = 0;
	int cur_tbl = 0;
	int nr_caps;
	int i, k, need_alloc;
	struct em_perf_domain *pd;
	unsigned int nr_cpus;

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	sram_base_addr_freq_scaling =
		ioremap(csram_res->start + REG_FREQ_SCALING, LUT_ROW_SIZE);
	if (!sram_base_addr_freq_scaling) {
		pr_info("Remap sram_base_addr_freq_scaling failed!\n");
		return -EIO;
	}
	if (readl_relaxed(sram_base_addr_freq_scaling))
		freq_scaling_disabled = false;

	if (mtk_em_pd_ptr_private == NULL || mtk_em_pd_ptr_public == NULL) {
		pr_info("%s: NULL mtk_em_pd_ptr, private: %p, public: %p\n",
			__func__, mtk_em_pd_ptr_private, mtk_em_pd_ptr_public);
		return -EFAULT;
	}

	if (is_gearless_support())
		mtk_em_pd_ptr = mtk_em_pd_ptr_private;
	else
		mtk_em_pd_ptr = mtk_em_pd_ptr_public;
#endif
	for_each_possible_cpu(cpu) {
		pd = em_cpu_get(cpu);
		if (!pd) {
			pr_info("em_cpu_get return NULL for cpu#%d", cpu);
			continue;
		}
		if (cpu != cpumask_first(to_cpumask(pd->cpus)))
			continue;
		pd_count++;
	}

	if (is_wl_support())
		nr_wl_type = mtk_mapping.total_type;
	else
		nr_wl_type = 1;
	pd_wl_type = kcalloc(nr_wl_type, sizeof(struct pd_capacity_info *),
			GFP_KERNEL);
	cpu_wt = kcalloc(pd_count, sizeof(struct cpu_weighting *), GFP_KERNEL);
	for (i = 0; i < pd_count; i++)
		cpu_wt[i] = kcalloc(nr_wl_type, sizeof(struct cpu_weighting), GFP_KERNEL);

	for (i = 0; i < nr_wl_type; i++) {
		need_alloc = 1;
		for (k = i - 1; k >= 0; k--) {
			if (mtk_mapping.cpu_to_dsu[i].cpu_type
					== mtk_mapping.cpu_to_dsu[k].cpu_type) {
				pd_wl_type[i] = pd_wl_type[k];
				need_alloc = 0;
				break;
			}
		}

		if (need_alloc == 0)
			continue;
		else {
			pd_wl_type[i] = kcalloc(pd_count, sizeof(struct pd_capacity_info),
					GFP_KERNEL);
			if (!pd_wl_type[i])
				return -ENOMEM;
		}
	}

	for_each_possible_cpu(cpu) {
		pd = em_cpu_get(cpu);
		if (!pd) {
			pr_info("em_cpu_get return NULL for cpu#%d", cpu);
			continue;
		}
		nr_cpus = cpumask_last(to_cpumask(pd->cpus)) -
			cpumask_first(to_cpumask(pd->cpus)) + 1;
		if (cpu != cpumask_first(to_cpumask(pd->cpus)))
			continue;

		WARN_ON(cur_tbl >= pd_count);

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
		nr_caps = mtk_em_pd_ptr[cur_tbl].nr_perf_states;
#else
		nr_caps = pd->nr_perf_states;
		for (i = 0; i < nr_wl_type; i++) {
			pd_capacity_tbl = pd_wl_type[i];
			pd_capacity_tbl[cur_tbl].table = kcalloc(nr_caps,
				sizeof(struct mtk_em_perf_state), GFP_KERNEL);
			if (!pd_capacity_tbl[cur_tbl].table) {
				free_capacity_table();
				return -ENOMEM;
			}
		}
#endif
		for (i = 0; i < nr_wl_type; i++) {
			pd_capacity_tbl = pd_wl_type[i];
			pd_capacity_tbl[cur_tbl].nr_cpus = nr_cpus;
			pd_capacity_tbl[cur_tbl].nr_caps = nr_caps;
			pd_capacity_tbl[cur_tbl].nr_caps_legacy = pd->nr_perf_states;
			cpumask_copy(&pd_capacity_tbl[cur_tbl].cpus, to_cpumask(pd->cpus));

			pd_capacity_tbl[cur_tbl].freq_max =
				max(pd->table[pd->nr_perf_states - 1].frequency,
					pd->table[0].frequency);
			pd_capacity_tbl[cur_tbl].freq_min =
				min(pd->table[pd->nr_perf_states - 1].frequency,
					pd->table[0].frequency);
			pd_capacity_tbl[cur_tbl].util_opp_map = NULL;
			pd_capacity_tbl[cur_tbl].util_opp_map_legacy = NULL;
			pd_capacity_tbl[cur_tbl].freq_opp_map = NULL;
			pd_capacity_tbl[cur_tbl].freq_opp_map_legacy = NULL;
		}
		entry_count += nr_caps;

		cur_tbl++;
	}

	return 0;
}

static int init_sram_mapping(void)
{
	struct device_node *dvfs_node;
	struct platform_device *pdev_temp;

	dvfs_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (dvfs_node == NULL) {
		pr_info("failed to find node @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dvfs_node);
	if (pdev_temp == NULL) {
		pr_info("failed to find pdev @ %s\n", __func__);
		return -EINVAL;
	}

	csram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 1);

	if (csram_res)
		sram_base_addr = ioremap(csram_res->start + CAPACITY_TBL_OFFSET, CAPACITY_TBL_SIZE);
	else {
		pr_info("%s can't get resource\n", __func__);
		return -ENODEV;
	}

	if (!sram_base_addr) {
		pr_info("Remap capacity table failed!\n");
		return -EIO;
	}
	return 0;
}

bool cu_ctrl;
bool get_curr_uclamp_ctrl(void)
{
	return cu_ctrl;
}
EXPORT_SYMBOL_GPL(get_curr_uclamp_ctrl);
void set_curr_uclamp_ctrl(int val)
{
	cu_ctrl = val ? true : false;
}
EXPORT_SYMBOL_GPL(set_curr_uclamp_ctrl);
int set_curr_uclamp_hint(int pid, int set)
{
	struct task_struct *p;
	struct curr_uclamp_hint *cu_ht;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return -1;
	}

	if (p->exit_state) {
		rcu_read_unlock();
		return -1;
	}

	get_task_struct(p);
	cu_ht = &((struct mtk_task *) p->android_vendor_data1)->cu_hint;
	cu_ht->hint = set;
	put_task_struct(p);
	rcu_read_unlock();
	return 0;
}
EXPORT_SYMBOL_GPL(set_curr_uclamp_hint);
int get_curr_uclamp_hint(int pid)
{
	struct task_struct *p;
	struct curr_uclamp_hint *cu_ht;
	int hint;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return -1;
	}

	if (p->exit_state) {
		rcu_read_unlock();
		return -1;
	}

	get_task_struct(p);
	cu_ht = &((struct mtk_task *) p->android_vendor_data1)->cu_hint;
	hint = cu_ht->hint;
	put_task_struct(p);
	rcu_read_unlock();
	return hint;
}
EXPORT_SYMBOL_GPL(get_curr_uclamp_hint);

#if IS_ENABLED(CONFIG_UCLAMP_TASK_GROUP)
#define UCLAMP_BUCKET_DELTA DIV_ROUND_CLOSEST(SCHED_CAPACITY_SCALE, UCLAMP_BUCKETS)
static int gear_uclamp_max[MAX_NR_CPUS] = {
			[0 ... MAX_NR_CPUS - 1] = SCHED_CAPACITY_SCALE
};
static inline unsigned int uclamp_bucket_id(unsigned int clamp_value)
{
	return min_t(unsigned int, clamp_value / UCLAMP_BUCKET_DELTA, UCLAMP_BUCKETS - 1);
}
static inline void uclamp_se_set(struct uclamp_se *uc_se,
				 unsigned int value, bool user_defined)
{
	uc_se->value = value;
	uc_se->bucket_id = uclamp_bucket_id(value);
	uc_se->user_defined = user_defined;
}
static bool gu_ctrl;
bool get_gear_uclamp_ctrl(void)
{
	return gu_ctrl;
}
EXPORT_SYMBOL_GPL(get_gear_uclamp_ctrl);
void set_gear_uclamp_ctrl(int val)
{
	int i;

	gu_ctrl = val ? true : false;
	if (gu_ctrl == false) {
		for (i = 0; i < pd_count; i++)
			gear_uclamp_max[i] = SCHED_CAPACITY_SCALE;
	}
}
EXPORT_SYMBOL_GPL(set_gear_uclamp_ctrl);

int get_gear_uclamp_max(int gearid)
{
	return gear_uclamp_max[gearid];
}
EXPORT_SYMBOL_GPL(get_gear_uclamp_max);
int get_cpu_gear_uclamp_max(int cpu)
{
	if (gu_ctrl == false)
		return SCHED_CAPACITY_SCALE;
	return gear_uclamp_max[per_cpu(gear_id, cpu)];
}
EXPORT_SYMBOL_GPL(get_cpu_gear_uclamp_max);
void set_gear_uclamp_max(int gearid, int val)
{
	gear_uclamp_max[gearid] = val;
}
EXPORT_SYMBOL_GPL(set_gear_uclamp_max);

void mtk_uclamp_eff_get(void *data, struct task_struct *p, enum uclamp_id clamp_id,
		struct uclamp_se *uc_max, struct uclamp_se *uc_eff, int *ret)
{
	unsigned int tg_min, tg_max, value = SCHED_CAPACITY_SCALE;
	unsigned int gearid;

	if (gu_ctrl == false)
		return;

	*uc_eff =  p->uclamp_req[clamp_id];

	if (task_group_is_autogroup(task_group(p)))
		goto sys_restriction;
	if (task_group(p) == &root_task_group)
		goto sys_restriction;

	tg_min = task_group(p)->uclamp[UCLAMP_MIN].value;
	tg_max = task_group(p)->uclamp[UCLAMP_MAX].value;
	value = uc_eff->value;
	value = clamp(value, tg_min, tg_max);

sys_restriction:
	gearid = per_cpu(gear_id, task_cpu(p));
	value = min_t(unsigned int, value, gear_uclamp_max[gearid]);
	uclamp_se_set(uc_eff, value, false);
	if (uc_eff->value > uc_max->value)
		*uc_eff = *uc_max;

	*ret = 1;
}
#endif

int init_opp_cap_info(struct proc_dir_entry *dir)
{
	int ret, i;

	ret = init_sram_mapping();
	if (ret)
		return ret;

	ret = alloc_capacity_table();
	if (ret)
		return ret;

	ret = init_capacity_table();
	if (ret)
		return ret;

	ret = init_util_freq_opp_mapping_table();
	if (ret)
		return ret;

	for (i = 0; i < pd_count; i++) {
		mtk_update_wl_table(i, 0); /* set default wl type = 0 */
		set_target_margin(i, 20);
		set_target_margin_low(i, 20);
		set_turn_point_freq(i, 0);
	}

	if (is_wl_support()) {
		ret = dsu_pwr_swpm_init();
		if (ret) {
			pr_info("dsu_pwr_swpm_init failed\n");
			return ret;
		}
		l3ctl_sram_base_addr = get_l3ctl_sram_base_addr();
		init_dsu();

		init_eas_dsu_ctrl();
	}

	init_grp_dvfs();

	init_sbb_cpu_data();

	init_adaptive_margin();

	register_fpsgo_sugov_hooks();

	return ret;
}

void clear_opp_cap_info(void)
{
	free_capacity_table();
}

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
static inline void mtk_arch_set_freq_scale_gearless(struct cpufreq_policy *policy,
		unsigned int *target_freq)
{
	int i;
	unsigned long cap, max_cap;
	struct cpufreq_mtk *c = policy->driver_data;

	if (c->last_index == policy->cached_resolved_idx) {
		cap = pd_X2Y(policy->cpu, *target_freq, FREQ, CAP, false);
		max_cap = pd_X2Y(policy->cpu, policy->cpuinfo.max_freq, FREQ, CAP, false);
		for_each_cpu(i, policy->related_cpus)
			per_cpu(arch_freq_scale, i) = ((cap << SCHED_CAPACITY_SHIFT) / max_cap);
	}
}

static unsigned int curr_cap[MAX_NR_CPUS];
unsigned int get_curr_cap(int cpu)
{
	return curr_cap[per_cpu(gear_id, cpu)];
}
EXPORT_SYMBOL_GPL(get_curr_cap);

void mtk_cpufreq_fast_switch(void *data, struct cpufreq_policy *policy,
		unsigned int *target_freq, unsigned int old_target_freq)
{
	int cpu = policy->cpu;

	irq_log_store();

	if (trace_sugov_ext_gear_state_enabled())
		trace_sugov_ext_gear_state(per_cpu(gear_id, cpu),
			pd_get_freq_opp(cpu, *target_freq));

	if (policy->cached_target_freq != *target_freq) {
		policy->cached_target_freq = *target_freq;
		policy->cached_resolved_idx = pd_X2Y(cpu, *target_freq, FREQ, OPP, true);
	}

	curr_cap[per_cpu(gear_id, cpu)] = pd_get_opp_capacity_legacy(policy->cpu,
		policy->cached_resolved_idx);

	if (is_gearless_support())
		mtk_arch_set_freq_scale_gearless(policy, target_freq);

	if (freq_state.is_eas_dsu_support) {
		if (freq_state.is_eas_dsu_ctrl)
			set_dsu_target_freq(policy);
		else {
			struct cpufreq_mtk *c = policy->driver_data;

			c->sb_ch = -1;
			if (trace_sugov_ext_dsu_freq_vote_enabled())
				trace_sugov_ext_dsu_freq_vote(UINT_MAX,
					per_cpu(gear_id, cpu), *target_freq, UINT_MAX);
		}
	}

	irq_log_store();
}

void mtk_arch_set_freq_scale(void *data, const struct cpumask *cpus,
		unsigned long freq, unsigned long max, unsigned long *scale)
{
	int cpu = cpumask_first(cpus);
	unsigned long cap, max_cap;
	struct cpufreq_policy *policy;

	irq_log_store();

	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		freq = policy->cached_target_freq;
		cpufreq_cpu_put(policy);
	}
	cap = pd_X2Y(cpu, freq, FREQ, CAP, false);
	max_cap = pd_X2Y(cpu, max, FREQ, CAP, false);
	*scale = ((cap << SCHED_CAPACITY_SHIFT) / max_cap);
	irq_log_store();
}

unsigned int util_scale = 1280;
unsigned int sysctl_sched_capacity_margin_dvfs = 20;
unsigned int turn_point_util[MAX_NR_CPUS];
unsigned int target_margin[MAX_NR_CPUS];
unsigned int target_margin_low[MAX_NR_CPUS];
/*
 * set sched capacity margin for DVFS, Default = 20
 */
int set_sched_capacity_margin_dvfs(int capacity_margin)
{
	if (capacity_margin < 0 || capacity_margin > 95)
		return -1;

	sysctl_sched_capacity_margin_dvfs = capacity_margin;
	util_scale = ((SCHED_CAPACITY_SCALE * 100) / (100 - sysctl_sched_capacity_margin_dvfs));

	return 0;
}
EXPORT_SYMBOL_GPL(set_sched_capacity_margin_dvfs);

unsigned int get_sched_capacity_margin_dvfs(void)
{

	return sysctl_sched_capacity_margin_dvfs;
}
EXPORT_SYMBOL_GPL(get_sched_capacity_margin_dvfs);

int set_target_margin(int gearid, int margin)
{
	if (gearid < 0 || gearid > pd_count)
		return -1;

	if (margin < 0 || margin > 95)
		return -1;
	target_margin[gearid] = (SCHED_CAPACITY_SCALE * 100 / (100 - margin));
	return 0;
}
EXPORT_SYMBOL_GPL(set_target_margin);

int set_target_margin_low(int gearid, int margin)
{
	if (gearid < 0 || gearid > pd_count)
		return -1;

	if (margin < 0 || margin > 95)
		return -1;
	target_margin_low[gearid] = (SCHED_CAPACITY_SCALE * 100 / (100 - margin));
	return 0;
}
EXPORT_SYMBOL_GPL(set_target_margin_low);

unsigned int get_target_margin(int gearid)
{
	return (100 - (SCHED_CAPACITY_SCALE * 100)/target_margin[gearid]);
}
EXPORT_SYMBOL_GPL(get_target_margin);

unsigned int get_target_margin_low(int gearid)
{
	return (100 - (SCHED_CAPACITY_SCALE * 100)/target_margin_low[gearid]);
}
EXPORT_SYMBOL_GPL(get_target_margin_low);

/*
 *for vonvenient, pass freq, but converty to util
 */
int set_turn_point_freq(int gearid, unsigned long freq)
{
	int idx;
	struct pd_capacity_info *pd_info;

	if (gearid < 0 || gearid > pd_count)
		return -1;

	if (freq == 0) {
		turn_point_util[gearid] = 0;
		return 0;
	}

	pd_info = &pd_capacity_tbl[gearid];
	idx = map_freq_idx_by_tbl(pd_info, freq);
	idx = pd_info->freq_opp_map[idx];
	turn_point_util[gearid] = pd_info->table[idx].capacity;

	return 0;
}
EXPORT_SYMBOL_GPL(set_turn_point_freq);

inline unsigned long get_turn_point_freq(int gearid)
{
	int idx;
	struct pd_capacity_info *pd_info;
	if (turn_point_util[gearid] == 0)
		return 0;
	pd_info = &pd_capacity_tbl[gearid];
	idx = map_util_idx_by_tbl(pd_info, turn_point_util[gearid]);
	idx = pd_info->util_opp_map[idx];
	return max(pd_info->table[idx].freq, pd_info->freq_min);
}
EXPORT_SYMBOL_GPL(get_turn_point_freq);

/* adaptive margin */
static int am_wind_dura = 4000; /* microsecond */
static int am_wind_cnt_shift = 1; /* wind_cnt = 1 << am_wind_cnt_shift */
static int am_floor = 1024; /* 1024: 0% margin */
static int am_ceiling = 1280; /* 1280: 20% margin */
static int am_target_active_ratio_cap[MAX_NR_CPUS] = {
	[0 ... MAX_NR_CPUS - 1] = 819};
static unsigned int adaptive_margin[MAX_NR_CPUS] = {
	[0 ... MAX_NR_CPUS - 1] = 1280};
static unsigned int his_ptr[MAX_NR_CPUS];
static unsigned int margin_his[MAX_NR_CPUS][MAX_NR_CPUS];
static u64 last_wall_time_stamp[MAX_NR_CPUS];
static u64 last_idle_time_stamp[MAX_NR_CPUS];
static u64 last_idle_duratio[MAX_NR_CPUS];
static unsigned int cpu_active_ratio_cap[MAX_NR_CPUS];
static unsigned int gear_max_active_ratio_cap[MAX_NR_CPUS];
static unsigned int gear_update_active_ratio_cnt[MAX_NR_CPUS];
static unsigned int gear_update_active_ratio_cnt_last[MAX_NR_CPUS];
static unsigned int duration_wind[MAX_NR_CPUS];
static unsigned int duration_act[MAX_NR_CPUS];
static unsigned int ramp_up[MAX_NR_CPUS] = {
	[0 ... MAX_NR_CPUS - 1] = 0};
unsigned int get_adaptive_margin(int cpu)
{
	if (!turn_point_util[per_cpu(gear_id, cpu)] && am_ctrl)
		return READ_ONCE(adaptive_margin[per_cpu(gear_id, cpu)]);
	else
		return util_scale;
}
EXPORT_SYMBOL_GPL(get_adaptive_margin);

int get_cpu_active_ratio_cap(int cpu)
{
	return cpu_active_ratio_cap[cpu];
}
EXPORT_SYMBOL_GPL(get_cpu_active_ratio_cap);

int get_gear_max_active_ratio_cap(int gearid)
{
	return gear_max_active_ratio_cap[gearid];
}
EXPORT_SYMBOL_GPL(get_gear_max_active_ratio_cap);

void set_target_active_ratio_pct(int gear_id, int val)
{
	am_target_active_ratio_cap[gear_id] = clamp_val(val, 1, 100) << SCHED_CAPACITY_SHIFT / 100;
}
EXPORT_SYMBOL_GPL(set_target_active_ratio_pct);

void set_target_active_ratio_cap(int gear_id, int val)
{
	am_target_active_ratio_cap[gear_id] = clamp_val(val, 1, SCHED_CAPACITY_SCALE);
}
EXPORT_SYMBOL_GPL(set_target_active_ratio_cap);

void set_am_ceiling(int val)
{
	am_ceiling = val;
}
EXPORT_SYMBOL_GPL(set_am_ceiling);

int get_am_ceiling(void)
{
	return am_ceiling;
}
EXPORT_SYMBOL_GPL(get_am_ceiling);

bool ignore_idle_ctrl;
void set_ignore_idle_ctrl(bool val)
{
	ignore_idle_ctrl = val;
}
EXPORT_SYMBOL_GPL(set_ignore_idle_ctrl);
bool get_ignore_idle_ctrl(void)
{
	return ignore_idle_ctrl;
}
EXPORT_SYMBOL_GPL(get_ignore_idle_ctrl);

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
unsigned long (*flt_sched_get_cpu_group_util_eas_hook)(int cpu, int group_id);
EXPORT_SYMBOL(flt_sched_get_cpu_group_util_eas_hook);
unsigned long (*flt_get_cpu_util_hook)(int cpu);
EXPORT_SYMBOL(flt_get_cpu_util_hook);
int (*get_group_hint_hook)(int group);
EXPORT_SYMBOL(get_group_hint_hook);
#endif

int group_aware_dvfs_util(struct cpumask *cpumask)
{
	unsigned long cpu_util = 0;
	unsigned long ret_util = 0;
	unsigned long max_ret_util = 0;
	unsigned long umax = 0;
	int cpu;
	struct rq *rq;
	int am = 0;

	for_each_cpu(cpu, cpumask) {
		if (available_idle_cpu(cpu))
			goto skip_idle;

		am = get_adaptive_margin(cpu);

		cpu_util = flt_get_cpu_util_hook(cpu);
		rq = cpu_rq(cpu);
		umax = rq->uclamp[UCLAMP_MAX].value;
		ret_util = min_t(unsigned long,
			cpu_util, umax * am >> SCHED_CAPACITY_SHIFT);

		max_ret_util = max(max_ret_util, ret_util);
skip_idle:
		if (trace_sugov_ext_tar_enabled())
			trace_sugov_ext_tar(cpu, ret_util, cpu_util, umax, am);
	}
	return max_ret_util;
}

inline int update_cpu_active_ratio(int cpu_idx)
{
	u64 idle_time_stamp, wall_time_stamp, duration;
	u64 idle_duration, active_duration;

	idle_time_stamp = get_cpu_idle_time(cpu_idx, &wall_time_stamp, 1);
	duration = wall_time_stamp - last_wall_time_stamp[cpu_idx];
	idle_duration = idle_time_stamp - last_idle_time_stamp[cpu_idx];
	idle_duration = min_t(u64, idle_duration, duration);
	last_idle_duratio[cpu_idx] = idle_duration;
	active_duration = duration - idle_duration;
	last_idle_time_stamp[cpu_idx] = idle_time_stamp;
	last_wall_time_stamp[cpu_idx] = wall_time_stamp;
	duration_wind[cpu_idx] = duration;
	duration_act[cpu_idx] = active_duration;
	return ((active_duration << SCHED_CAPACITY_SHIFT) / duration);
}

void update_active_ratio_gear(struct cpumask *cpumask)
{
	unsigned int cpu_idx, gear_idx, cpu_id;
	unsigned int gear_max_active_ratio_tmp = 0;

	for_each_cpu(cpu_idx, cpumask) {
		gear_idx = per_cpu(gear_id, cpu_idx);
		cpu_active_ratio_cap[cpu_idx] = update_cpu_active_ratio(cpu_idx);
		if (cpu_active_ratio_cap[cpu_idx] > gear_max_active_ratio_tmp) {
			gear_max_active_ratio_tmp = cpu_active_ratio_cap[cpu_idx];
			cpu_id = cpu_idx;
		}
	}
	if (last_idle_duratio[cpu_id] > am_wind_dura)
		ramp_up[gear_idx] = 2;
	gear_max_active_ratio_cap[gear_idx] = gear_max_active_ratio_tmp;
	gear_update_active_ratio_cnt[gear_idx]++;
}
static bool grp_trigger;
void update_active_ratio_all(void)
{
	int i;
	struct pd_capacity_info *pd_info;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];
		update_active_ratio_gear(&pd_info->cpus);
	}
	grp_trigger = true;
}
EXPORT_SYMBOL(update_active_ratio_all);

inline void update_adaptive_margin(struct cpufreq_policy *policy)
{
	unsigned int i, ptr = 0;
	int cpu = cpumask_first(policy->cpus);
	int gearid = per_cpu(gear_id, cpu);
	unsigned int adaptive_margin_tmp;

	if (gear_update_active_ratio_cnt_last[gearid] != gear_update_active_ratio_cnt[gearid]) {
		gear_update_active_ratio_cnt_last[gearid] = gear_update_active_ratio_cnt[gearid];
		his_ptr[gearid]++;
		his_ptr[gearid] %= MAX_NR_CPUS;

		if (ramp_up[gearid] == 0) {
			unsigned int adaptive_ratio = ((gear_max_active_ratio_cap[gearid] <<
				SCHED_CAPACITY_SHIFT) / am_target_active_ratio_cap[gearid]);

			adaptive_margin_tmp = READ_ONCE(adaptive_margin[gearid]);
			adaptive_margin_tmp =
				(adaptive_margin_tmp * adaptive_ratio)
				>> SCHED_CAPACITY_SHIFT;
			adaptive_margin_tmp =
				clamp_val(adaptive_margin_tmp, am_floor, am_ceiling);
			margin_his[gearid][his_ptr[gearid]] = adaptive_margin_tmp;
			if (adaptive_ratio < SCHED_CAPACITY_SCALE) {
				adaptive_margin_tmp = 0;
				ptr = (his_ptr[gearid] + MAX_NR_CPUS - (1 << am_wind_cnt_shift) + 1)
					% MAX_NR_CPUS;
				for (i = 0; i < (1 << am_wind_cnt_shift); i++) {
					adaptive_margin_tmp += margin_his[gearid][ptr];
					ptr++;
					ptr %= MAX_NR_CPUS;
				}
				adaptive_margin_tmp >>= am_wind_cnt_shift;
				margin_his[gearid][his_ptr[gearid]] = adaptive_margin_tmp;
			}
		} else {
			margin_his[gearid][his_ptr[gearid]] = adaptive_margin_tmp = util_scale;
			ramp_up[gearid]--;
		}
		WRITE_ONCE(adaptive_margin[gearid], adaptive_margin_tmp);

		if (trace_sugov_ext_adaptive_margin_enabled())
			trace_sugov_ext_adaptive_margin(gearid,
			READ_ONCE(adaptive_margin[gearid]), gear_max_active_ratio_cap[gearid]);
	}
}

inline void mtk_map_util_freq_adap_grp(void *data, unsigned long util,
				int cpu, unsigned long *next_freq, struct cpumask *cpumask)
{
	int gearid = per_cpu(gear_id, cpu);
	struct sugov_policy *sg_policy;
	struct cpufreq_policy *policy;
	unsigned long flt_util = 0, pelt_util_with_margin;
	unsigned long util_ori = util;
	u64 idle_time_stamp, wall_time_stamp;

	if (data != NULL) {
		sg_policy = (struct sugov_policy *)data;
		policy = sg_policy->policy;
		if (grp_dvfs_ctrl_mode == 0 || grp_trigger == false) {
			idle_time_stamp = get_cpu_idle_time(cpu, &wall_time_stamp, 1);
			if (wall_time_stamp - last_wall_time_stamp[cpu] > am_wind_dura)
				update_active_ratio_gear(cpumask);
		}
		update_adaptive_margin(policy);
	}

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	if (flt_get_cpu_util_hook && grp_dvfs_ctrl_mode && wl_type_curr != 4)
		flt_util = group_aware_dvfs_util(cpumask);
	if (grp_dvfs_ctrl_mode == 9)
		flt_util = 0;
#endif

	if (am_ctrl == 0 || am_ctrl == 9)
		WRITE_ONCE(adaptive_margin[gearid], util_scale);
	pelt_util_with_margin = (util * READ_ONCE(adaptive_margin[gearid])) >> SCHED_CAPACITY_SHIFT;

	util = max_t(int, pelt_util_with_margin, flt_util);

	*next_freq = pd_get_util_freq(cpu, util);

	if (trace_sugov_ext_group_dvfs_enabled())
		trace_sugov_ext_group_dvfs(gearid, util, pelt_util_with_margin,
			flt_util, util_ori, READ_ONCE(adaptive_margin[gearid]), *next_freq);

	if (data != NULL) {
		policy->cached_target_freq = *next_freq;
		policy->cached_resolved_idx = pd_get_freq_opp_legacy(cpu, *next_freq);
		sg_policy->cached_raw_freq = *next_freq;
	}
}

void mtk_map_util_freq(void *data, unsigned long util, unsigned long freq, struct cpumask *cpumask,
		unsigned long *next_freq, int wl_type)
{
	int cpu, orig_util = util, gearid;
	struct pd_capacity_info *pd_info;

	cpu = cpumask_first(cpumask);
	gearid = per_cpu(gear_id, cpu);

	pd_info = &pd_capacity_tbl[gearid];
	if (!turn_point_util[gearid] && (am_ctrl || grp_dvfs_ctrl_mode)) {
		mtk_map_util_freq_adap_grp(data, util, cpu, next_freq, cpumask);
		return;
	}

	util = (util * util_scale) >> SCHED_CAPACITY_SHIFT;
	if (turn_point_util[gearid] &&
		util > turn_point_util[gearid])
		util = max(turn_point_util[gearid], orig_util * target_margin[gearid]
					>> SCHED_CAPACITY_SHIFT);
	else if (turn_point_util[gearid] &&
		util < turn_point_util[gearid])
		util = min(turn_point_util[gearid], orig_util * target_margin_low[gearid]
					>> SCHED_CAPACITY_SHIFT);

	*next_freq = pd_X2Y(cpu, util, CAP, FREQ, false);
	if (data != NULL) {
		struct sugov_policy *sg_policy = (struct sugov_policy *)data;
		struct cpufreq_policy *policy = sg_policy->policy;

		policy->cached_target_freq = *next_freq;
		policy->cached_resolved_idx = pd_X2Y(cpu, *next_freq, FREQ, OPP, true);
		sg_policy->cached_raw_freq = *next_freq;
	}

	if (trace_sugov_ext_turn_point_margin_enabled() && turn_point_util[gearid]) {
		orig_util = (orig_util * util_scale) >> SCHED_CAPACITY_SHIFT;
		trace_sugov_ext_turn_point_margin(per_cpu(gear_id, cpu), orig_util, util,
			turn_point_util[gearid], target_margin[gearid]);
	}
}
EXPORT_SYMBOL_GPL(mtk_map_util_freq);
#endif
#else

static int init_opp_cap_info(struct proc_dir_entry *dir) { return 0; }
#define clear_opp_cap_info()

#endif
