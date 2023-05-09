// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

/**
 * @file    mtk_gpufreq_core.c
 * @brief   GPU-DVFS Driver Platform Implementation
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/timekeeping.h>

#include <gpufreq_v2.h>
#include <gpufreq_mssv.h>
#include <gpuppm.h>
#include <gpufreq_common.h>
#include <gpufreq_mt6897.h>
#include <gpufreq_hrid_mt6897.h>
#include <gpufreq_reg_mt6897.h>
#include <mtk_gpu_utility.h>
#include <gpufreq_history_common.h>
#include <gpufreq_history_mt6897.h>

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
#include <mtk_battery_oc_throttling.h>
#endif
#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING)
#include <mtk_bp_thl.h>
#endif
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
#include <mtk_low_battery_throttling.h>
#endif
#if IS_ENABLED(CONFIG_MTK_STATIC_POWER)
#include <leakage_table_v2/mtk_static_power.h>
#endif
#if IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING)
#include <clk-mtk.h>
#endif

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
/* misc function */
static unsigned int __gpufreq_custom_init_enable(void);
static unsigned int __gpufreq_dvfs_enable(void);
static void __gpufreq_set_dvfs_state(unsigned int set, unsigned int state);
static void __gpufreq_fake_mtcmos_control(unsigned int mode);
static void __gpufreq_set_margin_mode(unsigned int mode);
static void __gpufreq_set_gpm_mode(unsigned int version, unsigned int mode);
static void __gpufreq_set_ips_mode(unsigned int mode);
#if GPUFREQ_IPS_ENABLE
static void __gpufreq_ips_rpc_control(enum gpufreq_power_state power);
#endif /* GPUFREQ_IPS_ENABLE */
static void __gpufreq_apply_restore_margin(unsigned int mode);
static void __gpufreq_measure_power(void);
static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx);
static int __gpufreq_pause_dvfs(void);
static void __gpufreq_resume_dvfs(void);
static void __gpufreq_update_shared_status_opp_table(void);
static void __gpufreq_update_shared_status_adj_table(void);
static void __gpufreq_update_shared_status_init_reg(void);
static void __gpufreq_update_shared_status_power_reg(void);
static void __gpufreq_update_shared_status_active_sleep_reg(void);
static void __gpufreq_update_shared_status_dvfs_reg(void);
#if GPUFREQ_MSSV_TEST_MODE
static void __gpufreq_mssv_set_del_sel(unsigned int val);
#endif /* GPUFREQ_MSSV_TEST_MODE */
/* dvfs function */
static int __gpufreq_generic_scale_gpu(
	unsigned int freq_old, unsigned int freq_new,
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new);
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key);
static int __gpufreq_custom_commit_stack(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key);
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_freq_scale_stack(unsigned int freq_old, unsigned int freq_new);
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new);
static int __gpufreq_switch_clksrc(enum gpufreq_target target, enum gpufreq_clk_src clksrc);
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv_power);
static unsigned int __gpufreq_settle_time_vgpu(enum gpufreq_opp_direct direct, int deltaV);
static unsigned int __gpufreq_settle_time_vsram(enum gpufreq_opp_direct direct, int deltaV);
/* get function */
static unsigned int __gpufreq_get_fmeter_freq(enum gpufreq_target target);
static unsigned int __gpufreq_get_fmeter_main_fgpu(void);
static unsigned int __gpufreq_get_fmeter_main_fstack(void);
static unsigned int __gpufreq_get_fmeter_sub_fgpu(void);
static unsigned int __gpufreq_get_fmeter_sub_fstack(void);
static unsigned int __gpufreq_get_real_fgpu(void);
static unsigned int __gpufreq_get_real_fstack(void);
static unsigned int __gpufreq_get_real_vgpu(void);
static unsigned int __gpufreq_get_real_vsram(void);
static unsigned int __gpufreq_get_vsram_by_vlogic(unsigned int volt);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void);
static enum gpufreq_posdiv __gpufreq_get_real_posdiv_stack(void);
static enum gpufreq_posdiv __gpufreq_get_posdiv_by_freq(unsigned int freq);
/* power control function */
#if !GPUFREQ_PDCA_ENABLE
static void __gpufreq_mfgx_rpc_control(enum gpufreq_power_state power, void __iomem *pwr_con);
#endif /* GPUFREQ_PDCA_ENABLE */
static void __gpufreq_mfg1_rpc_control(enum gpufreq_power_state power);
static int __gpufreq_clock_control(enum gpufreq_power_state power);
static int __gpufreq_mtcmos_control(enum gpufreq_power_state power);
static int __gpufreq_buck_control(enum gpufreq_power_state power);
static void __gpufreq_check_bus_idle(void);
static void __gpufreq_iso_latch_config(enum gpufreq_power_state power);
static void __gpufreq_top_hwdcm_config(enum gpufreq_power_state power);
static void __gpufreq_stack_hwdcm_config(enum gpufreq_power_state power);
static void __gpufreq_acp_config(void);
static void __gpufreq_ocl_timestamp_config(void);
static void __gpufreq_gpm1_config(void);
static void __gpufreq_transaction_config(void);
static void __gpufreq_axuser_priority_config(void);
static void __gpufreq_axuser_slc_config(void);
static void __gpufreq_dfd_config(void);
static void __gpufreq_power_tracker_config(void);
static void __gpufreq_mfg_backup_restore(enum gpufreq_power_state power);
/* bringup function */
static unsigned int __gpufreq_bringup(void);
static void __gpufreq_dump_bringup_status(struct platform_device *pdev);
/* init function */
static void __gpufreq_interpolate_volt(void);
static void __gpufreq_apply_adjustment(void);
static unsigned int __gpufreq_compute_avs_freq(u32 val);
static unsigned int __gpufreq_compute_avs_volt(u32 val);
static void __gpufreq_compute_avs(void);
static void __gpufreq_compute_aging(void);
static int __gpufreq_init_opp_idx(void);
static void __gpufreq_init_opp_table(void);
static void __gpufreq_init_shader_present(void);
static void __gpufreq_init_segment_id(void);
static int __gpufreq_init_mtcmos(void);
static int __gpufreq_init_clk(struct platform_device *pdev);
static int __gpufreq_init_pmic(struct platform_device *pdev);
static int __gpufreq_init_platform_info(struct platform_device *pdev);
static int __gpufreq_pdrv_probe(struct platform_device *pdev);
static int __gpufreq_pdrv_remove(struct platform_device *pdev);

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static const struct of_device_id g_gpufreq_of_match[] = {
	{ .compatible = "mediatek,gpufreq" },
	{ /* sentinel */ }
};
static struct platform_driver g_gpufreq_pdrv = {
	.probe = __gpufreq_pdrv_probe,
	.remove = __gpufreq_pdrv_remove,
	.driver = {
		.name = "gpufreq",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_of_match,
	},
};

static void __iomem *g_mfg_top_base;
static void __iomem *g_mfg_pll_base;
static void __iomem *g_mfgsc_pll_base;
static void __iomem *g_mfg_rpc_base;
static void __iomem *g_sleep;
static void __iomem *g_topckgen_base;
static void __iomem *g_nth_emicfg_base;
static void __iomem *g_sth_emicfg_base;
static void __iomem *g_nth_emicfg_ao_mem_base;
static void __iomem *g_sth_emicfg_ao_mem_base;
static void __iomem *g_ifrbus_ao_base;
static void __iomem *g_infra_ao_debug_ctrl;
static void __iomem *g_infra_ao1_debug_ctrl;
static void __iomem *g_nth_emi_ao_debug_ctrl;
static void __iomem *g_sth_emi_ao_debug_ctrl;
static void __iomem *g_efuse_base;
static void __iomem *g_mfg_secure_base;
static void __iomem *g_drm_debug_base;
static void __iomem *g_mfg_ips_base;
static void __iomem *g_mali_base;
static void __iomem *g_emi_base;
static void __iomem *g_sub_emi_base;
static void __iomem *g_nemi_mi32_smi_sub;
static void __iomem *g_nemi_mi33_smi_sub;
static void __iomem *g_semi_mi32_smi_sub;
static void __iomem *g_semi_mi33_smi_sub;
static struct gpufreq_pmic_info *g_pmic;
static struct gpufreq_clk_info *g_clk;
static struct gpufreq_status g_gpu;
static struct gpufreq_status g_stack;
static struct gpufreq_asensor_info g_asensor_info;
static unsigned int g_shader_present;
static unsigned int g_mcl50_load;
static unsigned int g_aging_table_idx;
static unsigned int g_asensor_enable;
static unsigned int g_aging_load;
static unsigned int g_aging_margin;
static unsigned int g_avs_enable;
static unsigned int g_avs_margin;
static unsigned int g_gpm1_mode;
static unsigned int g_gpm3_mode;
static unsigned int g_ptp_version;
static unsigned int g_dfd_mode;
static unsigned int g_ips_mode;
static unsigned int g_power_tracker_mode;
static unsigned int g_gpueb_support;
static unsigned int g_gpufreq_ready;
static unsigned int g_del_sel_reg;
static enum gpufreq_dvfs_state g_dvfs_state;
static struct gpufreq_shared_status *g_shared_status;
static DEFINE_MUTEX(gpufreq_lock);
static DEFINE_MUTEX(spm_sema_lock);

static struct gpufreq_platform_fp platform_ap_fp = {
	.power_ctrl_enable = __gpufreq_power_ctrl_enable,
	.active_sleep_ctrl_enable = __gpufreq_active_sleep_ctrl_enable,
	.get_power_state = __gpufreq_get_power_state,
	.get_dvfs_state = __gpufreq_get_dvfs_state,
	.get_shader_present = __gpufreq_get_shader_present,
	.get_cur_fgpu = __gpufreq_get_cur_fgpu,
	.get_cur_vgpu = __gpufreq_get_cur_vgpu,
	.get_cur_vsram_gpu = __gpufreq_get_cur_vsram_gpu,
	.get_cur_pgpu = __gpufreq_get_cur_pgpu,
	.get_max_pgpu = __gpufreq_get_max_pgpu,
	.get_min_pgpu = __gpufreq_get_min_pgpu,
	.get_cur_idx_gpu = __gpufreq_get_cur_idx_gpu,
	.get_opp_num_gpu = __gpufreq_get_opp_num_gpu,
	.get_signed_opp_num_gpu = __gpufreq_get_signed_opp_num_gpu,
	.get_fgpu_by_idx = __gpufreq_get_fgpu_by_idx,
	.get_pgpu_by_idx = __gpufreq_get_pgpu_by_idx,
	.get_idx_by_fgpu = __gpufreq_get_idx_by_fgpu,
	.get_lkg_pgpu = __gpufreq_get_lkg_pgpu,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.power_control = __gpufreq_power_control,
	.active_sleep_control = __gpufreq_active_sleep_control,
	.fix_target_oppidx_gpu = __gpufreq_fix_target_oppidx_gpu,
	.fix_custom_freq_volt_gpu = __gpufreq_fix_custom_freq_volt_gpu,
	.dump_infra_status = __gpufreq_dump_infra_status,
	.dump_power_tracker_status = __gpufreq_dump_power_tracker_status,
	.set_mfgsys_config = __gpufreq_set_mfgsys_config,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
	.pdca_config = __gpufreq_pdca_config,
	.update_debug_opp_info = __gpufreq_update_debug_opp_info,
	.set_shared_status = __gpufreq_set_shared_status,
	.mssv_commit = __gpufreq_mssv_commit,
	.devapc_vio_handler = __gpufreq_devapc_vio_handler,
};

static struct gpufreq_platform_fp platform_eb_fp = {
	.dump_infra_status = __gpufreq_dump_infra_status,
	.dump_power_tracker_status = __gpufreq_dump_power_tracker_status,
	.get_dyn_pgpu = __gpufreq_get_dyn_pgpu,
	.get_core_mask_table = __gpufreq_get_core_mask_table,
	.get_core_num = __gpufreq_get_core_num,
	.devapc_vio_handler = __gpufreq_devapc_vio_handler,
};

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */
/* API: get POWER_CTRL enable status */
unsigned int __gpufreq_power_ctrl_enable(void)
{
	return GPUFREQ_POWER_CTRL_ENABLE;
}

/* API: get ACTIVE_SLEEP_CTRL status */
unsigned int __gpufreq_active_sleep_ctrl_enable(void)
{
	return GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE && GPUFREQ_POWER_CTRL_ENABLE;
}

/* API: get power state (on/off) */
unsigned int __gpufreq_get_power_state(void)
{
	if (g_gpu.power_count > 0)
		return GPU_PWR_ON;
	else
		return GPU_PWR_OFF;
}

/* API: get DVFS state (free/disable/keep) */
unsigned int __gpufreq_get_dvfs_state(void)
{
	return g_dvfs_state;
}

/* API: get GPU shader core setting */
unsigned int __gpufreq_get_shader_present(void)
{
	return g_shader_present;
}

/* API: get current Freq of GPU */
unsigned int __gpufreq_get_cur_fgpu(void)
{
	return g_gpu.cur_freq;
}

/* API: get current Freq of STACK */
unsigned int __gpufreq_get_cur_fstack(void)
{
	return g_stack.cur_freq;
}

/* API: get current Volt of GPU */
unsigned int __gpufreq_get_cur_vgpu(void)
{
	return g_gpu.cur_volt;
}

/* API: get current Volt of STACK */
unsigned int __gpufreq_get_cur_vstack(void)
{
	return 0;
}

/* API: get current Vsram of GPU */
unsigned int __gpufreq_get_cur_vsram_gpu(void)
{
	return g_gpu.cur_vsram;
}

/* API: get current Vsram of STACK */
unsigned int __gpufreq_get_cur_vsram_stack(void)
{
	return 0;
}

/* API: get current Power of GPU */
unsigned int __gpufreq_get_cur_pgpu(void)
{
	return g_gpu.working_table[g_gpu.cur_oppidx].power;
}

/* API: get current Power of STACK */
unsigned int __gpufreq_get_cur_pstack(void)
{
	return 0;
}

/* API: get max Power of GPU */
unsigned int __gpufreq_get_max_pgpu(void)
{
	return g_gpu.working_table[g_gpu.max_oppidx].power;
}

/* API: get max Power of STACK */
unsigned int __gpufreq_get_max_pstack(void)
{
	return 0;
}

/* API: get min Power of GPU */
unsigned int __gpufreq_get_min_pgpu(void)
{
	return g_gpu.working_table[g_gpu.min_oppidx].power;
}

/* API: get min Power of STACK */
unsigned int __gpufreq_get_min_pstack(void)
{
	return 0;
}

/* API: get current working OPP index of GPU */
int __gpufreq_get_cur_idx_gpu(void)
{
	return g_gpu.cur_oppidx;
}

/* API: get current working OPP index of STACK */
int __gpufreq_get_cur_idx_stack(void)
{
	return -1;
}

/* API: get number of working OPP of GPU */
int __gpufreq_get_opp_num_gpu(void)
{
	return g_gpu.opp_num;
}

/* API: get number of working OPP of STACK */
int __gpufreq_get_opp_num_stack(void)
{
	return -1;
}

/* API: get number of signed OPP of GPU */
int __gpufreq_get_signed_opp_num_gpu(void)
{
	return g_gpu.signed_opp_num;
}

/* API: get number of signed OPP of STACK */
int __gpufreq_get_signed_opp_num_stack(void)
{
	return 0;
}

/* API: get pointer of working OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_working_table_gpu(void)
{
	return g_gpu.working_table;
}

/* API: get pointer of working OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_working_table_stack(void)
{
	return NULL;
}

/* API: get pointer of signed OPP table of GPU */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_gpu(void)
{
	return g_gpu.signed_table;
}

/* API: get pointer of signed OPP table of STACK */
const struct gpufreq_opp_info *__gpufreq_get_signed_table_stack(void)
{
	return NULL;
}

/* API: get Freq of GPU via OPP index */
unsigned int __gpufreq_get_fgpu_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_gpu.opp_num)
		return g_gpu.working_table[oppidx].freq;
	else
		return 0;
}

/* API: get Freq of STACK via OPP index */
unsigned int __gpufreq_get_fstack_by_idx(int oppidx)
{
	GPUFREQ_UNREFERENCED(oppidx);

	return 0;
}

/* API: get Power of GPU via OPP index */
unsigned int __gpufreq_get_pgpu_by_idx(int oppidx)
{
	if (oppidx >= 0 && oppidx < g_gpu.opp_num)
		return g_gpu.working_table[oppidx].power;
	else
		return 0;
}

/* API: get Power of STACK via OPP index */
unsigned int __gpufreq_get_pstack_by_idx(int oppidx)
{
	GPUFREQ_UNREFERENCED(oppidx);

	return 0;
}

/* API: get working OPP index of GPU via Freq */
int __gpufreq_get_idx_by_fgpu(unsigned int freq)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given freq */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].freq >= freq)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;

	return oppidx;
}

/* API: get working OPP index of STACK via Freq */
int __gpufreq_get_idx_by_fstack(unsigned int freq)
{
	GPUFREQ_UNREFERENCED(freq);

	return -1;
}

/* API: get working OPP index of GPU via Volt */
int __gpufreq_get_idx_by_vgpu(unsigned int volt)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given volt */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].volt >= volt)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;

	return oppidx;
}

/* API: get working OPP index of STACK via Volt */
int __gpufreq_get_idx_by_vstack(unsigned int volt)
{
	GPUFREQ_UNREFERENCED(volt);

	return -1;
}

/* API: get working OPP index of GPU via Power */
int __gpufreq_get_idx_by_pgpu(unsigned int power)
{
	int oppidx = -1;
	int i = 0;

	/* find the smallest index that satisfy given power */
	for (i = g_gpu.min_oppidx; i >= g_gpu.max_oppidx; i--) {
		if (g_gpu.working_table[i].power >= power)
			break;
	}
	/* use max_oppidx if not found */
	oppidx = (i > g_gpu.max_oppidx) ? i : g_gpu.max_oppidx;

	return oppidx;
}

/* API: get working OPP index of STACK via Power */
int __gpufreq_get_idx_by_pstack(unsigned int power)
{
	GPUFREQ_UNREFERENCED(power);

	return -1;
}

/* API: get leakage Power of GPU */
unsigned int __gpufreq_get_lkg_pgpu(unsigned int volt, int temper)
{
	GPUFREQ_UNREFERENCED(volt);
	GPUFREQ_UNREFERENCED(temper);

	return GPU_LKG_POWER;
}

/* API: get dynamic Power of GPU */
unsigned int __gpufreq_get_dyn_pgpu(unsigned int freq, unsigned int volt)
{
	unsigned long long p_dynamic = GPU_DYN_REF_POWER;
	unsigned int ref_freq = GPU_DYN_REF_POWER_FREQ;
	unsigned int ref_volt = GPU_DYN_REF_POWER_VOLT;

	p_dynamic = p_dynamic *
		((freq * 100) / ref_freq) *
		((volt * 100) / ref_volt) *
		((volt * 100) / ref_volt) /
		(100 * 100 * 100);

	return (unsigned int)p_dynamic;
}

/* API: get leakage Power of STACK */
unsigned int __gpufreq_get_lkg_pstack(unsigned int volt, int temper)
{
	GPUFREQ_UNREFERENCED(volt);
	GPUFREQ_UNREFERENCED(temper);

	return 0;
}

/* API: get dynamic Power of STACK */
unsigned int __gpufreq_get_dyn_pstack(unsigned int freq, unsigned int volt)
{
	GPUFREQ_UNREFERENCED(freq);
	GPUFREQ_UNREFERENCED(volt);

	return 0;
}

/*
 * API: control power state of whole MFG system
 * return power_count if success
 * return GPUFREQ_EINVAL if failure
 */
int __gpufreq_power_control(enum gpufreq_power_state power)
{
	int ret = 0;
	u64 power_time = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	mutex_lock(&gpufreq_lock);

	GPUFREQ_LOGD("+ PWR_STATUS: 0x%08lx", MFG_0_14_PWR_STATUS);
	GPUFREQ_LOGD("switch power: %s (Power: %d, Active: %d, Buck: %d, MTCMOS: %d, CG: %d)",
		power ? "On" : "Off", g_gpu.power_count,
		g_gpu.active_count, g_gpu.buck_count,
		g_gpu.mtcmos_count, g_gpu.cg_count);

	if (power == GPU_PWR_ON)
		g_gpu.power_count++;
	else
		g_gpu.power_count--;
	__gpufreq_footprint_power_count(g_gpu.power_count);

	if (power == GPU_PWR_ON && g_gpu.power_count == 1) {
		__gpufreq_footprint_power_step(0x01);

		/* enable Buck */
		ret = __gpufreq_buck_control(GPU_PWR_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to enable Buck (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x02);

		/* clear AOC ISO/LATCH after Buck on */
		__gpufreq_iso_latch_config(GPU_PWR_ON);
		__gpufreq_footprint_power_step(0x03);

		/* enable MTCMOS */
		ret = __gpufreq_mtcmos_control(GPU_PWR_ON);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to enable MTCMOS (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x04);

		/* enable Clock */
		ret = __gpufreq_clock_control(GPU_PWR_ON);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to enable Clock (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x05);

		/* restore MFG registers */
		__gpufreq_mfg_backup_restore(GPU_PWR_ON);
		__gpufreq_footprint_power_step(0x06);

		/* set PDCA register when power on and let GPU DDK control MTCMOS */
		__gpufreq_pdca_config(GPU_PWR_ON);
		__gpufreq_footprint_power_step(0x07);

		/* config TOP HWDCM */
		__gpufreq_top_hwdcm_config(GPU_PWR_ON);
		__gpufreq_footprint_power_step(0x08);

		/* config STACK HWDCM */
		__gpufreq_stack_hwdcm_config(GPU_PWR_ON);
		__gpufreq_footprint_power_step(0x09);

		/* config ACP */
		__gpufreq_acp_config();
		__gpufreq_footprint_power_step(0x0A);

		/* config ocl timestamp */
		__gpufreq_ocl_timestamp_config();
		__gpufreq_footprint_power_step(0x0B);

		/* config AXI transaction */
		__gpufreq_transaction_config();
		__gpufreq_footprint_power_step(0x0C);

		/* config AXUSER priority */
		__gpufreq_axuser_priority_config();
		__gpufreq_footprint_power_step(0x0D);

		/* config AXUSER SLC setting */
		__gpufreq_axuser_slc_config();
		__gpufreq_footprint_power_step(0x0E);

		/* config GPM 1.0 */
		__gpufreq_gpm1_config();
		__gpufreq_footprint_power_step(0x0F);

		/* config DFD */
		__gpufreq_dfd_config();
		__gpufreq_footprint_power_step(0x10);

		/* config Power Tracker */
		__gpufreq_power_tracker_config();
		__gpufreq_footprint_power_step(0x11);

		/* free DVFS when power on */
		g_dvfs_state &= ~DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x12);
	} else if (power == GPU_PWR_OFF && g_gpu.power_count == 0) {
		__gpufreq_footprint_power_step(0x13);

		/* check all transaction complete before power off */
		__gpufreq_check_bus_idle();
		__gpufreq_footprint_power_step(0x14);

		/* freeze DVFS when power off */
		g_dvfs_state |= DVFS_POWEROFF;
		__gpufreq_footprint_power_step(0x15);

		/* backup MFG registers */
		__gpufreq_mfg_backup_restore(GPU_PWR_OFF);
		__gpufreq_footprint_power_step(0x16);

		/* config STACK HWDCM */
		__gpufreq_stack_hwdcm_config(GPU_PWR_OFF);
		__gpufreq_footprint_power_step(0x17);

		/* disable Clock */
		ret = __gpufreq_clock_control(GPU_PWR_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to disable Clock (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x18);

		/* disable MTCMOS */
		ret = __gpufreq_mtcmos_control(GPU_PWR_OFF);
		if (unlikely(ret < 0)) {
			GPUFREQ_LOGE("fail to disable MTCMOS (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x19);

		/* set AOC ISO/LATCH before Buck off */
		__gpufreq_iso_latch_config(GPU_PWR_OFF);
		__gpufreq_footprint_power_step(0x1A);

		/* disable Buck */
		ret = __gpufreq_buck_control(GPU_PWR_OFF);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to disable Buck (%d)", ret);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		__gpufreq_footprint_power_step(0x1B);
	}

	/* return power count if successfully control power */
	ret = g_gpu.power_count;
	/* record time of successful power control */
	power_time = ktime_get_ns();

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->power_count = g_gpu.power_count;
		g_shared_status->active_count = g_gpu.active_count;
		g_shared_status->buck_count = g_gpu.buck_count;
		g_shared_status->mtcmos_count = g_gpu.mtcmos_count;
		g_shared_status->cg_count = g_gpu.cg_count;
		g_shared_status->power_time_h = (power_time >> 32) & GENMASK(31, 0);
		g_shared_status->power_time_l = power_time & GENMASK(31, 0);
		if (power == GPU_PWR_ON)
			__gpufreq_update_shared_status_power_reg();
	}

	if (power == GPU_PWR_ON)
		__gpufreq_footprint_power_step(0x1C);
	else if (power == GPU_PWR_OFF)
		__gpufreq_footprint_power_step(0x1D);

done_unlock:
	GPUFREQ_LOGD("- PWR_STATUS: 0x%08lx", MFG_0_14_PWR_STATUS);

	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: control runtime active-idle state of GPU
 * return active_count if success
 * return GPUFREQ_EINVAL if failure
 */
int __gpufreq_active_sleep_control(enum gpufreq_power_state power)
{
	int ret = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	mutex_lock(&gpufreq_lock);

	GPUFREQ_LOGD("switch runtime state: %s (Active: %d)",
		power ? "Active" : "Idle", g_gpu.active_count);

	/* active-idle control is only available at power-on */
	if (__gpufreq_get_power_state() == GPU_PWR_OFF)
		__gpufreq_abort("switch active-idle at power-off (%d)", g_gpu.power_count);

	if (power == GPU_PWR_ON)
		g_gpu.active_count++;
	else
		g_gpu.active_count--;

	if (power == GPU_PWR_ON && g_gpu.active_count == 1) {
		/* switch GPU MUX to PLL */
		ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_MAIN);
		if (ret)
			goto done;
		/* switch STACK MUX to PLL */
		ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_MAIN);
		if (ret)
			goto done;
		/* free DVFS when active */
		g_dvfs_state &= ~DVFS_SLEEP;
	} else if (power == GPU_PWR_OFF && g_gpu.active_count == 0) {
		/* freeze DVFS when idle */
		g_dvfs_state |= DVFS_SLEEP;
		/* switch STACK MUX to REF_SEL */
		ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_SUB);
		if (ret)
			goto done;
		/* switch GPU MUX to REF_SEL */
		ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_SUB);
		if (ret)
			goto done;
	} else if (g_gpu.active_count < 0)
		__gpufreq_abort("incorrect active count: %d", g_gpu.active_count);

	/* return active count if successfully control runtime state */
	ret = g_gpu.active_count;

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->active_count = g_gpu.active_count;
		__gpufreq_update_shared_status_active_sleep_reg();
	}

