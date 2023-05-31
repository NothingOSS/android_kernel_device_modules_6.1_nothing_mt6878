// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_disp_gamma.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_disp_pq_helper.h"

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

#ifdef CONFIG_LEDS_MTK_MODULE
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#include <linux/leds-mtk.h>
#else
#define mtk_leds_brightness_set(x, y) do { } while (0)
#endif

#define DISP_GAMMA_EN 0x0000
#define DISP_GAMMA_SHADOW_SRAM 0x0014
#define DISP_GAMMA_CFG 0x0020
#define DISP_GAMMA_SIZE 0x0030
#define DISP_GAMMA_PURE_COLOR 0x0038
#define DISP_GAMMA_BANK 0x0100
#define DISP_GAMMA_LUT 0x0700
#define DISP_GAMMA_LUT_0 0x0700
#define DISP_GAMMA_LUT_1 0x0B00

#define DISP_GAMMA_BLOCK_0_R_GAIN 0x0054
#define DISP_GAMMA_BLOCK_0_G_GAIN 0x0058
#define DISP_GAMMA_BLOCK_0_B_GAIN 0x005C

#define DISP_GAMMA_BLOCK_12_R_GAIN 0x0060
#define DISP_GAMMA_BLOCK_12_G_GAIN 0x0064
#define DISP_GAMMA_BLOCK_12_B_GAIN 0x0068

#define LUT_10BIT_MASK 0x03ff

#define GAMMA_EN BIT(0)
#define GAMMA_LUT_EN BIT(1)
#define GAMMA_RELAYMODE BIT(0)
#define DISP_GAMMA_BLOCK_SIZE 256
#define DISP_GAMMA_GAIN_SIZE 3

static void mtk_gamma_init(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int width;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support)
		width = gamma->tile_overhead.width;
	else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_SIZE,
		(width << 16) | cfg->h, ~0);
	if (gamma->primary_data->data_mode == HW_12BIT_MODE_8BIT ||
		gamma->primary_data->data_mode == HW_12BIT_MODE_12BIT) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_BANK,
			(gamma->primary_data->data_mode - 1) << 2, 0x4);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_PURE_COLOR,
			gamma->primary_data->color_protect.gamma_color_protect_support |
			gamma->primary_data->color_protect.gamma_color_protect_lsb, ~0);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_EN, GAMMA_EN, ~0);

//	atomic_set(&g_gamma_sof_filp, 0);
	atomic_set(&gamma->primary_data->sof_irq_available, 0);
}


static void mtk_disp_gamma_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (!gamma->is_right_pipe) {
			gamma->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead += gamma->tile_overhead.comp_overhead;
			cfg->tile_overhead.left_in_width += gamma->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			gamma->tile_overhead.width = cfg->tile_overhead.left_in_width;
		} else {
			gamma->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				gamma->tile_overhead.comp_overhead;
			cfg->tile_overhead.right_in_width +=
				gamma->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			gamma->tile_overhead.width = cfg->tile_overhead.right_in_width;
		}
	}

}

static void mtk_gamma_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	/* TODO: only call init function if frame dirty */
	mtk_gamma_init(comp, cfg, handle);
	//cmdq_pkt_write(handle, comp->cmdq_base,
	//	comp->regs_pa + DISP_GAMMA_SIZE,
	//	(cfg->w << 16) | cfg->h, ~0);
	//cmdq_pkt_write(handle, comp->cmdq_base,
	//	comp->regs_pa + DISP_GAMMA_CFG,
	//	GAMMA_RELAYMODE, BIT(0));
}

static int mtk_gamma_write_lut_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock)
{
	struct DISP_GAMMA_LUT_T *gamma_lut;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	int i;
	int ret = 0;

	if (lock)
		mutex_lock(&gamma->primary_data->global_lock);
	gamma_lut = gamma->primary_data->gamma_lut;
	if (gamma_lut == NULL) {
		DDPINFO("%s: table not initialized\n", __func__);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	for (i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			(comp->regs_pa + DISP_GAMMA_LUT + i * 4),
			gamma_lut->lut[i], ~0);

		if ((i & 0x3f) == 0) {
			DDPINFO("[0x%08lx](%d) = 0x%x\n",
				(long)(comp->regs_pa + DISP_GAMMA_LUT + i * 4),
				i, gamma_lut->lut[i]);
		}
	}
	i--;
	DDPINFO("[0x%08lx](%d) = 0x%x\n",
		(long)(comp->regs_pa + DISP_GAMMA_LUT + i * 4),
		i, gamma_lut->lut[i]);

	if ((int)(gamma_lut->lut[0] & 0x3FF) -
		(int)(gamma_lut->lut[510] & 0x3FF) > 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x1 << 2, 0x4);
		DDPINFO("decreasing LUT\n");
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x0 << 2, 0x4);
		DDPINFO("Incremental LUT\n");
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG,
			0x2 | gamma->primary_data->relay_value, 0x3);

gamma_write_lut_unlock:
	if (lock)
		mutex_unlock(&gamma->primary_data->global_lock);

	return ret;
}

static void disp_gamma_on_start_of_frame(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	if ((!atomic_read(&gamma->primary_data->sof_irq_available))
		&& (atomic_read(&gamma->primary_data->sof_filp))) {
		DDPINFO("%s: wake up thread\n", __func__);
		atomic_set(&gamma->primary_data->sof_irq_available, 1);
		wake_up_interruptible(&gamma->primary_data->sof_irq_wq);
	}
}

