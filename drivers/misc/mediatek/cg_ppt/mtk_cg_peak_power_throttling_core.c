// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Clouds Lee <clouds.lee@mediatek.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>

#define CREATE_TRACE_POINTS
#include "mtk_cg_peak_power_throttling_trace.h"

#include "mtk_cg_peak_power_throttling_table.h"
#include "mtk_cg_peak_power_throttling_def.h"

/*
 * -----------------------------------------------
 * Memory Layout
 * -----------------------------------------------
 */
void __iomem *g_thermal_sram_virt_addr;
void __iomem *g_dlpt_sram_virt_addr;
struct DlptSramLayout *g_dlpt_sram_layout_ptr;

/*
 * -----------------------------------------------
 * Platform Data
 * -----------------------------------------------
 */
static const struct platform_data *g_platform_data;

/*
 * -----------------------------------------------
 * Sysfs device node
 * -----------------------------------------------
 */
#define DRIVER_NAME "cg_ppt" /*cg_peak_power_throttling*/

static struct class *g_sysfs_class;
static struct device *g_sysfs_device;

/*
 * -----------------------------------------------
 * Deferrable Timer
 * -----------------------------------------------
 */
static bool g_defer_timer_enabled; /*= false*/
static struct timer_list g_defer_timer;
static int g_defer_timer_period_ms;

/*
 * -----------------------------------------------
 * HR Timer
 * -----------------------------------------------
 */
// Timer state
static bool g_hr_timer_enabled; /*= false*/

// Timer
static struct hrtimer g_hr_timer;
static ktime_t hr_timer_interval_ns;

/*
 * -----------------------------------------------
 * work queue
 * -----------------------------------------------
 */
static struct work_struct g_cgppt_work;
static struct work_struct g_trace_work;


/*
 * -----------------------------------------------
 * Platform Data
 * -----------------------------------------------
 */
struct platform_data {
	/* Platform-specific settings */
	int default_cg_ppt_mode;
	int default_mo_gpu_curr_freq_power_calc;
	int default_defer_timer_period_ms;
	int default_gacboost_mode;
};

static const struct platform_data default_platform_data = {
	/* MT6989-specific settings */
	.default_cg_ppt_mode = 2, /*OFF*/
	.default_mo_gpu_curr_freq_power_calc = 1,
	.default_defer_timer_period_ms = 1000,
	.default_gacboost_mode = 0
};

static const struct platform_data mt6989_platform_data = {
	/* MT6989-specific settings */
	.default_cg_ppt_mode = 0,
	.default_mo_gpu_curr_freq_power_calc = 1,
	.default_defer_timer_period_ms = 1000,
	.default_gacboost_mode = 0
};



static const struct of_device_id cgppt_of_ids[] = {
	{ .compatible = "mediatek,MT6989", .data = &mt6989_platform_data },
	{ /* sentinel */ }
};



/*
 * -----------------------------------------------
 * cpu cluster max freq
 * -----------------------------------------------
 */

static void data_init_cpu_max_freq(void)
{
	unsigned int cpu_cluster_max_freq[CPU_MAX_CLUSTERS];
	struct cpufreq_policy *policy;
	unsigned int cpu;
	int cluster;

	memset(cpu_cluster_max_freq, 0, sizeof(cpu_cluster_max_freq));

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		// cluster = topology_physical_package_id(cpu);
		cluster = topology_cluster_id(cpu);
		if (cluster >= CPU_MAX_CLUSTERS) {
			pr_info("[CG PPT] CPU Cluster id %d exceeds CPU_MAX_CLUSTERS\n",
				cluster);
			cpufreq_cpu_put(policy);
			continue;
		}
		if (policy->cpuinfo.max_freq > cpu_cluster_max_freq[cluster])
			cpu_cluster_max_freq[cluster] = policy->cpuinfo.max_freq;
		cpufreq_cpu_put(policy);
	}

	for (cluster = 0; cluster < CPU_MAX_CLUSTERS; cluster++) {
		iowrite32(cpu_cluster_max_freq[cluster]/1000,
		&g_dlpt_sram_layout_ptr->cswrun_info.cpu_max_freq_m[cluster]);

		// g_dlpt_sram_layout_ptr->cswrun_info.cpu_max_freq_m[cluster]
		// = cpu_cluster_max_freq[cluster]/1000;

		// if (cpu_cluster_max_freq[cluster] > 0)
		pr_info("[CG PPT] Cluster %d max freq_m: %u\n",
			cluster,
			ioread32(&g_dlpt_sram_layout_ptr->cswrun_info.cpu_max_freq_m[cluster]));
	}
}


/*
 * ========================================================
 * Data initial
 * ========================================================
 */
static void data_init_dlptsram(uint32_t *mem, size_t size)
{
	size_t i;

	for (i = 0; i < size / sizeof(uint32_t); ++i)
		mem[i] = PPT_SRAM_INIT_VALUE;
}