done:
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

int __gpufreq_generic_commit_gpu(int target_oppidx, enum gpufreq_dvfs_state key)
{
	int ret = GPUFREQ_SUCCESS;
	int cur_oppidx = 0, opp_num = g_gpu.opp_num;
	/* GPU */
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	unsigned int cur_fgpu = 0, cur_vgpu = 0, target_fgpu = 0, target_vgpu = 0;
	/* SRAM */
	unsigned int cur_vsram = 0, target_vsram = 0;

	GPUFREQ_TRACE_START("target_oppidx=%d, key=%d", target_oppidx, key);

	/* validate 0 <= target_oppidx < opp_num */
	if (target_oppidx < 0 || target_oppidx >= opp_num) {
		GPUFREQ_LOGE("invalid target idx: %d (num: %d)", target_oppidx, opp_num);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	mutex_lock(&gpufreq_lock);

	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGD("unavailable DVFS state (0x%x)", g_dvfs_state);
		/* still update Volt when DVFS is fixed by fix OPP cmd */
		if (g_dvfs_state == DVFS_FIX_OPP)
			target_oppidx = g_gpu.cur_oppidx;
		/* otherwise skip */
		else {
			ret = GPUFREQ_SUCCESS;
			goto done_unlock;
		}
	}

	/* prepare OPP setting */
	cur_oppidx = g_gpu.cur_oppidx;
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram = g_gpu.cur_vsram;

	target_fgpu = working_gpu[target_oppidx].freq;
	target_vgpu = working_gpu[target_oppidx].volt;
	target_vsram = working_gpu[target_oppidx].vsram;

	GPUFREQ_LOGD("begin to commit OPP idx: (%d->%d)", cur_oppidx, target_oppidx);

	ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
		cur_vgpu, target_vgpu, cur_vsram, target_vsram);
	if (unlikely(ret)) {
		GPUFREQ_LOGE(
			"fail to scale GPU: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
			cur_oppidx, target_oppidx, cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram, target_vsram);
		goto done_unlock;
	}

	g_gpu.cur_oppidx = target_oppidx;

	__gpufreq_footprint_oppidx(target_oppidx);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		__gpufreq_update_shared_status_dvfs_reg();
	}

#ifdef GPUFREQ_HISTORY_ENABLE
	/* update history record to shared memory */
	if (target_oppidx != cur_oppidx)
		__gpufreq_record_history_entry(HISTORY_FREE);
#endif /* GPUFREQ_HISTORY_ENABLE */

done_unlock:
	mutex_unlock(&gpufreq_lock);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: commit DVFS to STACK by given OPP index
 * this is the main entrance of generic DVFS
 */
int __gpufreq_generic_commit_stack(int target_oppidx, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

int __gpufreq_generic_commit_dual(int target_oppidx_gpu, int target_oppidx_stack,
	enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_oppidx_gpu);
	GPUFREQ_UNREFERENCED(target_oppidx_stack);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

/* API: fix OPP of GPU via given OPP index */
int __gpufreq_fix_target_oppidx_gpu(int oppidx)
{
	int opp_num = g_gpu.opp_num;
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (unlikely(ret < 0))
		goto done;

	if (oppidx == -1) {
		__gpufreq_set_dvfs_state(false, DVFS_FIX_OPP);
		ret = GPUFREQ_SUCCESS;
	} else if (oppidx >= 0 && oppidx < opp_num) {
		__gpufreq_set_dvfs_state(true, DVFS_FIX_OPP);

#ifdef GPUFREQ_HISTORY_ENABLE
		gpufreq_set_history_target_opp(TARGET_GPU, oppidx);
#endif /* GPUFREQ_HISTORY_ENABLE */

		ret = __gpufreq_generic_commit_gpu(oppidx, DVFS_FIX_OPP);
		if (unlikely(ret))
			__gpufreq_set_dvfs_state(false, DVFS_FIX_OPP);
	} else
		ret = GPUFREQ_EINVAL;

	__gpufreq_power_control(GPU_PWR_OFF);

done:
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to commit idx: %d", oppidx);

	return ret;
}

/* API: fix OPP of STACK via given OPP index */
int __gpufreq_fix_target_oppidx_stack(int oppidx)
{
	GPUFREQ_UNREFERENCED(oppidx);

	return GPUFREQ_EINVAL;
}

/* API: fix Freq and Volt of GPU via given Freq and Volt */
int __gpufreq_fix_custom_freq_volt_gpu(unsigned int freq, unsigned int volt)
{
	unsigned int max_freq = 0, min_freq = 0;
	unsigned int max_volt = 0, min_volt = 0;
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (unlikely(ret < 0))
		goto done;

	max_freq = POSDIV_2_MAX_FREQ;
	max_volt = VGPU_MAX_VOLT;
	min_freq = POSDIV_16_MIN_FREQ;
	min_volt = VGPU_MIN_VOLT;

	if (freq == 0 && volt == 0) {
		__gpufreq_set_dvfs_state(false, DVFS_FIX_FREQ_VOLT);
		ret = GPUFREQ_SUCCESS;
	} else if (freq > max_freq || freq < min_freq) {
		ret = GPUFREQ_EINVAL;
	} else if (volt > max_volt || volt < min_volt) {
		ret = GPUFREQ_EINVAL;
	} else {
		__gpufreq_set_dvfs_state(true, DVFS_FIX_FREQ_VOLT);

		ret = __gpufreq_custom_commit_gpu(freq, volt, DVFS_FIX_FREQ_VOLT);
		if (unlikely(ret))
			__gpufreq_set_dvfs_state(false, DVFS_FIX_FREQ_VOLT);
	}

	__gpufreq_power_control(GPU_PWR_OFF);

done:
	if (unlikely(ret))
		GPUFREQ_LOGE("fail to commit Freq: %d, Volt: %d", freq, volt);

	return ret;
}

/* API: fix Freq and Volt of STACK via given Freq and Volt */
int __gpufreq_fix_custom_freq_volt_stack(unsigned int freq, unsigned int volt)
{
	GPUFREQ_UNREFERENCED(freq);
	GPUFREQ_UNREFERENCED(volt);

	return GPUFREQ_EINVAL;
}

void __gpufreq_dump_infra_status(void)
{
	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGI("== [GPUFREQ INFRA STATUS] ==");
	GPUFREQ_LOGI("[Clk] MFG_PLL: %d, MFG_SEL: 0x%lx, MFGSC_PLL: %d, MFGSC_SEL: 0x%lx",
		__gpufreq_get_real_fgpu(), DRV_Reg32(TOPCK_CLK_CFG_30) & MFG_SEL_MFGPLL_MASK,
		__gpufreq_get_real_fstack(), DRV_Reg32(TOPCK_CLK_CFG_30) & MFGSC_SEL_MFGPSCLL_MASK);

	/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
	DRV_WriteReg32(MFG_QCHANNEL_CON, (DRV_Reg32(MFG_QCHANNEL_CON) | BIT(0)));
	/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
	DRV_WriteReg32(MFG_DEBUG_SEL, (DRV_Reg32(MFG_DEBUG_SEL) | GENMASK(1, 0)));

	/* MFG_DEBUG_SEL */
	/* MFG_DEBUG_TOP */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[MFG]",
		0x13FBF170, DRV_Reg32(MFG_DEBUG_SEL),
		0x13FBF178, DRV_Reg32(MFG_DEBUG_TOP));

	/* MFG_RPC_SLP_PROT_EN_SET */
	/* MFG_RPC_SLP_PROT_EN_CLR */
	/* MFG_RPC_SLP_PROT_EN_STA */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[MFG]",
		0x13F91040, DRV_Reg32(MFG_RPC_SLP_PROT_EN_SET),
		0x13F91044, DRV_Reg32(MFG_RPC_SLP_PROT_EN_CLR),
		0x13F91048, DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA));

	/* MFG_RPC_AO_CLK_CFG */
	/* MFG_RPC_IPS_SES_PWR_CON */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[MFG]",
		0x13F91034, DRV_Reg32(MFG_RPC_AO_CLK_CFG),
		0x13F910FC, DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON));

	/* NTH_MFG_EMI1_GALS_SLV_DBG */
	/* NTH_MFG_EMI0_GALS_SLV_DBG */
	/* STH_MFG_EMI1_GALS_SLV_DBG */
	/* STH_MFG_EMI0_GALS_SLV_DBG */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x1021C82C, DRV_Reg32(NTH_MFG_EMI1_GALS_SLV_DBG),
		0x1021C830, DRV_Reg32(NTH_MFG_EMI0_GALS_SLV_DBG),
		0x1021E82C, DRV_Reg32(STH_MFG_EMI1_GALS_SLV_DBG),
		0x1021E830, DRV_Reg32(STH_MFG_EMI0_GALS_SLV_DBG));

	/* NTH_APU_EMI1_GALS_SLV_DBG */
	/* NTH_APU_EMI0_GALS_SLV_DBG */
	/* STH_APU_EMI1_GALS_SLV_DBG */
	/* STH_APU_EMI0_GALS_SLV_DBG */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x1021C824, DRV_Reg32(NTH_APU_EMI1_GALS_SLV_DBG),
		0x1021C828, DRV_Reg32(NTH_APU_EMI0_GALS_SLV_DBG),
		0x1021E824, DRV_Reg32(STH_APU_EMI1_GALS_SLV_DBG),
		0x1021E828, DRV_Reg32(STH_APU_EMI0_GALS_SLV_DBG));

	/* NTH_M6M7_IDLE_BIT_EN_1 */
	/* NTH_M6M7_IDLE_BIT_EN_0 */
	/* STH_M6M7_IDLE_BIT_EN_1 */
	/* STH_M6M7_IDLE_BIT_EN_0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x10270228, DRV_Reg32(NTH_M6M7_IDLE_BIT_EN_1),
		0x1027022C, DRV_Reg32(NTH_M6M7_IDLE_BIT_EN_0),
		0x1030E228, DRV_Reg32(STH_M6M7_IDLE_BIT_EN_1),
		0x1030E22C, DRV_Reg32(STH_M6M7_IDLE_BIT_EN_0));

	/* NTH_SLEEP_PROT_MASK */
	/* NTH_GLITCH_PROT_RDY */
	/* STH_SLEEP_PROT_MASK */
	/* STH_GLITCH_PROT_RDY */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x10270000, DRV_Reg32(NTH_SLEEP_PROT_MASK),
		0x1027008C, DRV_Reg32(NTH_GLITCH_PROT_RDY),
		0x1030E000, DRV_Reg32(STH_SLEEP_PROT_MASK),
		0x1030E08C, DRV_Reg32(STH_GLITCH_PROT_RDY));

	/* EMI_MD_LAT_HRT_UGT_CNT */
	/* EMI_MD_HRT_UGT_CNT */
	/* EMI_DISP_HRT_UGT_CNT */
	/* EMI_CAM_HRT_UGT_CNT */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x10219860, DRV_Reg32(EMI_MD_LAT_HRT_UGT_CNT),
		0x10219864, DRV_Reg32(EMI_MD_HRT_UGT_CNT),
		0x10219868, DRV_Reg32(EMI_DISP_HRT_UGT_CNT),
		0x1021986C, DRV_Reg32(EMI_CAM_HRT_UGT_CNT));

	/* EMI_MD_WR_LAT_HRT_UGT_CNT */
	/* EMI_MDMCU_HIGH_LAT_UGT_CNT */
	/* EMI_MDMCU_HIGH_WR_LAT_UGT_CNT */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x102199A4, DRV_Reg32(EMI_MD_WR_LAT_HRT_UGT_CNT),
		0x10219CC4, DRV_Reg32(EMI_MDMCU_HIGH_LAT_UGT_CNT),
		0x10219CCC, DRV_Reg32(EMI_MDMCU_HIGH_WR_LAT_UGT_CNT));

	/* SEMI_MD_LAT_HRT_UGT_CNT */
	/* SEMI_MD_HRT_UGT_CNT */
	/* SEMI_DISP_HRT_UGT_CNT */
	/* SEMI_CAM_HRT_UGT_CNT */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x1021D860, DRV_Reg32(SEMI_MD_LAT_HRT_UGT_CNT),
		0x1021D864, DRV_Reg32(SEMI_MD_HRT_UGT_CNT),
		0x1021D868, DRV_Reg32(SEMI_DISP_HRT_UGT_CNT),
		0x1021D86C, DRV_Reg32(SEMI_CAM_HRT_UGT_CNT));

	/* SEMI_MD_WR_LAT_HRT_UGT_CNT */
	/* SEMI_MDMCU_HIGH_LAT_UGT_CNT */
	/* SEMI_MDMCU_HIGH_WR_LAT_UGT_CNT */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI]",
		0x1021D9A4, DRV_Reg32(SEMI_MD_WR_LAT_HRT_UGT_CNT),
		0x1021DCC4, DRV_Reg32(SEMI_MDMCU_HIGH_LAT_UGT_CNT),
		0x1021DCCC, DRV_Reg32(SEMI_MDMCU_HIGH_WR_LAT_UGT_CNT));

	/* NEMI_MI32_SMI_SUB_DEBUG_S0 */
	/* NEMI_MI32_SMI_SUB_DEBUG_S1 */
	/* NEMI_MI32_SMI_SUB_DEBUG_S2 */
	/* NEMI_MI32_SMI_SUB_DEBUG_M0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI_SMI]",
		0x1025E400, DRV_Reg32(NEMI_MI32_SMI_SUB_DEBUG_S0),
		0x1025E404, DRV_Reg32(NEMI_MI32_SMI_SUB_DEBUG_S1),
		0x1025E408, DRV_Reg32(NEMI_MI32_SMI_SUB_DEBUG_S2),
		0x1025E430, DRV_Reg32(NEMI_MI32_SMI_SUB_DEBUG_M0));

	/* NEMI_MI33_SMI_SUB_DEBUG_S0 */
	/* NEMI_MI33_SMI_SUB_DEBUG_S1 */
	/* NEMI_MI33_SMI_SUB_DEBUG_S2 */
	/* NEMI_MI33_SMI_SUB_DEBUG_M0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI_SMI]",
		0x1025F400, DRV_Reg32(NEMI_MI33_SMI_SUB_DEBUG_S0),
		0x1025F404, DRV_Reg32(NEMI_MI33_SMI_SUB_DEBUG_S1),
		0x1025F408, DRV_Reg32(NEMI_MI33_SMI_SUB_DEBUG_S2),
		0x1025F430, DRV_Reg32(NEMI_MI33_SMI_SUB_DEBUG_M0));

	/* SEMI_MI32_SMI_SUB_DEBUG_S0 */
	/* SEMI_MI32_SMI_SUB_DEBUG_S1 */
	/* SEMI_MI32_SMI_SUB_DEBUG_M0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI_SMI]",
		0x10309400, DRV_Reg32(SEMI_MI32_SMI_SUB_DEBUG_S0),
		0x10309404, DRV_Reg32(SEMI_MI32_SMI_SUB_DEBUG_S1),
		0x10309408, DRV_Reg32(SEMI_MI32_SMI_SUB_DEBUG_S2),
		0x10309430, DRV_Reg32(SEMI_MI32_SMI_SUB_DEBUG_M0));

	/* SEMI_MI33_SMI_SUB_DEBUG_S0 */
	/* SEMI_MI33_SMI_SUB_DEBUG_S1 */
	/* SEMI_MI33_SMI_SUB_DEBUG_S2 */
	/* SEMI_MI33_SMI_SUB_DEBUG_M0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI_SMI]",
		0x1030A400, DRV_Reg32(SEMI_MI33_SMI_SUB_DEBUG_S0),
		0x1030A404, DRV_Reg32(SEMI_MI33_SMI_SUB_DEBUG_S1),
		0x1030A408, DRV_Reg32(SEMI_MI33_SMI_SUB_DEBUG_S2),
		0x1030A430, DRV_Reg32(SEMI_MI33_SMI_SUB_DEBUG_M0));

	/* NEMI_MI32_SMI_SUB_DEBUG_MISC */
	/* NEMI_MI33_SMI_SUB_DEBUG_MISC */
	/* SEMI_MI32_SMI_SUB_DEBUG_MISC */
	/* SEMI_MI33_SMI_SUB_DEBUG_MISC */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI_SMI]",
		0x1025E440, DRV_Reg32(NEMI_MI32_SMI_SUB_DEBUG_MISC),
		0x1025F440, DRV_Reg32(NEMI_MI33_SMI_SUB_DEBUG_MISC),
		0x10309440, DRV_Reg32(SEMI_MI32_SMI_SUB_DEBUG_MISC),
		0x1030A440, DRV_Reg32(SEMI_MI33_SMI_SUB_DEBUG_MISC));

	/* IFR_MFGSYS_PROT_EN_STA_0 */
	/* IFR_MFGSYS_PROT_EN_W1S_0 */
	/* IFR_MFGSYS_PROT_EN_W1C_0 */
	/* IFR_MFGSYS_PROT_RDY_STA_0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[INFRA]",
		0x1002C1A0, DRV_Reg32(IFR_MFGSYS_PROT_EN_STA_0),
		0x1002C1A4, DRV_Reg32(IFR_MFGSYS_PROT_EN_W1S_0),
		0x1002C1A8, DRV_Reg32(IFR_MFGSYS_PROT_EN_W1C_0),
		0x1002C1AC, DRV_Reg32(IFR_MFGSYS_PROT_RDY_STA_0));

	/* IFR_EMISYS_PROTECT_EN_STA_0 */
	/* IFR_EMISYS_PROTECT_EN_W1S_0 */
	/* IFR_EMISYS_PROTECT_EN_W1C_0 */
	/* IFR_EMISYS_PROTECT_RDY_STA_0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[INFRA]",
		0x1002C100, DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_0),
		0x1002C104, DRV_Reg32(IFR_EMISYS_PROTECT_EN_W1S_0),
		0x1002C108, DRV_Reg32(IFR_EMISYS_PROTECT_EN_W1C_0),
		0x1002C10C, DRV_Reg32(IFR_EMISYS_PROTECT_RDY_STA_0));

	/* IFR_EMISYS_PROTECT_EN_STA_1 */
	/* IFR_EMISYS_PROTECT_EN_W1S_1 */
	/* IFR_EMISYS_PROTECT_EN_W1C_1 */
	/* IFR_EMISYS_PROTECT_RDY_STA_1 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[INFRA]",
		0x1002C120, DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_1),
		0x1002C124, DRV_Reg32(IFR_EMISYS_PROTECT_EN_W1S_1),
		0x1002C128, DRV_Reg32(IFR_EMISYS_PROTECT_EN_W1C_1),
		0x1002C12C, DRV_Reg32(IFR_EMISYS_PROTECT_RDY_STA_1));

	/* NTH_EMI_AO_DEBUG_CTRL0 */
	/* STH_EMI_AO_DEBUG_CTRL0 */
	/* INFRA_AO_BUS0_U_DEBUG_CTRL0 */
	/* INFRA_AO1_BUS1_U_DEBUG_CTRL0 */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x, (0x%x): 0x%08x",
		"[EMI_INFRA]",
		0x10042000, DRV_Reg32(NTH_EMI_AO_DEBUG_CTRL0),
		0x10028000, DRV_Reg32(STH_EMI_AO_DEBUG_CTRL0),
		0x10023000, DRV_Reg32(INFRA_AO_BUS0_U_DEBUG_CTRL0),
		0x1002B000, DRV_Reg32(INFRA_AO1_BUS1_U_DEBUG_CTRL0));

	/* SPM_SRC_REQ */
	GPUFREQ_LOGI("%-11s (0x%x): 0x%08x",
		"[SPM]",
		0x1C001818, DRV_Reg32(SPM_SRC_REQ));

	GPUFREQ_LOGI("%-11s 0x%08x, 0x%08x",
		"[PWR_STATUS]",
		DRV_Reg32(SPM_XPU_PWR_STATUS), DRV_Reg32(SPM_XPU_PWR_STATUS_2ND));

	GPUFREQ_LOGI("%-11s 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x",
		"[MFG0-4]", DRV_Reg32(SPM_MFG0_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG1_PWR_CON), DRV_Reg32(MFG_RPC_MFG2_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG3_PWR_CON), DRV_Reg32(MFG_RPC_MFG4_PWR_CON));
	GPUFREQ_LOGI("%-11s 0x%08x, 0x%08x, 0x%08x, 0x%08x",
		"[MFG6-10]",
		DRV_Reg32(MFG_RPC_MFG6_PWR_CON), DRV_Reg32(MFG_RPC_MFG7_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG9_PWR_CON), DRV_Reg32(MFG_RPC_MFG10_PWR_CON));
	GPUFREQ_LOGI("%-11s 0x%08x, 0x%08x, 0x%08x, 0x%08x",
		"[MFG11-14]",
		DRV_Reg32(MFG_RPC_MFG11_PWR_CON), DRV_Reg32(MFG_RPC_MFG12_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG13_PWR_CON), DRV_Reg32(MFG_RPC_MFG14_PWR_CON));
}

void __gpufreq_dump_power_tracker_status(void)
{
	unsigned int val = 0;
	int i = 0;

	if (!g_gpufreq_ready)
		return;

	GPUFREQ_LOGI("== [PDC Power Tracker STATUS] ==");
	GPUFREQ_LOGI("Current Pointer: %d",
		(int)((DRV_Reg32(MFG_POWER_TRACKER_SETTING) >> 9) & GENMASK(4, 0)));

	for (i = 0; i < 32; i++) {
		val = DRV_Reg32(MFG_POWER_TRACKER_SETTING);
		val &= ~GENMASK(8, 4);
		val |= i << 4;
		DRV_WriteReg32(MFG_POWER_TRACKER_SETTING, val);
		udelay(1);

		GPUFREQ_LOGI("[SLOT %d] TIME STAMP: 0x%08x, STATUS0: 0x%08x, STATUS1: 0x%08x",
			i, DRV_Reg32(MFG_POWER_TRACKER_TIME_STAMP),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS0),
			DRV_Reg32(MFG_POWER_TRACKER_PDC_STATUS1));
	}
}

/* API: get working OPP index of GPU limited by BATTERY_OC via given level */
int __gpufreq_get_batt_oc_idx(int batt_oc_level)
{
#if (GPUFREQ_BATT_OC_ENABLE && IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING))
	if (batt_oc_level == 1)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_BATT_OC_FREQ);
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(batt_oc_level);

	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_BATT_OC_ENABLE && CONFIG_MTK_BATTERY_OC_POWER_THROTTLING */
}

/* API: get working OPP index of GPU limited by BATTERY_PERCENT via given level */
int __gpufreq_get_batt_percent_idx(int batt_percent_level)
{
#if (GPUFREQ_BATT_PERCENT_ENABLE && IS_ENABLED(CONFIG_MTK_BATTERY_PERCENT_THROTTLING))
	if (batt_percent_level == 2)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_BATT_PERCENT_LV2_FREQ);
	else if (batt_percent_level == 3)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_BATT_PERCENT_LV3_FREQ);
	else if (batt_percent_level == 4)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_BATT_PERCENT_LV4_FREQ);
	else if (batt_percent_level == 5)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_BATT_PERCENT_LV5_FREQ);
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(batt_percent_level);

	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_BATT_PERCENT_ENABLE && CONFIG_MTK_BATTERY_PERCENT_THROTTLING */
}

/* API: get working OPP index of GPU limited by LOW_BATTERY via given level */
int __gpufreq_get_low_batt_idx(int low_batt_level)
{
#if (GPUFREQ_LOW_BATT_ENABLE && IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING))
	if (low_batt_level == 1)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_LOW_BATT_LV1_FREQ);
	else if (low_batt_level == 2)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_LOW_BATT_LV2_FREQ);
	else if (low_batt_level == 3)
		return __gpufreq_get_idx_by_fgpu(GPUFREQ_LOW_BATT_LV3_FREQ);
	else
		return GPUPPM_RESET_IDX;
#else
	GPUFREQ_UNREFERENCED(low_batt_level);

	return GPUPPM_KEEP_IDX;
#endif /* GPUFREQ_LOW_BATT_ENABLE && CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING */
}

/* API: handle DEVAPC violation */
void __gpufreq_devapc_vio_handler(void)
{
#if GPUFREQ_HWDCM_ENABLE
	/* disable HWDCM */
	/* (A) MFG_GLOBAL_CON 0x13FBF0B0 [8]  GPU_SOCIF_MST_FREE_RUN = 1'b1 */
	DRV_WriteReg32(MFG_GLOBAL_CON, DRV_Reg32(MFG_GLOBAL_CON) | BIT(8));
	/* (C) MFG_RPC_AO_CLK_CFG 0x13F91034 [0] CG_FAXI_CK_SOC_IN_FREE_RUN = 1'b1 */
	DRV_WriteReg32(MFG_RPC_AO_CLK_CFG, DRV_Reg32(MFG_RPC_AO_CLK_CFG) | BIT(0));
#endif /* GPUFREQ_HWDCM_ENABLE */
}

