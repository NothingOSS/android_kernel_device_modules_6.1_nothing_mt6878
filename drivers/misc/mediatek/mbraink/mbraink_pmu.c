// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include "mbraink_pmu.h"

#define MBRAIN_ENABLE_ADDR_OFFSET    0x1330
#define CPU_INST_SPEC_UPDATE_INTEVAL   10   // ex: 0.1 * HZ

bool mbraink_pmu_dts_exist;
u32 cpu_inst_spec_offset = 0x1274;  // 0x1254 + 0x20 for Ponsot and before

static DEFINE_PER_CPU(u64, cpu_last_inst_spec);
static u64 cpu0_pmu_data_inst_spec;
static u64 cpu1_pmu_data_inst_spec;
static u64 cpu2_pmu_data_inst_spec;
static u64 cpu3_pmu_data_inst_spec;
static u64 cpu4_pmu_data_inst_spec;
static u64 cpu5_pmu_data_inst_spec;
static u64 cpu6_pmu_data_inst_spec;
static u64 cpu7_pmu_data_inst_spec;

static void __iomem *csram_base;
static void __iomem *pmu_tcm_base;

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

static void set_pmu_enable(unsigned int enable)
{
	if (IS_ERR_OR_NULL((void *)csram_base))
		return;

	__raw_writel((u32)enable, csram_base + MBRAIN_ENABLE_ADDR_OFFSET);
	/* make sure register access in order */
	wmb();
}

u64 get_cpu_pmu(int cpu, u32 offset)
{
	u64 count = 0;

	if (mbraink_pmu_dts_exist) {
		if (IS_ERR_OR_NULL((void *)pmu_tcm_base))
			return count;
		count = __raw_readl(pmu_tcm_base + offset + (cpu * 0x4));
	} else {
		if (IS_ERR_OR_NULL((void *)csram_base))
			return count;
		count = __raw_readl(csram_base + offset + (cpu * 0x4));
	}
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

DEFINE_GET_CUR_CPU_PMU_FUNC(inst_spec, cpu_inst_spec_offset);

bool is_mbraink_pmu_dts_exist(void)
{
	struct device_node *mbraink_pmu_node;
	bool ret = false;

	mbraink_pmu_node = of_find_node_by_name(NULL, "mbraink-pmu-info");
	if (!mbraink_pmu_node) {
		pr_info("failed to find node @ %s\n", __func__);
		ret = false;
	} else {
		pr_info("mbraink-pmu-info node found.\n");
		ret = true;
	}
	return ret;
}

u32 get_mbraink_pmu_dts_property(const char *property_name)
{
	struct device_node *mbraink_pmu_node;
	u32 para = 0;

	mbraink_pmu_node = of_find_node_by_name(NULL, "mbraink-pmu-info");
	if (mbraink_pmu_node == NULL)
		pr_info("failed to find node @ %s\n", __func__);
	else {
		int ret;

		ret = of_property_read_u32(mbraink_pmu_node, property_name, &para);
		if (ret < 0)
			pr_info("no %s dts_ret=%d\n", property_name, ret);
		else
			pr_info("%s enabled\n", property_name);
	}

	return para;
}

#if IS_ENABLED(CONFIG_MTK_LPM_MT6989)

int mbraink_pmu_init(void)
{
	int ret = 0;
	u32 cdfv_tcm_base_start = 0, pmu_tcm_base_start = 0;
	struct device_node *dn = NULL;
	struct platform_device *pdev = NULL;
	struct resource *csram_res = NULL;

	pr_notice("mbraink pmu init.\n");
	csram_base = NULL;
	pmu_tcm_base = NULL;
	mbraink_pmu_dts_exist = is_mbraink_pmu_dts_exist();
	if (mbraink_pmu_dts_exist) {
		cpu_inst_spec_offset = get_mbraink_pmu_dts_property("cpu-inst-spec-offset");
		cdfv_tcm_base_start = get_mbraink_pmu_dts_property("cdfv-tcm-base");
		if (cdfv_tcm_base_start == 0) {
			ret = -ENODEV;
			pr_info("%s: find cdfv-tcm-base from dts failed\n", __func__);
			goto get_base_failed;
		} else {
			csram_base = ioremap(cdfv_tcm_base_start, get_mbraink_pmu_dts_property("cdfv-tcm-base-len"));
		}

		pmu_tcm_base_start = get_mbraink_pmu_dts_property("pmu-tcm-base");
		if (pmu_tcm_base_start == 0) {
			ret = -ENODEV;
			pr_info("%s: find pmu-tcm-base from dts failed\n", __func__);
			goto get_base_failed;
		} else {
			pmu_tcm_base = ioremap(pmu_tcm_base_start, get_mbraink_pmu_dts_property("pmu-tcm-base-len"));
		}
	} else {
		/* get cpufreq driver base address */
		dn = of_find_node_by_name(NULL, "cpuhvfs");
		if (!dn) {
			ret = -ENOMEM;
			pr_info("%s: find cpuhvfs node failed\n", __func__);
			goto get_base_failed;
		}

		pdev = of_find_device_by_node(dn);
		of_node_put(dn);
		if (!pdev) {
			ret = -ENODEV;
			pr_info("%s: cpuhvfs is not ready\n", __func__);
			goto get_base_failed;
		}

		csram_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!csram_res) {
			ret = -ENODEV;
			pr_info("%s: cpuhvfs resource is not found\n", __func__);
			goto get_base_failed;
		}

		csram_base = ioremap(csram_res->start, resource_size(csram_res));
		if (IS_ERR_OR_NULL((void *)csram_base)) {
			ret = -ENOMEM;
			pr_info("%s: find csram base failed\n", __func__);
			goto get_base_failed;
		}
	}
get_base_failed:
	return ret;
}

int mbraink_pmu_uninit(void)
{
	pr_notice("mbraink pmu uninit.\n");
	if (!csram_base) {
		iounmap(csram_base);
		csram_base = NULL;
	}
	if (!pmu_tcm_base) {
		iounmap(pmu_tcm_base);
		pmu_tcm_base = NULL;
	}
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
		set_pmu_enable(1);
	} else {
		uninit_pmu_keep_data();
		set_pmu_enable(0);
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
	mbraink_pmu_dts_exist = false;
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