static void data_init_movetable(int value, int b_dry)
{
	memcpy_toio(g_dlpt_sram_layout_ptr->ip_peak_power_table, ip_peak_power_table,
	       sizeof(ip_peak_power_table));
	memcpy_toio(g_dlpt_sram_layout_ptr->leakage_scale_table, leakage_scale_table,
	       sizeof(leakage_scale_table));
	memcpy_toio(g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu,
	       peak_power_combo_table_gpu, sizeof(peak_power_combo_table_gpu));
	memcpy_toio(g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu,
	       peak_power_combo_table_cpu, sizeof(peak_power_combo_table_cpu));
	// g_dlpt_sram_layout_ptr->data_moved = 1;
	iowrite32(1, &g_dlpt_sram_layout_ptr->data_moved);
}

static int data_init(void)
{
	phys_addr_t dlpt_phys_addr = DLPT_CSRAM_BASE; //0x116400;
	unsigned int dlpt_mem_size =
		DLPT_CSRAM_SIZE; //5*1024; //total 5K divide into 2
	phys_addr_t thermal_phys_addr = THERMAL_CSRAM_BASE; //0x00114000;
	unsigned int thermal_mem_size = THERMAL_CSRAM_SIZE; //1K

	pr_info("[CG PPT] size of DlptSramLayout:%lu.\n",
		sizeof(struct DlptSramLayout));

    /*
     * ...................................
     * DLPT SRAM mapping
     * ...................................
     */
	g_dlpt_sram_virt_addr = ioremap(dlpt_phys_addr, dlpt_mem_size);
	if (!g_dlpt_sram_virt_addr) {
		pr_info("[CG PPT] Failed to remap the physical address of dlpt_phys_addr.\n");
		return -ENOMEM;
	}
	//DLPT SRAM Layout
	g_dlpt_sram_layout_ptr = (struct DlptSramLayout *)g_dlpt_sram_virt_addr;
	cg_ppt_dlpt_sram_remap((uintptr_t)g_dlpt_sram_virt_addr);

	pr_info("[CG PPT] dlpt_phys_addr = %llx\n",
		(unsigned long long)dlpt_phys_addr);
	pr_info("[CG PPT] g_dlpt_sram_virt_addr = %llx\n",
		(unsigned long long)g_dlpt_sram_virt_addr);
	pr_info("[CG PPT] g_dlpt_sram_layout_ptr->ip_peak_power_table = %llx\n",
		(unsigned long long)
			g_dlpt_sram_layout_ptr->ip_peak_power_table);
	pr_info("[CG PPT] g_dlpt_sram_layout_ptr->leakage_scale_table = %llx\n",
		(unsigned long long)
			g_dlpt_sram_layout_ptr->leakage_scale_table);
	pr_info("[CG PPT] g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu = %llx\n",
		(unsigned long long)g_dlpt_sram_layout_ptr
			->peak_power_combo_table_gpu);
	pr_info("[CG PPT] g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu = %llx\n",
		(unsigned long long)g_dlpt_sram_layout_ptr
			->peak_power_combo_table_cpu);


	//Initial DLPT SRAM
	data_init_dlptsram(g_dlpt_sram_virt_addr,
			   DLPT_CSRAM_SIZE - DLPT_CSRAM_CTRL_RESERVED_SIZE);


    /*
     * ...................................
     * Thermal SRAM mapping
     * ...................................
     */
	g_thermal_sram_virt_addr = ioremap(thermal_phys_addr, thermal_mem_size);
	if (!g_thermal_sram_virt_addr) {
		pr_info("[CG PPT] Failed to remap the physical address of thermal_phys_addr.\n");
		return -ENOMEM;
	}
	cg_ppt_thermal_sram_remap((uintptr_t)g_thermal_sram_virt_addr);
	pr_info("[CG PPT] thermal_phys_addr=%llx\n",
		(unsigned long long)thermal_phys_addr);
	pr_info("[CG PPT] g_thermal_sram_virt_addr=%llx\n",
		(unsigned long long)g_thermal_sram_virt_addr);

    /*
     * ...................................
     * Default Setting
     * ...................................
     */
	{
	struct DlptCsramCtrlBlock *dlpt_csram_ctrl_block =
		dlpt_csram_ctrl_block_get();

	iowrite32(g_platform_data->default_cg_ppt_mode, &dlpt_csram_ctrl_block->peak_power_budget_mode);
	iowrite32(g_platform_data->default_mo_gpu_curr_freq_power_calc,
		&g_dlpt_sram_layout_ptr->mo_info.mo_gpu_curr_freq_power_calc);
	iowrite32(g_platform_data->default_gacboost_mode, &g_dlpt_sram_layout_ptr->cgsm_info.gacboost_mode);

	g_defer_timer_period_ms = g_platform_data->default_defer_timer_period_ms;

	}

    /*
     * ...................................
     * Move data to SRAM
     * ...................................
     */
	data_init_movetable(0, 0);

	data_init_cpu_max_freq();

	return 0;
}