/* API: update debug info to shared memory */
void __gpufreq_update_debug_opp_info(void)
{
	if (!g_shared_status)
		return;

	mutex_lock(&gpufreq_lock);

	/* update current status to shared memory */
	if (__gpufreq_get_power_state()) {
		g_shared_status->cur_con1_fgpu = __gpufreq_get_real_fgpu();
		g_shared_status->cur_con1_fstack = __gpufreq_get_real_fstack();
		g_shared_status->cur_fmeter_fgpu = __gpufreq_get_fmeter_freq(TARGET_GPU);
		g_shared_status->cur_fmeter_fstack = __gpufreq_get_fmeter_freq(TARGET_STACK);
		g_shared_status->cur_regulator_vgpu = __gpufreq_get_real_vgpu();
		g_shared_status->cur_regulator_vsram_gpu = __gpufreq_get_real_vsram();
		g_shared_status->cur_regulator_vsram_stack = __gpufreq_get_real_vsram();
	} else {
		g_shared_status->cur_con1_fgpu = 0;
		g_shared_status->cur_con1_fstack = 0;
		g_shared_status->cur_fmeter_fgpu = 0;
		g_shared_status->cur_fmeter_fstack = 0;
		g_shared_status->cur_regulator_vgpu = 0;
		g_shared_status->cur_regulator_vstack = 0;
		g_shared_status->cur_regulator_vsram_stack = 0;
	}
	g_shared_status->mfg_pwr_status = MFG_0_14_PWR_STATUS;

	mutex_unlock(&gpufreq_lock);
}

/* API: general interface to set MFGSYS config */
void __gpufreq_set_mfgsys_config(enum gpufreq_config_target target, enum gpufreq_config_value val)
{
	mutex_lock(&gpufreq_lock);

	switch (target) {
	case CONFIG_STRESS_TEST:
		gpuppm_set_stress_test(val);
		break;
	case CONFIG_MARGIN:
		__gpufreq_set_margin_mode(val);
		break;
	case CONFIG_GPM1:
		__gpufreq_set_gpm_mode(1, val);
		break;
	case CONFIG_DFD:
		g_dfd_mode = val;
		break;
	case CONFIG_IPS:
		__gpufreq_set_ips_mode(val);
		break;
	case CONFIG_FAKE_MTCMOS_CTRL:
		__gpufreq_fake_mtcmos_control(val);
		break;
	default:
		GPUFREQ_LOGE("invalid config target: %d", target);
		break;
	}

	mutex_unlock(&gpufreq_lock);
}

/* API: get core_mask table */
struct gpufreq_core_mask_info *__gpufreq_get_core_mask_table(void)
{
	return g_core_mask_table;
}

/* API: get max number of shader cores */
unsigned int __gpufreq_get_core_num(void)
{
	return SHADER_CORE_NUM;
}

/* PDCA: GPU IP automatically control GPU shader MTCMOS */
void __gpufreq_pdca_config(enum gpufreq_power_state power)
{
#if GPUFREQ_PDCA_ENABLE
	if (power == GPU_PWR_ON) {
		/* MFG_ACTIVE_POWER_CON_CG 0x13FBF100 [0] rg_cg_active_pwrctl_en = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_CG,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_CG) | BIT(0)));
		/* MFG_ACTIVE_POWER_CON_ST0 0x13FBF120 [0] rg_st0_active_pwrctl_en = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST0,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST0) | BIT(0)));
		/* MFG_ACTIVE_POWER_CON_ST1 0x13FBF140 [0] rg_st1_active_pwrctl_en = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST1,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST1) | BIT(0)));
		/* MFG_ACTIVE_POWER_CON_ST4 0x13FBF0C0 [0] rg_st4_active_pwrctl_en = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST4,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST4) | BIT(0)));
		/* MFG_ACTIVE_POWER_CON_ST5 0x13FBF098 [0] rg_st5_active_pwrctl_en = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST5,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST5) | BIT(0)));
		/* MFG_ACTIVE_POWER_CON_00 0x13FBF400 [0] rg_sc0_active_pwrctl_en = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_00,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_00) | BIT(0)));
		/* MFG_ACTIVE_POWER_CON_01 0x13FBF404 [31] rg_sc0_active_pwrctl_rsv = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_01,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_01) | BIT(31)));
		/* MFG_ACTIVE_POWER_CON_06 0x13FBF418 [0] rg_sc1_active_pwrctl_en = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_06) | BIT(0)));
		/* MFG_ACTIVE_POWER_CON_07 0x13FBF41C [31] rg_sc1_active_pwrctl_rsv = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_07,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_07) | BIT(31)));
		/* MFG_ACTIVE_POWER_CON_12 0x13FBF430 [0] rg_sc2_active_pwrctl_en = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_12,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_12) | BIT(0)));
		/* MFG_ACTIVE_POWER_CON_13 0x13FBF434 [31] rg_sc2_active_pwrctl_rsv = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_13,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_13) | BIT(31)));
		/* MFG_ACTIVE_POWER_CON_18 0x13FBF448 [0] rg_sc3_active_pwrctl_en = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_18,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_18) | BIT(0)));
		/* MFG_ACTIVE_POWER_CON_19 0x13FBF44C [31] rg_sc3_active_pwrctl_rsv = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_19,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_19) | BIT(31)));
		/* MFG_ACTIVE_POWER_CON_24 0x13FBF460 [0] rg_sc4_active_pwrctl_en = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_24,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_24) | BIT(0)));
		/* MFG_ACTIVE_POWER_CON_25 0x13FBF464 [31] rg_sc4_active_pwrctl_rsv = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_25,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_25) | BIT(31)));
		/* MFG_ACTIVE_POWER_CON_30 0x13FBF478 [0] rg_sc5_active_pwrctl_en = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_30,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_30) | BIT(0)));
		/* MFG_ACTIVE_POWER_CON_31 0x13FBF47C [31] rg_sc5_active_pwrctl_rsv = 1'b1 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_31,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_31) | BIT(31)));

#if GPUFREQ_PDCA_PIPELINE_ENABLE
		/* PDCA pipeline shader cores control */
		/* config PDCA concurrently power on/off cores */
		DRV_WriteReg32(MALI_PWR_KEY, 0x2968A817);
		DRV_WriteReg32(MALI_PWR_OVERRIDE0, 0x00040000);
		DRV_WriteReg32(MALI_PWR_KEY, 0x2968A819);
		DRV_WriteReg32(MALI_PWR_OVERRIDE1, 0x00000000);
		/* PDCA power-on interval: 0x17 * 26MHz = 884ns */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_02, 0x0000);
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_08, 0x1700);
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_14, 0x2E00);
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_20, 0x4500);
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_26, 0x5C00);
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_32, 0x7300);
		/* PDCA power-off interval: 0x17 * 26MHz = 884ns */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_03, 0x0000);
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_10, 0x1700);
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_16, 0x2E00);
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_22, 0x4500);
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_28, 0x5C00);
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_34, 0x7300);
#endif /* GPUFREQ_PDCA_PIPELINE_ENABLE */
	} else {
		/* MFG_ACTIVE_POWER_CON_CG 0x13FBF100 [0] rg_cg_active_pwrctl_en = 1'b0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_CG,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_CG) & ~BIT(0)));
		/* MFG_ACTIVE_POWER_CON_ST0 0x13FBF120 [0] rg_st0_active_pwrctl_en = 1'b0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST0,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST0) & ~BIT(0)));
		/* MFG_ACTIVE_POWER_CON_ST1 0x13FBF140 [0] rg_st1_active_pwrctl_en = 1'b0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST1,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST1) & ~BIT(0)));
		/* MFG_ACTIVE_POWER_CON_ST4 0x13FBF0C0 [0] rg_st4_active_pwrctl_en = 1'b0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST4,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST4) & ~BIT(0)));
		/* MFG_ACTIVE_POWER_CON_ST5 0x13FBF098 [0] rg_st5_active_pwrctl_en = 1'b0 */
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_ST5,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_ST5) & ~BIT(0)));
		/* MFG_ACTIVE_POWER_CON_00 0x13FBF400 [0] rg_sc0_active_pwrctl_en = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_00,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_00) & ~BIT(0)));
		/* MFG_ACTIVE_POWER_CON_01 0x13FBF404 [31] rg_sc0_active_pwrctl_rsv = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_01,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_01) & ~BIT(31)));
		/* MFG_ACTIVE_POWER_CON_06 0x13FBF418 [0] rg_sc1_active_pwrctl_en = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_06,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_06) & ~BIT(0)));
		/* MFG_ACTIVE_POWER_CON_07 0x13FBF41C [31] rg_sc1_active_pwrctl_rsv = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_07,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_07) & ~BIT(31)));
		/* MFG_ACTIVE_POWER_CON_12 0x13FBF430 [0] rg_sc2_active_pwrctl_en = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_12,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_12) & ~BIT(0)));
		/* MFG_ACTIVE_POWER_CON_13 0x13FBF434 [31] rg_sc2_active_pwrctl_rsv = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_13,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_13) & ~BIT(31)));
		/* MFG_ACTIVE_POWER_CON_18 0x13FBF448 [0] rg_sc3_active_pwrctl_en = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_18,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_18) & ~BIT(0)));
		/* MFG_ACTIVE_POWER_CON_19 0x13FBF44C [31] rg_sc3_active_pwrctl_rsv = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_19,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_19) & ~BIT(31)));
		/* MFG_ACTIVE_POWER_CON_24 0x13FBF460 [0] rg_sc4_active_pwrctl_en = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_24,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_24) & ~BIT(0)));
		/* MFG_ACTIVE_POWER_CON_25 0x13FBF464 [31] rg_sc4_active_pwrctl_rsv = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_25,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_25) & ~BIT(31)));
		/* MFG_ACTIVE_POWER_CON_30 0x13FBF478 [0] rg_sc5_active_pwrctl_en = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_30,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_30) & ~BIT(0)));
		/* MFG_ACTIVE_POWER_CON_31 0x13FBF47C [31] rg_sc5_active_pwrctl_rsv = 1'b0*/
		DRV_WriteReg32(MFG_ACTIVE_POWER_CON_31,
			(DRV_Reg32(MFG_ACTIVE_POWER_CON_31) & ~BIT(31)));
	}
#else
	GPUFREQ_UNREFERENCED(power);
#endif /* GPUFREQ_PDCA_ENABLE */
}

/* API: init first time shared status */
void __gpufreq_set_shared_status(struct gpufreq_shared_status *shared_status)
{
	mutex_lock(&gpufreq_lock);

	if (shared_status)
		g_shared_status = shared_status;
	else
		__gpufreq_abort("null gpufreq shared status: 0x%llx", shared_status);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->magic = GPUFREQ_MAGIC_NUMBER;
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->opp_num_gpu = g_gpu.opp_num;
		g_shared_status->signed_opp_num_gpu = g_gpu.signed_opp_num;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->max_power_gpu = g_gpu.working_table[g_gpu.max_oppidx].power;
		g_shared_status->min_power_gpu = g_gpu.working_table[g_gpu.min_oppidx].power;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		g_shared_status->power_count = g_gpu.power_count;
		g_shared_status->buck_count = g_gpu.buck_count;
		g_shared_status->mtcmos_count = g_gpu.mtcmos_count;
		g_shared_status->cg_count = g_gpu.cg_count;
		g_shared_status->active_count = g_gpu.active_count;
		g_shared_status->power_control = __gpufreq_power_ctrl_enable();
		g_shared_status->active_sleep_control = __gpufreq_active_sleep_ctrl_enable();
		g_shared_status->dvfs_state = g_dvfs_state;
		g_shared_status->shader_present = g_shader_present;
		g_shared_status->asensor_enable = g_asensor_enable;
		g_shared_status->aging_load = g_aging_load;
		g_shared_status->aging_margin = g_aging_margin;
		g_shared_status->avs_enable = g_avs_enable;
		g_shared_status->avs_margin = g_avs_margin;
		g_shared_status->ptp_version = g_ptp_version;
		g_shared_status->gpm1_mode = g_gpm1_mode;
		g_shared_status->gpm3_mode = g_gpm3_mode;
		g_shared_status->power_tracker_mode = g_power_tracker_mode;
		g_shared_status->dual_buck = true;
		g_shared_status->segment_id = g_gpu.segment_id;
		g_shared_status->test_mode = true;
#if GPUFREQ_MSSV_TEST_MODE
		g_shared_status->reg_top_delsel.addr = 0x13FBF080;
#endif /* GPUFREQ_MSSV_TEST_MODE */
		__gpufreq_update_shared_status_opp_table();
		__gpufreq_update_shared_status_adj_table();
		__gpufreq_update_shared_status_init_reg();
	}

	mutex_unlock(&gpufreq_lock);
}

/* API: MSSV test function */
int __gpufreq_mssv_commit(unsigned int target, unsigned int val)
{
#if GPUFREQ_MSSV_TEST_MODE
	int ret = GPUFREQ_SUCCESS;

	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (unlikely(ret < 0))
		goto done;

	mutex_lock(&gpufreq_lock);

	switch (target) {
	case TARGET_MSSV_FGPU:
		if (val > POSDIV_2_MAX_FREQ || val < POSDIV_16_MIN_FREQ)
			ret = GPUFREQ_EINVAL;
		else
			ret = __gpufreq_freq_scale_gpu(g_gpu.cur_freq, val);
		break;
	case TARGET_MSSV_VGPU:
		if (val > VGPU_MAX_VOLT || val < VGPU_MIN_VOLT)
			ret = GPUFREQ_EINVAL;
		else
			ret = __gpufreq_volt_scale_gpu(
				g_gpu.cur_volt, val,
				g_gpu.cur_vsram, __gpufreq_get_vsram_by_vlogic(val));
		break;
	case TARGET_MSSV_FSTACK:
		if (val > POSDIV_2_MAX_FREQ || val < POSDIV_16_MIN_FREQ)
			ret = GPUFREQ_EINVAL;
		else
			ret = __gpufreq_freq_scale_stack(g_stack.cur_freq, val);
		break;
	case TARGET_MSSV_TOP_DELSEL:
		if (val == 1 || val == 0) {
			__gpufreq_mssv_set_del_sel(val);
			ret = GPUFREQ_SUCCESS;
		} else
			ret = GPUFREQ_EINVAL;
		break;
	default:
		ret = GPUFREQ_EINVAL;
		break;
	}

	if (unlikely(ret))
		GPUFREQ_LOGE("invalid MSSV cmd, target: %d, val: %d", target, val);
	else {
		if (g_shared_status) {
			g_shared_status->cur_fgpu = g_gpu.cur_freq;
			g_shared_status->cur_vgpu = g_gpu.cur_volt;
			g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
			g_shared_status->cur_fstack = g_stack.cur_freq;
			g_shared_status->reg_top_delsel.val = DRV_Reg32(MFG_SRAM_FUL_SEL_ULV);
		}
	}

	mutex_unlock(&gpufreq_lock);

	__gpufreq_power_control(GPU_PWR_OFF);

done:
	return ret;
#else
	GPUFREQ_UNREFERENCED(target);
	GPUFREQ_UNREFERENCED(val);

	return GPUFREQ_EINVAL;
#endif /* GPUFREQ_MSSV_TEST_MODE */
}

/**
 * ===============================================
 * Internal Function Definition
 * ===============================================
 */
static void __gpufreq_update_shared_status_opp_table(void)
{
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* GPU */
	/* working table */
	copy_size = sizeof(struct gpufreq_opp_info) * g_gpu.opp_num;
	memcpy(g_shared_status->working_table_gpu, g_gpu.working_table, copy_size);
	/* signed table */
	copy_size = sizeof(struct gpufreq_opp_info) * g_gpu.signed_opp_num;
	memcpy(g_shared_status->signed_table_gpu, g_gpu.signed_table, copy_size);
}

static void __gpufreq_update_shared_status_adj_table(void)
{
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* GPU */
	/* aging table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_GPU_SIGNED_IDX;
	memcpy(g_shared_status->aging_table_gpu, g_gpu_aging_table[g_aging_table_idx], copy_size);
	/* avs table */
	copy_size = sizeof(struct gpufreq_adj_info) * NUM_GPU_SIGNED_IDX;
	memcpy(g_shared_status->avs_table_gpu, g_gpu_avs_table, copy_size);
}

static void __gpufreq_update_shared_status_init_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	/* [WARNING] GPU is power off at this moment */
	g_reg_mfgsys[IDX_EFUSE_GPU_PTP0_AVS].val = DRV_Reg32(EFUSE_GPU_PTP0_AVS);
	g_reg_mfgsys[IDX_EFUSE_GPU_PTP1_AVS].val = DRV_Reg32(EFUSE_GPU_PTP1_AVS);
	g_reg_mfgsys[IDX_EFUSE_GPU_PTP2_AVS].val = DRV_Reg32(EFUSE_GPU_PTP2_AVS);
	g_reg_mfgsys[IDX_SPM_SPM2GPUPM_CON].val = DRV_Reg32(SPM_SPM2GPUPM_CON);

	copy_size = sizeof(struct gpufreq_reg_info) * NUM_MFGSYS_REG;
	memcpy(g_shared_status->reg_mfgsys, g_reg_mfgsys, copy_size);
#endif /* GPUFREQ_SHARED_STATUS_REG */
}

static void __gpufreq_update_shared_status_power_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	g_reg_mfgsys[IDX_MFG_CG_CON].val = DRV_Reg32(MFG_CG_CON);
	g_reg_mfgsys[IDX_MFG_DCM_CON_0].val = DRV_Reg32(MFG_DCM_CON_0);
	g_reg_mfgsys[IDX_MFG_ASYNC_CON].val = DRV_Reg32(MFG_ASYNC_CON);
	g_reg_mfgsys[IDX_MFG_ASYNC_CON3].val = DRV_Reg32(MFG_ASYNC_CON3);
	g_reg_mfgsys[IDX_MFG_GLOBAL_CON].val = DRV_Reg32(MFG_GLOBAL_CON);
	g_reg_mfgsys[IDX_MFG_AXCOHERENCE_CON].val = DRV_Reg32(MFG_AXCOHERENCE_CON);
	g_reg_mfgsys[IDX_MFG_SRAM_FUL_SEL_ULV].val = DRV_Reg32(MFG_SRAM_FUL_SEL_ULV);
	g_reg_mfgsys[IDX_MFG_PLL_CON0].val = DRV_Reg32(MFG_PLL_CON0);
	g_reg_mfgsys[IDX_MFG_PLL_CON1].val = DRV_Reg32(MFG_PLL_CON1);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON0].val = DRV_Reg32(MFGSC_PLL_CON0);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON1].val = DRV_Reg32(MFGSC_PLL_CON1);
	g_reg_mfgsys[IDX_MFG_RPC_AO_CLK_CFG].val = DRV_Reg32(MFG_RPC_AO_CLK_CFG);
	g_reg_mfgsys[IDX_MFG_RPC_MFG1_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG1_PWR_CON);
#if !GPUFREQ_PDCA_ENABLE
	g_reg_mfgsys[IDX_MFG_RPC_MFG2_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG2_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG3_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG3_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG4_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG4_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG6_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG6_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG7_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG7_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG9_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG9_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG10_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG10_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG11_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG11_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG12_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG12_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG13_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG13_PWR_CON);
	g_reg_mfgsys[IDX_MFG_RPC_MFG14_PWR_CON].val = DRV_Reg32(MFG_RPC_MFG14_PWR_CON);
#endif /* GPUFREQ_PDCA_ENABLE */
	g_reg_mfgsys[IDX_MFG_RPC_SLP_PROT_EN_STA].val = DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA);
	g_reg_mfgsys[IDX_SPM_MFG0_PWR_CON].val = DRV_Reg32(SPM_MFG0_PWR_CON);
	g_reg_mfgsys[IDX_SPM_SOC_BUCK_ISO_CON].val = DRV_Reg32(SPM_SOC_BUCK_ISO_CON);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_3].val = DRV_Reg32(TOPCK_CLK_CFG_3);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_30].val = DRV_Reg32(TOPCK_CLK_CFG_30);
	g_reg_mfgsys[IDX_NTH_MFG_EMI1_GALS_SLV_DBG].val = DRV_Reg32(NTH_MFG_EMI1_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_NTH_MFG_EMI0_GALS_SLV_DBG].val = DRV_Reg32(NTH_MFG_EMI0_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_STH_MFG_EMI1_GALS_SLV_DBG].val = DRV_Reg32(STH_MFG_EMI1_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_STH_MFG_EMI0_GALS_SLV_DBG].val = DRV_Reg32(STH_MFG_EMI0_GALS_SLV_DBG);
	g_reg_mfgsys[IDX_NTH_M6M7_IDLE_BIT_EN_1].val = DRV_Reg32(NTH_M6M7_IDLE_BIT_EN_1);
	g_reg_mfgsys[IDX_NTH_M6M7_IDLE_BIT_EN_0].val = DRV_Reg32(NTH_M6M7_IDLE_BIT_EN_0);
	g_reg_mfgsys[IDX_STH_M6M7_IDLE_BIT_EN_1].val = DRV_Reg32(STH_M6M7_IDLE_BIT_EN_1);
	g_reg_mfgsys[IDX_STH_M6M7_IDLE_BIT_EN_0].val = DRV_Reg32(STH_M6M7_IDLE_BIT_EN_0);
	g_reg_mfgsys[IDX_IFR_MFGSYS_PROT_EN_STA_0].val = DRV_Reg32(IFR_MFGSYS_PROT_EN_STA_0);
	g_reg_mfgsys[IDX_IFR_MFGSYS_PROT_RDY_STA_0].val = DRV_Reg32(IFR_MFGSYS_PROT_RDY_STA_0);
	g_reg_mfgsys[IDX_IFR_EMISYS_PROTECT_EN_STA_0].val = DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_0);
	g_reg_mfgsys[IDX_IFR_EMISYS_PROTECT_EN_STA_1].val = DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_1);
	g_reg_mfgsys[IDX_NTH_EMI_AO_DEBUG_CTRL0].val = DRV_Reg32(NTH_EMI_AO_DEBUG_CTRL0);
	g_reg_mfgsys[IDX_STH_EMI_AO_DEBUG_CTRL0].val = DRV_Reg32(STH_EMI_AO_DEBUG_CTRL0);
	g_reg_mfgsys[IDX_INFRA_AO_BUS0_U_DEBUG_CTRL0].val = DRV_Reg32(INFRA_AO_BUS0_U_DEBUG_CTRL0);
	g_reg_mfgsys[IDX_INFRA_AO1_BUS1_U_DEBUG_CTRL0].val =
		DRV_Reg32(INFRA_AO1_BUS1_U_DEBUG_CTRL0);

	copy_size = sizeof(struct gpufreq_reg_info) * NUM_MFGSYS_REG;
	memcpy(g_shared_status->reg_mfgsys, g_reg_mfgsys, copy_size);
#endif /* GPUFREQ_SHARED_STATUS_REG */
}

static void __gpufreq_update_shared_status_active_sleep_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	g_reg_mfgsys[IDX_MFG_PLL_CON0].val = DRV_Reg32(MFG_PLL_CON0);
	g_reg_mfgsys[IDX_MFG_PLL_CON1].val = DRV_Reg32(MFG_PLL_CON1);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON0].val = DRV_Reg32(MFGSC_PLL_CON0);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON1].val = DRV_Reg32(MFGSC_PLL_CON1);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_3].val = DRV_Reg32(TOPCK_CLK_CFG_3);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_30].val = DRV_Reg32(TOPCK_CLK_CFG_30);

	copy_size = sizeof(struct gpufreq_reg_info) * NUM_MFGSYS_REG;
	memcpy(g_shared_status->reg_mfgsys, g_reg_mfgsys, copy_size);
#endif /* GPUFREQ_SHARED_STATUS_REG */
}

static void __gpufreq_update_shared_status_dvfs_reg(void)
{
#if GPUFREQ_SHARED_STATUS_REG
	unsigned int copy_size = 0;

	if (!g_shared_status)
		return;

	g_reg_mfgsys[IDX_MFG_SRAM_FUL_SEL_ULV].val = DRV_Reg32(MFG_SRAM_FUL_SEL_ULV);
	g_reg_mfgsys[IDX_MFG_PLL_CON0].val = DRV_Reg32(MFG_PLL_CON0);
	g_reg_mfgsys[IDX_MFG_PLL_CON1].val = DRV_Reg32(MFG_PLL_CON1);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON0].val = DRV_Reg32(MFGSC_PLL_CON0);
	g_reg_mfgsys[IDX_MFGSC_PLL_CON1].val = DRV_Reg32(MFGSC_PLL_CON1);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_3].val = DRV_Reg32(TOPCK_CLK_CFG_3);
	g_reg_mfgsys[IDX_TOPCK_CLK_CFG_30].val = DRV_Reg32(TOPCK_CLK_CFG_30);

	copy_size = sizeof(struct gpufreq_reg_info) * NUM_MFGSYS_REG;
	memcpy(g_shared_status->reg_mfgsys, g_reg_mfgsys, copy_size);
#endif /* GPUFREQ_SHARED_STATUS_REG */
}

static unsigned int __gpufreq_custom_init_enable(void)
{
	return GPUFREQ_CUST_INIT_ENABLE;
}

static unsigned int __gpufreq_dvfs_enable(void)
{
	return GPUFREQ_DVFS_ENABLE;
}

