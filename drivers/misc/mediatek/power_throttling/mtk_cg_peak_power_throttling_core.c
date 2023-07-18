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

#define CREATE_TRACE_POINTS
#include "mtk_cg_peak_power_throttling_trace.h"

#include "mtk_cg_peak_power_throttling_table.h"
#include "mtk_cg_peak_power_throttling_def.h"

#define DEFER_TIMER 0

/*
 * -----------------------------------------------
 * Memory Layout
 * -----------------------------------------------
 */
void __iomem *thermal_sram_virt_addr;
void __iomem *dlpt_sram_virt_addr;
struct DlptSramLayout *dlpt_sram_layout_ptr;

/*
 * -----------------------------------------------
 * default setting
 * -----------------------------------------------
 */
#define DEFAULT_CG_PPT_MODE 0

/*
 * -----------------------------------------------
 * Sysfs device node
 * -----------------------------------------------
 */
#define DRIVER_NAME "cg_ppt" /*cg_peak_power_throttling*/

static struct class *sysfs_class;
static struct device *sysfs_device;

/*
 * -----------------------------------------------
 * Deferrable Timer
 * -----------------------------------------------
 */
#if DEFER_TIMER
static struct timer_list defer_timer;
#endif

/*
 * -----------------------------------------------
 * HR Timer
 * -----------------------------------------------
 */
// Timer state
static bool hr_timer_enabled; /*= false*/

// Timer
static struct hrtimer hr_timer;
static ktime_t hr_timer_interval_ns;

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
	memcpy_toio(dlpt_sram_layout_ptr->ip_peak_power_table, ip_peak_power_table,
	       sizeof(ip_peak_power_table));
	memcpy_toio(dlpt_sram_layout_ptr->leakage_scale_table, leakage_scale_table,
	       sizeof(leakage_scale_table));
	memcpy_toio(dlpt_sram_layout_ptr->peak_power_combo_table_gpu,
	       peak_power_combo_table_gpu, sizeof(peak_power_combo_table_gpu));
	memcpy_toio(dlpt_sram_layout_ptr->peak_power_combo_table_cpu,
	       peak_power_combo_table_cpu, sizeof(peak_power_combo_table_cpu));
	dlpt_sram_layout_ptr->data_moved = 1;
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
	dlpt_sram_virt_addr = ioremap(dlpt_phys_addr, dlpt_mem_size);
	if (!dlpt_sram_virt_addr) {
		pr_info("[CG PPT] Failed to remap the physical address of dlpt_phys_addr.\n");
		return -ENOMEM;
	}
	//DLPT SRAM Layout
	dlpt_sram_layout_ptr = (struct DlptSramLayout *)dlpt_sram_virt_addr;
	cg_ppt_dlpt_sram_remap((uintptr_t)dlpt_sram_virt_addr);

	pr_info("[CG PPT] dlpt_phys_addr = %llx\n",
		(unsigned long long)dlpt_phys_addr);
	pr_info("[CG PPT] dlpt_sram_virt_addr = %llx\n",
		(unsigned long long)dlpt_sram_virt_addr);
	pr_info("[CG PPT] dlpt_sram_layout_ptr->ip_peak_power_table = %llx\n",
		(unsigned long long)
			dlpt_sram_layout_ptr->ip_peak_power_table);
	pr_info("[CG PPT] dlpt_sram_layout_ptr->leakage_scale_table = %llx\n",
		(unsigned long long)
			dlpt_sram_layout_ptr->leakage_scale_table);
	pr_info("[CG PPT] dlpt_sram_layout_ptr->peak_power_combo_table_gpu = %llx\n",
		(unsigned long long)dlpt_sram_layout_ptr
			->peak_power_combo_table_gpu);
	pr_info("[CG PPT] dlpt_sram_layout_ptr->peak_power_combo_table_cpu = %llx\n",
		(unsigned long long)dlpt_sram_layout_ptr
			->peak_power_combo_table_cpu);


	//Initial DLPT SRAM
	data_init_dlptsram(dlpt_sram_virt_addr,
			   DLPT_CSRAM_SIZE - DLPT_CSRAM_CTRL_RESERVED_SIZE);


    /*
     * ...................................
     * Thermal SRAM mapping
     * ...................................
     */
	thermal_sram_virt_addr = ioremap(thermal_phys_addr, thermal_mem_size);
	if (!thermal_sram_virt_addr) {
		pr_info("[CG PPT] Failed to remap the physical address of thermal_phys_addr.\n");
		return -ENOMEM;
	}
	cg_ppt_thermal_sram_remap((uintptr_t)thermal_sram_virt_addr);
	pr_info("[CG PPT] thermal_phys_addr=%llx\n",
		(unsigned long long)thermal_phys_addr);
	pr_info("[CG PPT] thermal_sram_virt_addr=%llx\n",
		(unsigned long long)thermal_sram_virt_addr);

    /*
     * ...................................
     * Set default cg ppt mode to 0
     * ...................................
     */
	{
		struct DlptCsramCtrlBlock *dlpt_csram_ctrl_block =
			dlpt_csram_ctrl_block_get();
		dlpt_csram_ctrl_block->peak_power_budget_mode =
			DEFAULT_CG_PPT_MODE;
	}

    /*
     * ...................................
     * Move data to SRAM
     * ...................................
     */
	data_init_movetable(0, 0);

	return 0;
}

