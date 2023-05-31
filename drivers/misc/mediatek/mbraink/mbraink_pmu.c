// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/timer.h>
#include "mbraink_pmu.h"

#define CPU_INST_SPEC_OFFSET	0x20
#define max_cpus 8
#define CPU_INST_SPEC_UPDATE_INTEVAL   10   // ex: 0.1 * HZ

static DEFINE_PER_CPU(u64, cpu_last_inst_spec);
static u64 cpu0_pmu_data_inst_spec;
static u64 cpu1_pmu_data_inst_spec;
static u64 cpu2_pmu_data_inst_spec;
static u64 cpu3_pmu_data_inst_spec;
static u64 cpu4_pmu_data_inst_spec;
static u64 cpu5_pmu_data_inst_spec;
static u64 cpu6_pmu_data_inst_spec;
static u64 cpu7_pmu_data_inst_spec;

static void __iomem *general_tcm_base_addr;
static void __iomem *pmu_data_tcm_base_addr;

static struct timer_list timer_inst_spec;

static void timer_inst_spect_update_func(struct timer_list *timer)
{
	int ret = 0;

	mbraink_update_pmu_inst_spec();
	ret = mod_timer(timer, jiffies + CPU_INST_SPEC_UPDATE_INTEVAL);
	if (ret)
		pr_notice("timer_inst_spec fired failed!\n");
}

void init_pmu_keep_data(void)
{
#if IS_ENABLED(CONFIG_MTK_LPM_MT6989)
	int i = 0;
	int ret = 0;

	for (i = 0; i < num_possible_cpus(); i++)
		per_cpu(cpu_last_inst_spec, i) = 0;

	timer_setup(&timer_inst_spec, timer_inst_spect_update_func, 0);
	ret = mod_timer(&timer_inst_spec, jiffies + CPU_INST_SPEC_UPDATE_INTEVAL);
	if (ret)
		pr_notice("timer_inst_spec fired failed!\n");
	else
		pr_info("-- %s: setup timer: %lu for pmu_inst_spec--\n", __func__, jiffies);
#else
	pr_info("%s: not enable do nothing\n", __func__);
#endif
}

void uninit_pmu_keep_data(void)
{
#if IS_ENABLED(CONFIG_MTK_LPM_MT6989)
	int ret = 0;

	ret = del_timer(&timer_inst_spec);
	if (ret)
		pr_notice("timer_inst_spec fired failed!\n");
	else
		pr_info("-- %s: delete timer: %lu for pmu_inst_spec--\n", __func__, jiffies);
#else
	pr_info("%s: not enable do nothing\n", __func__);
#endif
}

u64 get_cpu_pmu(int cpu, u32 offset)
{
	u64 count = 0;

	if (IS_ERR_OR_NULL((void *)pmu_data_tcm_base_addr))
		return count;

	count = __raw_readl(pmu_data_tcm_base_addr + offset + (cpu * 0x4));
	return count;
}

 /* 3GHz * 4ms * 10 IPC * 3 chances */