/* API: set/reset DVFS state with lock */
static void __gpufreq_set_dvfs_state(unsigned int set, unsigned int state)
{
	mutex_lock(&gpufreq_lock);

	if (set)
		g_dvfs_state |= state;
	else
		g_dvfs_state &= ~state;

	/* update current status to shared memory */
	if (g_shared_status)
		g_shared_status->dvfs_state = g_dvfs_state;

	mutex_unlock(&gpufreq_lock);
}

/* API: fake PWR_CON value to temporarily disable PDCA */
static void __gpufreq_fake_mtcmos_control(unsigned int mode)
{
#if GPUFREQ_PDCA_ENABLE
	if (mode == FEAT_ENABLE) {
		/* fake power on value of SPM MFG2-14 */
		DRV_WriteReg32(MFG_RPC_MFG2_PWR_CON, 0xC000000D);  /* MFG2  */
		DRV_WriteReg32(MFG_RPC_MFG3_PWR_CON, 0xC000000D);  /* MFG3  */
		DRV_WriteReg32(MFG_RPC_MFG4_PWR_CON, 0xC000000D);  /* MFG4  */
		DRV_WriteReg32(MFG_RPC_MFG6_PWR_CON, 0xC000000D);  /* MFG6  */
		DRV_WriteReg32(MFG_RPC_MFG7_PWR_CON, 0xC000000D);  /* MFG7  */
		DRV_WriteReg32(MFG_RPC_MFG9_PWR_CON, 0xC000000D);  /* MFG9  */
		DRV_WriteReg32(MFG_RPC_MFG10_PWR_CON, 0xC000000D); /* MFG10 */
		DRV_WriteReg32(MFG_RPC_MFG11_PWR_CON, 0xC000000D); /* MFG11 */
		DRV_WriteReg32(MFG_RPC_MFG12_PWR_CON, 0xC000000D); /* MFG12 */
		DRV_WriteReg32(MFG_RPC_MFG13_PWR_CON, 0xC000000D); /* MFG13 */
		DRV_WriteReg32(MFG_RPC_MFG14_PWR_CON, 0xC000000D); /* MFG14 */
	} else if (mode == FEAT_DISABLE) {
		/* fake power off value of SPM MFG2-14 */
		DRV_WriteReg32(MFG_RPC_MFG14_PWR_CON, 0x1112); /* MFG14 */
		DRV_WriteReg32(MFG_RPC_MFG13_PWR_CON, 0x1112); /* MFG13 */
		DRV_WriteReg32(MFG_RPC_MFG12_PWR_CON, 0x1112); /* MFG12 */
		DRV_WriteReg32(MFG_RPC_MFG11_PWR_CON, 0x1112); /* MFG11 */
		DRV_WriteReg32(MFG_RPC_MFG10_PWR_CON, 0x1112); /* MFG10 */
		DRV_WriteReg32(MFG_RPC_MFG9_PWR_CON, 0x1112);  /* MFG9  */
		DRV_WriteReg32(MFG_RPC_MFG7_PWR_CON, 0x1112);  /* MFG7  */
		DRV_WriteReg32(MFG_RPC_MFG6_PWR_CON, 0x1112);  /* MFG6  */
		DRV_WriteReg32(MFG_RPC_MFG4_PWR_CON, 0x1112);  /* MFG4  */
		DRV_WriteReg32(MFG_RPC_MFG3_PWR_CON, 0x1112);  /* MFG3  */
		DRV_WriteReg32(MFG_RPC_MFG2_PWR_CON, 0x1112);  /* MFG2  */
	}
#else
	GPUFREQ_UNREFERENCED(mode);
#endif /* GPUFREQ_PDCA_ENABLE */
}

/* API: apply/restore Vaging to working table of GPU */
static void __gpufreq_set_margin_mode(unsigned int mode)
{
	/* update volt margin */
	__gpufreq_apply_restore_margin(mode);

	/* update power info to working table */
	__gpufreq_measure_power();

	/* update current status to shared memory */
	if (g_shared_status)
		__gpufreq_update_shared_status_opp_table();
}

/* API: enable/disable GPM 1.0 */
static void __gpufreq_set_gpm_mode(unsigned int version, unsigned int mode)
{
	if (version == 1)
		g_gpm1_mode = mode;

	/* update current status to shared memory */
	if (g_shared_status)
		g_shared_status->gpm1_mode = g_gpm1_mode;
}

#if GPUFREQ_IPS_ENABLE
/* Control IPS MTCMOS on/off */
static void __gpufreq_ips_rpc_control(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == GPU_PWR_ON) {
		/* IPS_SES_PWR_CON 0x13F910FC [2] PWR_ON = 1'b1 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) | BIT(2)));
		/* IPS_SES_PWR_CON 0x13F910FC [30] PWR_ACK = 1'b1 */
		i = 0;
		while ((DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & BIT(30)) != BIT(30)) {
			udelay(10);
			if (++i > 10)
				break;
		}
		/* IPS_SES_PWR_CON 0x13F910FC [3] PWR_ON_2ND = 1'b1 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) | BIT(3)));
		/* IPS_SES_PWR_CON 0x13F910FC [31] PWR_ACK_2ND = 1'b1 */
		i = 0;
		while ((DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & BIT(31)) != BIT(31)) {
			udelay(10);
			if (++i > 10)
				break;
		}
		/* IPS_SES_PWR_CON 0x13F910FC [30] PWR_ACK = 1'b1 */
		/* IPS_SES_PWR_CON 0x13F910FC [31] PWR_ACK_2ND = 1'b1 */
		i = 0;
		while ((DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & GENMASK(31, 30)) != GENMASK(31, 30)) {
			udelay(10);
			if (++i > 500)
				goto timeout;
		}
		/* IPS_SES_PWR_CON 0x13F910FC [4] PWR_CLK_DIS = 1'b0 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & ~BIT(4)));
		/* IPS_SES_PWR_CON 0x13F910FC [1] PWR_ISO = 1'b0 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & ~BIT(1)));
		/* IPS_SES_PWR_CON 0x13F910FC [0] PWR_RST_B = 1'b1 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) | BIT(0)));
		/* IPS_SES_PWR_CON 0x13F910FC [8] PWR_SRAM_PDN = 1'b0 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & ~BIT(8)));
		/* IPS_SES_PWR_CON 0x13F910FC [12] PWR_SRAM_PDN_ACK = 1'b0 */
		i = 0;
		while (DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & BIT(12)) {
			udelay(10);
			if (++i > 500)
				goto timeout;
		}
	} else {
		/* IPS_SES_PWR_CON 0x13F910FC [8] PWR_SRAM_PDN = 1'b1 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) | BIT(8)));
		/* check SRAM_PDN_ACK */
		/* IPS_SES_PWR_CON 0x13F910FC [12] PWR_SRAM_PDN_ACK = 1'b1 */
		i = 0;
		while ((DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & BIT(12)) != BIT(12)) {
			udelay(10);
			if (++i > 500)
				goto timeout;
		}
		/* IPS_SES_PWR_CON 0x13F910FC [1] PWR_ISO = 1'b1 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) | BIT(1)));
		/* IPS_SES_PWR_CON 0x13F910FC [4] PWR_CLK_DIS = 1'b1 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) | BIT(4)));
		/* IPS_SES_PWR_CON 0x13F910FC [0] PWR_RST_B = 1'b0 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & ~BIT(0)));
		/* IPS_SES_PWR_CON 0x13F910FC [2] PWR_ON = 1'b0 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & ~BIT(2)));
		/* IPS_SES_PWR_CON 0x13F910FC [3] PWR_ON_2ND = 1'b0 */
		DRV_WriteReg32(MFG_RPC_IPS_SES_PWR_CON,
			(DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & ~BIT(3)));
		/* IPS_SES_PWR_CON 0x13F910FC [30] PWR_ACK = 1'b0 */
		/* IPS_SES_PWR_CON 0x13F910FC [31] PWR_ACK_2ND = 1'b0 */
		i = 0;
		while (DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON) & GENMASK(31, 30)) {
			udelay(10);
			if (++i > 500)
				goto timeout;
		}
	}

	GPUFREQ_LOGI("Power %s IPS, (0x13F910FC): 0x%x",
		power ? "On" : "Off", DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON));

	return;

timeout:
	__gpufreq_abort("Power %s IPS timeout, (0x13F910FC): 0x%x",
		power ? "On" : "Off", DRV_Reg32(MFG_RPC_IPS_SES_PWR_CON));
}
#endif /* GPUFREQ_IPS_ENABLE */

/* API: enable/disable IPS mode and get Vmin */
static void __gpufreq_set_ips_mode(unsigned int mode)
{
#if GPUFREQ_IPS_ENABLE
	u32 val = 0, autok_trim0 = 0, autok_trim1 = 0, autok_trim2 = 0;
	unsigned int autok_result = false;
	unsigned long long vmin_val = 0;

	if (mode == FEAT_ENABLE) {
		/* enable IPS MTCMOS */
		__gpufreq_ips_rpc_control(GPU_PWR_ON);
		/* init */
		DRV_WriteReg32(MFG_IPS_01, 0x00000000);
		DRV_WriteReg32(MFG_IPS_13, 0x00400000);
		DRV_WriteReg32(MFG_IPS_01, 0x0001A400);
		DRV_WriteReg32(MFG_IPS_10, 0x044040FE);
		DRV_WriteReg32(MFG_IPS_01, 0x0001A500);

		/* delay 500us */
		udelay(500);

		GPUFREQ_LOGI("IPS_01: 0x%x, IPS_10: 0x%x, IPS_12: 0x%x, IPS_13: 0x%x",
			DRV_Reg32(MFG_IPS_01), DRV_Reg32(MFG_IPS_10),
			DRV_Reg32(MFG_IPS_12), DRV_Reg32(MFG_IPS_13));

		/* check autok */
		val = DRV_Reg32(MFG_IPS_12);
		/* SupplEyeScanV7P0_12 0x13FE002C [0] AutoCalibDone = 1'b1 */
		/* SupplEyeScanV7P0_12 0x13FE002C [21:19] AutoCalibError = 3'b000 */
		if ((val & BIT(0)) && ((val & GENMASK(21, 19)) == 0)) {
			autok_trim0 = 0;
			autok_trim1 = 0;
			autok_trim2 = 0;
			autok_result = true;
		} else {
			autok_trim0 = (DRV_Reg32(MFG_IPS_12) & GENMASK(6, 1)) >> 1;
			autok_trim1 = (DRV_Reg32(MFG_IPS_12) & GENMASK(12, 7)) >> 7;
			autok_trim2 = (DRV_Reg32(MFG_IPS_12) & GENMASK(18, 13)) >> 13;
			autok_result = false;
		}

		/* clear IRQ config */
		DRV_WriteReg32(MFG_IPS_05, 0x00000000);

		g_ips_mode = mode;
		/* update current status to shared memory */
		if (g_shared_status) {
			g_shared_status->ips_mode = g_ips_mode;
			g_shared_status->ips_info.autok_result = autok_result;
			g_shared_status->ips_info.autok_trim0 = autok_trim0;
			g_shared_status->ips_info.autok_trim1 = autok_trim1;
			g_shared_status->ips_info.autok_trim2 = autok_trim2;
		}
	} else if (mode == FEAT_DISABLE) {
		/* reset */
		DRV_WriteReg32(MFG_IPS_01, 0x00000000);
		/* disable IPS MTCMOS */
		__gpufreq_ips_rpc_control(GPU_PWR_OFF);

		g_ips_mode = mode;
		/* update current status to shared memory */
		if (g_shared_status) {
			g_shared_status->ips_mode = g_ips_mode;
			g_shared_status->ips_info.autok_result = false;
			g_shared_status->ips_info.autok_trim0 = 0;
			g_shared_status->ips_info.autok_trim1 = 0;
			g_shared_status->ips_info.autok_trim2 = 0;
		}
	} else if (val == IPS_VMIN_GET) {
		/* SupplEyeScanV7P0_06 0x13FE0014 [7:0] VminValue */
		val = DRV_Reg32(MFG_IPS_06) & GENMASK(7, 0);
		/* mV * 100 */
		vmin_val = (((unsigned long long)val * 75000) / 255) + 37500;

		/* update current status to shared memory */
		if (g_shared_status) {
			g_shared_status->ips_info.vmin_reg_val = val;
			g_shared_status->ips_info.vmin_val = (unsigned int)vmin_val;
		}
	}
#endif /* GPUFREQ_IPS_ENABLE */
}

/* API: apply (enable) / restore (disable) margin */
static void __gpufreq_apply_restore_margin(unsigned int mode)
{
	struct gpufreq_opp_info *working_table = NULL;
	struct gpufreq_opp_info *signed_table = NULL;
	int working_opp_num = 0, signed_opp_num = 0, segment_upbound = 0, i = 0;

	working_table = g_gpu.working_table;
	signed_table = g_gpu.signed_table;
	working_opp_num = g_gpu.opp_num;
	signed_opp_num = g_gpu.signed_opp_num;
	segment_upbound = g_gpu.segment_upbound;

	/* update margin to signed table */
	for (i = 0; i < signed_opp_num; i++) {
		if (mode == FEAT_DISABLE)
			signed_table[i].volt += signed_table[i].margin;
		else if (mode == FEAT_ENABLE)
			signed_table[i].volt -= signed_table[i].margin;
		signed_table[i].vsram = __gpufreq_get_vsram_by_vlogic(signed_table[i].volt);
	}

	for (i = 0; i < working_opp_num; i++) {
		working_table[i].volt = signed_table[segment_upbound + i].volt;
		working_table[i].vsram = signed_table[segment_upbound + i].vsram;

		GPUFREQ_LOGD("Margin mode: %d, GPU[%d] Volt: %d, Vsram: %d",
			mode, i, working_table[i].volt, working_table[i].vsram);
	}
}

#if GPUFREQ_MSSV_TEST_MODE
static void __gpufreq_mssv_set_del_sel(unsigned int val)
{
	if (val == 1)
		DRV_WriteReg32(MFG_SRAM_FUL_SEL_ULV,
			(DRV_Reg32(MFG_SRAM_FUL_SEL_ULV) | BIT(0)));
	else if (val == 0)
		DRV_WriteReg32(MFG_SRAM_FUL_SEL_ULV,
			(DRV_Reg32(MFG_SRAM_FUL_SEL_ULV) & ~BIT(0)));
}
#endif /* GPUFREQ_MSSV_TEST_MODE */

/* API: DVFS order control of GPU */
static int __gpufreq_generic_scale_gpu(
	unsigned int freq_old, unsigned int freq_new,
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	int ret = GPUFREQ_SUCCESS;
	unsigned int vgpu_park = 0;
	unsigned int target_vgpu = 0, target_vsram = 0;

	GPUFREQ_TRACE_START
		("freq_old=%d, freq_new=%d, volt_old=%d, volt_new=%d, vsram_old=%d, vsram_new=%d",
		freq_old, freq_new, vgpu_old, vgpu_new, vsram_old, vsram_new);

	/* scale-up: Vsram -> Vgpu -> Freq */
	if (freq_new > freq_old) {
		/* volt scaling */
		ret = __gpufreq_volt_scale_gpu(vgpu_old, vgpu_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram: (%d->%d)",
				vgpu_old, vgpu_new, vsram_old, vsram_new);
			goto done;
		}

		/* GPU freq scaling */
		ret = __gpufreq_freq_scale_gpu(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
		/* STACK freq scaling */
		ret = __gpufreq_freq_scale_stack(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fstack: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
	/* scale-down: Freq -> Vgpu -> Vsram */
	} else if (freq_new < freq_old) {
		/* freq scaling */
		ret = __gpufreq_freq_scale_gpu(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fgpu: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
		/* STACK freq scaling */
		ret = __gpufreq_freq_scale_stack(freq_old, freq_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Fstack: (%d->%d)",
				freq_old, freq_new);
			goto done;
		}
		/* volt scaling */
		ret = __gpufreq_volt_scale_gpu(vgpu_old, vgpu_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram: (%d->%d)",
				vgpu_old, vgpu_new, vsram_old, vsram_new);
			goto done;
		}
	/* keep: volt only */
	} else {
		/* volt scaling */
		ret = __gpufreq_volt_scale_gpu(vgpu_old, vgpu_new, vsram_old, vsram_new);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to scale Vgpu: (%d->%d), Vsram_gpu: (%d->%d)",
				vgpu_old, vgpu_new, vsram_old, vsram_new);
			goto done;
		}
	}

	/* freq of GPU and STACK should always be equal */
	if (g_gpu.cur_freq != g_stack.cur_freq) {
		__gpufreq_abort("unequal Fgpu: %d and Fstack: %d",
			g_gpu.cur_freq, g_stack.cur_freq);
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: commit DVFS to GPU by given freq and volt
 * this is debug function and use it with caution
 */
static int __gpufreq_custom_commit_gpu(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key)
{
	int ret = GPUFREQ_SUCCESS;
	/* GPU */
	int cur_oppidx = 0,  target_oppidx = 0;
	unsigned int cur_fgpu = 0, cur_vgpu = 0, target_fgpu = 0, target_vgpu = 0;
	/* SRAM */
	unsigned int cur_vsram = 0, target_vsram = 0;

	GPUFREQ_TRACE_START("target_freq=%d, target_volt=%d, key=%d",
		target_freq, target_volt, key);

	mutex_lock(&gpufreq_lock);

	/* check dvfs state */
	if (g_dvfs_state & ~key) {
		GPUFREQ_LOGD("unavailable DVFS state (0x%x)", g_dvfs_state);
		ret = GPUFREQ_SUCCESS;
		goto done_unlock;
	}

	/* prepare OPP setting */
	cur_oppidx = g_gpu.cur_oppidx;
	cur_fgpu = g_gpu.cur_freq;
	cur_vgpu = g_gpu.cur_volt;
	cur_vsram = g_gpu.cur_vsram;

	target_oppidx = __gpufreq_get_idx_by_fgpu(target_freq);
	target_fgpu = target_freq;
	target_vgpu = target_volt;
	target_vsram = __gpufreq_get_vsram_by_vlogic(target_volt);


	GPUFREQ_LOGD("begin to commit GPU Freq: (%d->%d), Volt: (%d->%d)",
		cur_fgpu, target_fgpu, cur_vgpu, target_vgpu);

	ret = __gpufreq_generic_scale_gpu(cur_fgpu, target_fgpu,
		cur_vgpu, target_vgpu, cur_vsram, target_vsram);
	if (unlikely(ret)) {
		GPUFREQ_LOGE(
			"fail to scale GPU: Idx(%d->%d), Freq(%d->%d), Volt(%d->%d), Vsram(%d->%d)",
			cur_oppidx, target_oppidx, cur_fgpu, target_fgpu,
			cur_vgpu, target_vgpu, cur_vsram, target_vsram);
		goto done_unlock;
	}

	g_gpu.cur_oppidx = target_oppidx;

	__gpufreq_footprint_oppidx(target_oppidx);

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_oppidx_gpu = g_gpu.cur_oppidx;
		g_shared_status->cur_fgpu = g_gpu.cur_freq;
		g_shared_status->cur_vgpu = g_gpu.cur_volt;
		g_shared_status->cur_vsram_gpu = g_gpu.cur_vsram;
		g_shared_status->cur_power_gpu = g_gpu.working_table[g_gpu.cur_oppidx].power;
		g_shared_status->cur_fstack = g_stack.cur_freq;
		__gpufreq_update_shared_status_dvfs_reg();
	}

done_unlock:
	mutex_unlock(&gpufreq_lock);

	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: commit DVFS to STACK by given freq and volt
 * this is debug function and use it with caution
 */
static int __gpufreq_custom_commit_stack(unsigned int target_freq,
	unsigned int target_volt, enum gpufreq_dvfs_state key)
{
	GPUFREQ_UNREFERENCED(target_freq);
	GPUFREQ_UNREFERENCED(target_volt);
	GPUFREQ_UNREFERENCED(key);

	return GPUFREQ_EINVAL;
}

static int __gpufreq_switch_clksrc(enum gpufreq_target target, enum gpufreq_clk_src clksrc)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("clksrc=%d", clksrc);

	if (target == TARGET_STACK) {
		if (clksrc == CLOCK_MAIN) {
			ret = clk_set_parent(g_clk->clk_sc_mux, g_clk->clk_sc_main_parent);
			g_stack.cur_freq = __gpufreq_get_real_fstack();
		} else if (clksrc == CLOCK_SUB) {
			ret = clk_set_parent(g_clk->clk_sc_mux, g_clk->clk_sc_sub_parent);
			g_stack.cur_freq = __gpufreq_get_fmeter_sub_fstack();
		}
	} else {
		if (clksrc == CLOCK_MAIN) {
			ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_main_parent);
			g_gpu.cur_freq = __gpufreq_get_real_fgpu();
		} else if (clksrc == CLOCK_SUB) {
			ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_sub_parent);
			g_gpu.cur_freq = __gpufreq_get_fmeter_sub_fgpu();
		}
	}

	if (unlikely(ret))
		__gpufreq_abort("fail to switch %s clk src: %d (%d)",
			target == TARGET_STACK ? "STACK" : "GPU",
			clksrc, ret);

	GPUFREQ_TRACE_END();

	return ret;
}

/*
 * API: calculate pcw for setting CON1
 * Fin is 26 MHz
 * VCO Frequency = Fin * N_INFO
 * MFGPLL output Frequency = VCO Frequency / POSDIV
 * N_INFO = MFGPLL output Frequency * POSDIV / FIN
 * N_INFO[21:14] = FLOOR(N_INFO, 8)
 */
static unsigned int __gpufreq_calculate_pcw(unsigned int freq, enum gpufreq_posdiv posdiv)
{
	/*
	 * MFGPLL VCO range: 1.5GHz - 3.8GHz by divider 1/2/4/8/16,
	 * MFGPLL range: 125MHz - 3.8GHz,
	 * | VCO MAX | VCO MIN | POSDIV | PLL OUT MAX | PLL OUT MIN |
	 * |  3800   |  1500   |    1   |   3800MHz   |   1500MHz   |
	 * |  3800   |  1500   |    2   |   1900MHz   |    750MHz   |
	 * |  3800   |  1500   |    4   |    950MHz   |    375MHz   |
	 * |  3800   |  1500   |    8   |    475MHz   |  187.5MHz   |
	 * |  3800   |  2000   |   16   |  237.5MHz   |    125MHz   |
	 */
	unsigned long long pcw = 0;

	if ((freq >= POSDIV_16_MAX_FREQ) && (freq <= POSDIV_2_MAX_FREQ))
		pcw = (((unsigned long long)freq * (1 << posdiv)) << DDS_SHIFT) / MFGPLL_FIN / 1000;
	else
		__gpufreq_abort("out of range Freq: %d", freq);

	GPUFREQ_LOGD("target freq: %d, posdiv: %d, pcw: 0x%llx", freq, posdiv, pcw);

	return (unsigned int)pcw;
}