static void data_deinit(void)
{
	if (dlpt_sram_virt_addr != 0)
		iounmap(dlpt_sram_virt_addr);

	if (thermal_sram_virt_addr != 0)
		iounmap(thermal_sram_virt_addr);
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
	ret_struct.cpu_low_key = ThermalCsramCtrlBlock_ptr->cpu_low_key;
	ret_struct.g2c_pp_lmt_freq_ack_timeout =
		ThermalCsramCtrlBlock_ptr->g2c_pp_lmt_freq_ack_timeout;
	// DlptSramLayout->cswrun_info
	ret_struct.cg_sync_enable =
		dlpt_sram_layout_ptr->cswrun_info.cg_sync_enable;
	ret_struct.is_fastdvfs_enabled =
		dlpt_sram_layout_ptr->cswrun_info.is_fastdvfs_enabled;
	// DlptSramLayout->gswrun_info
	ret_struct.gpu_preboost_time_us =
		dlpt_sram_layout_ptr->gswrun_info.gpu_preboost_time_us;
	ret_struct.cgsync_action =
		dlpt_sram_layout_ptr->gswrun_info.cgsync_action;
	ret_struct.is_gpu_favor = dlpt_sram_layout_ptr->gswrun_info.is_gpu_favor;
	ret_struct.combo_idx = dlpt_sram_layout_ptr->gswrun_info.combo_idx;
	// DlptCsramCtrlBlock
	ret_struct.peak_power_budget_mode =
		DlptCsramCtrlBlock_ptr->peak_power_budget_mode;

	return &ret_struct;
}

static struct cg_ppt_freq_info *get_cg_ppt_freq_info(void)
{
	static struct cg_ppt_freq_info ret_struct;
	struct ThermalCsramCtrlBlock *ThermalCsramCtrlBlock_ptr =
		thermal_csram_ctrl_block_get();

	//ThermalCsramCtrlBlock
	ret_struct.g2c_b_pp_lmt_freq =
		ThermalCsramCtrlBlock_ptr->g2c_b_pp_lmt_freq;
	ret_struct.g2c_b_pp_lmt_freq_ack =
		ThermalCsramCtrlBlock_ptr->g2c_b_pp_lmt_freq_ack;
	ret_struct.g2c_m_pp_lmt_freq =
		ThermalCsramCtrlBlock_ptr->g2c_m_pp_lmt_freq;
	ret_struct.g2c_m_pp_lmt_freq_ack =
		ThermalCsramCtrlBlock_ptr->g2c_m_pp_lmt_freq_ack;
	ret_struct.g2c_l_pp_lmt_freq =
		ThermalCsramCtrlBlock_ptr->g2c_l_pp_lmt_freq;
	ret_struct.g2c_l_pp_lmt_freq_ack =
		ThermalCsramCtrlBlock_ptr->g2c_l_pp_lmt_freq_ack;
	// DlptSramLayout->gswrun_info
	ret_struct.gpu_limit_freq_m =
		dlpt_sram_layout_ptr->gswrun_info.gpu_limit_freq_m;

	return &ret_struct;
}

static struct cg_ppt_power_info *get_cg_ppt_power_info(void)
{
	static struct cg_ppt_power_info ret_struct;
	struct DlptCsramCtrlBlock *DlptCsramCtrlBlock_ptr =
		dlpt_csram_ctrl_block_get();