static int mtk_gamma_write_12bit_lut_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock)
{
	struct DISP_GAMMA_12BIT_LUT_T *gamma_lut;
	int i, j, block_num;
	int ret = 0;
	unsigned int table_config_sel, table_out_sel;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	if (lock)
		mutex_lock(&gamma->primary_data->global_lock);
	gamma_lut = gamma->primary_data->gamma_12bit_lut;
	if (gamma_lut == NULL) {
		DDPINFO("%s: table not initialized\n", __func__);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	if (gamma->primary_data->data_mode == HW_12BIT_MODE_12BIT) {
		block_num = DISP_GAMMA_12BIT_LUT_SIZE / DISP_GAMMA_BLOCK_SIZE;
	} else if (gamma->primary_data->data_mode == HW_12BIT_MODE_8BIT) {
		block_num = DISP_GAMMA_LUT_SIZE / DISP_GAMMA_BLOCK_SIZE;
	} else {
		DDPINFO("%s: g_gamma_data_mode is error\n", __func__);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	if (readl(comp->regs + DISP_GAMMA_SHADOW_SRAM) & 0x2) {
		table_config_sel = 0;
		table_out_sel = 0;
	} else {
		table_config_sel = 1;
		table_out_sel = 1;
	}

	writel(table_config_sel << 1 |
		(readl(comp->regs + DISP_GAMMA_SHADOW_SRAM) & 0x1),
		comp->regs + DISP_GAMMA_SHADOW_SRAM);

	for (i = 0; i < block_num; i++) {
		writel(i | (gamma->primary_data->data_mode - 1) << 2,
			comp->regs + DISP_GAMMA_BANK);
		for (j = 0; j < DISP_GAMMA_BLOCK_SIZE; j++) {
			writel(gamma_lut->lut_0[i * DISP_GAMMA_BLOCK_SIZE + j],
				comp->regs + DISP_GAMMA_LUT_0 + j * 4);
			writel(gamma_lut->lut_1[i * DISP_GAMMA_BLOCK_SIZE + j],
				comp->regs + DISP_GAMMA_LUT_1 + j * 4);
		}
	}

	if ((int)(gamma_lut->lut_0[0] & 0x3FF) -
		(int)(gamma_lut->lut_0[510] & 0x3FF) > 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x1 << 2, 0x4);
		DDPINFO("decreasing LUT\n");
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x0 << 2, 0x4);
		DDPINFO("Incremental LUT\n");
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG,
			0x2 | gamma->primary_data->relay_value, 0x3);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_SHADOW_SRAM,
			table_config_sel << 1 | table_out_sel, ~0);
gamma_write_lut_unlock:
	if (lock)
		mutex_unlock(&gamma->primary_data->global_lock);

	return ret;
}

static int mtk_gamma_write_gain_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock)
{
	int i;
	int ret = 0;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	if (lock)
		mutex_lock(&gamma->primary_data->global_lock);

	if ((gamma->primary_data->sb_param.gain[0] == 8192)
		&& (gamma->primary_data->sb_param.gain[1] == 8192)
		&& (gamma->primary_data->sb_param.gain[2] == 8192)) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x0 << 3, 0x8);
		DDPINFO("all gain == 8192\n");
		goto unlock;
	}

	if ((gamma->primary_data->sb_param.gain[0] == 0)
		&& (gamma->primary_data->sb_param.gain[1] == 0)
		&& (gamma->primary_data->sb_param.gain[2] == 0)) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x0 << 3, 0x8);
		DDPINFO("all gain == 0\n");
		goto unlock;
	}

	for (i = 0; i < DISP_GAMMA_GAIN_SIZE; i++) {
		if (gamma->primary_data->sb_param.gain[i] == 8192)
			gamma->primary_data->sb_param.gain[i] = 8191;
	}

	for (i = 0; i < DISP_GAMMA_GAIN_SIZE; i++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_BLOCK_0_R_GAIN + i * 4,
			gamma->primary_data->sb_param.gain[i], ~0);
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_CFG, 0x1 << 3, 0x8);

unlock:
	if (lock)
		mutex_unlock(&gamma->primary_data->global_lock);
	return ret;
}

static int mtk_gamma_set_lut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_GAMMA_LUT_T *user_gamma_lut)
{
	/* TODO: use CPU to write register */
	int ret = 0;
	struct DISP_GAMMA_LUT_T *gamma_lut, *old_lut;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	DDPINFO("%s\n", __func__);

	gamma_lut = kmalloc(sizeof(struct DISP_GAMMA_LUT_T),
		GFP_KERNEL);
	if (gamma_lut == NULL) {
		DDPPR_ERR("%s: no memory\n", __func__);
		return -EFAULT;
	}

	if (user_gamma_lut == NULL) {
		ret = -EFAULT;
		kfree(gamma_lut);
	} else {
		memcpy(gamma_lut, user_gamma_lut,
			sizeof(struct DISP_GAMMA_LUT_T));

		mutex_lock(&gamma->primary_data->global_lock);

		old_lut = gamma->primary_data->gamma_lut;
		gamma->primary_data->gamma_lut = gamma_lut;

		DDPINFO("%s: Set module(%d) lut\n", __func__, comp->id);
		ret = mtk_gamma_write_lut_reg(comp, handle, 0);

		mutex_unlock(&gamma->primary_data->global_lock);

		if (old_lut != NULL)
			kfree(old_lut);
			//if (comp->mtk_crtc != NULL)
			//	mtk_crtc_check_trigger(comp->mtk_crtc, false,
			//		false);
	}

	return ret;
}