static void data_deinit(void)
{
	if (g_dlpt_sram_virt_addr != 0)
		iounmap(g_dlpt_sram_virt_addr);

	if (g_thermal_sram_virt_addr != 0)
		iounmap(g_thermal_sram_virt_addr);
}


/*
 * ========================================================
 * trace data
 * ========================================================
 */
static struct cg_ppt_status_info *get_cg_ppt_status_info(void)
{
	static struct cg_ppt_status_info ret_struct;
	struct ThermalCsramCtrlBlock *ThermalCsramCtrlBlock_ptr =
		thermal_csram_ctrl_block_get();
	struct DlptCsramCtrlBlock *DlptCsramCtrlBlock_ptr =
		dlpt_csram_ctrl_block_get();

	// ThermalCsramCtrlBlock
	ret_struct.cpu_low_key = ioread32(&ThermalCsramCtrlBlock_ptr->cpu_low_key);
	ret_struct.g2c_pp_lmt_freq_ack_timeout =
		ioread32(&ThermalCsramCtrlBlock_ptr->g2c_pp_lmt_freq_ack_timeout);
	// DlptSramLayout->cswrun_info
	ret_struct.cg_sync_enable =
		ioread32(&g_dlpt_sram_layout_ptr->cswrun_info.cg_sync_enable);
	ret_struct.is_fastdvfs_enabled =
		ioread32(&g_dlpt_sram_layout_ptr->cswrun_info.is_fastdvfs_enabled);
	// DlptSramLayout->gswrun_info
	ret_struct.gpu_preboost_time_us =
		ioread32(&g_dlpt_sram_layout_ptr->gswrun_info.gpu_preboost_time_us);
	ret_struct.cgsync_action =
		ioread32(&g_dlpt_sram_layout_ptr->gswrun_info.cgsync_action);
	ret_struct.is_gpu_favor = ioread32(&g_dlpt_sram_layout_ptr->gswrun_info.is_gpu_favor);
	ret_struct.combo_idx = ioread32(&g_dlpt_sram_layout_ptr->gswrun_info.combo_idx);
	// DlptCsramCtrlBlock
	ret_struct.peak_power_budget_mode =
		ioread32(&DlptCsramCtrlBlock_ptr->peak_power_budget_mode);
	// DlptSramLayout->mo_info
	ret_struct.mo_status =
		ioread32(&g_dlpt_sram_layout_ptr->mo_info.mo_status);

	return &ret_struct;
}

static struct cg_ppt_freq_info *get_cg_ppt_freq_info(void)
{
	static struct cg_ppt_freq_info ret_struct;
	struct ThermalCsramCtrlBlock *ThermalCsramCtrlBlock_ptr =
		thermal_csram_ctrl_block_get();

	//ThermalCsramCtrlBlock
	ret_struct.g2c_b_pp_lmt_freq =
		ioread32(&ThermalCsramCtrlBlock_ptr->g2c_b_pp_lmt_freq);
	ret_struct.g2c_b_pp_lmt_freq_ack =
		ioread32(&ThermalCsramCtrlBlock_ptr->g2c_b_pp_lmt_freq_ack);
	ret_struct.g2c_m_pp_lmt_freq =
		ioread32(&ThermalCsramCtrlBlock_ptr->g2c_m_pp_lmt_freq);
	ret_struct.g2c_m_pp_lmt_freq_ack =
		ioread32(&ThermalCsramCtrlBlock_ptr->g2c_m_pp_lmt_freq_ack);
	ret_struct.g2c_l_pp_lmt_freq =
		ioread32(&ThermalCsramCtrlBlock_ptr->g2c_l_pp_lmt_freq);
	ret_struct.g2c_l_pp_lmt_freq_ack =
		ioread32(&ThermalCsramCtrlBlock_ptr->g2c_l_pp_lmt_freq_ack);
	// DlptSramLayout->gswrun_info
	ret_struct.gpu_limit_freq_m =
		ioread32(&g_dlpt_sram_layout_ptr->gswrun_info.gpu_limit_freq_m);

	return &ret_struct;
}

static struct cg_ppt_power_info *get_cg_ppt_power_info(void)
{
	static struct cg_ppt_power_info ret_struct;
	struct DlptCsramCtrlBlock *DlptCsramCtrlBlock_ptr =
		dlpt_csram_ctrl_block_get();