	//DlptSramLayout->gswrun_info
	ret_struct.cgppb_mw = dlpt_sram_layout_ptr->gswrun_info.cgppb_mw;
	//DlptCsramCtrl
	ret_struct.cg_min_power_mw = DlptCsramCtrlBlock_ptr->cg_min_power_mw;
	ret_struct.vsys_power_budget_mw =
		DlptCsramCtrlBlock_ptr->vsys_power_budget_mw;
	ret_struct.vsys_power_budget_ack =
		DlptCsramCtrlBlock_ptr->vsys_power_budget_ack;
	ret_struct.flash_peak_power_mw =
		DlptCsramCtrlBlock_ptr->flash_peak_power_mw;
	ret_struct.audio_peak_power_mw =
		DlptCsramCtrlBlock_ptr->audio_peak_power_mw;
	ret_struct.camera_peak_power_mw =
		DlptCsramCtrlBlock_ptr->camera_peak_power_mw;
	ret_struct.apu_peak_power_mw =
		DlptCsramCtrlBlock_ptr->apu_peak_power_mw;
	ret_struct.display_lcd_peak_power_mw =
		DlptCsramCtrlBlock_ptr->display_lcd_peak_power_mw;
	ret_struct.dram_peak_power_mw =
		DlptCsramCtrlBlock_ptr->dram_peak_power_mw;
	/*shadow mem*/
	ret_struct.modem_peak_power_mw_shadow =
		DlptCsramCtrlBlock_ptr->modem_peak_power_mw_shadow;
	ret_struct.wifi_peak_power_mw_shadow =
		DlptCsramCtrlBlock_ptr->wifi_peak_power_mw_shadow;
	/*misc*/
	ret_struct.apu_peak_power_ack =
		DlptCsramCtrlBlock_ptr->apu_peak_power_ack;

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
	return sprintf(buf, "%d\n", hr_timer_enabled ? 1 : 0);
}

static ssize_t hr_enable_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	int input;

	if (kstrtoint(buf, 10, &input) != 0)
		return count;

	if (input == 1 && !hr_timer_enabled) {
		hr_timer_enabled = true;
		hrtimer_start(&hr_timer, hr_timer_interval_ns,
			      HRTIMER_MODE_REL);
		pr_info("HR timer started.\n");
	} else if (input == 0 && hr_timer_enabled) {
		hr_timer_enabled = false;
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
		pr_info("[CG PPT] dlpt_sram_virt_addr       = %llx\n",
			(unsigned long long)dlpt_sram_virt_addr);
		pr_info("[CG PPT] dlpt_sram_layout_ptr      = %llx\n",
			(unsigned long long)dlpt_sram_layout_ptr);
		pr_info("[CG PPT] thermal_sram_virt_addr       = %llx\n",
			(unsigned long long)thermal_sram_virt_addr);
		pr_info("\n");
		pr_info("[CG PPT] size of DlptSramLayout:%lu.\n",
			sizeof(struct DlptSramLayout));
		pr_info("[CG PPT] dlpt_sram_layout_ptr->ip_peak_power_table    = %llx\n",
			(unsigned long long)
				dlpt_sram_layout_ptr->ip_peak_power_table);
		pr_info("[CG PPT] dlpt_sram_layout_ptr->leakage_scale_table    = %llx\n",
			(unsigned long long)
				dlpt_sram_layout_ptr->leakage_scale_table);
		pr_info("[CG PPT] dlpt_sram_layout_ptr->peak_power_combo_table_cpu    = %llx\n",
			(unsigned long long)dlpt_sram_layout_ptr
				->peak_power_combo_table_cpu);
		pr_info("[CG PPT] dlpt_sram_layout_ptr->peak_power_combo_table_gpu    = %llx\n",
			(unsigned long long)dlpt_sram_layout_ptr
				->peak_power_combo_table_gpu);
		pr_info("\n");
		// show table content
		pr_info("\n");
		pr_info("[CG PPT] dlpt_sram_layout_ptr struct content\n");
		print_structure_values(dlpt_sram_layout_ptr,
				       sizeof(struct DlptSramLayout));
		pr_info("\n");

		return count;
	}

	if (strncmp(buf, "show table gpu",
		    min(count, strlen("show table gpu"))) == 0) {
		print_peak_power_combo_table(
			dlpt_sram_layout_ptr->peak_power_combo_table_gpu,
			PEAK_POWER_COMBO_TABLE_GPU_IDX_ROW_COUNT);
		return count;
	}

	if (strncmp(buf, "show table cpu",
		    min(count, strlen("show table cpu"))) == 0) {
		print_peak_power_combo_table(
			dlpt_sram_layout_ptr->peak_power_combo_table_cpu,
			PEAK_POWER_COMBO_TABLE_CPU_IDX_ROW_COUNT);
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
 * sysfs init
 * -----------------------------------------------
 */
static DEVICE_ATTR_RW(mode);
static DEVICE_ATTR_RW(hr_enable);
static DEVICE_ATTR_RW(command);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_hr_enable.attr,
	&dev_attr_command.attr,
	NULL,
};