static enum gpufreq_posdiv __gpufreq_get_real_posdiv_gpu(void)
{
	u32 con1 = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	con1 = DRV_Reg32(MFG_PLL_CON1);

	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_real_posdiv_stack(void)
{
	u32 con1 = 0;
	enum gpufreq_posdiv posdiv = POSDIV_POWER_1;

	con1 = DRV_Reg32(MFGSC_PLL_CON1);

	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	return posdiv;
}

static enum gpufreq_posdiv __gpufreq_get_posdiv_by_freq(unsigned int freq)
{
	if (freq > POSDIV_4_MAX_FREQ)
		return POSDIV_POWER_2;
	else if (freq > POSDIV_8_MAX_FREQ)
		return POSDIV_POWER_4;
	else if (freq > POSDIV_16_MAX_FREQ)
		return POSDIV_POWER_8;
	else if (freq >= POSDIV_16_MIN_FREQ)
		return POSDIV_POWER_16;
	else {
		__gpufreq_abort("invalid freq: %d", freq);
		return POSDIV_POWER_16;
	}
}

/* API: scale Freq of GPU via CON1 Reg or FHCTL */
static int __gpufreq_freq_scale_gpu(unsigned int freq_old, unsigned int freq_new)
{
	enum gpufreq_posdiv cur_posdiv = POSDIV_POWER_1;
	enum gpufreq_posdiv target_posdiv = POSDIV_POWER_1;
	unsigned int pcw = 0;
	unsigned int pll = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d", freq_old, freq_new);

	GPUFREQ_LOGD("begin to scale Fgpu: (%d->%d)", freq_old, freq_new);

	if (freq_new == freq_old)
		goto done;

	/*
	 * MFG_PLL_CON1[31:31]: MFGPLL_SDM_PCW_CHG
	 * MFG_PLL_CON1[26:24]: MFGPLL_POSDIV
	 * MFG_PLL_CON1[21:0] : MFGPLL_SDM_PCW (DDS)
	 */
	cur_posdiv = __gpufreq_get_real_posdiv_gpu();
	target_posdiv = __gpufreq_get_posdiv_by_freq(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (unlikely(!pcw)) {
		__gpufreq_abort("invalid PCW: 0x%x", pcw);
		goto done;
	}

#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING))
	if (unlikely(!mtk_fh_set_rate)) {
		__gpufreq_abort("null hopping fp");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	/* POSDIV remain the same */
	if (target_posdiv == cur_posdiv) {
		/* change PCW by hopping only */
		ret = mtk_fh_set_rate(MFG_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	/* freq scale up */
	} else if (freq_new > freq_old) {
		/* 1. change PCW by hopping */
		ret = mtk_fh_set_rate(MFG_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
		/* 2. compute CON1 with target POSDIV */
		pll = (DRV_Reg32(MFG_PLL_CON1) & ~GENMASK(26, 24))
			| (target_posdiv << POSDIV_SHIFT);
		/* 3. change POSDIV by writing CON1 */
		DRV_WriteReg32(MFG_PLL_CON1, pll);
		/* 4. wait until PLL stable */
		udelay(20);
	/* freq scale down */
	} else {
		/* 1. compute CON1 with target POSDIV */
		pll = (DRV_Reg32(MFG_PLL_CON1) & ~GENMASK(26, 24))
			| (target_posdiv << POSDIV_SHIFT);
		/* 2. change POSDIV by writing CON1 */
		DRV_WriteReg32(MFG_PLL_CON1, pll);
		/* 3. wait until PLL stable */
		udelay(20);
		/* 4. change PCW by hopping */
		ret = mtk_fh_set_rate(MFG_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	}
#else
	/* 1. switch to parking clk source */
	ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_SUB);
	if (unlikely(ret))
		goto done;
	/* 2. compute CON1 with PCW and POSDIV */
	pll = BIT(31) | (target_posdiv << POSDIV_SHIFT) | pcw;
	/* 3. change PCW and POSDIV by writing CON1 */
	DRV_WriteReg32(MFG_PLL_CON1, pll);
	/* 4. wait until PLL stable */
	udelay(20);
	/* 5. switch to main clk source */
	ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_MAIN);
	if (unlikely(ret))
		goto done;
#endif /* GPUFREQ_FHCTL_ENABLE && CONFIG_COMMON_CLK_MTK_FREQ_HOPPING */

	g_gpu.cur_freq = __gpufreq_get_real_fgpu();

#ifdef GPUFREQ_HISTORY_ENABLE
	__gpufreq_record_history_entry(HISTORY_CHANGE_FREQ_TOP);
#endif /* GPUFREQ_HISTORY_ENABLE */

	if (unlikely(g_gpu.cur_freq != freq_new))
		__gpufreq_abort("inconsistent cur_freq: %d, target_freq: %d",
			g_gpu.cur_freq, freq_new);

	GPUFREQ_LOGD("Fgpu: %d, PCW: 0x%x, CON1: 0x%08x", g_gpu.cur_freq, pcw, pll);

	/* because return value is different across the APIs */
	ret = GPUFREQ_SUCCESS;

	/* notify gpu freq change to DDK  */
	mtk_notify_gpu_freq_change(0, freq_new);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: scale Freq of STACK via CON1 Reg or FHCTL */
static int __gpufreq_freq_scale_stack(unsigned int freq_old, unsigned int freq_new)
{
	enum gpufreq_posdiv cur_posdiv = POSDIV_POWER_1;
	enum gpufreq_posdiv target_posdiv = POSDIV_POWER_1;
	unsigned int pcw = 0;
	unsigned int pll = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("freq_old=%d, freq_new=%d", freq_old, freq_new);

	GPUFREQ_LOGD("begin to scale Fstack: (%d->%d)", freq_old, freq_new);

	if (freq_new == freq_old)
		goto done;

	/*
	 * MFGSC_PLL_CON1[31:31]: MFGPLL_SDM_PCW_CHG
	 * MFGSC_PLL_CON1[26:24]: MFGPLL_POSDIV
	 * MFGSC_PLL_CON1[21:0] : MFGPLL_SDM_PCW (DDS)
	 */
	cur_posdiv = __gpufreq_get_real_posdiv_stack();
	target_posdiv = __gpufreq_get_posdiv_by_freq(freq_new);
	/* compute PCW based on target Freq */
	pcw = __gpufreq_calculate_pcw(freq_new, target_posdiv);
	if (unlikely(!pcw)) {
		__gpufreq_abort("invalid PCW: 0x%x", pcw);
		goto done;
	}

#if (GPUFREQ_FHCTL_ENABLE && IS_ENABLED(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING))
	if (unlikely(!mtk_fh_set_rate)) {
		__gpufreq_abort("null hopping fp");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	/* POSDIV remain the same */
	if (target_posdiv == cur_posdiv) {
		/* change PCW by hopping only */
		ret = mtk_fh_set_rate(MFGSC_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	/* freq scale up */
	} else if (freq_new > freq_old) {
		/* 1. change PCW by hopping */
		ret = mtk_fh_set_rate(MFGSC_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
		/* 2. compute CON1 with target POSDIV */
		pll = (DRV_Reg32(MFGSC_PLL_CON1) & ~GENMASK(26, 24))
			| (target_posdiv << POSDIV_SHIFT);
		/* 3. change POSDIV by writing CON1 */
		DRV_WriteReg32(MFGSC_PLL_CON1, pll);
		/* 4. wait until PLL stable */
		udelay(20);
	/* freq scale down */
	} else {
		/* 1. compute CON1 with target POSDIV */
		pll = (DRV_Reg32(MFGSC_PLL_CON1) & ~GENMASK(26, 24))
			| (target_posdiv << POSDIV_SHIFT);
		/* 2. change POSDIV by writing CON1 */
		DRV_WriteReg32(MFGSC_PLL_CON1, pll);
		/* 3. wait until PLL stable */
		udelay(20);
		/* 4. change PCW by hopping */
		ret = mtk_fh_set_rate(MFGSC_PLL_NAME, pcw, target_posdiv);
		if (unlikely(!ret)) {
			__gpufreq_abort("fail to hopping PCW: 0x%x (%d)", pcw, ret);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
	}
#else
	/* 1. switch to parking clk source */
	ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_SUB);
	if (unlikely(ret))
		goto done;
	/* 2. compute CON1 with PCW and POSDIV */
	pll = BIT(31) | (target_posdiv << POSDIV_SHIFT) | pcw;
	/* 3. change PCW and POSDIV by writing CON1 */
	DRV_WriteReg32(MFGSC_PLL_CON1, pll);
	/* 4. wait until PLL stable */
	udelay(20);
	/* 5. switch to main clk source */
	ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_MAIN);
	if (unlikely(ret))
		goto done;
#endif /* GPUFREQ_FHCTL_ENABLE && CONFIG_COMMON_CLK_MTK_FREQ_HOPPING */

	g_stack.cur_freq = __gpufreq_get_real_fstack();

#ifdef GPUFREQ_HISTORY_ENABLE
	__gpufreq_record_history_entry(HISTORY_CHANGE_FREQ_STACK);
#endif /* GPUFREQ_HISTORY_ENABLE */

	if (unlikely(g_stack.cur_freq != freq_new))
		__gpufreq_abort("inconsistent cur_freq: %d, target_freq: %d",
			g_stack.cur_freq, freq_new);

	GPUFREQ_LOGD("Fstack: %d, PCW: 0x%x, CON1: 0x%08x", g_stack.cur_freq, pcw, pll);

	/* because return value is different across the APIs */
	ret = GPUFREQ_SUCCESS;

	/* notify stack freq change to DDK */
	mtk_notify_gpu_freq_change(1, freq_new);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static unsigned int __gpufreq_settle_time_vgpu(enum gpufreq_opp_direct direct, int deltaV)
{
	/*
	 * [MT6368_VBUCK2][VGPUSTACK]
	 * DVFS Rising : (deltaV / 12.5(mV)) + 3.85us + 2us
	 * DVFS Falling: (deltaV / 12.5(mV)) + 3.85us + 2us
	 * deltaV = mV x 100
	 */
	unsigned int t_settle = 0;

	if (direct == SCALE_UP) {
		/* rising */
		t_settle = (deltaV / 1250) + 4 + 2;
	} else if (direct == SCALE_DOWN) {
		/* falling */
		t_settle = (deltaV / 1250) + 4 + 2;
	}

	return t_settle; /* us */
}

static unsigned int __gpufreq_settle_time_vsram(enum gpufreq_opp_direct direct, int deltaV)
{
	/*
	 * [MT6363_VSRAM_MDFE][VSRAM]
	 * DVFS Rising : (deltaV / 12.5(mV)) + 5us
	 * DVFS Falling: (deltaV / 12.5(mV)) + 5us
	 * deltaV = mV x 100
	 */
	unsigned int t_settle = 0;

	if (direct == SCALE_UP) {
		/* rising */
		t_settle = (deltaV / 1250) + 5;
	} else if (direct == SCALE_DOWN) {
		/* falling */
		t_settle = (deltaV / 1250) + 5;
	}

	return t_settle; /* us */
}

/* API: scale Volt of GPU via Regulator */
static int __gpufreq_volt_scale_gpu(
	unsigned int vgpu_old, unsigned int vgpu_new,
	unsigned int vsram_old, unsigned int vsram_new)
{
	int ret = GPUFREQ_SUCCESS;
	unsigned int t_settle = 0, vgpu_target = 0, del_sel_volt = 0;

	del_sel_volt = g_gpu.signed_table[SRAM_DEL_SEL_OPP].volt;

	GPUFREQ_TRACE_START("vgpu_old=%d, vgpu_new=%d, vsram_old=%d, vsram_new=%d",
		vgpu_old, vgpu_new, vsram_old, vsram_new);

	GPUFREQ_LOGD("begin to scale Vgpu: (%d->%d)", vgpu_old, vgpu_new);
	if (vgpu_new > vgpu_old) {
		/* VGPU to del_sel volt */
		if ((vgpu_old < del_sel_volt) && (vgpu_new >= del_sel_volt)) {
			vgpu_target = del_sel_volt;
			t_settle = __gpufreq_settle_time_vgpu(SCALE_UP, (vgpu_target - vgpu_old));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				vgpu_target * 10, VGPU_MAX_VOLT * 10 + 125);

		#ifdef CFG_GPU_HISTORY_SUPPORT
			__gpufreq_set_parking_vtop(vgpu_target);
			__gpufreq_record_history_entry(HISTORY_VOLT_PARK);
		#endif /* CFG_GPU_HISTORY_SUPPORT */

			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU: %d (%d)",
					vgpu_target, ret);
				goto done;
			}
			udelay(t_settle);
			vgpu_old = del_sel_volt;

			DRV_WriteReg32(MFG_SRAM_FUL_SEL_ULV,
				(DRV_Reg32(MFG_SRAM_FUL_SEL_ULV) & ~BIT(0)));
		#ifdef CFG_GPU_HISTORY_SUPPORT
			__gpufreq_set_parking_vtop(vgpu_target);
			__gpufreq_set_delsel_bit(DRV_Reg32(MFG_SRAM_FUL_SEL_ULV) & BIT(0));
			__gpufreq_record_history_entry(HISTORY_VOLT_PARK);
		#endif /* CFG_GPU_HISTORY_SUPPORT */
		}
		/* VGPU to park */
		if ((vsram_new > VSRAM_THRESH) && (vgpu_old + VSRAM_VLOGIC_DIFF < vgpu_new)) {
			vgpu_target = VSRAM_THRESH;
			t_settle = __gpufreq_settle_time_vgpu(SCALE_UP, (vgpu_target - vgpu_old));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				vgpu_target * 10, VGPU_MAX_VOLT * 10 + 125);

		#ifdef CFG_GPU_HISTORY_SUPPORT
			__gpufreq_set_parking_vtop(vgpu_target);
			__gpufreq_record_history_entry(HISTORY_VOLT_PARK);
		#endif /* CFG_GPU_HISTORY_SUPPORT */

			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU: %d (%d)",
					vgpu_target, ret);
				goto done;
			}
			udelay(t_settle);
			vgpu_old = vgpu_target;
		}
		/* Vsram scaling*/
		if (vsram_new != vsram_old) {
			t_settle = __gpufreq_settle_time_vsram(SCALE_UP, (vsram_new - vsram_old));
			ret = regulator_set_voltage(g_pmic->reg_vsram,
				vsram_new * 10, VSRAM_MAX_VOLT * 10 + 125);

		#if GPUFREQ_HISTORY_ENABLE
			__gpufreq_set_parking_vsram(vsram_new);
			__gpufreq_record_history_entry(HISTORY_VSRAM_PARK);
		#endif /* GPUFREQ_HISTORY_ENABLE */

			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VSRAM: %d (%d)",
					vsram_new, ret);
				goto done;
			}
			udelay(t_settle);
		}
		/* VGPU to new */
		if (vgpu_old != vgpu_new) {
			t_settle =  __gpufreq_settle_time_vgpu(SCALE_UP, (vgpu_new - vgpu_old));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
					vgpu_new * 10, VGPU_MAX_VOLT * 10 + 125);

		#if GPUFREQ_HISTORY_ENABLE
			__gpufreq_set_parking_vtop(vgpu_new);
			__gpufreq_record_history_entry(HISTORY_CHANGE_VOLT_TOP);
		#endif /* GPUFREQ_HISTORY_ENABLE */

			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU: %d (%d)",
					vgpu_new, ret);
				goto done;
			}
			udelay(t_settle);
		}
	/* volt scaling down */
	} else if (vgpu_new < vgpu_old) {
		/* VGPU to park */
		if ((vsram_old > VSRAM_THRESH) && (vgpu_old - vgpu_new > VSRAM_VLOGIC_DIFF)) {
			vgpu_target = VSRAM_THRESH;
			t_settle = __gpufreq_settle_time_vgpu(SCALE_DOWN, (vgpu_old - vgpu_target));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				vgpu_target * 10, VGPU_MAX_VOLT * 10 + 125);

		#ifdef CFG_GPU_HISTORY_SUPPORT
			__gpufreq_set_parking_vtop(vgpu_target);
			__gpufreq_record_history_entry(HISTORY_VOLT_PARK);
		#endif /* CFG_GPU_HISTORY_SUPPORT */

			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU: %d (%d)", vgpu_target, ret);
				goto done;
			}
			udelay(t_settle);
			vgpu_old = vgpu_target;
		}
		/* Vsram scaling*/
		if (vsram_new != vsram_old) {
			t_settle = __gpufreq_settle_time_vsram(SCALE_DOWN, (vsram_old - vsram_new));
			ret = regulator_set_voltage(g_pmic->reg_vsram,
				vsram_new * 10, VSRAM_MAX_VOLT * 10 + 125);

		#ifdef CFG_GPU_HISTORY_SUPPORT
			__gpufreq_set_parking_vsram(vsram_new);
			__gpufreq_record_history_entry(HISTORY_VSRAM_PARK);
		#endif /* CFG_GPU_HISTORY_SUPPORT */

			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VSRAM: %d (%d)",
					vsram_new, ret);
				goto done;
			}
			udelay(t_settle);
		}
		/* VGPU to del_sel volt */
		if ((vgpu_old >= del_sel_volt) && (vgpu_new < del_sel_volt)) {
			vgpu_target = del_sel_volt;
			t_settle = __gpufreq_settle_time_vgpu(SCALE_DOWN, (vgpu_old - vgpu_target));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				vgpu_target * 10, VGPU_MAX_VOLT * 10 + 125);

		#ifdef CFG_GPU_HISTORY_SUPPORT
			__gpufreq_set_parking_vtop(vgpu_target);
			__gpufreq_record_history_entry(HISTORY_VOLT_PARK);
		#endif /* CFG_GPU_HISTORY_SUPPORT */

			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU: %d (%d)",
					vgpu_target, ret);
				goto done;
			}
			udelay(t_settle);
			vgpu_old = del_sel_volt;

			DRV_WriteReg32(MFG_SRAM_FUL_SEL_ULV,
				(DRV_Reg32(MFG_SRAM_FUL_SEL_ULV) | BIT(0)));

		#ifdef CFG_GPU_HISTORY_SUPPORT
			__gpufreq_set_parking_vtop(vgpu_target);
			__gpufreq_set_delsel_bit(DRV_Reg32(MFG_SRAM_FUL_SEL_ULV) & BIT(0));
			__gpufreq_record_history_entry(HISTORY_VOLT_PARK);
		#endif /* CFG_GPU_HISTORY_SUPPORT */
		}
		/* Vgpu scaling */
		if (vgpu_old != vgpu_new) {
			t_settle = __gpufreq_settle_time_vgpu(SCALE_DOWN, (vgpu_old - vgpu_new));
			ret = regulator_set_voltage(g_pmic->reg_vgpu,
				vgpu_new * 10, VGPU_MAX_VOLT * 10 + 125);

		#if GPUFREQ_HISTORY_ENABLE
			__gpufreq_set_parking_vtop(vgpu_new);
			__gpufreq_record_history_entry(HISTORY_CHANGE_VOLT_TOP);
		#endif /* GPUFREQ_HISTORY_ENABLE */

			if (unlikely(ret)) {
				__gpufreq_abort("fail to set regulator VGPU: %d (%d)",
					vgpu_new, ret);
				goto done;
			}
			udelay(t_settle);
		}
	/* keep volt */
	} else {
		ret = GPUFREQ_SUCCESS;
	}

	if (vgpu_new == del_sel_volt)
		DRV_WriteReg32(MFG_SRAM_FUL_SEL_ULV,
			(DRV_Reg32(MFG_SRAM_FUL_SEL_ULV) & ~BIT(0)));

	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	if (unlikely(g_gpu.cur_volt != vgpu_new))
		__gpufreq_abort("inconsistent scaled Vgpu, cur_volt: %d, target_volt: %d",
			g_gpu.cur_volt, vgpu_new);

	g_gpu.cur_vsram = __gpufreq_get_real_vsram();
	if (unlikely(g_gpu.cur_vsram != vsram_new))
		__gpufreq_abort("inconsistent scaled Vsram, cur_volt: %d, target_volt: %d",
			g_gpu.cur_vsram, vsram_new);

	GPUFREQ_LOGD("Vgpu: %d, Vsram: %d, udelay: %d",
		g_gpu.cur_volt, g_gpu.cur_vsram, t_settle);

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static void __gpufreq_dump_bringup_status(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct resource *res = NULL;

	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		return;
	}

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (unlikely(!g_mali_base)) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		return;
	}

	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		return;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_top_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		return;
	}

	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		return;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		return;
	}

	/* 0x13FA0C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfgsc_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFGSC_PLL");
		return;
	}
	g_mfgsc_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfgsc_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFGSC_PLL: 0x%llx", res->start);
		return;
	}

	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		return;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		return;
	}

	/* 0x1C001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		return;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sleep)) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		return;
	}

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "topckgen");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource TOPCKGEN");
		return;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_topckgen_base)) {
		GPUFREQ_LOGE("fail to ioremap TOPCKGEN: 0x%llx", res->start);
		return;
	}

	GPUFREQ_LOGI("[SPM] SPM_SPM2GPUPM_CON: 0x%08x", DRV_Reg32(SPM_SPM2GPUPM_CON));

	GPUFREQ_LOGI("[RPC] MFG_0_14_PWR_STATUS: 0x%08lx, MFG_RPC_MFG1_PWR_CON: 0x%08x",
		MFG_0_14_PWR_STATUS,
		DRV_Reg32(MFG_RPC_MFG1_PWR_CON));

	GPUFREQ_LOGI("[TOP] CON0: 0x%08x, CON1: %d, FMETER: %d, SEL: 0x%08lx, REF_SEL: 0x%08lx",
		DRV_Reg32(MFG_PLL_CON0),
		__gpufreq_get_real_fgpu(),
		__gpufreq_get_fmeter_main_fgpu(),
		DRV_Reg32(TOPCK_CLK_CFG_30) & MFG_SEL_MFGPLL_MASK,
		DRV_Reg32(TOPCK_CLK_CFG_3) & MFG_REF_SEL_MASK);

	GPUFREQ_LOGI("[STK] CON0: 0x%08x, CON1: %d, FMETER: %d, SEL: 0x%08lx, REF_SEL: 0x%08lx",
		DRV_Reg32(MFGSC_PLL_CON0),
		__gpufreq_get_real_fstack(),
		__gpufreq_get_fmeter_main_fstack(),
		DRV_Reg32(TOPCK_CLK_CFG_30) & MFGSC_SEL_MFGPSCLL_MASK,
		DRV_Reg32(TOPCK_CLK_CFG_3) & MFGSC_REF_SEL_MASK);

	GPUFREQ_LOGI("[GPU] MALI_GPU_ID: 0x%08x", DRV_Reg32(MALI_GPU_ID));
}

static unsigned int __gpufreq_get_fmeter_freq(enum gpufreq_target target)
{
	u32 mux_src = 0;
	unsigned int freq = 0;

	if (target == TARGET_STACK) {
		/* CLK_CFG_30 0x100001F0 [17] mfgsc_sel_mfgscpll */
		mux_src = DRV_Reg32(TOPCK_CLK_CFG_30) & MFGSC_SEL_MFGPSCLL_MASK;

		if (mux_src == MFGSC_SEL_MFGPSCLL_MASK)
			freq = __gpufreq_get_fmeter_main_fstack();
		else if (mux_src == 0x0)
			freq = __gpufreq_get_fmeter_sub_fstack();
	} else {
		/* CLK_CFG_30 0x100001F0 [16] mfg_sel_mfgpll */
		mux_src = DRV_Reg32(TOPCK_CLK_CFG_30) & MFG_SEL_MFGPLL_MASK;

		if (mux_src == MFG_SEL_MFGPLL_MASK)
			freq = __gpufreq_get_fmeter_main_fgpu();
		else if (mux_src == 0x0)
			freq = __gpufreq_get_fmeter_sub_fgpu();
	}

	return freq;
}

static unsigned int __gpufreq_get_fmeter_main_fgpu(void)
{
	u32 val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFG_PLL_CON0);
	DRV_WriteReg32(MFG_PLL_CON0, (val | BIT(12)));

	DRV_WriteReg32(MFG_PLL_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFG_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFG_PLL_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFG_PLL_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFG_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFG_PLL_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFG_PLL_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFGPLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFG_PLL_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static unsigned int __gpufreq_get_fmeter_main_fstack(void)
{
	u32 val = 0, ckgen_load_cnt = 0, ckgen_k1 = 0;
	int i = 0;
	unsigned int freq = 0;

	/* Enable clock PLL_TST_CK */
	val = DRV_Reg32(MFGSC_PLL_CON0);
	DRV_WriteReg32(MFGSC_PLL_CON0, (val | BIT(12)));

	DRV_WriteReg32(MFGSC_PLL_FQMTR_CON1, GENMASK(23, 16));
	val = DRV_Reg32(MFGSC_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFGSC_PLL_FQMTR_CON0, (val & GENMASK(23, 0)));
	/* Enable fmeter & select measure clock PLL_TST_CK */
	DRV_WriteReg32(MFGSC_PLL_FQMTR_CON0, (BIT(12) | BIT(15)));

	ckgen_load_cnt = DRV_Reg32(MFGSC_PLL_FQMTR_CON1) >> 16;
	ckgen_k1 = DRV_Reg32(MFGSC_PLL_FQMTR_CON0) >> 24;

	val = DRV_Reg32(MFGSC_PLL_FQMTR_CON0);
	DRV_WriteReg32(MFGSC_PLL_FQMTR_CON0, (val | BIT(4) | BIT(12)));

	/* wait fmeter finish */
	while (DRV_Reg32(MFGSC_PLL_FQMTR_CON0) & BIT(4)) {
		udelay(10);
		i++;
		if (i > 1000) {
			GPUFREQ_LOGE("wait MFGSCPLL Fmeter timeout");
			break;
		}
	}

	val = DRV_Reg32(MFGSC_PLL_FQMTR_CON1) & GENMASK(15, 0);
	/* KHz */
	freq = (val * 26000 * (ckgen_k1 + 1)) / (ckgen_load_cnt + 1);

	return freq;
}

static unsigned int __gpufreq_get_fmeter_sub_fgpu(void)
{
	unsigned int freq = 0;

	/* parking clock use CCF API directly */
	freq = clk_get_rate(g_clk->clk_sub_parent) / 1000; /* Hz */

	return freq;
}

static unsigned int __gpufreq_get_fmeter_sub_fstack(void)
{
	unsigned int freq = 0;

	/* parking clock use CCF API directly */
	freq = clk_get_rate(g_clk->clk_sc_sub_parent) / 1000; /* Hz */

	return freq;
}

/*
 * API: get real current frequency from CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_real_fgpu(void)
{
	u32 con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFG_PLL_CON1);

	pcw = con1 & GENMASK(21, 0);

	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

/*
 * API: get real current frequency from CON1 (khz)
 * Freq = ((PLL_CON1[21:0] * 26M) / 2^14) / 2^PLL_CON1[26:24]
 */
static unsigned int __gpufreq_get_real_fstack(void)
{
	u32 con1 = 0;
	unsigned int posdiv = 0;
	unsigned long long freq = 0, pcw = 0;

	con1 = DRV_Reg32(MFGSC_PLL_CON1);

	pcw = con1 & GENMASK(21, 0);

	posdiv = (con1 & GENMASK(26, 24)) >> POSDIV_SHIFT;

	freq = (((pcw * 1000) * MFGPLL_FIN) >> DDS_SHIFT) / (1 << posdiv);

	return FREQ_ROUNDUP_TO_10((unsigned int)freq);
}

/* API: get real current Vgpu from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vgpu(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vgpu))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vgpu) / 10;

	return volt;
}

/* API: get real current Vsram from regulator (mV * 100) */
static unsigned int __gpufreq_get_real_vsram(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vsram))
		/* regulator_get_voltage return volt with uV */
		volt = regulator_get_voltage(g_pmic->reg_vsram) / 10;

	return volt;
}

static unsigned int __gpufreq_get_vsram_by_vlogic(unsigned int volt)
{
	unsigned int vsram = 0;

	if (volt <= VSRAM_THRESH)
		vsram = VSRAM_THRESH;
	else
		vsram = volt;

	return vsram;
}