	//DlptSramLayout->gswrun_info
	ret_struct.cgppb_mw = ioread32(&g_dlpt_sram_layout_ptr->gswrun_info.cgppb_mw);
	//DlptCsramCtrl
	ret_struct.cg_min_power_mw = ioread32(&DlptCsramCtrlBlock_ptr->cg_min_power_mw);
	ret_struct.vsys_power_budget_mw =
		ioread32(&DlptCsramCtrlBlock_ptr->vsys_power_budget_mw);
	ret_struct.vsys_power_budget_ack =
		ioread32(&DlptCsramCtrlBlock_ptr->vsys_power_budget_ack);
	ret_struct.flash_peak_power_mw =
		ioread32(&DlptCsramCtrlBlock_ptr->flash_peak_power_mw);
	ret_struct.audio_peak_power_mw =
		ioread32(&DlptCsramCtrlBlock_ptr->audio_peak_power_mw);
	ret_struct.camera_peak_power_mw =
		ioread32(&DlptCsramCtrlBlock_ptr->camera_peak_power_mw);
	ret_struct.apu_peak_power_mw =
		ioread32(&DlptCsramCtrlBlock_ptr->apu_peak_power_mw);
	ret_struct.display_lcd_peak_power_mw =
		ioread32(&DlptCsramCtrlBlock_ptr->display_lcd_peak_power_mw);
	ret_struct.dram_peak_power_mw =
		ioread32(&DlptCsramCtrlBlock_ptr->dram_peak_power_mw);
	/*shadow mem*/
	ret_struct.modem_peak_power_mw_shadow =
		ioread32(&DlptCsramCtrlBlock_ptr->modem_peak_power_mw_shadow);
	ret_struct.wifi_peak_power_mw_shadow =
		ioread32(&DlptCsramCtrlBlock_ptr->wifi_peak_power_mw_shadow);
	/*misc*/
	ret_struct.apu_peak_power_ack =
		ioread32(&DlptCsramCtrlBlock_ptr->apu_peak_power_ack);

	return &ret_struct;
}

static struct cg_ppt_combo_info *get_cg_ppt_combo_info(void)
{
	static struct cg_ppt_combo_info ret_struct;
	int last_i;

	//DlptSramLayout->gswrun_info
	ret_struct.cgppb_mw = g_dlpt_sram_layout_ptr->gswrun_info.cgppb_mw;

	ret_struct.gpu_combo0 =
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu[0].combopeakpowerin_mw);
	ret_struct.gpu_combo1 =
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu[1].combopeakpowerin_mw);
	ret_struct.gpu_combo2 =
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu[2].combopeakpowerin_mw);
	ret_struct.gpu_combo3 =
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu[3].combopeakpowerin_mw);
	last_i = GPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT-1;
	ret_struct.gpu_combo4 =
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu[last_i].combopeakpowerin_mw);


	ret_struct.cpu_combo0 =
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu[0].combopeakpowerin_mw);
	ret_struct.cpu_combo1 =
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu[1].combopeakpowerin_mw);
	ret_struct.cpu_combo2 =
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu[2].combopeakpowerin_mw);
	ret_struct.cpu_combo3 =
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu[3].combopeakpowerin_mw);
	last_i = CPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT-1;
	ret_struct.cpu_combo4 =
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu[last_i].combopeakpowerin_mw);

	return &ret_struct;
}

static struct cg_sm_info *get_cg_sm_info(void)
{
	static struct cg_sm_info ret_struct;

	// memcpy_fromio(&ret_struct, &g_dlpt_sram_layout_ptr->cgsm_info, sizeof(struct cg_sm_info));
	//DlptSramLayout->cgsm_info
	ret_struct.gacboost_mode = ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gacboost_mode);
	ret_struct.gacboost_hint = ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gacboost_hint);
	ret_struct.gf_ema_1 = ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gf_ema_1);
	ret_struct.gf_ema_2 = ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gf_ema_2);
	ret_struct.gf_ema_3 = ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gf_ema_3);
	ret_struct.gt_ema_1 = ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gt_ema_1);
	ret_struct.gt_ema_2 = ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gt_ema_2);
	ret_struct.gt_ema_3 = ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gt_ema_3);


	return &ret_struct;
}

/*
 * ========================================================
 * Sysfs device node
 * ========================================================
 */
/*
 * -----------------------------------------------
 * device node: mode
 * -----------------------------------------------
 */
static ssize_t mode_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct DlptCsramCtrlBlock *dlpt_csram_ctrl_block
		= dlpt_csram_ctrl_block_get();

	return snprintf(buf, PAGE_SIZE, "%d\n",
			dlpt_csram_ctrl_block->peak_power_budget_mode);
}

static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int num;
	char user_input[32];

	strncpy(user_input, buf, sizeof(user_input) - 1);
	user_input[sizeof(user_input) - 1] = 0;

    /*
     * ...................................
     * set cg_ppt_mode
     * ...................................
     */
	if (kstrtoint(user_input, 10, &num) == 0) {
		struct DlptCsramCtrlBlock *dlpt_csram_ctrl_block =
			dlpt_csram_ctrl_block_get();

		dlpt_csram_ctrl_block->peak_power_budget_mode = num;
		pr_info("[CG PPT] peak_power_budget_mode = %d\n",
			dlpt_csram_ctrl_block->peak_power_budget_mode);
	}

	return -EINVAL;
}