static const struct attribute_group sysfs_group = {
	.attrs = sysfs_attrs,
};

static int sysfs_device_init(void)
{
	sysfs_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(sysfs_class)) {
		pr_info("[CG PPT] Failed to create device class\n");
		return PTR_ERR(sysfs_class);
	}

	sysfs_device = device_create(sysfs_class, NULL, 0, NULL, DRIVER_NAME);
	if (IS_ERR(sysfs_device)) {
		pr_info("[CG PPT] Failed to create sysfs device\n");
		class_destroy(sysfs_class);
		return PTR_ERR(sysfs_device);
	}

	if (sysfs_create_group(&sysfs_device->kobj, &sysfs_group) != 0) {
		pr_info("[CG PPT] Failed to create sysfs group\n");
		device_destroy(sysfs_class, 0);
		class_destroy(sysfs_class);
		return -ENOMEM;
	}

	pr_info("[CG PPT] %s driver loaded\n", DRIVER_NAME);
	return 0;
}

static void sysfs_device_deinit(void)
{
	sysfs_remove_group(&sysfs_device->kobj, &sysfs_group);
	device_destroy(sysfs_class, 0);
	class_destroy(sysfs_class);
	pr_info("[CG PPT] %s driver unloaded\n", DRIVER_NAME);
}

/*
 * ========================================================
 * Deferrable Timer
 * ========================================================
 */
#if DEFER_TIMER
static void defer_timer_callback(struct timer_list *t)
{
	pr_info("[err] [CG PPT] throttling driver: 1 second has passed.\n");

	// Restart the timer
	mod_timer(&defer_timer, jiffies + msecs_to_jiffies(1000));
}

static void defer_timer_init(void)
{
	// Initialize the deferrable timer
	timer_setup(&defer_timer, defer_timer_callback, TIMER_DEFERRABLE);

	// Set the timer interval to 1 second
	mod_timer(&defer_timer, jiffies + msecs_to_jiffies(1000));
}

static void defer_timer_deinit(void)
{
	// Remove the deferrable timer
	del_timer(&defer_timer);
}
#endif

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
	if (hr_timer_enabled) {
		/*ftrace event*/
		trace_cg_ppt_status_info(get_cg_ppt_status_info());
		trace_cg_ppt_freq_info(get_cg_ppt_freq_info());
		trace_cg_ppt_power_info(get_cg_ppt_power_info());

		hrtimer_forward_now(&hr_timer, hr_timer_interval_ns);
		return HRTIMER_RESTART;
	}

	return HRTIMER_NORESTART;
}

static void hr_timer_init(void)
{
	// Initialize and set up hr_timer
	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = hr_timer_callback;
	hr_timer_interval_ns = ktime_set(0, 1000 * 1000); // 1 ms
}

static void hr_timer_deinit(void)
{
	if (hr_timer_enabled)
		hrtimer_cancel(&hr_timer);
}

/*
 * ========================================================
 * Driver Initial
 * ========================================================
 */
// Module init function
static int __init cg_peak_power_throttling_init(void)
{
	int result;

	pr_info("[CG PPT] throttling driver: Init\n");

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

#if DEFER_TIMER
	// defer timer initial
	defer_timer_init();
#endif

	// HR timer initial
	hr_timer_init();

	return 0;
}

// Module exit function
static void __exit cg_peak_power_throttling_exit(void)
{
	pr_info("[CG PPT] throttling driver: Exit\n");

	//delete sysfs device
	sysfs_device_deinit();

	// data deinit
	data_deinit();

#if DEFER_TIMER
	// deinit deffer timer
	defer_timer_deinit();
#endif

	// deinit hr timer
	hr_timer_deinit();
}

module_init(cg_peak_power_throttling_init);
module_exit(cg_peak_power_throttling_exit);

MODULE_AUTHOR("Clouds Lee");
MODULE_DESCRIPTION("MTK cg peak power throttling driver");
MODULE_LICENSE("GPL");