/* AOC2.0: set AOC ISO/LATCH before SRAM power off to prevent leakage and SRAM shutdown */
static void __gpufreq_iso_latch_config(enum gpufreq_power_state power)
{
	/* power on: clear AOCISO -> clear AOCLHENB */
	if (power == GPU_PWR_ON) {
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001FAC [8] VGPU_EXT_BUCK_ISO */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_CLR, BIT(8));
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001FAC [9] AOC_VGPU_SRAM_ISO_DIN */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_CLR, BIT(9));
		udelay(1);
		/* SPM_SOC_BUCK_ISO_CON_CLR 0x1C001FAC [10] AOC_VGPU_SRAM_LATCH_ENB */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_CLR, BIT(10));
	/* power off: set AOCLHENB -> set AOCISO */
	} else {
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001FA8 [10] AOC_VGPU_SRAM_LATCH_ENB */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_SET, BIT(10));
		udelay(1);
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001FA8 [9] AOC_VGPU_SRAM_ISO_DIN */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_SET, BIT(9));
		/* SPM_SOC_BUCK_ISO_CON_SET 0x1C001FA8 [8] VGPU_EXT_BUCK_ISO */
		DRV_WriteReg32(SPM_SOC_BUCK_ISO_CON_SET, BIT(8));
	}

	GPUFREQ_LOGD("power: %d, SPM_SOC_BUCK_ISO_CON: 0x%08x",
		power, DRV_Reg32(SPM_SOC_BUCK_ISO_CON));
}

/* HWDCM: mask clock when GPU idle (dynamic clock mask) */
static void __gpufreq_top_hwdcm_config(enum gpufreq_power_state power)
{
#if GPUFREQ_HWDCM_ENABLE
	u32 val = 0;

	if (power == GPU_PWR_ON) {
		/* (A) MFG_GLOBAL_CON 0x13FBF0B0 [8]  GPU_SOCIF_MST_FREE_RUN = 1'b0 */
		val = DRV_Reg32(MFG_GLOBAL_CON) & ~BIT(8);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (B) MFG_ASYNC_CON 0x13FBF020 [23] MEM0_SLV_CG_ENABLE = 1'b1 */
		val = DRV_Reg32(MFG_ASYNC_CON) | BIT(23);
		DRV_WriteReg32(MFG_ASYNC_CON, val);

		/* MFG_ASYNC_CON 0x13FBF020 [25] MEM1_SLV_CG_ENABLE = 1'b1 */
		val = DRV_Reg32(MFG_ASYNC_CON) | BIT(25);
		DRV_WriteReg32(MFG_ASYNC_CON, val);

		/* MFG_ASYNC_CON3 0x13FBF02C [13] chip_mfg_axi0_1_out_idle_enable = 1'b1 */
		val = DRV_Reg32(MFG_ASYNC_CON3) | BIT(13);
		DRV_WriteReg32(MFG_ASYNC_CON3, val);

		/* MFG_ASYNC_CON3 0x13FBF02C [15] chip_mfg_axi1_1_out_idle_enable = 1'b1 */
		val = DRV_Reg32(MFG_ASYNC_CON3) | BIT(15);
		DRV_WriteReg32(MFG_ASYNC_CON3, val);

		/* (C) MFG_RPC_AO_CLK_CFG 0x13F91034 [0] CG_FAXI_CK_SOC_IN_FREE_RUN = 1'b0 */
		val = DRV_Reg32(MFG_RPC_AO_CLK_CFG) & ~BIT(0);
		DRV_WriteReg32(MFG_RPC_AO_CLK_CFG, val);

		/* (D) MFG_DCM_CON_0 0x13FBF010 [15]  BG3D_DCM_EN = 1'b1 */
		val = DRV_Reg32(MFG_DCM_CON_0) | BIT(15);
		DRV_WriteReg32(MFG_DCM_CON_0, val);

		/* MFG_DCM_CON_0 0x13FBF010 [6:0] BG3D_DBC_CNT = 7'b0111111 */
		val = (DRV_Reg32(MFG_DCM_CON_0) & ~BIT(6)) | GENMASK(5, 0);
		DRV_WriteReg32(MFG_DCM_CON_0, val);

		/* (E) MFG_GLOBAL_CON 0x13FBF0B0 [21] dvfs_hint_cg_en = 1'b0 */
		val = DRV_Reg32(MFG_GLOBAL_CON) & ~BIT(21);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (F) MFG_GLOBAL_CON 0x13FBF0B0 [10] GPU_CLK_FREE_RUN = 1'b0 */
		val = DRV_Reg32(MFG_GLOBAL_CON) & ~BIT(10);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (K) MFG_I2M_PROTECTOR_CFG_03 0x13FBFFA8 [2] = 1'b0 */
		val = DRV_Reg32(MFG_I2M_PROTECTOR_CFG_03) & ~BIT(2);
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_03, val);

		/* (L) MFG_I2M_PROTECTOR_CFG_01 0x13FBFF64 [27] = 1'b0 */
		val = DRV_Reg32(MFG_I2M_PROTECTOR_CFG_01) & ~BIT(27);
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_01, val);
	} else {
		/* (A) MFG_GLOBAL_CON 0x13FBF0B0 [8]  GPU_SOCIF_MST_FREE_RUN = 1'b1 */
		val = DRV_Reg32(MFG_GLOBAL_CON) | BIT(8);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (B) MFG_ASYNC_CON 0x13FBF020 [23] MEM0_SLV_CG_ENABLE = 1'b0 */
		val = DRV_Reg32(MFG_ASYNC_CON) & ~BIT(23);
		DRV_WriteReg32(MFG_ASYNC_CON, val);

		/* MFG_ASYNC_CON 0x13FBF020 [25] MEM1_SLV_CG_ENABLE = 1'b0 */
		val = DRV_Reg32(MFG_ASYNC_CON) & ~BIT(25);
		DRV_WriteReg32(MFG_ASYNC_CON, val);

		/* MFG_ASYNC_CON3 0x13FBF02C [13] chip_mfg_axi0_1_out_idle_enable = 1'b0 */
		val = DRV_Reg32(MFG_ASYNC_CON3) & ~BIT(13);
		DRV_WriteReg32(MFG_ASYNC_CON3, val);

		/* MFG_ASYNC_CON3 0x13FBF02C [15] chip_mfg_axi1_1_out_idle_enable = 1'b0 */
		val = DRV_Reg32(MFG_ASYNC_CON3) & ~BIT(15);
		DRV_WriteReg32(MFG_ASYNC_CON3, val);

		/* (C) MFG_RPC_AO_CLK_CFG 0x13F91034 [0] CG_FAXI_CK_SOC_IN_FREE_RUN = 1'b1 */
		val = DRV_Reg32(MFG_RPC_AO_CLK_CFG) | BIT(0);
		DRV_WriteReg32(MFG_RPC_AO_CLK_CFG, val);

		/* (D) MFG_DCM_CON_0 0x13FBF010 [15]  BG3D_DCM_EN = 1'b0 */
		val = DRV_Reg32(MFG_DCM_CON_0) & ~BIT(15);
		DRV_WriteReg32(MFG_DCM_CON_0, val);

		/* (E) MFG_GLOBAL_CON 0x13FBF0B0 [21] dvfs_hint_cg_en = 1'b1 */
		val = DRV_Reg32(MFG_GLOBAL_CON) | BIT(21);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (F) MFG_GLOBAL_CON 0x13FBF0B0 [10] GPU_CLK_FREE_RUN = 1'b1 */
		val = DRV_Reg32(MFG_GLOBAL_CON) | BIT(10);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (K) MFG_I2M_PROTECTOR_CFG_03 0x13FBFFA8 [2] = 1'b1 */
		val = DRV_Reg32(MFG_I2M_PROTECTOR_CFG_03) | BIT(2);
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_03, val);

		/* (L) MFG_I2M_PROTECTOR_CFG_01 0x13FBFF64 [27] = 1'b1 */
		val = DRV_Reg32(MFG_I2M_PROTECTOR_CFG_01) | BIT(27);
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_01, val);
	}

#endif /* GPUFREQ_HWDCM_ENABLE */
}

/* HWDCM: mask clock when STACK idle (dynamic clock mask) */
static void __gpufreq_stack_hwdcm_config(enum gpufreq_power_state power)
{
#if GPUFREQ_HWDCM_ENABLE
	u32 val = 0;

	if (power == GPU_PWR_ON) {
		/* (G) MFG_GLOBAL_CON 0x13FBF0B0 [24]  stack_hd_bg3d_cg_free_run = 1'b0 */
		val = DRV_Reg32(MFG_GLOBAL_CON) & ~BIT(24);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (H) MFG_GLOBAL_CON 0x13FBF0B0 [25]  stack_hd_bg3d_gpu_cg_free_run = 1'b0 */
		val = DRV_Reg32(MFG_GLOBAL_CON) & ~BIT(25);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (I) MFG_GLOBAL_CON 0x13FBF0B0 [30]  mfg_fll_stack_hd_bg3d_cg_free_run = 1'b1 */
		val = DRV_Reg32(MFG_GLOBAL_CON) | BIT(30);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (J) MFG_GLOBAL_CON 0x13FBF0B0 [31]  mfg_fll_stack_hd_bg3d_gpu_cg_free_run = 1'b1 */
		val = DRV_Reg32(MFG_GLOBAL_CON) | BIT(31);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);
	} else {
		/* (G) MFG_GLOBAL_CON 0x13FBF0B0 [24]  stack_hd_bg3d_cg_free_run = 1'b1 */
		val = DRV_Reg32(MFG_GLOBAL_CON) | BIT(24);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (H) MFG_GLOBAL_CON 0x13FBF0B0 [25]  stack_hd_bg3d_gpu_cg_free_run = 1'b1 */
		val = DRV_Reg32(MFG_GLOBAL_CON) | BIT(25);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (I) MFG_GLOBAL_CON 0x13FBF0B0 [30]  mfg_fll_stack_hd_bg3d_cg_free_run = 1'b0 */
		val = DRV_Reg32(MFG_GLOBAL_CON) & ~BIT(30);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);

		/* (J) MFG_GLOBAL_CON 0x13FBF0B0 [31]  mfg_fll_stack_hd_bg3d_gpu_cg_free_run = 1'b0 */
		val = DRV_Reg32(MFG_GLOBAL_CON) & ~BIT(31);
		DRV_WriteReg32(MFG_GLOBAL_CON, val);
	}

#endif /* GPUFREQ_HWDCM_ENABLE */
}

/* ACP: GPU can access CPU cache directly */
static void __gpufreq_acp_config(void)
{
#if GPUFREQ_ACP_ENABLE
	/* MFG_1TO2AXI_CON_00 0x13FBF8E0 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	DRV_WriteReg32(MFG_1TO2AXI_CON_00, 0x00FFC855);
	/* MFG_1TO2AXI_CON_02 0x13FBF8E8 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	DRV_WriteReg32(MFG_1TO2AXI_CON_02, 0x00FFC855);
	/* MFG_1TO2AXI_CON_04 0x13FBF910 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	DRV_WriteReg32(MFG_1TO2AXI_CON_04, 0x00FFC855);
	/* MFG_1TO2AXI_CON_06 0x13FBF918 [24:0] mfg_axi1to2_R_dispatch_mode = 0x855 */
	DRV_WriteReg32(MFG_1TO2AXI_CON_06, 0x00FFC855);
	/* MFG_OUT_1TO2AXI_CON_00 0x13FBF900 [24:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	DRV_WriteReg32(MFG_OUT_1TO2AXI_CON_00, 0x00FFC055);
	/* MFG_OUT_1TO2AXI_CON_02 0x13FBF908 [24:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	DRV_WriteReg32(MFG_OUT_1TO2AXI_CON_02, 0x00FFC055);
	/* MFG_OUT_1TO2AXI_CON_04 0x13FBF920 [24:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	DRV_WriteReg32(MFG_OUT_1TO2AXI_CON_04, 0x00FFC055);
	/* MFG_OUT_1TO2AXI_CON_06 0x13FBF928 [24:0] mfg_axi1to2_R_dispatch_mode = 0x055 */
	DRV_WriteReg32(MFG_OUT_1TO2AXI_CON_06, 0x00FFC055);

	/* ACP Enable */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [0] M0_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [1] M1_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [2] M2_coherence_enable = 1'b1 */
	/* MFG_AXCOHERENCE_CON 0x13FBF168 [3] M3_coherence_enable = 1'b1 */
	DRV_WriteReg32(MFG_AXCOHERENCE_CON, (DRV_Reg32(MFG_AXCOHERENCE_CON) | GENMASK(3, 0)));

	/* Secure register rule 2/3 */
	/* MFG_SECURE_REG 0x13FBCFE0 [31] acp_mpu_rule3_disable = 1'b1 */
	DRV_WriteReg32(MFG_SECURE_REG, (DRV_Reg32(MFG_SECURE_REG) | BIT(31)));
	/* NORM_ACP_FLT_CON 0x13FBCFE4 = 0x0000003F */
	DRV_WriteReg32(MFG_NORM_ACP_FLT_CON, 0x0000003F);
	/* SECU_ACP_FLT_CON 0x13FBCFE4 = 0x0303002A */
	DRV_WriteReg32(MFG_SECU_ACP_FLT_CON, 0x0303002A);

	/* ECO1 for axport[1] = 1 */
	DRV_WriteReg32(MFG_SECURE_REG, (DRV_Reg32(MFG_SECURE_REG) | BIT(12)));

	/* NTH_APU_ACP_GALS_SLV_CTRL  0x1021C600 [26:25] MFG_ACP_AR_MPAM_1_0 = 2'b11 */
	/* NTH_APU_EMI1_GALS_SLV_CTRL 0x1021C624 [26:25] MFG_ACP_AW_MPAM_1_0 = 2'b11 */
	DRV_WriteReg32(NTH_APU_ACP_GALS_SLV_CTRL,
				(DRV_Reg32(NTH_APU_ACP_GALS_SLV_CTRL) | GENMASK(26, 25)));
	DRV_WriteReg32(NTH_APU_EMI1_GALS_SLV_CTRL,
				(DRV_Reg32(NTH_APU_EMI1_GALS_SLV_CTRL) | GENMASK(26, 25)));
#endif /* GPUFREQ_ACP_ENABLE */
}

static void __gpufreq_ocl_timestamp_config(void)
{
	/* MFG_TIMESTAMP 0x13FBF130 [0] top_tsvalueb_en = 1'b1 */
	/* MFG_TIMESTAMP 0x13FBF130 [1] timer_sel = 1'b1 */
	DRV_WriteReg32(MFG_TIMESTAMP, GENMASK(1, 0));
}

/* GPM1.0: di/dt reduction by slowing down speed of frequency scaling up or down */
static void __gpufreq_gpm1_config(void)
{
#if GPUFREQ_GPM1_ENABLE
	if (g_gpm1_mode) {
		/* MFG_I2M_PROTECTOR_CFG_00 0x13FBFF60 = 0x20300316 */
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_00, 0x20300316);
		/* MFG_I2M_PROTECTOR_CFG_01 0x13FBFF64 = 0x1800000C */
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_01, 0x1800000C);
		/* MFG_I2M_PROTECTOR_CFG_02 0x13FBFF68 = 0x01010802 */
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_02, 0x01010802);
		/* MFG_I2M_PROTECTOR_CFG_03 0x13FBFFA8 = 0x00030FF3 */
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_03, 0x00030FF3);
		/* wait 1us */
		udelay(1);
		/* MFG_I2M_PROTECTOR_CFG_00 0x13FBFF60 = 0x20300317 */
		DRV_WriteReg32(MFG_I2M_PROTECTOR_CFG_00, 0x20300317);
	}
#endif /* GPUFREQ_GPM1_ENABLE */
}

/* Merge GPU transaction to maximize DRAM efficiency */
static void __gpufreq_transaction_config(void)
{
#if GPUFREQ_MERGER_ENABLE
	/* Merge AXI READ to window size 8T */
	DRV_WriteReg32(MFG_MERGE_R_CON_00, 0x0808FF81);
	DRV_WriteReg32(MFG_MERGE_R_CON_02, 0x0808FF81);
	DRV_WriteReg32(MFG_MERGE_R_CON_04, 0x0808FF81);
	DRV_WriteReg32(MFG_MERGE_R_CON_06, 0x0808FF81);

	/* Merge AXI WRITE to window size 64T */
	DRV_WriteReg32(MFG_MERGE_W_CON_00, 0x4040FF81);
	DRV_WriteReg32(MFG_MERGE_W_CON_02, 0x4040FF81);
	DRV_WriteReg32(MFG_MERGE_W_CON_04, 0x4040FF81);
	DRV_WriteReg32(MFG_MERGE_W_CON_06, 0x4040FF81);
#endif /* GPUFREQ_MERGER_ENABLE */
}

/* Set priority of AxUSER (CSF, MMU) to pre-ultra level */
static void __gpufreq_axuser_priority_config(void)
{
#if GPUFREQ_AXUSER_PREULTRA_ENABLE
	/* MFG_MALI_AXUSER_M0_CFG1 0x13FBF704 [2] CSF_rd = 1'b1 */
	/* MFG_MALI_AXUSER_M0_CFG1 0x13FBF704 [4] MMU_rd = 1'b1 */
	DRV_WriteReg32(MFG_MALI_AXUSER_M0_CFG1, 0x00000014);
	/* MFG_MALI_AXUSER_M0_CFG2 0x13FBF708 [2] CSF_wr = 1'b1 */
	DRV_WriteReg32(MFG_MALI_AXUSER_M0_CFG2, 0x00000004);
	/* Set AR/AW sideband pre-ultra */
	DRV_WriteReg32(MFG_MALI_AXUSER_M0_CFG3, 0x00400400);
#endif /* GPUFREQ_AXUSER_PREULTRA_ENABLE */
}

/* Set slc setting of AxUSER */
static void __gpufreq_axuser_slc_config(void)
{
#if GPUFREQ_AXUSER_SLC_ENABLE
	/* group4: set slc policy 1A_1 for PBHA6 */
	DRV_WriteReg32(MFG_MALI_AXUSER_SLC_CFG10, 0x113C0B01);
	DRV_WriteReg32(MFG_MALI_AXUSER_SLC_CFG11, 0x104C3F85);
	DRV_WriteReg32(MFG_MALI_AXUSER_SLC_CFG12, 0x06012006);

	/* group5: set slc policy 1A_2 for PBHA6 */
	DRV_WriteReg32(MFG_MALI_AXUSER_SLC_CFG13, 0x00020480);
	DRV_WriteReg32(MFG_MALI_AXUSER_SLC_CFG14, 0x00330000);
	DRV_WriteReg32(MFG_MALI_AXUSER_SLC_CFG15, 0x00000006);

	/* group6: set slc policy 1A_3 for PBHA6 */
	DRV_WriteReg32(MFG_MALI_AXUSER_SLC_CFG16, 0x00010014);
	DRV_WriteReg32(MFG_MALI_AXUSER_SLC_CFG17, 0x00000000);
	DRV_WriteReg32(MFG_MALI_AXUSER_SLC_CFG18, 0x0000E006);
#endif /* GPUFREQ_AXUSER_SLC_ENABLE */
}

static void __gpufreq_dfd_config(void)
{
#if GPUFREQ_DFD_ENABLE
	if (g_dfd_mode) {
		if (g_dfd_mode == DFD_FORCE_DUMP)
			DRV_WriteReg32(MFG_DEBUGMON_CON_00, MFG_DEBUGMON_CON_00_ENABLE);

		DRV_WriteReg32(MFG_DFD_CON_0,
			(DRV_Reg32(MFG_DFD_CON_0) & 0x0FDEFFEE) | MFG_DFD_CON_0_ENABLE);
		DRV_WriteReg32(MFG_DFD_CON_1, MFG_DFD_CON_1_ENABLE);
		DRV_WriteReg32(MFG_DFD_CON_3,
			(DRV_Reg32(MFG_DFD_CON_0) & 0xFFFF0063) | MFG_DFD_CON_3_ENABLE);
		DRV_WriteReg32(MFG_DFD_CON_4, MFG_DFD_CON_4_ENABLE);
		DRV_WriteReg32(MFG_DFD_CON_17, (DRV_Reg32(MFG_DFD_CON_0) & 0xEFFFFCCE));
		DRV_WriteReg32(MFG_DFD_CON_18, MFG_DFD_CON_18_ENABLE);
		DRV_WriteReg32(MFG_DFD_CON_19, MFG_DFD_CON_19_ENABLE);

		if ((DRV_Reg32(DRM_DEBUG_MFG_REG) & BIT(0)) != BIT(0)) {
			DRV_WriteReg32(DRM_DEBUG_MFG_REG, 0x77000000);
			udelay(10);
			DRV_WriteReg32(DRM_DEBUG_MFG_REG, 0x77000001);
		}
	}
#endif /* GPUFREQ_DFD_ENABLE */
}

static void __gpufreq_power_tracker_config(void)
{
#if GPUFREQ_POWER_TRACKER_ENABLE
	/* Enable CLK POWER_TRACKER_SETTING [0] = 1’b1 */
	DRV_WriteReg32(MFG_POWER_TRACKER_SETTING,
		DRV_Reg32(MFG_POWER_TRACKER_SETTING) | BIT(0));
#endif /* GPUFREQ_POWER_TRACKER_ENABLE */
}

static void __gpufreq_mfg_backup_restore(enum gpufreq_power_state power)
{
	/* restore */
	if (power == GPU_PWR_ON) {
		if (g_del_sel_reg)
			/* MFG_SRAM_FUL_SEL_ULV 0x13FBF080 [0] */
			DRV_WriteReg32(MFG_SRAM_FUL_SEL_ULV,
				(DRV_Reg32(MFG_SRAM_FUL_SEL_ULV) | BIT(0)));
	/* backup */
	} else {
		/* MFG_SRAM_FUL_SEL_ULV 0x13FBF080 [0] */
		g_del_sel_reg = DRV_Reg32(MFG_SRAM_FUL_SEL_ULV) & BIT(0);
	}

	#ifdef CFG_GPU_HISTORY_SUPPORT
		__gpufreq_set_delsel_bit(DRV_Reg32(MFG_SRAM_FUL_SEL_ULV) & BIT(0));
	#endif /* CFG_GPU_HISTORY_SUPPORT */
}

static int __gpufreq_clock_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == GPU_PWR_ON) {
		/* enable GPU MUX and GPU PLL */
		ret = clk_prepare_enable(g_clk->clk_mux);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable clk_mux (%d)", ret);
			goto done;
		}
		g_gpu.cg_count++;

		/* enable STACK MUX and STACK MUX */
		ret = clk_prepare_enable(g_clk->clk_sc_mux);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable clk_sc_mux (%d)", ret);
			goto done;
		}
		g_stack.cg_count++;

		/* switch GPU MUX to PLL */
		ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_MAIN);
		if (unlikely(ret))
			goto done;

		/* switch STACK MUX to PLL */
		ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_MAIN);
		if (unlikely(ret))
			goto done;
	} else {
		/* switch STACK MUX to REF_SEL */
		ret = __gpufreq_switch_clksrc(TARGET_STACK, CLOCK_SUB);
		if (unlikely(ret))
			goto done;

		/* switch GPU MUX to REF_SEL */
		ret = __gpufreq_switch_clksrc(TARGET_GPU, CLOCK_SUB);
		if (unlikely(ret))
			goto done;

		/* disable STACK MUX and STACK PLL */
		clk_disable_unprepare(g_clk->clk_sc_mux);
		g_stack.cg_count--;

		/* disable GPU MUX and GPU PLL */
		clk_disable_unprepare(g_clk->clk_mux);
		g_gpu.cg_count--;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

#if !GPUFREQ_PDCA_ENABLE
static void __gpufreq_mfgx_rpc_control(enum gpufreq_power_state power, void __iomem *pwr_con)
{
	int i = 0;

	if (power == GPU_PWR_ON) {
		/* MFGx_PWR_ON = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(2)));
		__gpufreq_footprint_power_step(0xB0);
		/* MFGx_PWR_ACK = 1'b1 */
		i = 0;
		while ((DRV_Reg32(pwr_con) & BIT(30)) != BIT(30)) {
			udelay(10);
			if (++i > 10) {
				__gpufreq_footprint_power_step(0xB1);
				break;
			}
		}
		/* MFGx_PWR_ON_2ND = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(3)));
		__gpufreq_footprint_power_step(0xB2);
		/* MFGx_PWR_ACK_2ND = 1'b1 */
		i = 0;
		while ((DRV_Reg32(pwr_con) & BIT(31)) != BIT(31)) {
			udelay(10);
			if (++i > 10) {
				__gpufreq_footprint_power_step(0xB3);
				break;
			}
		}
		/* MFGx_PWR_ACK = 1'b1 */
		/* MFGx_PWR_ACK_2ND = 1'b1 */
		i = 0;
		while ((DRV_Reg32(pwr_con) & GENMASK(31, 30)) != GENMASK(31, 30)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xB4);
				goto timeout;
			}
		}
		/* MFGx_PWR_CLK_DIS = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(4)));
		__gpufreq_footprint_power_step(0xB5);
		/* MFGx_PWR_ISO = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(1)));
		__gpufreq_footprint_power_step(0xB6);
		/* MFGx_PWR_RST_B = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(0)));
		__gpufreq_footprint_power_step(0xB7);
		/* MFGx_PWR_SRAM_PDN = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(8)));
		__gpufreq_footprint_power_step(0xB8);
		/* MFGx_PWR_SRAM_PDN_ACK = 1'b0 */
		i = 0;
		while (DRV_Reg32(pwr_con) & BIT(12)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xB9);
				goto timeout;
			}
		}
	} else {
		/* MFGx_PWR_SRAM_PDN = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(8)));
		__gpufreq_footprint_power_step(0xBA);
		/* MFGx_PWR_SRAM_PDN_ACK = 1'b1 */
		i = 0;
		while ((DRV_Reg32(pwr_con) & BIT(12)) != BIT(12)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xBB);
				goto timeout;
			}
		}
		/* MFGx_PWR_ISO = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(1)));
		__gpufreq_footprint_power_step(0xBC);
		/* MFGx_PWR_CLK_DIS = 1'b1 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) | BIT(4)));
		__gpufreq_footprint_power_step(0xBD);
		/* MFGx_PWR_RST_B = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(0)));
		__gpufreq_footprint_power_step(0xBE);
		/* MFGx_PWR_ON = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(2)));
		__gpufreq_footprint_power_step(0xBF);
		/* MFGx_PWR_ON_2ND = 1'b0 */
		DRV_WriteReg32(pwr_con, (DRV_Reg32(pwr_con) & ~BIT(3)));
		__gpufreq_footprint_power_step(0xC0);
		/* MFGx_PWR_ACK = 1'b0 */
		/* MFGx_PWR_ACK_2ND = 1'b0 */
		i = 0;
		while (DRV_Reg32(pwr_con) & GENMASK(31, 30)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xC1);
				goto timeout;
			}
		}
	}

	return;