static int mtk_gamma_12bit_set_lut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_GAMMA_12BIT_LUT_T *user_gamma_lut)
{
	/* TODO: use CPU to write register */
	int ret = 0;
	struct DISP_GAMMA_12BIT_LUT_T *gamma_lut, *old_lut;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	DDPINFO("%s\n", __func__);

	gamma_lut = kmalloc(sizeof(struct DISP_GAMMA_12BIT_LUT_T),
		GFP_KERNEL);
	if (gamma_lut == NULL) {
		DDPPR_ERR("%s: no memory\n", __func__);
		return -EFAULT;
	}

	if (user_gamma_lut == NULL) {
		ret = -EFAULT;
		kfree(gamma_lut);
	} else {
		memcpy(gamma_lut, user_gamma_lut,
			sizeof(struct DISP_GAMMA_12BIT_LUT_T));

		mutex_lock(&gamma->primary_data->global_lock);

		old_lut = gamma->primary_data->gamma_12bit_lut;
		gamma->primary_data->gamma_12bit_lut = gamma_lut;

		DDPINFO("%s: Set module(%d) lut\n", __func__, comp->id);
		ret = mtk_gamma_write_12bit_lut_reg(comp, handle, 0);

		mutex_unlock(&gamma->primary_data->global_lock);

		if (old_lut != NULL)
			kfree(old_lut);
	}

	return ret;
}
static int mtk_gamma_set_gain(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct mtk_disp_gamma_sb_param *user_gamma_gain)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	int ret = 0;

	if (user_gamma_gain == NULL) {
		ret = -EFAULT;
	} else {
		mutex_lock(&gamma->primary_data->global_lock);
		ret = mtk_gamma_write_gain_reg(comp, handle, 0);
		mutex_unlock(&gamma->primary_data->global_lock);
	}

	return ret;
}

struct mtk_ddp_comp *mtk_gamma_get_comp_by_default_crtc(struct drm_device *dev)
{
	struct drm_crtc *crtc;

	crtc = list_first_entry(&(dev)->mode_config.crtc_list,
					typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("%s, crtc is null!\n", __func__);
		return NULL;
	}

	return mtk_ddp_comp_sel_in_cur_crtc_path(to_mtk_crtc(crtc), MTK_DISP_GAMMA, 0);
}

int mtk_drm_ioctl_set_gammalut(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_ddp_comp *comp;
	struct mtk_disp_gamma *gamma;

	comp = mtk_gamma_get_comp_by_default_crtc(dev);
	if (comp == NULL) {
		DDPPR_ERR("%s, null pointer!\n", __func__);
		return -1;
	}
	gamma = comp_to_gamma(comp);

	gamma->primary_data->gamma_lut_db = *((struct DISP_GAMMA_LUT_T *)data);

	return mtk_crtc_user_cmd(&comp->mtk_crtc->base, comp, SET_GAMMALUT, data);
}

int mtk_drm_set_12bit_gammalut_internal(void *data,
		struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	mutex_lock(&gamma->primary_data->sram_lock);
	CRTC_MMP_EVENT_START(0, gamma_ioctl, 0, 0);
	memcpy(&gamma->primary_data->ioctl_data, (struct DISP_GAMMA_12BIT_LUT_T *)data,
			sizeof(struct DISP_GAMMA_12BIT_LUT_T));
	atomic_set(&gamma->primary_data->sof_filp, 1);
	if (comp->mtk_crtc != NULL) {
		mtk_drm_idlemgr_kick(__func__, &comp->mtk_crtc->base, 1);
		mtk_crtc_check_trigger(comp->mtk_crtc, true, false);
	}
	DDPINFO("%s:update IOCTL g_gamma_sof_filp to 1\n", __func__);
	CRTC_MMP_EVENT_END(0, gamma_ioctl, 0, 1);
	mutex_unlock(&gamma->primary_data->sram_lock);

	return 0;
}

int mtk_drm_ioctl_set_12bit_gammalut(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_ddp_comp *comp;

	comp = mtk_gamma_get_comp_by_default_crtc(dev);
	if (comp == NULL) {
		DDPPR_ERR("%s, null pointer!\n", __func__);
		return -1;
	}

	mtk_drm_set_12bit_gammalut_internal(data, comp);

	return 0;
}

int mtk_drm_12bit_gammalut_ioctl_impl(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;

	mutex_lock(&gamma->primary_data->sram_lock);
	gamma->primary_data->gamma_12bit_lut_db = gamma->primary_data->ioctl_data;
	ret = mtk_crtc_user_cmd(crtc, comp, SET_12BIT_GAMMALUT,
			(void *)(&gamma->primary_data->ioctl_data));
	mutex_unlock(&gamma->primary_data->sram_lock);
	return ret;
}

int mtk_drm_ioctl_bypass_disp_gamma(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_ddp_comp *comp;

	comp = mtk_gamma_get_comp_by_default_crtc(dev);
	if (comp == NULL) {
		DDPPR_ERR("%s, null pointer!\n", __func__);
		return -1;
	}

	return mtk_crtc_user_cmd(&comp->mtk_crtc->base, comp, BYPASS_GAMMA, data);
}

static void disp_gamma_wait_sof_irq(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	int ret = 0;

	if (atomic_read(&gamma->primary_data->sof_irq_available) == 0) {
		DDPINFO("wait_event_interruptible\n");
		ret = wait_event_interruptible(gamma->primary_data->sof_irq_wq,
				atomic_read(&gamma->primary_data->sof_irq_available) == 1);
		CRTC_MMP_EVENT_START(0, gamma_sof, 0, 0);
		DDPINFO("sof_irq_available = 1, waken up, ret = %d", ret);
	} else {
		DDPINFO("sof_irq_available = 0");
		return;
	}

	ret = mtk_drm_12bit_gammalut_ioctl_impl(comp);
	if (ret != 0) {
		DDPPR_ERR("%s:12bit gammalut ioctl impl failed!!\n", __func__);
		CRTC_MMP_MARK(0, gamma_sof, 0, 1);
	}

	atomic_set(&gamma->primary_data->sof_filp, 0);
	DDPINFO("set g_gamma_ioctl_lock to 0\n");
	CRTC_MMP_EVENT_END(0, gamma_sof, 0, 2);
}