/*
 * -----------------------------------------------
 * device node: hr_enable
 * -----------------------------------------------
 */
static ssize_t hr_enable_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%d\n", g_hr_timer_enabled ? 1 : 0);
}

static ssize_t hr_enable_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	int input;

	if (kstrtoint(buf, 10, &input) != 0)
		return count;

	if (input == 1 && !g_hr_timer_enabled) {
		g_hr_timer_enabled = true;
		hrtimer_start(&g_hr_timer, hr_timer_interval_ns,
			      HRTIMER_MODE_REL);
		pr_info("HR timer started.\n");
	} else if (input == 0 && g_hr_timer_enabled) {
		g_hr_timer_enabled = false;
		pr_info("HR timer stopped.\n");
	}

	return count;
}

/*
 * -----------------------------------------------
 * device node: command
 * -----------------------------------------------
 */
static ssize_t command_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "hello");
}

static ssize_t command_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	if (strncmp(buf, "dump", min(count, strlen("dump"))) == 0) {
		pr_info("[CG PPT] g_dlpt_sram_virt_addr       = %llx\n",
			(unsigned long long)g_dlpt_sram_virt_addr);
		pr_info("[CG PPT] g_dlpt_sram_layout_ptr      = %llx\n",
			(unsigned long long)g_dlpt_sram_layout_ptr);
		pr_info("[CG PPT] g_thermal_sram_virt_addr       = %llx\n",
			(unsigned long long)g_thermal_sram_virt_addr);
		pr_info("\n");
		pr_info("[CG PPT] size of DlptSramLayout:%lu.\n",
			sizeof(struct DlptSramLayout));
		pr_info("[CG PPT] g_dlpt_sram_layout_ptr->ip_peak_power_table    = %llx\n",
			(unsigned long long)
				g_dlpt_sram_layout_ptr->ip_peak_power_table);
		pr_info("[CG PPT] g_dlpt_sram_layout_ptr->leakage_scale_table    = %llx\n",
			(unsigned long long)
				g_dlpt_sram_layout_ptr->leakage_scale_table);
		pr_info("[CG PPT] g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu    = %llx\n",
			(unsigned long long)g_dlpt_sram_layout_ptr
				->peak_power_combo_table_cpu);
		pr_info("[CG PPT] g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu    = %llx\n",
			(unsigned long long)g_dlpt_sram_layout_ptr
				->peak_power_combo_table_gpu);
		pr_info("\n");
		// show table content
		pr_info("\n");
		pr_info("[CG PPT] g_dlpt_sram_layout_ptr struct content\n");
		print_structure_values(g_dlpt_sram_layout_ptr,
				       sizeof(struct DlptSramLayout));
		pr_info("\n");

		/*
		 * ...................................
		 * CPU cluster info
		 * ...................................
		 */
		data_init_cpu_max_freq();

		return count;
	}

	if (strncmp(buf, "show table gpu",
		    min(count, strlen("show table gpu"))) == 0) {
		print_peak_power_combo_table(
			g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu,
			GPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT);
		return count;
	}

	if (strncmp(buf, "show table cpu",
		    min(count, strlen("show table cpu"))) == 0) {
		print_peak_power_combo_table(
			g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu,
			CPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT);
		return count;
	}

    /*
     * ...................................
     * move data
     * ...................................
     */
	if (strncmp(buf, "move data", min(count, strlen("move data"))) == 0) {
		// write table content
		data_init_movetable(1, 0);
		return count;
	}

    /*
     * ...................................
     * move data (dry)
     * ...................................
     */
	if (strncmp(buf, "move data dry",
		    min(count, strlen("move data dry"))) == 0) {
		// write table content
		data_init_movetable(1, 1);
		return count;
	}

	return -EINVAL;
}

/*
 * -----------------------------------------------
 * device node: model_option
 * -----------------------------------------------
 */
static ssize_t model_option_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return snprintf(buf, PAGE_SIZE,
	"favor_cpu: %d\nfavor_gpu: %d\nfavor_multiscene: %d\ncpu_avs: %d\ngpu_avs: %d\ngpu_curr_freq_power_calc: %d\nmo_status: %u\n",
	ioread32(&g_dlpt_sram_layout_ptr->mo_info.mo_favor_cpu),
	ioread32(&g_dlpt_sram_layout_ptr->mo_info.mo_favor_gpu),
	ioread32(&g_dlpt_sram_layout_ptr->mo_info.mo_favor_multiscene),
	ioread32(&g_dlpt_sram_layout_ptr->mo_info.mo_cpu_avs),
	ioread32(&g_dlpt_sram_layout_ptr->mo_info.mo_gpu_avs),
	ioread32(&g_dlpt_sram_layout_ptr->mo_info.mo_gpu_curr_freq_power_calc),
	ioread32(&g_dlpt_sram_layout_ptr->mo_info.mo_status));

}