timeout:
	GPUFREQ_LOGE("(0x13F91070)=0x%x, (0x13F910A0)=0x%x, (0x13F910A4)=0x%x, (0x13F910A8)=0x%x",
		DRV_Reg32(MFG_RPC_MFG1_PWR_CON), DRV_Reg32(MFG_RPC_MFG2_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG3_PWR_CON), DRV_Reg32(MFG_RPC_MFG4_PWR_CON));
	GPUFREQ_LOGE("(0x13F910B0)=0x%x, (0x13F910B4)=0x%x, (0x13F910BC)=0x%x, (0x13F910C0)=0x%x",
		DRV_Reg32(MFG_RPC_MFG6_PWR_CON), DRV_Reg32(MFG_RPC_MFG7_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG9_PWR_CON), DRV_Reg32(MFG_RPC_MFG10_PWR_CON));
	GPUFREQ_LOGE("(0x13F910C4)=0x%x, (0x13F910C8)=0x%x, (0x13F910CC)=0x%x, (0x13F910D0)=0x%x",
		DRV_Reg32(MFG_RPC_MFG11_PWR_CON), DRV_Reg32(MFG_RPC_MFG12_PWR_CON),
		DRV_Reg32(MFG_RPC_MFG13_PWR_CON), DRV_Reg32(MFG_RPC_MFG14_PWR_CON));
	__gpufreq_abort("timeout");
}
#endif /* GPUFREQ_PDCA_ENABLE */

static void __gpufreq_mfg1_rpc_control(enum gpufreq_power_state power)
{
	int i = 0;

	if (power == GPU_PWR_ON) {
		/* MFG1_PWR_CON 0x13F91070 [2] MFG1_PWR_ON = 1'b1 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) | BIT(2)));
		__gpufreq_footprint_power_step(0xD0);
		/* MFG1_PWR_CON 0x13F91070 [30] MFG1_PWR_ACK = 1'b1 */
		i = 0;
		while ((DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & BIT(30)) != BIT(30)) {
			udelay(10);
			if (++i > 10) {
				__gpufreq_footprint_power_step(0xD1);
				break;
			}
		}
		/* MFG1_PWR_CON 0x13F91070 [3] MFG1_PWR_ON_2ND = 1'b1 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) | BIT(3)));
		__gpufreq_footprint_power_step(0xD2);
		/* MFG1_PWR_CON 0x13F91070 [31] MFG1_PWR_ACK_2ND = 1'b1 */
		i = 0;
		while ((DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & BIT(31)) != BIT(31)) {
			udelay(10);
			if (++i > 10) {
				__gpufreq_footprint_power_step(0xD3);
				break;
			}
		}
		/* MFG1_PWR_CON 0x13F91070 [30] MFG1_PWR_ACK = 1'b1 */
		/* MFG1_PWR_CON 0x13F91070 [31] MFG1_PWR_ACK_2ND = 1'b1 */
		i = 0;
		while ((DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & GENMASK(31, 30)) != GENMASK(31, 30)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xD4);
				goto timeout;
			}
		}
		/* MFG1_PWR_CON 0x13F91070 [4] MFG1_PWR_CLK_DIS = 1'b0 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & ~BIT(4)));
		__gpufreq_footprint_power_step(0xD5);
		/* MFG1_PWR_CON 0x13F91070 [1] MFG1_PWR_ISO = 1'b0 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & ~BIT(1)));
		__gpufreq_footprint_power_step(0xD6);
		/* MFG1_PWR_CON 0x13F91070 [0] MFG1_PWR_RST_B = 1'b1 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) | BIT(0)));
		__gpufreq_footprint_power_step(0xD7);
		/* MFG1_PWR_CON 0x13F91070 [8] MFG1_PWR_SRAM_PDN = 1'b0 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & ~BIT(8)));
		__gpufreq_footprint_power_step(0xD8);
		/* MFG1_PWR_CON 0x13F91070 [12] MFG1_PWR_SRAM_PDN_ACK = 1'b0 */
		i = 0;
		while (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & BIT(12)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xD9);
				goto timeout;
			}
		}
		/* IFR_EMISYS_PROTECT_EN_W1C_0 0x1002C108 [20:19] = 2'b11 */
		DRV_WriteReg32(IFR_EMISYS_PROTECT_EN_W1C_0, GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xDA);
		/* IFR_EMISYS_PROTECT_EN_STA_0 0x1002C100 [20:19] = 2'b00 */
		i = 0;
		while ((DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_0) & GENMASK(20, 19))) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xDB);
				goto timeout;
			}
		}
		/* IFR_EMISYS_PROTECT_EN_W1C_1 0x1002C128 [20:19] = 2'b11 */
		DRV_WriteReg32(IFR_EMISYS_PROTECT_EN_W1C_1, GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xDC);
		/* IFR_EMISYS_PROTECT_EN_STA_1 0x1002C120 [20:19] = 2'b00 */
		i = 0;
		while ((DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_1) & GENMASK(20, 19))) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xDD);
				goto timeout;
			}
		}
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [19:16] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_CLR, GENMASK(19, 16));
		__gpufreq_footprint_power_step(0xDE);
		/* MFG_RPC_SLP_PROT_EN_STA 0x13F91048 [19:16] = 4'b0000 */
		i = 0;
		while (DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(19, 16)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xDF);
				goto timeout;
			}
		}
		/* MFG_RPC_SLP_PROT_EN_CLR 0x13F91044 [3:0] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_CLR, GENMASK(3, 0));
		__gpufreq_footprint_power_step(0xE0);
		/* MFG_RPC_SLP_PROT_EN_STA 0x13F91048 [3:0] = 4'b0000 */
		i = 0;
		while (DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(3, 0)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xE1);
				goto timeout;
			}
		}
	} else {
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [3:0] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_SET, GENMASK(3, 0));
		__gpufreq_footprint_power_step(0xE2);
		/* MFG_RPC_SLP_PROT_EN_STA 0x13F91048 [3:0] = 4'b1111 */
		i = 0;
		while ((DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(3, 0)) != GENMASK(3, 0)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xE3);
				goto timeout;
			}
		}
		/* MFG_RPC_SLP_PROT_EN_SET 0x13F91040 [19:16] = 4'b1111 */
		DRV_WriteReg32(MFG_RPC_SLP_PROT_EN_SET, GENMASK(19, 16));
		__gpufreq_footprint_power_step(0xE4);
		/* MFG_RPC_SLP_PROT_EN_STA 0x13F91048 [19:16] = 4'b1111 */
		i = 0;
		while ((DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA) & GENMASK(19, 16)) != GENMASK(19, 16)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xE5);
				goto timeout;
			}
		}
		/* IFR_EMISYS_PROTECT_EN_W1S_0 0x1002C104 [20:19] = 2'b11 */
		DRV_WriteReg32(IFR_EMISYS_PROTECT_EN_W1S_0, GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xE6);
		/* IFR_EMISYS_PROTECT_EN_STA_0 0x1002C100 [20:19] = 2'b11 */
		i = 0;
		while ((DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_0) & GENMASK(20, 19))
			!= GENMASK(20, 19)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xE7);
				goto timeout;
			}
		}
		/* IFR_EMISYS_PROTECT_EN_W1S_1 0x1002C124 [20:19] = 2'b11 */
		DRV_WriteReg32(IFR_EMISYS_PROTECT_EN_W1S_1, GENMASK(20, 19));
		__gpufreq_footprint_power_step(0xE8);
		/* IFR_EMISYS_PROTECT_EN_STA_1 0x1002C120 [20:19] = 2'b11 */
		i = 0;
		while ((DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_1) & GENMASK(20, 19))
			!= GENMASK(20, 19)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xE9);
				goto timeout;
			}
		}
		/* MFG1_PWR_CON 0x13F91070 [8] MFG1_PWR_SRAM_PDN = 1'b1 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) | BIT(8)));
		__gpufreq_footprint_power_step(0xEA);
		/* MFG1_PWR_CON 0x13F91070 [12] MFG1_PWR_SRAM_PDN_ACK = 1'b1 */
		i = 0;
		while ((DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & BIT(12)) != BIT(12)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xEB);
				goto timeout;
			}
		}
		/* MFG1_PWR_CON 0x13F91070 [1] MFG1_PWR_ISO = 1'b1 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) | BIT(1)));
		__gpufreq_footprint_power_step(0xEC);
		/* MFG1_PWR_CON 0x13F91070 [4] MFG1_PWR_CLK_DIS = 1'b1 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) | BIT(4)));
		__gpufreq_footprint_power_step(0xED);
		/* MFG1_PWR_CON 0x13F91070 [0] MFG1_PWR_RST_B = 1'b0 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & ~BIT(0)));
		__gpufreq_footprint_power_step(0xEE);
		/* MFG1_PWR_CON 0x13F91070 [2] MFG1_PWR_ON = 1'b0 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & ~BIT(2)));
		__gpufreq_footprint_power_step(0xEF);
		/* MFG1_PWR_CON 0x13F91070 [3] MFG1_PWR_ON_2ND = 1'b0 */
		DRV_WriteReg32(MFG_RPC_MFG1_PWR_CON, (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & ~BIT(3)));
		__gpufreq_footprint_power_step(0xF0);
		/* MFG1_PWR_CON 0x13F91070 [30] MFG1_PWR_ACK = 1'b0 */
		/* MFG1_PWR_CON 0x13F91070 [31] MFG1_PWR_ACK_2ND = 1'b0 */
		i = 0;
		while (DRV_Reg32(MFG_RPC_MFG1_PWR_CON) & GENMASK(31, 30)) {
			udelay(10);
			if (++i > 1000) {
				__gpufreq_footprint_power_step(0xF1);
				goto timeout;
			}
		}
	}

	return;

timeout:
	GPUFREQ_LOGE("(0x13F91070)=0x%x, (0x13F91048)=0x%x, (0x1002C100)=0x%x, (0x1002C120)=0x%x",
		DRV_Reg32(MFG_RPC_MFG1_PWR_CON), DRV_Reg32(MFG_RPC_SLP_PROT_EN_STA),
		DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_0), DRV_Reg32(IFR_EMISYS_PROTECT_EN_STA_1));
	GPUFREQ_LOGE("(0x1021C82C)=0x%x, (0x1021C830)=0x%x, (0x1021E82C)=0x%x, (0x1021E830)=0x%x",
		DRV_Reg32(NTH_MFG_EMI1_GALS_SLV_DBG), DRV_Reg32(NTH_MFG_EMI0_GALS_SLV_DBG),
		DRV_Reg32(STH_MFG_EMI1_GALS_SLV_DBG), DRV_Reg32(STH_MFG_EMI0_GALS_SLV_DBG));
	__gpufreq_abort("timeout");
}

static int __gpufreq_mtcmos_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;
	u32 val = 0;

	GPUFREQ_TRACE_START("power=%d", power);

	if (power == GPU_PWR_ON) {
		__gpufreq_mfg1_rpc_control(GPU_PWR_ON);
		g_gpu.mtcmos_count++;

#if GPUFREQ_CHECK_MFG_PWR_STATUS
		val = MFG_0_1_PWR_STATUS & MFG_0_1_PWR_MASK;
		if (unlikely(val != MFG_0_1_PWR_MASK)) {
			__gpufreq_abort("incorrect MFG0-1 power on status: 0x%08x", val);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
#endif /* GPUFREQ_CHECK_MFG_PWR_STATUS */

#if !GPUFREQ_PDCA_ENABLE
		__gpufreq_mfgx_rpc_control(GPU_PWR_ON, MFG_RPC_MFG2_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_ON, MFG_RPC_MFG3_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_ON, MFG_RPC_MFG4_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_ON, MFG_RPC_MFG6_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_ON, MFG_RPC_MFG7_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_ON, MFG_RPC_MFG9_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_ON, MFG_RPC_MFG10_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_ON, MFG_RPC_MFG11_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_ON, MFG_RPC_MFG12_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_ON, MFG_RPC_MFG13_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_ON, MFG_RPC_MFG14_PWR_CON);
#if GPUFREQ_CHECK_MFG_PWR_STATUS
		val = MFG_0_14_PWR_STATUS & MFG_0_14_PWR_MASK;
		if (unlikely(val != MFG_0_14_PWR_MASK)) {
			__gpufreq_abort("incorrect MFG0-14 power on status: 0x%08x", val);
			ret = GPUFREQ_EINVAL;
			goto done;
		}
#endif /* GPUFREQ_CHECK_MFG_PWR_STATUS */
#endif /* GPUFREQ_PDCA_ENABLE */
	} else {
#if !GPUFREQ_PDCA_ENABLE
		__gpufreq_mfgx_rpc_control(GPU_PWR_OFF, MFG_RPC_MFG14_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_OFF, MFG_RPC_MFG13_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_OFF, MFG_RPC_MFG12_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_OFF, MFG_RPC_MFG11_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_OFF, MFG_RPC_MFG10_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_OFF, MFG_RPC_MFG9_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_OFF, MFG_RPC_MFG7_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_OFF, MFG_RPC_MFG6_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_OFF, MFG_RPC_MFG4_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_OFF, MFG_RPC_MFG3_PWR_CON);
		__gpufreq_mfgx_rpc_control(GPU_PWR_OFF, MFG_RPC_MFG2_PWR_CON);
#endif /* GPUFREQ_PDCA_ENABLE */

		__gpufreq_mfg1_rpc_control(GPU_PWR_OFF);
		g_gpu.mtcmos_count--;

#if GPUFREQ_CHECK_MFG_PWR_STATUS
		val = MFG_0_14_PWR_STATUS & MFG_1_14_PWR_MASK;
		if (unlikely(val))
			/* only print error if pwr is incorrect when mtcmos off */
			GPUFREQ_LOGE("incorrect MFG1-14 power off status: 0x%08x", val);
#endif /* GPUFREQ_CHECK_MFG_PWR_STATUS */
	}

#if GPUFREQ_CHECK_MFG_PWR_STATUS
done:
#endif /* GPUFREQ_CHECK_MFG_PWR_STATUS */
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_buck_control(enum gpufreq_power_state power)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("power=%d", power);

	/* power on: VSRAM -> VGPU */
	if (power == GPU_PWR_ON) {
		ret = regulator_enable(g_pmic->reg_vsram);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable VSRAM (%d)", ret);
			goto done;
		}

		ret = regulator_enable(g_pmic->reg_vgpu);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to enable VGPU (%d)", ret);
			goto done;
		}

		g_gpu.buck_count++;
	/* power off: VGPU -> VSRAM */
	} else {
		ret = regulator_disable(g_pmic->reg_vgpu);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to disable VGPU (%d)", ret);
			goto done;
		}
		g_gpu.buck_count--;

		ret = regulator_disable(g_pmic->reg_vsram);
		if (unlikely(ret)) {
			__gpufreq_abort("fail to disable VSRAM (%d)", ret);
			goto done;
		}
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static void __gpufreq_check_bus_idle(void)
{
	/* MFG_QCHANNEL_CON 0x13FBF0B4 [0] MFG_ACTIVE_SEL = 1'b1 */
	DRV_WriteReg32(MFG_QCHANNEL_CON, (DRV_Reg32(MFG_QCHANNEL_CON) | BIT(0)));

	/* MFG_DEBUG_SEL 0x13FBF170 [1:0] MFG_DEBUG_TOP_SEL = 2'b11 */
	DRV_WriteReg32(MFG_DEBUG_SEL, (DRV_Reg32(MFG_DEBUG_SEL) | GENMASK(1, 0)));

	/*
	 * polling MFG_DEBUG_TOP 0x13FBF178 [0] MFG_DEBUG_TOP
	 * 0x0: bus idle
	 * 0x1: bus busy
	 */
	do {} while (DRV_Reg32(MFG_DEBUG_TOP) & BIT(0));
}

/* API: init first OPP idx by init freq set in preloader */
static int __gpufreq_init_opp_idx(void)
{
	struct gpufreq_opp_info *working_table = g_gpu.working_table;
	int target_oppidx = -1;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	/* get current GPU OPP idx by freq set in preloader */
	g_gpu.cur_oppidx = __gpufreq_get_idx_by_fgpu(g_gpu.cur_freq);

	/* decide first OPP idx by custom setting */
	if (__gpufreq_custom_init_enable())
		target_oppidx = GPUFREQ_CUST_INIT_OPPIDX;
	/* decide first OPP idx by preloader setting */
	else
		target_oppidx = g_gpu.cur_oppidx;

	GPUFREQ_LOGI(
		"init GPU[%d] F(%d->%d) V(%d->%d), VSRAM(%d->%d)",
		target_oppidx,
		g_gpu.cur_freq, working_table[target_oppidx].freq,
		g_gpu.cur_volt, working_table[target_oppidx].volt,
		g_gpu.cur_vsram, working_table[target_oppidx].vsram);

	/* init first OPP index */
	if (!__gpufreq_dvfs_enable()) {
		g_dvfs_state = DVFS_DISABLE;
		GPUFREQ_LOGI("DVFS state: 0x%x, disable DVFS", g_dvfs_state);

		/* set OPP once if DVFS is disabled but custom init is enabled */
		if (__gpufreq_custom_init_enable())
			ret = __gpufreq_generic_commit_gpu(target_oppidx, DVFS_DISABLE);
	} else {
		g_dvfs_state = DVFS_FREE;
		GPUFREQ_LOGI("DVFS state: 0x%x, enable DVFS", g_dvfs_state);

		ret = __gpufreq_generic_commit_gpu(target_oppidx, DVFS_FREE);
	}

#if GPUFREQ_MSSV_TEST_MODE
	/* disable DVFS when MSSV test */
	__gpufreq_set_dvfs_state(true, DVFS_MSSV_TEST);
#endif /* GPUFREQ_MSSV_TEST_MODE */

	GPUFREQ_TRACE_END();

	return ret;
}

/* API: calculate power of every OPP in working table */
static void __gpufreq_measure_power(void)
{
	struct gpufreq_opp_info *working_gpu = g_gpu.working_table;
	unsigned int freq = 0, volt = 0;
	unsigned int p_total = 0, p_dynamic = 0, p_leakage = 0;
	int opp_num_gpu = g_gpu.opp_num;
	int i = 0;

	for (i = 0; i < opp_num_gpu; i++) {
		freq = working_gpu[i].freq;
		volt = working_gpu[i].volt;

		p_leakage = __gpufreq_get_lkg_pgpu(volt, 30);
		p_dynamic = __gpufreq_get_dyn_pgpu(freq, volt);

		p_total = p_dynamic + p_leakage;

		working_gpu[i].power = p_total;

		GPUFREQ_LOGD("GPU[%02d] power: %d (dynamic: %d, leakage: %d)",
			i, p_total, p_dynamic, p_leakage);
	}

	/* update current status to shared memory */
	if (g_shared_status) {
		g_shared_status->cur_power_gpu = working_gpu[g_gpu.cur_oppidx].power;
		g_shared_status->max_power_gpu = working_gpu[g_gpu.max_oppidx].power;
		g_shared_status->min_power_gpu = working_gpu[g_gpu.min_oppidx].power;
	}
}

/*
 * API: interpolate OPP from signoff idx.
 * step = (large - small) / range
 * vnew = large - step * j
 */
static void __gpufreq_interpolate_volt(void)
{
	unsigned int large_volt = 0, small_volt = 0;
	unsigned int large_freq = 0, small_freq = 0;
	unsigned int inner_volt = 0, inner_freq = 0;
	unsigned int previous_volt = 0;
	int adj_num = 0, i = 0, j = 0;
	int front_idx = 0, rear_idx = 0, inner_idx = 0;
	int range = 0, slope = 0;
	const int *signed_idx = NULL;
	struct gpufreq_opp_info *signed_table = NULL;

	adj_num = NUM_GPU_SIGNED_IDX;
	signed_idx = g_gpu_signed_idx;
	signed_table = g_gpu.signed_table;

	mutex_lock(&gpufreq_lock);

	for (i = 1; i < adj_num; i++) {
		front_idx = signed_idx[i - 1];
		rear_idx = signed_idx[i];
		range = rear_idx - front_idx;

		/* freq division to amplify slope */
		large_volt = signed_table[front_idx].volt * 100;
		large_freq = signed_table[front_idx].freq / 1000;

		small_volt = signed_table[rear_idx].volt * 100;
		small_freq = signed_table[rear_idx].freq / 1000;

		/* slope = volt / freq */
		slope = (int)(large_volt - small_volt) / (int)(large_freq - small_freq);

		if (unlikely(slope < 0))
			__gpufreq_abort("invalid slope: %d", slope);

		GPUFREQ_LOGD("GPU[%02d*] Freq: %d, Volt: %d, slope: %d",
			rear_idx, small_freq * 1000, small_volt / 100, slope);

		/* start from small V and F, and use (+) instead of (-) */
		for (j = 1; j < range; j++) {
			inner_idx = rear_idx - j;
			inner_freq = signed_table[inner_idx].freq / 1000;
			inner_volt = (small_volt + slope * (inner_freq - small_freq)) / 100;
			inner_volt = VOLT_NORMALIZATION(inner_volt);

			/* compare interpolated volt with volt of previous OPP idx */
			previous_volt = signed_table[inner_idx + 1].volt;
			if (inner_volt < previous_volt)
				__gpufreq_abort("invalid GPU[%02d*] Volt: %d < [%02d*] Volt: %d",
					inner_idx, inner_volt, inner_idx + 1, previous_volt);

			/* record margin */
			signed_table[inner_idx].margin += signed_table[inner_idx].volt - inner_volt;
			/* update to signed table */
			signed_table[inner_idx].volt = inner_volt;
			signed_table[inner_idx].vsram = __gpufreq_get_vsram_by_vlogic(inner_volt);

			GPUFREQ_LOGD("GPU[%02d*] Freq: %d, Volt: %d, Vsram: %d",
				inner_idx, inner_freq * 1000, inner_volt,
				signed_table[inner_idx].vsram);
		}
		GPUFREQ_LOGD("GPU[%02d*] Freq: %d, Volt: %d",
			front_idx, large_freq * 1000, large_volt / 100);
	}

	mutex_unlock(&gpufreq_lock);
}

static void __gpufreq_compute_aging(void)
{
	unsigned int aging_table_idx = GPUFREQ_AGING_MAX_TABLE_IDX;

	if (g_aging_load)
		aging_table_idx = GPUFREQ_AGING_MOST_AGRRESIVE;

	if (aging_table_idx > GPUFREQ_AGING_MAX_TABLE_IDX)
		aging_table_idx = GPUFREQ_AGING_MAX_TABLE_IDX;

	/* Aging margin is set if any OPP is adjusted by Aging */
	if (aging_table_idx == 0)
		g_aging_margin = true;

	g_aging_table_idx = aging_table_idx;
}

static unsigned int __gpufreq_compute_avs_freq(u32 val)
{
	unsigned int freq = 0;

	freq |= (val & BIT(20)) >> 10;         /* Get freq[10]  from efuse[20]    */
	freq |= (val & GENMASK(11, 10)) >> 2;  /* Get freq[9:8] from efuse[11:10] */
	freq |= (val & GENMASK(1, 0)) << 6;    /* Get freq[7:6] from efuse[1:0]   */
	freq |= (val & GENMASK(7, 6)) >> 2;    /* Get freq[5:4] from efuse[7:6]   */
	freq |= (val & GENMASK(19, 18)) >> 16; /* Get freq[3:2] from efuse[19:18] */
	freq |= (val & GENMASK(13, 12)) >> 12; /* Get freq[1:0] from efuse[13:12] */
	/* Freq is stored in efuse with MHz unit */
	freq *= 1000;

	return freq;
}

static unsigned int __gpufreq_compute_avs_volt(u32 val)
{
	unsigned int volt = 0;

	volt |= (val & GENMASK(17, 14)) >> 14; /* Get volt[3:0] from efuse[17:14] */
	volt |= (val & GENMASK(5, 4));         /* Get volt[5:4] from efuse[5:4]   */
	volt |= (val & GENMASK(3, 2)) << 4;    /* Get volt[7:6] from efuse[3:2]   */
	/* Volt is stored in efuse with 6.25mV unit */
	volt *= 625;

	return volt;
}