static int mtk_gamma_sof_irq_trigger(void *data)
{
	struct mtk_ddp_comp *comp = (struct mtk_ddp_comp *)data;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	while (!kthread_should_stop()) {
		disp_gamma_wait_sof_irq(comp);
		atomic_set(&gamma->primary_data->sof_irq_available, 0);
	}

	return 0;
}

void mtk_gamma_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *data = comp_to_gamma(comp);
	struct mtk_disp_gamma *companion_data = comp_to_gamma(data->companion);
	char thread_name[20] = {0};
	struct sched_param param = {.sched_priority = 84 };
	int len = 0;

	if (data->is_right_pipe) {
		kfree(data->primary_data);
		data->primary_data = NULL;
		data->primary_data = companion_data->primary_data;
		return;
	}

	// init primary data
	init_waitqueue_head(&(data->primary_data->sof_irq_wq));
	spin_lock_init(&(data->primary_data->power_lock));
	mutex_init(&data->primary_data->global_lock);
	mutex_init(&data->primary_data->sram_lock);

	memset(&(data->primary_data->sb_param), 0,
			sizeof(data->primary_data->sb_param));
	memset(&(data->primary_data->gamma_lut_db), 0,
			sizeof(data->primary_data->gamma_lut_db));
	memset(&(data->primary_data->gamma_12bit_lut_db), 0,
			sizeof(data->primary_data->gamma_12bit_lut_db));
	memset(&(data->primary_data->ioctl_data), 0,
			sizeof(data->primary_data->ioctl_data));

	atomic_set(&(data->primary_data->irq_event), 0);
	atomic_set(&(data->primary_data->clock_on), 0);
	atomic_set(&(data->primary_data->sof_filp), 0);
	atomic_set(&(data->primary_data->sof_irq_available), 0);
	atomic_set(&(data->primary_data->force_delay_check_trig), 0);

	len = sprintf(thread_name, "gamma_sof_%d", comp->id);
	if (len < 0)
		strcpy(thread_name, "gamma_sof_0");
	data->primary_data->sof_irq_event_task =
		kthread_create(mtk_gamma_sof_irq_trigger,
			comp, thread_name);

	if (sched_setscheduler(data->primary_data->sof_irq_event_task, SCHED_RR, &param))
		pr_notice("gamma_sof_irq_event_task setschedule fail");

	wake_up_process(data->primary_data->sof_irq_event_task);
}

static void mtk_gamma_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;

	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_EN, GAMMA_EN, ~0);
	if (pq_data->new_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS])
		mtk_gamma_write_gain_reg(comp, handle, 0);

	if (gamma->primary_data->data_mode == HW_12BIT_MODE_8BIT ||
		gamma->primary_data->data_mode == HW_12BIT_MODE_12BIT)
		mtk_gamma_write_12bit_lut_reg(comp, handle, 0);
	else
		mtk_gamma_write_lut_reg(comp, handle, 0);
}

int mtk_drm_ioctl_gamma_mul_disable(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_ddp_comp *comp;

	comp = mtk_gamma_get_comp_by_default_crtc(dev);
	if (comp == NULL) {
		DDPPR_ERR("%s, null pointer!\n", __func__);
		return -1;
	}

	return mtk_crtc_user_cmd(&comp->mtk_crtc->base, comp, DISABLE_MUL_EN, data);
}

static void mtk_gamma_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_EN, 0x0, ~0);
}

static void mtk_gamma_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	struct mtk_disp_gamma *data = comp_to_gamma(comp);

	DDPINFO("%s\n", __func__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_CFG, bypass, 0x1);
	data->primary_data->relay_value = bypass;

}

static void mtk_gamma_set(struct mtk_ddp_comp *comp,
			  struct drm_crtc_state *state, struct cmdq_pkt *handle)
{
	unsigned int i;
	struct drm_color_lut *lut;
	u32 word = 0;
	u32 word_first = 0;
	u32 word_last = 0;

	DDPINFO("%s\n", __func__);

	if (state->gamma_lut) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_GAMMA_CFG,
			       1<<GAMMA_LUT_EN, 1<<GAMMA_LUT_EN);
		lut = (struct drm_color_lut *)state->gamma_lut->data;
		for (i = 0; i < MTK_LUT_SIZE; i++) {
			word = GAMMA_ENTRY(lut[i].red >> 6,
				lut[i].green >> 6, lut[i].blue >> 6);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa
				+ (DISP_GAMMA_LUT + i * 4),
				word, ~0);

			// first & last word for
			//	decreasing/incremental LUT
			if (i == 0)
				word_first = word;
			else if (i == MTK_LUT_SIZE - 1)
				word_last = word;
		}
	}
	if ((word_first - word_last) > 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x1 << 2, 0x4);
		DDPINFO("decreasing LUT\n");
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x0 << 2, 0x4);
		DDPINFO("Incremental LUT\n");
	}
}

static void calculateGammaLut(struct DISP_GAMMA_LUT_T *data,
		struct mtk_disp_gamma *gamma)
{
	int i;

	for (i = 0; i < DISP_GAMMA_LUT_SIZE; i++)
		data->lut[i] = (((gamma->primary_data->gamma_lut_db.lut[i] & 0x3ff) *
			gamma->primary_data->sb_param.gain[gain_b] + 4096) / 8192) |
			(((gamma->primary_data->gamma_lut_db.lut[i] >> 10 & 0x3ff) *
			gamma->primary_data->sb_param.gain[gain_g] + 4096) / 8192) << 10 |
			(((gamma->primary_data->gamma_lut_db.lut[i] >> 20 & 0x3ff) *
			gamma->primary_data->sb_param.gain[gain_r] + 4096) / 8192) << 20;

}