static ssize_t model_option_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	char cmd[128];
	int val;

	if (sscanf(buf, "%32s %d", cmd, &val) != 2)
		return -EINVAL;


	#define _ASSIGN_MO_(_var_name_, _val_) \
	do { \
		if (strncmp(cmd, #_var_name_, sizeof(cmd)) == 0) \
			iowrite32(_val_, &g_dlpt_sram_layout_ptr->mo_info.mo_##_var_name_); \
	} while (0)

	// g_dlpt_sram_layout_ptr->mo_info.mo_##_var_name_ = _val_;

	_ASSIGN_MO_(favor_cpu, val);
	_ASSIGN_MO_(favor_gpu, val);
	_ASSIGN_MO_(favor_multiscene, val);
	_ASSIGN_MO_(cpu_avs, val);
	_ASSIGN_MO_(gpu_avs, val);
	_ASSIGN_MO_(gpu_curr_freq_power_calc, val);
	_ASSIGN_MO_(status, val);

	return count;

}

/*
 * -----------------------------------------------
 * device node: cgppt_dump
 * -----------------------------------------------
 */
static ssize_t cgppt_dump_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct ThermalCsramCtrlBlock *ThermalCsramCtrlBlock_ptr =
		thermal_csram_ctrl_block_get();
	struct DlptCsramCtrlBlock *DlptCsramCtrlBlock_ptr =
		dlpt_csram_ctrl_block_get();
	int lastgi = GPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT-1;
	int lastci = CPU_PEAK_POWER_COMBO_TABLE_IDX_ROW_COUNT-1;


	return snprintf(buf, PAGE_SIZE,
	"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
	// Status
	ioread32(&DlptCsramCtrlBlock_ptr->peak_power_budget_mode),//cgppt_mod
	ioread32(&g_dlpt_sram_layout_ptr->mo_info.mo_status),//cgppt_m
	ioread32(&ThermalCsramCtrlBlock_ptr->cpu_low_key),//cgppt_clowke
	ioread32(&g_dlpt_sram_layout_ptr->gswrun_info.gpu_preboost_time_us),//cgppt_prebsttim
	ioread32(&g_dlpt_sram_layout_ptr->gswrun_info.is_gpu_favor),//cgppt_gpufavo
	ioread32(&g_dlpt_sram_layout_ptr->gswrun_info.combo_idx),//cgppt_comboid
	// Freq
	ioread32(&ThermalCsramCtrlBlock_ptr->g2c_b_pp_lmt_freq)*1000,//cgppt_blimtfre
	ioread32(&ThermalCsramCtrlBlock_ptr->g2c_m_pp_lmt_freq)*1000,//cgppt_mlimtfre
	ioread32(&ThermalCsramCtrlBlock_ptr->g2c_l_pp_lmt_freq)*1000,//cgppt_llimtfre
	ioread32(&g_dlpt_sram_layout_ptr->gswrun_info.gpu_limit_freq_m)*1000,//cgppt_glimtfre
	// ppb
	ioread32(&g_dlpt_sram_layout_ptr->gswrun_info.cgppb_mw),//cgppt_cgpp
	//cgppt_gcombo0-n
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu[0].combopeakpowerin_mw),
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu[1].combopeakpowerin_mw),
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu[2].combopeakpowerin_mw),
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu[3].combopeakpowerin_mw),
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_gpu[lastgi].combopeakpowerin_mw),
	//cgppt_ccombo0-n
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu[0].combopeakpowerin_mw),
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu[1].combopeakpowerin_mw),
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu[2].combopeakpowerin_mw),
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu[3].combopeakpowerin_mw),
	ioread32(&g_dlpt_sram_layout_ptr->peak_power_combo_table_cpu[lastci].combopeakpowerin_mw)
	);

}

/*
 * -----------------------------------------------
 * device node: cgsm_dump
 * -----------------------------------------------
 */
static ssize_t cgsm_dump_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return snprintf(buf, PAGE_SIZE,
	"%d,%d,%d,%d,%d,%d,%d,%d\n",
	ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gacboost_mode),
	ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gacboost_hint),
	ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gf_ema_1)*1000,
	ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gf_ema_2)*1000,
	ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gf_ema_3)*1000,
	ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gt_ema_1),
	ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gt_ema_2),
	ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gt_ema_3)
	);

}

/*
 * -----------------------------------------------
 * device node: defer_timer_period
 * -----------------------------------------------
 */
static ssize_t defer_timer_period_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%d %d\n", g_defer_timer_period_ms, g_defer_timer_enabled);
}

static ssize_t defer_timer_period_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	int input;

	if (kstrtoint(buf, 10, &input) != 0)
		return count;
	g_defer_timer_period_ms = input;
	return count;
}



/*
 * -----------------------------------------------
 * device node: gacboost_mode
 * -----------------------------------------------
 */
static ssize_t gacboost_mode_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%x\n", ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gacboost_mode));
}