static void __gpufreq_compute_avs(void)
{
	u32 val = 0;
	unsigned int temp_freq = 0, volt_ofs = 0;
	int i = 0, oppidx = 0;
	int adj_num = NUM_GPU_SIGNED_IDX;
#if GPUFREQ_HRID_LOOKUP_ENABLE
	unsigned long j = 0;
	u32 hrid_0 = 0, hrid_1 = 0;
	int hrid_idx = 0;

	hrid_0 = DRV_Reg32(EFUSE_HRID_0);
	hrid_1 = DRV_Reg32(EFUSE_HRID_1);

	for (j = 0; j < NUM_HRID; j++) {
		if ((hrid_0 == g_hrid_table[j].hrid_0) && (hrid_1 == g_hrid_table[j].hrid_1)) {
			hrid_idx = j;
			break;
		}
	}
	if (hrid_idx == NUM_HRID) {
		GPUFREQ_LOGW("cannot find HRID in lookup table");
		return;
	}
	GPUFREQ_LOGI("HRID_0: 0x%08x, HRID1: 0x%08x, HRID idx: %d", hrid_0, hrid_1, hrid_idx);

	g_gpu_avs_table[0].volt = g_hrid_table[hrid_idx].vgpu_p1;
	g_gpu_avs_table[1].volt = g_hrid_table[hrid_idx].vgpu_p2;
	g_gpu_avs_table[2].volt = g_hrid_table[hrid_idx].vgpu_p3;
	GPUFREQ_LOGI("[HRID]  GPU   P1: %d, P2: %d, P3: %d",
		g_gpu_avs_table[0].volt, g_gpu_avs_table[1].volt, g_gpu_avs_table[2].volt);
#else /* GPUFREQ_HRID_LOOKUP_ENABLE */
	/*
	 * Compute GPU AVS
	 *
	 * Freq (MHz) | Signoff Volt (V) | Efuse name | Efuse addr
	 * ============================================================
	 * 1400       | 0.9              | GPU_PTP0   | 0x11E80E14
	 * 820        | 0.675            | GPU_PTP1   | 0x11E80E18
	 * 265        | 0.5              | GPU_PTP2   | 0x11E80E1C
	 */
	/* read AVS efuse and compute Freq and Volt */
	for (i = 0; i < adj_num; i++) {
		oppidx = g_gpu_avs_table[i].oppidx;
		if (g_mcl50_load) {
			if (i == 0)
				val = 0x00CB1404;
			else if (i == 1)
				val = 0x00C50CE4;
			else if (i == 2)
				val = 0x00D984C9;
			else
				val = 0;
		} else if (g_avs_enable) {
			if (i == 0) {
				val = DRV_Reg32(EFUSE_GPU_PTP0_AVS);
			} else if (i == 1) {
				val = DRV_Reg32(EFUSE_GPU_PTP1_AVS);
			} else if (i == 2) {
				val = DRV_Reg32(EFUSE_GPU_PTP2_AVS);
			} else {
				val = 0;
			}
		} else {
			val = 0;
		}

		/* if efuse value is not set */
		if (!val)
			continue;

		/* compute Freq from efuse */
		temp_freq = __gpufreq_compute_avs_freq(val);
		/* verify with signoff Freq */
		if (temp_freq != g_gpu.signed_table[oppidx].freq) {
			__gpufreq_abort("GPU[%02d*]: efuse[%d].freq(%d) != signed-off.freq(%d)",
				oppidx, i, temp_freq, g_gpu.signed_table[oppidx].freq);
			return;
		}
		g_gpu_avs_table[i].freq = temp_freq;

		/* compute Volt from efuse */
		g_gpu_avs_table[i].volt = __gpufreq_compute_avs_volt(val);

		/* AVS margin is set if any OPP is adjusted by AVS */
		g_avs_margin = true;

		/* GPU_PTP0 [31:24] GPU_PTP_version */
		if (i == 0)
			g_ptp_version = (val & GENMASK(31, 24)) >> 24;
	}
#endif /* GPUFREQ_HRID_LOOKUP_ENABLE */
	/* check AVS Volt and update Vsram */
	for (i = adj_num - 1; i >= 0; i--) {
		oppidx = g_gpu_avs_table[i].oppidx;
		/* mV * 100 */
		volt_ofs = 1250;

		/* if AVS Volt is not set */
		if (!g_gpu_avs_table[i].volt)
			continue;

		/*
		 * AVS Volt reverse check, start from adj_num -2
		 * Volt of sign-off[i] should always be larger than sign-off[i + 1]
		 * if not, add Volt offset to sign-off[i]
		 */
		if (i != (adj_num - 1)) {
			if (g_gpu_avs_table[i].volt <= g_gpu_avs_table[i + 1].volt) {
				GPUFREQ_LOGW("GPU efuse[%02d].volt(%d) <= efuse[%02d].volt(%d)",
					i, g_gpu_avs_table[i].volt,
					i + 1, g_gpu_avs_table[i + 1].volt);
				g_gpu_avs_table[i].volt = g_gpu_avs_table[i + 1].volt + volt_ofs;
			}
		}

		/* clamp to signoff Volt */
		if (g_gpu_avs_table[i].volt > g_gpu.signed_table[oppidx].volt) {
			GPUFREQ_LOGW("GPU[%02d*]: efuse[%02d].volt(%d) > signed-off.volt(%d)",
				oppidx, i, g_gpu_avs_table[i].volt,
				g_gpu.signed_table[oppidx].volt);
			g_gpu_avs_table[i].volt = g_gpu.signed_table[oppidx].volt;
		}

		/* update Vsram */
		g_gpu_avs_table[i].vsram = __gpufreq_get_vsram_by_vlogic(g_gpu_avs_table[i].volt);
	}

	for (i = 0; i < adj_num; i++)
		GPUFREQ_LOGI("GPU[%02d*]: efuse[%02d] freq(%d), volt(%d)",
			g_gpu_avs_table[i].oppidx, i, g_gpu_avs_table[i].freq,
			g_gpu_avs_table[i].volt);
}

static void __gpufreq_apply_adjustment(void)
{
	struct gpufreq_opp_info *signed_gpu = NULL;
	int adj_num = 0, oppidx = 0, i = 0;
	unsigned int avs_volt = 0, avs_vsram = 0, aging_volt = 0;

	signed_gpu = g_gpu.signed_table;
	adj_num = NUM_GPU_SIGNED_IDX;

	/* apply AVS margin */
	if (g_avs_margin) {
		/* GPU AVS */
		for (i = 0; i < adj_num; i++) {
			oppidx = g_gpu_avs_table[i].oppidx;
			avs_volt = g_gpu_avs_table[i].volt ?
				g_gpu_avs_table[i].volt : signed_gpu[oppidx].volt;
			avs_vsram = g_gpu_avs_table[i].vsram ?
				g_gpu_avs_table[i].vsram : signed_gpu[oppidx].vsram;
			/* record margin */
			signed_gpu[oppidx].margin += signed_gpu[oppidx].volt - avs_volt;
			/* update to signed table */
			signed_gpu[oppidx].volt = avs_volt;
			signed_gpu[oppidx].vsram = avs_vsram;
		}
	} else
		GPUFREQ_LOGI("AVS margin is not set");

	/* apply Aging margin */
	if (g_aging_margin) {
		/* GPU Aging */
		for (i = 0; i < adj_num; i++) {
			oppidx = g_gpu_aging_table[g_aging_table_idx][i].oppidx;
			aging_volt = g_gpu_aging_table[g_aging_table_idx][i].volt;
			/* record margin */
			signed_gpu[oppidx].margin += aging_volt;
			/* update to signed table */
			signed_gpu[oppidx].volt -= aging_volt;
			signed_gpu[oppidx].vsram =
				__gpufreq_get_vsram_by_vlogic(signed_gpu[oppidx].volt);
		}
	} else
		GPUFREQ_LOGI("Aging margin is not set");

	/* compute others OPP exclude signoff idx */
	__gpufreq_interpolate_volt();
}

static void __gpufreq_init_shader_present(void)
{
	unsigned int segment_id = 0;

	segment_id = g_gpu.segment_id;

	switch (segment_id) {
	default:
		g_shader_present = GPU_SHADER_PRESENT_6;
	}
	GPUFREQ_LOGD("segment_id: %d, shader_present: %d", segment_id, g_shader_present);
}

/*
 * 1. init OPP segment range
 * 2. init segment/working OPP table
 * 3. init power measurement
 */
static void __gpufreq_init_opp_table(void)
{
	unsigned int segment_id = 0;
	int i = 0, j = 0;

	/* init current GPU/STACK freq and volt set by preloader */
	g_gpu.cur_freq = __gpufreq_get_real_fgpu();
	g_gpu.cur_volt = __gpufreq_get_real_vgpu();
	g_gpu.cur_vsram = __gpufreq_get_real_vsram();

	g_stack.cur_freq = __gpufreq_get_real_fstack();

	GPUFREQ_LOGI("preloader init [GPU] Freq: %d, Volt: %d, Vsram: %d, [STACK] Freq: %d",
		g_gpu.cur_freq, g_gpu.cur_volt, g_gpu.cur_vsram, g_stack.cur_freq);

	/* init GPU OPP table */
	segment_id = g_gpu.segment_id;
	if (segment_id == MT6897_SEGMENT)
		g_gpu.segment_upbound = 0;
	else
		g_gpu.segment_upbound = 0;
	g_gpu.segment_lowbound = NUM_GPU_SIGNED_OPP - 1;
	g_gpu.signed_opp_num = NUM_GPU_SIGNED_OPP;
	g_gpu.max_oppidx = 0;
	g_gpu.min_oppidx = g_gpu.segment_lowbound - g_gpu.segment_upbound;
	g_gpu.opp_num = g_gpu.min_oppidx + 1;
	g_gpu.signed_table = g_gpu_default_opp_table;

	g_gpu.working_table = kcalloc(g_gpu.opp_num, sizeof(struct gpufreq_opp_info), GFP_KERNEL);
	if (!g_gpu.working_table) {
		__gpufreq_abort("fail to alloc g_gpu.working_table (%dB)",
			g_gpu.opp_num * sizeof(struct gpufreq_opp_info));
		return;
	}
	GPUFREQ_LOGD("number of signed GPU OPP: %d, upper and lower bound: [%d, %d]",
		g_gpu.signed_opp_num, g_gpu.segment_upbound, g_gpu.segment_lowbound);
	GPUFREQ_LOGI("number of working GPU OPP: %d, max and min OPP index: [%d, %d]",
		g_gpu.opp_num, g_gpu.max_oppidx, g_gpu.min_oppidx);

	/* update signed OPP table from MFGSYS features */

	/* compute Aging table based on Aging Sensor */
	__gpufreq_compute_aging();
	/* compute AVS table based on EFUSE */
	__gpufreq_compute_avs();
	/* apply Segment/Aging/AVS/... adjustment to signed OPP table  */
	__gpufreq_apply_adjustment();

	/* after these, GPU signed table are settled down */

	/* init working table, based on signed table */
	for (i = 0; i < g_gpu.opp_num; i++) {
		j = i + g_gpu.segment_upbound;
		g_gpu.working_table[i].freq = g_gpu.signed_table[j].freq;
		g_gpu.working_table[i].volt = g_gpu.signed_table[j].volt;
		g_gpu.working_table[i].vsram = g_gpu.signed_table[j].vsram;
		g_gpu.working_table[i].posdiv = g_gpu.signed_table[j].posdiv;
		g_gpu.working_table[i].margin = g_gpu.signed_table[j].margin;
		g_gpu.working_table[i].power = g_gpu.signed_table[j].power;

		GPUFREQ_LOGD("GPU[%02d] Freq: %d, Volt: %d, Vsram: %d, Margin: %d",
			i, g_gpu.working_table[i].freq, g_gpu.working_table[i].volt,
			g_gpu.working_table[i].vsram, g_gpu.working_table[i].margin);
	}

	/* set power info to working table */
	__gpufreq_measure_power();
}

static void __iomem *__gpufreq_of_ioremap(const char *node_name, int idx)
{
	struct device_node *node;
	void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node)
		base = of_iomap(node, idx);
	else
		base = NULL;

	return base;
}

static void __gpufreq_init_segment_id(void)
{
	unsigned int efuse_id = 0x0;
	unsigned int segment_id = 0;

	switch (efuse_id) {
	default:
		segment_id = MT6897_SEGMENT;
		GPUFREQ_LOGW("unknown efuse id: 0x%x", efuse_id);
		break;
	}

	GPUFREQ_LOGI("efuse_id: 0x%x, segment_id: %d", efuse_id, segment_id);

	g_gpu.segment_id = segment_id;
}

static int __gpufreq_init_mtcmos(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START();

	/* do nothing */

	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_init_clk(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%lx", (unsigned long)pdev);

	g_clk = kzalloc(sizeof(struct gpufreq_clk_info), GFP_KERNEL);
	if (!g_clk) {
		__gpufreq_abort("fail to alloc g_clk (%dB)",
			sizeof(struct gpufreq_clk_info));
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_clk->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(g_clk->clk_mux)) {
		ret = PTR_ERR(g_clk->clk_mux);
		__gpufreq_abort("fail to get clk_mux (%ld)", ret);
		goto done;
	}

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		ret = PTR_ERR(g_clk->clk_main_parent);
		__gpufreq_abort("fail to get clk_main_parent (%ld)", ret);
		goto done;
	}

	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		ret = PTR_ERR(g_clk->clk_sub_parent);
		__gpufreq_abort("fail to get clk_sub_parent (%ld)", ret);
		goto done;
	}

	g_clk->clk_sc_mux = devm_clk_get(&pdev->dev, "clk_sc_mux");
	if (IS_ERR(g_clk->clk_sc_mux)) {
		ret = PTR_ERR(g_clk->clk_sc_mux);
		__gpufreq_abort("fail to get clk_sc_mux (%ld)", ret);
		goto done;
	}

	g_clk->clk_sc_main_parent = devm_clk_get(&pdev->dev, "clk_sc_main_parent");
	if (IS_ERR(g_clk->clk_sc_main_parent)) {
		ret = PTR_ERR(g_clk->clk_sc_main_parent);
		__gpufreq_abort("fail to get clk_sc_main_parent (%ld)", ret);
		goto done;
	}

	g_clk->clk_sc_sub_parent = devm_clk_get(&pdev->dev, "clk_sc_sub_parent");
	if (IS_ERR(g_clk->clk_sc_sub_parent)) {
		ret = PTR_ERR(g_clk->clk_sc_sub_parent);
		__gpufreq_abort("fail to get clk_sc_sub_parent (%ld)", ret);
		goto done;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

static int __gpufreq_init_pmic(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("pdev=0x%lx", (unsigned long)pdev);

	g_pmic = kzalloc(sizeof(struct gpufreq_pmic_info), GFP_KERNEL);
	if (!g_pmic) {
		__gpufreq_abort("fail to alloc g_pmic (%dB)",
			sizeof(struct gpufreq_pmic_info));
		ret = GPUFREQ_ENOMEM;
		goto done;
	}

	g_pmic->reg_vgpu = regulator_get_optional(&pdev->dev, "vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		ret = PTR_ERR(g_pmic->reg_vgpu);
		__gpufreq_abort("fail to get VGPU (%ld)", ret);
		goto done;
	}

	g_pmic->reg_vsram = regulator_get_optional(&pdev->dev, "vsram");
	if (IS_ERR(g_pmic->reg_vsram)) {
		ret = PTR_ERR(g_pmic->reg_vsram);
		__gpufreq_abort("fail to get VSRAM (%ld)", ret);
		goto done;
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

/* API: init reg base address and flavor config of the platform */
static int __gpufreq_init_platform_info(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct device_node *of_wrapper = NULL;
	struct resource *res = NULL;
	int ret = GPUFREQ_ENOENT;

	if (unlikely(!gpufreq_dev)) {
		GPUFREQ_LOGE("fail to find gpufreq device (ENOENT)");
		goto done;
	}

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (unlikely(!of_wrapper)) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node");
		goto done;
	}

	/* feature config */
	g_asensor_enable = GPUFREQ_ASENSOR_ENABLE;
	g_avs_enable = GPUFREQ_AVS_ENABLE;
	g_gpm1_mode = GPUFREQ_GPM1_ENABLE;
	g_gpm3_mode = GPUFREQ_GPM3_ENABLE;
	g_dfd_mode = GPUFREQ_DFD_ENABLE;
	g_power_tracker_mode = GPUFREQ_POWER_TRACKER_ENABLE;
	/* ignore return error and use default value if property doesn't exist */
	of_property_read_u32(gpufreq_dev->of_node, "aging-load", &g_aging_load);
	of_property_read_u32(gpufreq_dev->of_node, "mcl50-load", &g_mcl50_load);
	of_property_read_u32(of_wrapper, "gpueb-support", &g_gpueb_support);

	/* 0x13000000 */
	g_mali_base = __gpufreq_of_ioremap("mediatek,mali", 0);
	if (unlikely(!g_mali_base)) {
		GPUFREQ_LOGE("fail to ioremap MALI");
		goto done;
	}

	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		goto done;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_top_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FA0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_PLL");
		goto done;
	}
	g_mfg_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_PLL: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FA0C00 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfgsc_pll");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFGSC_PLL");
		goto done;
	}
	g_mfgsc_pll_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfgsc_pll_base)) {
		GPUFREQ_LOGE("fail to ioremap MFGSC_PLL: 0x%llx", res->start);
		goto done;
	}

	/* 0x13F90000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_rpc");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_RPC");
		goto done;
	}
	g_mfg_rpc_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_rpc_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_RPC: 0x%llx", res->start);
		goto done;
	}

	/* 0x1C001000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sleep");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SLEEP");
		goto done;
	}
	g_sleep = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sleep)) {
		GPUFREQ_LOGE("fail to ioremap SLEEP: 0x%llx", res->start);
		goto done;
	}

	/* 0x10000000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "topckgen");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource TOPCKGEN");
		goto done;
	}
	g_topckgen_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_topckgen_base)) {
		GPUFREQ_LOGE("fail to ioremap TOPCKGEN: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG");
		goto done;
	}
	g_nth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emicfg_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG");
		goto done;
	}
	g_sth_emicfg_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sth_emicfg_base)) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG: 0x%llx", res->start);
		goto done;
	}

	/* 0x10270000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emicfg_ao_mem");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMICFG_AO_MEM");
		goto done;
	}
	g_nth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emicfg_ao_mem_base)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMICFG_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x1030E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emicfg_ao_mem");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource STH_EMICFG_AO_MEM");
		goto done;
	}
	g_sth_emicfg_ao_mem_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sth_emicfg_ao_mem_base)) {
		GPUFREQ_LOGE("fail to ioremap STH_EMICFG_AO_MEM: 0x%llx", res->start);
		goto done;
	}

	/* 0x1002C000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ifrbus_ao");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource IFRBUS_AO");
		goto done;
	}
	g_ifrbus_ao_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_ifrbus_ao_base)) {
		GPUFREQ_LOGE("fail to ioremap IFRBUS_AO: 0x%llx", res->start);
		goto done;
	}

	/* 0x10023000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infra_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x1002B000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infra_ao1_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource INFRA_AO1_DEBUG_CTRL");
		goto done;
	}
	g_infra_ao1_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_infra_ao1_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap INFRA_AO1_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10042000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nth_emi_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NTH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_nth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nth_emi_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap NTH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x10028000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sth_emi_ao_debug_ctrl");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource STH_EMI_AO_DEBUG_CTRL");
		goto done;
	}
	g_sth_emi_ao_debug_ctrl = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sth_emi_ao_debug_ctrl)) {
		GPUFREQ_LOGE("fail to ioremap STH_EMI_AO_DEBUG_CTRL: 0x%llx", res->start);
		goto done;
	}

	/* 0x11E80000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource EFUSE");
		goto done;
	}
	g_efuse_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_efuse_base)) {
		GPUFREQ_LOGE("fail to ioremap EFUSE: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FBC000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_secure");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_SECURE");
		goto done;
	}
	g_mfg_secure_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_secure_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_SECURE: 0x%llx", res->start);
		goto done;
	}

	/* 0x1000D000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "drm_debug");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource DRM_DEBUG");
		goto done;
	}
	g_drm_debug_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_drm_debug_base)) {
		GPUFREQ_LOGE("fail to ioremap DRM_DEBUG: 0x%llx", res->start);
		goto done;
	}

	/* 0x13FE0000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_ips");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_IPS");
		goto done;
	}
	g_mfg_ips_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_ips_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_IPS: 0x%llx", res->start);
		goto done;
	}

	/* 0x10219000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emi_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource EMI_REG");
		goto done;
	}
	g_emi_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_emi_base)) {
		GPUFREQ_LOGE("fail to ioremap EMI_REG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1021D000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sub_emi_reg");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SUB_EMI_REG");
		goto done;
	}
	g_sub_emi_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_sub_emi_base)) {
		GPUFREQ_LOGE("fail to ioremap SUB_EMI_REG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1025E000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nemi_mi32_smi_sub");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NEMI_MI32_SMI_SUB");
		goto done;
	}
	g_nemi_mi32_smi_sub = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nemi_mi32_smi_sub)) {
		GPUFREQ_LOGE("fail to ioremap NEMI_MI32_SMI_SUB: 0x%llx", res->start);
		goto done;
	}

	/* 0x1025F000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nemi_mi33_smi_sub");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource NEMI_MI33_SMI_SUB");
		goto done;
	}
	g_nemi_mi33_smi_sub = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_nemi_mi33_smi_sub)) {
		GPUFREQ_LOGE("fail to ioremap NEMI_MI33_SMI_SUB: 0x%llx", res->start);
		goto done;
	}

	/* 0x10309000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "semi_mi32_smi_sub");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SEMI_MI32_SMI_SUB");
		goto done;
	}
	g_semi_mi32_smi_sub = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_semi_mi32_smi_sub)) {
		GPUFREQ_LOGE("fail to ioremap SEMI_MI32_SMI_SUB: 0x%llx", res->start);
		goto done;
	}

	/* 0x1030A000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "semi_mi33_smi_sub");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource SEMI_MI33_SMI_SUB");
		goto done;
	}
	g_semi_mi33_smi_sub = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_semi_mi33_smi_sub)) {
		GPUFREQ_LOGE("fail to ioremap SEMI_MI33_SMI_SUB: 0x%llx", res->start);
		goto done;
	}

	ret = GPUFREQ_SUCCESS;

done:
	return ret;
}

/* API: skip gpufreq driver probe if in bringup state */
static unsigned int __gpufreq_bringup(void)
{
	struct device_node *of_wrapper = NULL;
	unsigned int bringup_state = false;

	of_wrapper = of_find_compatible_node(NULL, NULL, "mediatek,gpufreq_wrapper");
	if (unlikely(!of_wrapper)) {
		GPUFREQ_LOGE("fail to find gpufreq_wrapper of_node, treat as bringup");
		return true;
	}

	/* check bringup state by dts */
	of_property_read_u32(of_wrapper, "gpufreq-bringup", &bringup_state);

	return bringup_state;
}

/* API: gpufreq driver probe */
static int __gpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to probe gpufreq platform driver");

	/* keep probe successful but do nothing when bringup */
	if (__gpufreq_bringup()) {
		GPUFREQ_LOGI("skip gpufreq platform driver probe when bringup");
		__gpufreq_dump_bringup_status(pdev);
		goto done;
	}

	/* init footprint */
	__gpufreq_reset_footprint();

	/* init reg base address and flavor config of the platform in both AP and EB mode */
	ret = __gpufreq_init_platform_info(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init platform info (%d)", ret);
		goto done;
	}

	/* init pmic regulator */
	ret = __gpufreq_init_pmic(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init pmic (%d)", ret);
		goto done;
	}

	/* skip most of probe in EB mode */
	if (g_gpueb_support) {
		GPUFREQ_LOGI("gpufreq platform probe only init reg/pmic/fp in EB mode");
		goto register_fp;
	}

	/* init clock source */
	ret = __gpufreq_init_clk(pdev);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init clk (%d)", ret);
		goto done;
	}

	/* init mtcmos power domain */
	ret = __gpufreq_init_mtcmos();
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init mtcmos (%d)", ret);
		goto done;
	}

	/* init segment id */
	__gpufreq_init_segment_id();

	/* init shader present */
	__gpufreq_init_shader_present();

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_history_memory_init();
#endif /* GPUFREQ_HISTORY_ENABLE */

	/* power on to init first OPP index */
	ret = __gpufreq_power_control(GPU_PWR_ON);
	if (unlikely(ret < 0)) {
		GPUFREQ_LOGE("fail to control power state: %d (%d)", GPU_PWR_ON, ret);
		goto done;
	}

	/* init OPP table */
	__gpufreq_init_opp_table();

	/* init first OPP index by current freq and volt */
	ret = __gpufreq_init_opp_idx();
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init OPP index (%d)", ret);
		goto done;
	}

	/* power off after init first OPP index */
	if (__gpufreq_power_ctrl_enable())
		__gpufreq_power_control(GPU_PWR_OFF);
	/* never power off if power control is disabled */
	else
		GPUFREQ_LOGI("power control always on");

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_history_memory_reset();
	__gpufreq_record_history_entry(HISTORY_FREE);
#endif /* GPUFREQ_HISTORY_ENABLE */

register_fp:
	/*
	 * GPUFREQ PLATFORM INIT DONE
	 * register differnet platform fp to wrapper depending on AP or EB mode
	 */
	if (g_gpueb_support)
		gpufreq_register_gpufreq_fp(&platform_eb_fp);
	else
		gpufreq_register_gpufreq_fp(&platform_ap_fp);

	/* init gpu ppm */
	ret = gpuppm_init(TARGET_GPU, g_gpueb_support);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to init gpuppm (%d)", ret);
		goto done;
	}

	g_gpufreq_ready = true;
	GPUFREQ_LOGI("gpufreq platform driver probe done");

done:
	return ret;
}

/* API: gpufreq driver remove */
static int __gpufreq_pdrv_remove(struct platform_device *pdev)
{
	kfree(g_gpu.working_table);
	kfree(g_clk);
	kfree(g_pmic);

	return GPUFREQ_SUCCESS;
}

/* API: register gpufreq platform driver */
static int __init __gpufreq_init(void)
{
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_LOGI("start to init gpufreq platform driver");

	/* register gpufreq platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (unlikely(ret)) {
		GPUFREQ_LOGE("fail to register gpufreq platform driver (%d)", ret);
		goto done;
	}

	GPUFREQ_LOGI("gpufreq platform driver init done");

done:
	return ret;
}

/* API: unregister gpufreq driver */
static void __exit __gpufreq_exit(void)
{
	platform_driver_unregister(&g_gpufreq_pdrv);

#if GPUFREQ_HISTORY_ENABLE
	__gpufreq_history_memory_uninit();
#endif /* GPUFREQ_HISTORY_ENABLE */
}

module_init(__gpufreq_init);
module_exit(__gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS platform driver");
MODULE_LICENSE("GPL");