static void calculateGamma12bitLut(struct DISP_GAMMA_12BIT_LUT_T *data,
		struct mtk_disp_gamma *gamma)
{
	int i, lut_size = DISP_GAMMA_LUT_SIZE;

	if (gamma->primary_data->data_mode == HW_12BIT_MODE_12BIT)
		lut_size = DISP_GAMMA_12BIT_LUT_SIZE;

	for (i = 0; i < lut_size; i++) {
		data->lut_0[i] =
			(((gamma->primary_data->gamma_12bit_lut_db.lut_0[i] & 0xfff) *
			gamma->primary_data->sb_param.gain[gain_r] + 4096) / 8192) |
			(((gamma->primary_data->gamma_12bit_lut_db.lut_0[i] >> 12 & 0xfff) *
			gamma->primary_data->sb_param.gain[gain_g] + 4096) / 8192) << 12;
		data->lut_1[i] =
			(((gamma->primary_data->gamma_12bit_lut_db.lut_1[i] & 0xfff) *
			gamma->primary_data->sb_param.gain[gain_b] + 4096) / 8192);
	}
}

void mtk_trans_gain_to_gamma(struct mtk_ddp_comp *comp,
	unsigned int gain[3], unsigned int bl, void *param)
{
	int ret;
	unsigned int mmsys_id = 0;
	struct DISP_AAL_ESS20_SPECT_PARAM *ess20_spect_param = param;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);
	struct DISP_GAMMA_LUT_T *lut_8bit_data;
	struct DISP_GAMMA_12BIT_LUT_T *lut_12bit_data;

	if (param == NULL)
		ret = -EFAULT;

	mmsys_id = mtk_get_mmsys_id(crtc);
	if (gamma->primary_data->sb_param.gain[gain_r] != gain[gain_r] ||
		gamma->primary_data->sb_param.gain[gain_g] != gain[gain_g] ||
		gamma->primary_data->sb_param.gain[gain_b] != gain[gain_b]) {

		gamma->primary_data->sb_param.gain[gain_r] = gain[gain_r];
		gamma->primary_data->sb_param.gain[gain_g] = gain[gain_g];
		gamma->primary_data->sb_param.gain[gain_b] = gain[gain_b];

		if (mmsys_id != MMSYS_MT6985) {
			if (gamma->primary_data->data_mode == HW_8BIT) {
				lut_8bit_data = kzalloc(sizeof(struct DISP_GAMMA_LUT_T),
							GFP_KERNEL);
				if (lut_8bit_data) {
					calculateGammaLut(lut_8bit_data, gamma);
					mtk_crtc_user_cmd(crtc, comp,
						SET_GAMMALUT, (void *)lut_8bit_data);
					kfree(lut_8bit_data);
				}
			}

			if (gamma->primary_data->data_mode == HW_12BIT_MODE_8BIT ||
				gamma->primary_data->data_mode == HW_12BIT_MODE_12BIT) {
				lut_12bit_data = kzalloc(sizeof(struct DISP_GAMMA_12BIT_LUT_T),
							GFP_KERNEL);
				if (lut_12bit_data) {
					calculateGamma12bitLut(lut_12bit_data, gamma);
					mtk_crtc_user_cmd(crtc, comp,
						SET_12BIT_GAMMALUT, (void *)lut_12bit_data);
					kfree(lut_12bit_data);
				}
			}
		} else {
			mtk_crtc_user_cmd(crtc, comp,
				SET_GAMMAGAIN, (void *)&gamma->primary_data->sb_param);
		}
		DDPINFO("[aal_kernel]ELVSSPN = %d, flag = %d\n",
			ess20_spect_param->ELVSSPN, ess20_spect_param->flag);
		CRTC_MMP_MARK(0, gamma_backlight, gain[gain_r], bl);
		mtk_leds_brightness_set("lcd-backlight", bl, ess20_spect_param->ELVSSPN,
					ess20_spect_param->flag);

		if (atomic_read(&gamma->primary_data->force_delay_check_trig) == 1)
			mtk_crtc_check_trigger(mtk_crtc, true, true);
		else
			mtk_crtc_check_trigger(mtk_crtc, false, true);
		DDPINFO("%s : gain = %d, backlight = %d\n",
			__func__, gamma->primary_data->sb_param.gain[gain_r], bl);
	} else {
		if ((gamma->primary_data->sb_param.bl != bl) ||
			(ess20_spect_param->flag & (1 << SET_ELVSS_PN))) {
			gamma->primary_data->sb_param.bl = bl;
			mtk_leds_brightness_set("lcd-backlight", bl, ess20_spect_param->ELVSSPN,
						ess20_spect_param->flag);
			CRTC_MMP_MARK(0, gamma_backlight, ess20_spect_param->flag, bl);
			DDPINFO("%s : backlight = %d, flag = %d, elvss = %d\n", __func__, bl,
				ess20_spect_param->flag, ess20_spect_param->ELVSSPN);
		}
	}
}

static int mtk_gamma_user_set_gammalut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data)
{
	struct DISP_GAMMA_LUT_T *config = data;

	if (mtk_gamma_set_lut(comp, handle, config) < 0) {
		DDPPR_ERR("%s: failed\n", __func__);
		return -EFAULT;
	}
	if ((comp->mtk_crtc != NULL) && comp->mtk_crtc->is_dual_pipe) {
		struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

		if (mtk_gamma_set_lut(gamma->companion, handle, config) < 0) {
			DDPPR_ERR("%s: comp_gamma1 failed\n", __func__);
			return -EFAULT;
		}
	}
	return 0;
}