static ssize_t gacboost_mode_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	int input;

	if (kstrtoint(buf, 16, &input) != 0)
		return count;

	// g_dlpt_sram_layout_ptr->cgsm_info.gacboost_mode = input;
	iowrite32(input, &g_dlpt_sram_layout_ptr->cgsm_info.gacboost_mode);

	return count;
}

/*
 * -----------------------------------------------
 * device node: gacboost_hint
 * -----------------------------------------------
 */
static ssize_t gacboost_hint_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%d\n", ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gacboost_hint));
}

/*
 * -----------------------------------------------
 * device node: gpu_passive_flag
 * -----------------------------------------------
 */
static ssize_t gpu_passive_flag_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%d\n", ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gacboost_hint));
}


/*
 * -----------------------------------------------
 * sysfs init
 * -----------------------------------------------
 */
static DEVICE_ATTR_RW(mode);
static DEVICE_ATTR_RW(hr_enable);
static DEVICE_ATTR_RW(command);
static DEVICE_ATTR_RW(model_option);
static DEVICE_ATTR_RO(cgppt_dump);
static DEVICE_ATTR_RO(cgsm_dump);
static DEVICE_ATTR_RW(defer_timer_period);
static DEVICE_ATTR_RW(gacboost_mode);
static DEVICE_ATTR_RO(gacboost_hint);
static DEVICE_ATTR_RO(gpu_passive_flag);


static struct attribute *sysfs_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_hr_enable.attr,
	&dev_attr_command.attr,
	&dev_attr_model_option.attr,
	&dev_attr_cgppt_dump.attr,
	&dev_attr_cgsm_dump.attr,
	&dev_attr_defer_timer_period.attr,
	&dev_attr_gacboost_mode.attr,
	&dev_attr_gacboost_hint.attr,
	&dev_attr_gpu_passive_flag.attr,
	NULL,
};

static const struct attribute_group sysfs_group = {
	.attrs = sysfs_attrs,
};

static int sysfs_device_init(void)
{
	g_sysfs_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(g_sysfs_class)) {
		pr_info("[CG PPT] Failed to create device class\n");
		return PTR_ERR(g_sysfs_class);
	}

	g_sysfs_device = device_create(g_sysfs_class, NULL, 0, NULL, DRIVER_NAME);
	if (IS_ERR(g_sysfs_device)) {
		pr_info("[CG PPT] Failed to create sysfs device\n");
		class_destroy(g_sysfs_class);
		return PTR_ERR(g_sysfs_device);
	}

	if (sysfs_create_group(&g_sysfs_device->kobj, &sysfs_group) != 0) {
		pr_info("[CG PPT] Failed to create sysfs group\n");
		device_destroy(g_sysfs_class, 0);
		class_destroy(g_sysfs_class);
		return -ENOMEM;
	}

	pr_info("[CG PPT] sysfs init\n");
	return 0;
}

static void sysfs_device_deinit(void)
{
	sysfs_remove_group(&g_sysfs_device->kobj, &sysfs_group);
	device_destroy(g_sysfs_class, 0);
	class_destroy(g_sysfs_class);
	pr_info("[CG PPT] sysfs deinit\n");
}

/*
 * ========================================================
 * Export API
 * ========================================================
 */
void cgppt_set_mo_multiscene(int value)
{
	if (g_dlpt_sram_layout_ptr)
		iowrite32(value, &g_dlpt_sram_layout_ptr->mo_info.mo_favor_multiscene);

		// g_dlpt_sram_layout_ptr->mo_info.mo_favor_multiscene = value;
}
EXPORT_SYMBOL(cgppt_set_mo_multiscene);

/*
 * ========================================================
 * CGPPT Workqueue
 * ========================================================
 */
static void cgppt_work_handler(struct work_struct *work)
{

}

static void cgppt_work_init(void)
{
	INIT_WORK(&g_cgppt_work, cgppt_work_handler);
}

static void cgppt_work_deinit(void)
{
	cancel_work_sync(&g_cgppt_work);
}

/*
 * ========================================================
 * trace Workqueue
 * ========================================================
 */
static void trace_work_handler(struct work_struct *work)
{
	/*ftrace event*/
	trace_cg_ppt_status_info(get_cg_ppt_status_info());
	trace_cg_ppt_freq_info(get_cg_ppt_freq_info());
	trace_cg_ppt_power_info(get_cg_ppt_power_info());
	trace_cg_ppt_combo_info(get_cg_ppt_combo_info());
	trace_cg_sm_info(get_cg_sm_info());

}

static void trace_work_init(void)
{
	INIT_WORK(&g_trace_work, trace_work_handler);
}

static void trace_work_deinit(void)
{
	cancel_work_sync(&g_trace_work);
}



/*
 * ========================================================
 * Deferrable Timer
 * ========================================================
 */