#define MAX_PMU_VALUE	360000000
#define DEFINE_GET_CUR_CPU_PMU_FUNC(_name, _pmu_offset)						\
u64 get_cur_cpu_##_name(int cpu)							\
{											\
		u64 cur = 0, res = 0;								\
		int retry = 3;									\
												\
		if (cpu >= nr_cpu_ids)						\
			return res;								\
												\
		do {										\
			cur = get_cpu_pmu(cpu, _pmu_offset);					\
			/* invalid counter */							\
			if (cur == 0 || cur == 0xDEADDEAD)					\
				return 0;							\
												\
			/* handle overflow case */						\
			if (cur < per_cpu(cpu_last_##_name, cpu))				\
				res = ((u64)0xffffffff -					\
					per_cpu(cpu_last_##_name, cpu) + (0x7fffffff & cur));	\
			else									\
				res = per_cpu(cpu_last_##_name, cpu) == 0 ?			\
					0 : cur - per_cpu(cpu_last_##_name, cpu);		\
			--retry;								\
		} while (res > MAX_PMU_VALUE && retry > 0);					\
												\
		if (res > MAX_PMU_VALUE && retry == 0) {					\
			return 0;								\
		}										\
												\
		per_cpu(cpu_last_##_name, cpu) = cur;						\
		return res;									\
}

DEFINE_GET_CUR_CPU_PMU_FUNC(inst_spec, CPU_INST_SPEC_OFFSET);

#if IS_ENABLED(CONFIG_MTK_LPM_MT6989)

int mbraink_pmu_init(void)
{
	pr_notice("mbraink pmu init.\n");
	general_tcm_base_addr = ioremap(GENERAL_TCM_ADDR, GENERAL_TCM_SIZE);
	pmu_data_tcm_base_addr = ioremap(PMU_DATA_TCM_ADDR, PMU_DATA_TCM_SIZE);
	return 0;
}

int mbraink_pmu_uninit(void)
{
	pr_notice("mbraink pmu uninit.\n");
	iounmap(general_tcm_base_addr);
	iounmap(pmu_data_tcm_base_addr);
	return 0;
}

int mbraink_enable_pmu_inst_spec(bool enable)
{
	pr_info("mbrain enable pmu inst spec, enable: %d", enable);
	if (enable) {
		cpu0_pmu_data_inst_spec = 0;
		cpu1_pmu_data_inst_spec = 0;
		cpu2_pmu_data_inst_spec = 0;
		cpu3_pmu_data_inst_spec = 0;
		cpu4_pmu_data_inst_spec = 0;
		cpu5_pmu_data_inst_spec = 0;
		cpu6_pmu_data_inst_spec = 0;
		cpu7_pmu_data_inst_spec = 0;
		init_pmu_keep_data();
		iowrite32(1, general_tcm_base_addr+MBRAIN_ENABLE_TCM_OFFSET);
	} else {
		uninit_pmu_keep_data();
		iowrite32(0, general_tcm_base_addr+MBRAIN_ENABLE_TCM_OFFSET);
	}
	return 0;
}

int mbraink_update_pmu_inst_spec(void)
{
	cpu0_pmu_data_inst_spec += get_cur_cpu_inst_spec(0);
	cpu1_pmu_data_inst_spec += get_cur_cpu_inst_spec(1);
	cpu2_pmu_data_inst_spec += get_cur_cpu_inst_spec(2);
	cpu3_pmu_data_inst_spec += get_cur_cpu_inst_spec(3);
	cpu4_pmu_data_inst_spec += get_cur_cpu_inst_spec(4);
	cpu5_pmu_data_inst_spec += get_cur_cpu_inst_spec(5);
	cpu6_pmu_data_inst_spec += get_cur_cpu_inst_spec(6);
	cpu7_pmu_data_inst_spec += get_cur_cpu_inst_spec(7);
	return 0;
}

int mbraink_get_pmu_inst_spec(struct mbraink_pmu_info *pmuInfo)
{
	pmuInfo->cpu0_pmu_data_inst_spec = cpu0_pmu_data_inst_spec;
	pmuInfo->cpu1_pmu_data_inst_spec = cpu1_pmu_data_inst_spec;
	pmuInfo->cpu2_pmu_data_inst_spec = cpu2_pmu_data_inst_spec;
	pmuInfo->cpu3_pmu_data_inst_spec = cpu3_pmu_data_inst_spec;
	pmuInfo->cpu4_pmu_data_inst_spec = cpu4_pmu_data_inst_spec;
	pmuInfo->cpu5_pmu_data_inst_spec = cpu5_pmu_data_inst_spec;
	pmuInfo->cpu6_pmu_data_inst_spec = cpu6_pmu_data_inst_spec;
	pmuInfo->cpu7_pmu_data_inst_spec = cpu7_pmu_data_inst_spec;

	pr_notice("%s: get inst spec: %llu %llu %llu %llu %llu %llu %llu %llu", __func__,
	cpu0_pmu_data_inst_spec, cpu1_pmu_data_inst_spec,
	cpu2_pmu_data_inst_spec, cpu3_pmu_data_inst_spec,
	cpu4_pmu_data_inst_spec, cpu5_pmu_data_inst_spec,
	cpu6_pmu_data_inst_spec, cpu7_pmu_data_inst_spec);

	return 0;
}

#else
int mbraink_pmu_init(void)
{
	pr_info("%s: not enable do nothing\n", __func__);
	return 0;
}

int mbraink_pmu_uninit(void)
{
	pr_info("%s: not enable do nothing\n", __func__);
	return 0;
}

int mbraink_pmu_enable(bool enable)
{
	pr_info("%s: not enable do nothing\n", __func__);
	return 0;
}

int mbraink_enable_pmu_inst_spec(bool enable)
{
	pr_info("%s: not enable do nothing\n", __func__);
	return 0;
}

int mbraink_update_pmu_inst_spec(void)
{
	pr_info("%s: not enable do nothing\n", __func__);
	return 0;
}

int mbraink_get_pmu_inst_spec(struct mbraink_pmu_info *pmuInfo)
{
	pmuInfo->cpu0_pmu_data_inst_spec = cpu0_pmu_data_inst_spec;
	pmuInfo->cpu1_pmu_data_inst_spec = cpu1_pmu_data_inst_spec;
	pmuInfo->cpu2_pmu_data_inst_spec = cpu2_pmu_data_inst_spec;
	pmuInfo->cpu3_pmu_data_inst_spec = cpu3_pmu_data_inst_spec;
	pmuInfo->cpu4_pmu_data_inst_spec = cpu4_pmu_data_inst_spec;
	pmuInfo->cpu5_pmu_data_inst_spec = cpu5_pmu_data_inst_spec;
	pmuInfo->cpu6_pmu_data_inst_spec = cpu6_pmu_data_inst_spec;
	pmuInfo->cpu7_pmu_data_inst_spec = cpu7_pmu_data_inst_spec;
	pr_info("%s: not enable do nothing\n", __func__);
	return 0;
}
#endif