static int mtk_gamma_user_set_12bit_gammalut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data)
{
	struct DISP_GAMMA_12BIT_LUT_T *config = data;

	CRTC_MMP_MARK(0, aal_ess20_gamma, comp->id, 0);
	if (mtk_gamma_12bit_set_lut(comp, handle, config) < 0) {
		DDPPR_ERR("%s: failed\n", __func__);
		return -EFAULT;
	}
	if ((comp->mtk_crtc != NULL) && comp->mtk_crtc->is_dual_pipe) {
		struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

		if (mtk_gamma_12bit_set_lut(gamma->companion, handle, config) < 0) {
			DDPPR_ERR("%s: comp_gamma1 failed\n", __func__);
			return -EFAULT;
		}
	}

	return 0;
}

static int mtk_gamma_user_bypass_gamma(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data)
{
	int *value = data;

	mtk_gamma_bypass(comp, *value, handle);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

		mtk_gamma_bypass(gamma->companion, *value, handle);
	}
	return 0;
}

static int mtk_gamma_user_set_gammagain(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data)
{
	struct mtk_disp_gamma_sb_param *config = data;

	if (mtk_gamma_set_gain(comp, handle, config) < 0)
		return -EFAULT;

	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

		if (mtk_gamma_set_gain(gamma->companion, handle, config) < 0)
			return -EFAULT;
	}
	return 0;
}

static int mtk_gamma_user_disable_mul_en(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_CFG, 0x0 << 3, 0x08);

	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

		cmdq_pkt_write(handle, gamma->companion->cmdq_base,
			gamma->companion->regs_pa + DISP_GAMMA_CFG, 0x0 << 3, 0x08);

	}
	return 0;
}

static int mtk_gamma_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	DDPINFO("%s: cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case SET_GAMMALUT:
	{
		int ret;

		ret = mtk_gamma_user_set_gammalut(comp, handle, data);
		if (ret < 0)
			return ret;
		if (comp->mtk_crtc != NULL)
			mtk_crtc_check_trigger(comp->mtk_crtc, true, false);
	}
	break;
	case SET_12BIT_GAMMALUT:
	{
		int ret;

		ret = mtk_gamma_user_set_12bit_gammalut(comp, handle, data);
		if (ret < 0)
			return ret;
		if (comp->mtk_crtc != NULL)
			mtk_crtc_check_trigger(comp->mtk_crtc, true, false);
	}
	break;
	case BYPASS_GAMMA:
		mtk_gamma_user_bypass_gamma(comp, handle, data);
		break;
	case SET_GAMMAGAIN:
	{
		int ret;

		ret = mtk_gamma_user_set_gammagain(comp, handle, data);
		if (ret < 0)
			return ret;
	}
	break;
	case DISABLE_MUL_EN:
		mtk_gamma_user_disable_mul_en(comp, handle, data);
		break;
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static void ddp_gamma_backup(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	gamma->primary_data->back_up_cfg =
		readl(comp->regs + DISP_GAMMA_CFG);
}

static void ddp_gamma_restore(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	writel(gamma->primary_data->back_up_cfg, comp->regs + DISP_GAMMA_CFG);
}

static void mtk_gamma_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&gamma->primary_data->clock_on, 1);
	ddp_gamma_restore(comp);
}

static void mtk_gamma_unprepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	DDPINFO("%s @ %d......... spin_trylock_irqsave ++ ",
		__func__, __LINE__);
	spin_lock_irqsave(&gamma->primary_data->power_lock, flags);
	DDPINFO("%s @ %d......... spin_trylock_irqsave -- ",
		__func__, __LINE__);
	atomic_set(&gamma->primary_data->clock_on, 0);
	spin_unlock_irqrestore(&gamma->primary_data->power_lock, flags);
	DDPINFO("%s @ %d......... spin_unlock_irqrestore ",
		__func__, __LINE__);
	ddp_gamma_backup(comp);
	mtk_ddp_comp_clk_unprepare(comp);
}

int mtk_gamma_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	      enum mtk_ddp_io_cmd cmd, void *params)
{
	switch (cmd) {
	case FORCE_TRIG_CTL:
	{
		uint32_t force_delay_trigger;
		struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

		force_delay_trigger = *(uint32_t *)params;
		atomic_set(&gamma->primary_data->force_delay_check_trig, force_delay_trigger);
	}
		break;
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_gamma *data = comp_to_gamma(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_gamma *companion_data;

		if (data->is_right_pipe)
			break;
		ret = mtk_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_gamma(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
	}
		break;
	default:
		break;
	}
	return 0;
}

void mtk_gamma_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	mtk_gamma_data_init(comp);
	mtk_gamma_config(comp, cfg, handle);
}

static int mtk_gamma_cfg_set_gammalut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct DISP_GAMMA_LUT_T *config = data;
	struct mtk_disp_gamma *gamma_data = comp_to_gamma(comp);

	gamma_data->primary_data->gamma_lut_db = *((struct DISP_GAMMA_LUT_T *)data);

	if (mtk_gamma_set_lut(comp, handle, config) < 0) {
		DDPPR_ERR("%s: failed\n", __func__);
		return -EFAULT;
	}
	if (comp->mtk_crtc->is_dual_pipe && gamma_data->companion) {
		if (mtk_gamma_set_lut(gamma_data->companion, handle, config) < 0) {
			DDPPR_ERR("%s: comp_gamma1 failed\n", __func__);
			return -EFAULT;
		}
	}
	return 0;
}

static int mtk_gamma_cfg_set_12bit_gammalut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	mutex_lock(&gamma->primary_data->sram_lock);
	CRTC_MMP_EVENT_START(0, gamma_ioctl, 0, 0);
	memcpy(&gamma->primary_data->ioctl_data, (struct DISP_GAMMA_12BIT_LUT_T *)data,
			sizeof(struct DISP_GAMMA_12BIT_LUT_T));
	atomic_set(&gamma->primary_data->sof_filp, 1);
	DDPINFO("%s:update IOCTL g_gamma_sof_filp to 1\n", __func__);
	CRTC_MMP_EVENT_END(0, gamma_ioctl, 0, 1);
	mutex_unlock(&gamma->primary_data->sram_lock);

	return 0;
}