static void defer_timer_callback(struct timer_list *t)
{
	pr_info("[CG PPT] defer timer callback.\n");

	/*
	 * ...................................
	 * cgppt work
	 * ...................................
	 */
	schedule_work(&g_cgppt_work);


	if (ioread32(&g_dlpt_sram_layout_ptr->cgsm_info.gacboost_mode) != 0) {
		// Restart the timer
		g_defer_timer_enabled = true;
		mod_timer(&g_defer_timer, jiffies + msecs_to_jiffies(g_defer_timer_period_ms));
	} else
		g_defer_timer_enabled = false;
}

static void defer_timer_init(void)
{
	// Initialize the deferrable timer
	timer_setup(&g_defer_timer, defer_timer_callback, TIMER_DEFERRABLE);
}


static void defer_timer_deinit(void)
{
	// Remove the deferrable timer
	del_timer(&g_defer_timer);
}

/*
 * ========================================================
 * HR Timer
 * ========================================================
 */
// hr_timer callback function
static enum hrtimer_restart hr_timer_callback(struct hrtimer *timer)
{
	// pr_info("hr_timer: timer triggered\n");

	// Restart the timer if it's still enabled
	if (g_hr_timer_enabled) {
		/*
		 * ...................................
		 * trace work
		 * ...................................
		 */
		schedule_work(&g_trace_work);

		hrtimer_forward_now(&g_hr_timer, hr_timer_interval_ns);
		return HRTIMER_RESTART;
	}

	return HRTIMER_NORESTART;
}

static void hr_timer_init(void)
{
	// Initialize and set up hr_timer
	hrtimer_init(&g_hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_hr_timer.function = hr_timer_callback;
	hr_timer_interval_ns = ktime_set(0, 1000 * 1000); // 1 ms
}

static void hr_timer_deinit(void)
{
	if (g_hr_timer_enabled)
		hrtimer_cancel(&g_hr_timer);
}

/*
 * ========================================================
 * Platform Data Initial
 * ========================================================
 */
static const struct platform_data *get_platform_data(void)
{
	const struct platform_data *ret = &default_platform_data;
	struct device_node *root;
	const struct of_device_id *match;
	const char *compatible;
	int len;


	/*
	 * ...................................
	 * find device tree compatible
	 * ...................................
	 */
	root = of_find_node_by_path("/");
	if (!root) {
		pr_info("[CG PPT] Failed to find root node\n");
		return ret;
	}

	/*
	 * ...................................
	 * root compatible info
	 * ...................................
	 */
	compatible = of_get_property(root, "compatible", &len);
	if (!compatible) {
		pr_info("[CG PPT] No compatible property in root, use default.\n");
		of_node_put(root);
		return ret;
	}
	pr_info("[CG PPT] Root node compatible: %.*s\n", len, compatible);


	/*
	 * ...................................
	 * match compatible
	 * ...................................
	 */
	match = of_match_node(cgppt_of_ids, root);
	if (!match)
		pr_info("[CG PPT] No matching compatible, use default.\n");
	else {
		/*
		 * ...................................
		 * set platform data to return
		 * ...................................
		 */
		pr_info("[CG PPT] Matching node compatible: %s\n", match->compatible);
		ret = match->data;
	}

	of_node_put(root);

	return ret;

}



/*
 * ========================================================
 * Module Initial
 * ========================================================
 */
// Module init function
static int __init cg_peak_power_throttling_init(void)
{
	int result;

	pr_info("[CG PPT] throttling module: Init\n");

	// get platform data
	g_platform_data = get_platform_data();
	pr_info("[CG PPT] g_platform_data->default_cg_ppt_mode = %d\n",
		g_platform_data->default_cg_ppt_mode);


	// create sysfs
	result = sysfs_device_init();
	if (result != 0) {
		pr_info("[CG PPT] sysfs_device_init() failed.\n");
		return result;
	}

	// move peak table to sram
	result = data_init();
	if (result != 0) {
		pr_info("[CG PPT] data_init() failed.\n");
		return result;
	}

	// defer timer initial
	defer_timer_init();

	// HR timer initial
	hr_timer_init();

	//cgppt work
	cgppt_work_init();

	//trace work
	trace_work_init();

	return 0;
}

// Module exit function
static void __exit cg_peak_power_throttling_exit(void)
{
	pr_info("[CG PPT] throttling module: Exit\n");


	//delete sysfs device
	sysfs_device_deinit();

	// data deinit
	data_deinit();

	// deinit deffer timer
	defer_timer_deinit();

	// deinit hr timer
	hr_timer_deinit();

	//cgppt work
	cgppt_work_deinit();

	//cgppt work
	trace_work_deinit();
}

module_init(cg_peak_power_throttling_init);
module_exit(cg_peak_power_throttling_exit);

MODULE_AUTHOR("Clouds Lee");
MODULE_DESCRIPTION("MTK cg peak power throttling driver");
MODULE_LICENSE("GPL");