static int mtk_gamma_cfg_bypass_disp_gamma(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_gamma *gamma_data = comp_to_gamma(comp);
	int *value = data;

	mtk_gamma_bypass(comp, *value, handle);
	if (comp->mtk_crtc->is_dual_pipe && gamma_data->companion)
		mtk_gamma_bypass(gamma_data->companion, *value, handle);

	return 0;
}

int mtk_cfg_trans_gain_to_gamma(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle, unsigned int gain[3], unsigned int bl, void *param)
{
	int ret;
	bool support_gammagain;
	struct DISP_AAL_ESS20_SPECT_PARAM *ess20_spect_param = param;
	struct mtk_ddp_comp *comp;
	struct mtk_disp_gamma *gamma_priv;

	if (param == NULL)
		ret = -EFAULT;
	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_GAMMA, 0);
	if (!comp) {
		DDPINFO("[aal_kernel] comp is null\n");
		return -EFAULT;
	}
	gamma_priv = comp_to_gamma(comp);
	support_gammagain = gamma_priv->data->support_gammagain;
	if (gamma_priv->primary_data->sb_param.gain[gain_r] != gain[gain_r] ||
		gamma_priv->primary_data->sb_param.gain[gain_g] != gain[gain_g] ||
		gamma_priv->primary_data->sb_param.gain[gain_b] != gain[gain_b]) {

		gamma_priv->primary_data->sb_param.gain[gain_r] = gain[gain_r];
		gamma_priv->primary_data->sb_param.gain[gain_g] = gain[gain_g];
		gamma_priv->primary_data->sb_param.gain[gain_b] = gain[gain_b];
		if (support_gammagain)
			mtk_gamma_user_set_gammagain(comp, handle,
				(void *)&gamma_priv->primary_data->sb_param);
		else
			DDPINFO("[aal_kernel] gamma gain not support!\n");
		DDPINFO("[aal_kernel]ELVSSPN = %d, flag = %d\n",
			ess20_spect_param->ELVSSPN, ess20_spect_param->flag);
		CRTC_MMP_MARK(0, gamma_backlight, gain[gain_r], bl);
		mtk_leds_brightness_set("lcd-backlight", bl, ess20_spect_param->ELVSSPN,
					ess20_spect_param->flag);
		DDPINFO("%s : gain = %d, backlight = %d\n",
			__func__, gamma_priv->primary_data->sb_param.gain[gain_r], bl);
	} else {
		if ((gamma_priv->primary_data->sb_param.bl != bl)
				|| (ess20_spect_param->flag & (1 << SET_ELVSS_PN))) {
			gamma_priv->primary_data->sb_param.bl = bl;
			CRTC_MMP_MARK(0, gamma_backlight, ess20_spect_param->flag, bl);
			mtk_leds_brightness_set("lcd-backlight", bl, ess20_spect_param->ELVSSPN,
						ess20_spect_param->flag);
			DDPINFO("%s : backlight = %d, flag = %d, elvss = %d\n", __func__, bl,
				ess20_spect_param->flag, ess20_spect_param->ELVSSPN);
		}
	}
	return 0;
}

static int mtk_gamma_cfg_gamma_mul_disable(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_gamma *gamma_priv = comp_to_gamma(comp);
	struct mtk_ddp_comp *companion;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_CFG, 0x0 << 3, 0x08);

	if (comp->mtk_crtc->is_dual_pipe && gamma_priv->companion) {
		companion = gamma_priv->companion;
		cmdq_pkt_write(handle, companion->cmdq_base,
			companion->regs_pa + DISP_GAMMA_CFG, 0x0 << 3, 0x08);
	}
	return 0;
}

static int mtk_gamma_pq_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	/* will only call left path */
	switch (cmd) {
	case PQ_GAMMA_SET_GAMMALUT:
		ret = mtk_gamma_cfg_set_gammalut(comp, handle, data, data_size);
		break;
	case PQ_GAMMA_SET_12BIT_GAMMALUT:
		ret = mtk_gamma_cfg_set_12bit_gammalut(comp, handle, data, data_size);
		break;
	case PQ_GAMMA_BYPASS_GAMMA:
		ret = mtk_gamma_cfg_bypass_disp_gamma(comp, handle, data, data_size);
		break;
	case PQ_GAMMA_DISABLE_MUL_EN:
		ret = mtk_gamma_cfg_gamma_mul_disable(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_gamma_ioctl_transact(struct mtk_ddp_comp *comp,
		unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	/* will only call left path */
	switch (cmd) {
	case PQ_GAMMA_SET_GAMMALUT:
		gamma->primary_data->gamma_lut_db = *((struct DISP_GAMMA_LUT_T *)data);
		return mtk_crtc_user_cmd(&comp->mtk_crtc->base, comp, SET_GAMMALUT, data);
	case PQ_GAMMA_SET_12BIT_GAMMALUT:
		return mtk_drm_set_12bit_gammalut_internal(data, comp);
	case PQ_GAMMA_BYPASS_GAMMA:
		return mtk_crtc_user_cmd(&comp->mtk_crtc->base, comp, BYPASS_GAMMA, data);
	case PQ_GAMMA_DISABLE_MUL_EN:
		return mtk_crtc_user_cmd(&comp->mtk_crtc->base, comp, DISABLE_MUL_EN, data);
	default:
		break;
	}
	return ret;
}

static const struct mtk_ddp_comp_funcs mtk_disp_gamma_funcs = {
	.gamma_set = mtk_gamma_set,
	.config = mtk_gamma_config,
	.first_cfg = mtk_gamma_first_cfg,
	.start = mtk_gamma_start,
	.stop = mtk_gamma_stop,
	.bypass = mtk_gamma_bypass,
	.user_cmd = mtk_gamma_user_cmd,
	.io_cmd = mtk_gamma_io_cmd,
	.prepare = mtk_gamma_prepare,
	.unprepare = mtk_gamma_unprepare,
	.config_overhead = mtk_disp_gamma_config_overhead,
	.pq_frame_config = mtk_gamma_pq_frame_config,
	.pq_ioctl_transact = mtk_gamma_ioctl_transact,
	.mutex_sof_irq = disp_gamma_on_start_of_frame,
};

static int mtk_disp_gamma_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_gamma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_gamma_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_gamma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_gamma_component_ops = {
	.bind = mtk_disp_gamma_bind, .unbind = mtk_disp_gamma_unbind,
};

void mtk_gamma_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x24, 0x28);
	mtk_cust_dump_reg(baddr, 0x54, 0x58, 0x5c, 0x50);
	mtk_cust_dump_reg(baddr, 0x14, 0x20, 0x700, 0xb00);
}

void mtk_gamma_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_gamma *gamma_data = comp_to_gamma(comp);
	void __iomem  *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp),
			&comp->regs_pa);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0; k <= 0xff0; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && gamma_data->companion) {
		baddr = gamma_data->companion->regs;
		DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(gamma_data->companion),
				&gamma_data->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(gamma_data->companion));
		for (k = 0; k <= 0xff0; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(gamma_data->companion));
	}
}

static void mtk_disp_gamma_dts_parse(const struct device_node *np,
	struct mtk_ddp_comp *comp)
{
	struct gamma_color_protect_mode color_protect_mode;
	struct mtk_disp_gamma *gamma = comp_to_gamma(comp);

	if (of_property_read_u32(np, "gamma-data-mode",
		&gamma->primary_data->data_mode)) {
		DDPPR_ERR("comp_id: %d, gamma_data_mode = %d\n",
			comp->id, gamma->primary_data->data_mode);
		gamma->primary_data->data_mode = HW_8BIT;
	}

	if (of_property_read_u32(np, "color-protect-lsb",
		&gamma->primary_data->color_protect.gamma_color_protect_lsb)) {
		DDPPR_ERR("comp_id: %d, color_protect_lsb = %d\n",
			comp->id, gamma->primary_data->color_protect.gamma_color_protect_lsb);
		gamma->primary_data->color_protect.gamma_color_protect_lsb = 0;
	}

	if (of_property_read_u32(np, "color-protect-red",
		&color_protect_mode.red_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_red = %d\n",
			comp->id, color_protect_mode.red_support);
		color_protect_mode.red_support = 0;
	}

	if (of_property_read_u32(np, "color-protect-green",
		&color_protect_mode.green_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_green = %d\n",
			comp->id, color_protect_mode.green_support);
		color_protect_mode.green_support = 0;
	}

	if (of_property_read_u32(np, "color-protect-blue",
		&color_protect_mode.blue_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_blue = %d\n",
			comp->id, color_protect_mode.blue_support);
		color_protect_mode.blue_support = 0;
	}

	if (of_property_read_u32(np, "color-protect-black",
		&color_protect_mode.black_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_black = %d\n",
			comp->id, color_protect_mode.black_support);
		color_protect_mode.black_support = 0;
	}

	if (of_property_read_u32(np, "color-protect-white",
		&color_protect_mode.white_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_white = %d\n",
			comp->id, color_protect_mode.white_support);
		color_protect_mode.white_support = 0;
	}

	gamma->primary_data->color_protect.gamma_color_protect_support =
		color_protect_mode.red_support << 4 |
		color_protect_mode.green_support << 5 |
		color_protect_mode.blue_support << 6 |
		color_protect_mode.black_support << 7 |
		color_protect_mode.white_support << 8;
}

static int mtk_disp_gamma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_gamma *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		DDPPR_ERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_GAMMA);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
		goto error_primary;
	}

	mtk_disp_gamma_dts_parse(dev->of_node, &priv->ddp_comp);

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_gamma_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_gamma_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);
error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static int mtk_disp_gamma_remove(struct platform_device *pdev)
{
	struct mtk_disp_gamma *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_gamma_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

struct mtk_disp_gamma_data legacy_driver_data = {
	.support_gammagain = false,
};

struct mtk_disp_gamma_data mt6985_driver_data = {
	.support_gammagain = true,
};

struct mtk_disp_gamma_data mt6897_driver_data = {
	.support_gammagain = true,
};

struct mtk_disp_gamma_data mt6989_driver_data = {
	.support_gammagain = true,
};

static const struct of_device_id mtk_disp_gamma_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6779-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6885-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6873-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6853-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6833-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6983-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6895-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6879-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6855-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6985-disp-gamma",
	  .data = &mt6985_driver_data,},
	{ .compatible = "mediatek,mt6886-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6835-disp-gamma",
	  .data = &legacy_driver_data,},
	{ .compatible = "mediatek,mt6897-disp-gamma",
	  .data = &mt6897_driver_data,},
	{ .compatible = "mediatek,mt6989-disp-gamma",
	  .data = &mt6989_driver_data,},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_gamma_driver_dt_match);

struct platform_driver mtk_disp_gamma_driver = {
	.probe = mtk_disp_gamma_probe,
	.remove = mtk_disp_gamma_remove,
	.driver = {

			.name = "mediatek-disp-gamma",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_gamma_driver_dt_match,
		},
};

void disp_gamma_set_bypass(struct drm_crtc *crtc, int bypass)
{
	int ret;
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(to_mtk_crtc(crtc), MTK_DISP_GAMMA, 0);

	ret = mtk_crtc_user_cmd(crtc, comp, BYPASS_GAMMA, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}
