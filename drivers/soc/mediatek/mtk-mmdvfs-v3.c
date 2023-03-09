// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include <soc/mediatek/smi.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/rpmsg.h>
#include <linux/rpmsg/mtk_rpmsg.h>
#include <linux/iommu.h>
#include <linux/workqueue.h>

#include "clk-fmeter.h"
#include "clk-mtk.h"
#include "mtk-mmdvfs-v3.h"

#include "vcp.h"
#include "vcp_reg.h"
#include "vcp_status.h"

#include "mtk-mmdvfs-v3-memory.h"

#include "../../misc/mediatek/smi/mtk-smi-dbg.h"

static u8 mmdvfs_clk_num;
static struct mtk_mmdvfs_clk *mtk_mmdvfs_clks;

static u8 mmdvfs_pwr_opp[PWR_MMDVFS_NUM];
static struct clk *mmdvfs_pwr_clk[PWR_MMDVFS_NUM];

static phys_addr_t mmdvfs_memory_iova;
static phys_addr_t mmdvfs_memory_pa;
static void *mmdvfs_memory_va;

static bool mmdvfs_free_run;
static bool mmdvfs_init_done;

static int vcp_power;
static int vcp_pwr_usage[VCP_PWR_USR_NUM];
static DEFINE_MUTEX(mmdvfs_vcp_pwr_mutex);
static struct workqueue_struct *vmm_notify_wq;

static bool mmdvfs_vcp_cb_ready;
static int mmdvfs_ipi_status;
static DEFINE_MUTEX(mmdvfs_vcp_ipi_mutex);
static struct ipi_callbacks clkmux_cb;
static struct notifier_block vcp_ready_notifier;
static struct notifier_block force_on_notifier;
static struct notifier_block disp_pd_notifier;

static int ccu_power;
static int ccu_pwr_usage[CCU_PWR_USR_NUM];
static DEFINE_MUTEX(mmdvfs_ccu_pwr_mutex);
static struct rproc *ccu_rproc;
static struct platform_device *ccu_pdev;

static struct device *mmdvfs_v3_dev;
static int vmm_power;
static DEFINE_MUTEX(mmdvfs_vmm_pwr_mutex);
static int last_vote_step[PWR_MMDVFS_NUM];
static int last_force_step[PWR_MMDVFS_NUM];
static int dpsw_thr;
static int vmm_ceil_step;

enum {
	log_pwr,
	log_ipi,
	log_clk_ops,
	log_adb,
};
static int log_level;
static int vcp_log_level;
static int vmrc_log_level;

static call_ccu call_ccu_fp;

void mmdvfs_call_ccu_set_fp(call_ccu fp)
{
	call_ccu_fp = fp;
}
EXPORT_SYMBOL_GPL(mmdvfs_call_ccu_set_fp);

void *mmdvfs_get_vcp_base(phys_addr_t *pa)
{
	if (pa)
		*pa = mmdvfs_memory_pa;
	return mmdvfs_memory_va;
}
EXPORT_SYMBOL_GPL(mmdvfs_get_vcp_base);

bool mmdvfs_is_init_done(void)
{
	return mmdvfs_free_run ? mmdvfs_init_done : false;
}
EXPORT_SYMBOL_GPL(mmdvfs_is_init_done);

int mtk_mmdvfs_get_ipi_status(void)
{
	u64 tick = sched_clock();

	while (mmdvfs_ipi_status)
		if (sched_clock() - tick >= 60000000) // 60ms
			break;

	return mmdvfs_ipi_status;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_get_ipi_status);

int mtk_mmdvfs_enable_vcp(const bool enable, const u8 idx)
{
	int ret = 0;

	if (!mmdvfs_free_run)
		return 0;

	if (is_vcp_suspending_ex())
		return -EBUSY;

	mutex_lock(&mmdvfs_vcp_pwr_mutex);
	if (enable) {
		if (!vcp_power) {
			ret = vcp_register_feature_ex(MMDVFS_FEATURE_ID);
			if (ret)
				goto enable_vcp_end;
		}
		vcp_power += 1;
		vcp_pwr_usage[idx] += 1;
	} else {
		if (!vcp_pwr_usage[idx] || !vcp_power) {
			ret = -EINVAL;
			goto enable_vcp_end;
		}
		if (vcp_power == 1) {
			mutex_lock(&mmdvfs_vcp_ipi_mutex);
			mutex_unlock(&mmdvfs_vcp_ipi_mutex);
			ret = vcp_deregister_feature_ex(MMDVFS_FEATURE_ID);
			if (ret)
				goto enable_vcp_end;
		}
		vcp_pwr_usage[idx] -= 1;
		vcp_power -= 1;
	}

enable_vcp_end:
	if (ret || (log_level & (1 << log_pwr)))
		MMDVFS_ERR("ret:%d enable:%d vcp_power:%d idx:%hhu usage:%d",
			ret, enable, vcp_power, idx, vcp_pwr_usage[idx]);
	mutex_unlock(&mmdvfs_vcp_pwr_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_enable_vcp);

int mtk_mmdvfs_enable_ccu(const bool enable, const u8 idx)
{
	int ret = 0;

	if (!mmdvfs_free_run || !ccu_rproc)
		return 0;

	mutex_lock(&mmdvfs_ccu_pwr_mutex);
	if (enable) {
		if (!ccu_power) {
#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG)
			ret = rproc_bootx(ccu_rproc, RPROC_UID_MMDVFS);
#else
			ret = rproc_boot(ccu_rproc);
#endif
			if (ret)
				goto enable_ccu_end;
		}
		ccu_power += 1;
		ccu_pwr_usage[idx] += 1;
	} else {
		if (!ccu_pwr_usage[idx] || !ccu_power) {
			ret = -EINVAL;
			goto enable_ccu_end;
		}
		if (ccu_power == 1)
#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG)
			rproc_shutdownx(ccu_rproc, RPROC_UID_MMDVFS);
#else
			rproc_shutdown(ccu_rproc);
#endif
		ccu_pwr_usage[idx] -= 1;
		ccu_power -= 1;
	}

enable_ccu_end:
	if (ret || (log_level & (1 << log_pwr)))
		MMDVFS_ERR("ret:%d enable:%d ccu_power:%d idx:%hhu usage:%d",
			ret, enable, ccu_power, idx, ccu_pwr_usage[idx]);
	mutex_unlock(&mmdvfs_ccu_pwr_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_enable_ccu);

static int set_clkmux_memory(const u8 idx, const u8 enable)
{
	u32 val = readl(MEM_CLKMUX_ENABLE);

	if ((enable && (val & (1 << idx))) || (!enable && !(val & (1 << idx))))
		return -EINVAL;

	writel(enable ? (val | (1 << idx)) : (val & ~(1 << idx)), MEM_CLKMUX_ENABLE);
	return 0;
}

static int mmdvfs_vcp_ipi_send(const u8 func, const u8 idx, const u8 opp, u32 *data) // ap > vcp
{
	struct mmdvfs_ipi_data slot = {
		func, idx, opp, mmdvfs_memory_iova >> 32, (u32)mmdvfs_memory_iova};
	int gen, ret = 0, retry = 0;
	static u8 times;
	u32 val;

	if (!mmdvfs_is_init_done())
		return -ENODEV;

	mutex_lock(&mmdvfs_vcp_ipi_mutex);
	switch (func) {
	case FUNC_VMM_CEIL_ENABLE:
		writel(opp, MEM_VMM_CEIL_ENABLE);
		break;
	case FUNC_VMM_GENPD_NOTIFY:
		if (idx >= VMM_USR_NUM) {
			ret = -EINVAL;
			goto ipi_lock_end;
		}
		writel(opp, MEM_GENPD_ENABLE_USR(idx));
		break;
	case FUNC_VMM_AVS_UPDATE:
		if (idx >= VMM_USR_NUM) {
			ret = -EINVAL;
			goto ipi_lock_end;
		}
		writel(data[0], MEM_AGING_CNT_USR(idx));
		writel(data[1], MEM_FRESH_CNT_USR(idx));
		break;
	case FUNC_FORCE_OPP:
		writel(opp, MEM_FORCE_OPP_PWR(idx));
		break;
	case FUNC_VOTE_OPP:
		writel(opp, MEM_VOTE_OPP_USR(idx));
		break;
	case FUNC_MMDVFS_LP_MODE:
		writel(idx, MEM_MMDVFS_LP_MODE);
		break;
	}
	val = readl(MEM_IPI_SYNC_FUNC);
	mutex_unlock(&mmdvfs_vcp_ipi_mutex);

	while (!is_vcp_ready_ex(VCP_A_ID) || !mmdvfs_vcp_cb_ready) {
		if (!mmdvfs_vcp_cb_ready &&
			(func == FUNC_MMDVFS_INIT || func == FUNC_MMDVFSRC_INIT))
			break;
		if (func == FUNC_VMM_GENPD_NOTIFY || func == FUNC_VMM_CEIL_ENABLE)
			goto ipi_send_end;
		if (++retry > 100) {
			ret = -ETIMEDOUT;
			goto ipi_send_end;
		}
		usleep_range(1000, 2000);
	}

	mutex_lock(&mmdvfs_vcp_ipi_mutex);
	writel(0, MEM_IPI_SYNC_DATA);
	writel(val | (1 << func), MEM_IPI_SYNC_FUNC);
	gen = vcp_cmd_ex(VCP_GET_GEN);

	ret = mtk_ipi_send(vcp_get_ipidev(), IPI_OUT_MMDVFS, IPI_SEND_WAIT,
		&slot, PIN_OUT_SIZE_MMDVFS, IPI_TIMEOUT_MS);
	if (ret != IPI_ACTION_DONE)
		goto ipi_lock_end;

	retry = 0;
	while (!(readl(MEM_IPI_SYNC_DATA) & (1 << func))) {
		if (++retry > 10000) {
			ret = IPI_COMPL_TIMEOUT;
			break;
		}
		if (!is_vcp_ready_ex(VCP_A_ID)) {
			ret = -ETIMEDOUT;
			break;
		}
		udelay(10);
	}

	if (!ret)
		writel(val & ~readl(MEM_IPI_SYNC_DATA), MEM_IPI_SYNC_FUNC);
	else if (gen == vcp_cmd_ex(VCP_GET_GEN)) {
		if (!times)
			vcp_cmd_ex(VCP_SET_HALT);
		times += 1;
	}

ipi_lock_end:
	val = readl(MEM_IPI_SYNC_FUNC);
	mutex_unlock(&mmdvfs_vcp_ipi_mutex);

ipi_send_end:
	if (ret || (log_level & (1 << log_ipi)))
		MMDVFS_ERR(
			"ret:%d retry:%d ready:%d cb_ready:%d slot:%#llx vcp_power:%d unfinish func:%#x",
			ret, retry, is_vcp_ready_ex(VCP_A_ID), mmdvfs_vcp_cb_ready,
			*(u64 *)&slot, vcp_power, val);
	mmdvfs_ipi_status = ret;
	return ret;
}

static int mtk_mmdvfs_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct mtk_mmdvfs_clk *clk = container_of(hw, typeof(*clk), clk_hw);
	u8 opp, pwr_opp = MAX_OPP, user_opp = MAX_OPP;
	u32 img_clk = rate / 1000000UL;
	int i, ret = 0, retry = 0;

	if (!mmdvfs_is_init_done())
		return 0;

	for (i = 0; i < clk->freq_num; i++)
		if (rate <= clk->freqs[i])
			break;

	opp = (clk->freq_num - ((i == clk->freq_num) ? (i - 1) : i) - 1);
	if (log_level & (1 << log_clk_ops))
		MMDVFS_DBG("user_id:%hhu clk_id:%hhu opp:%hhu rate:%lu opp:%hhu",
			clk->user_id, clk->clk_id, clk->opp, rate, opp);

	if (clk->opp == opp)
		return 0;
	clk->opp = opp;

	/* Choose max step among all users of special independence */
	if (clk->spec_type == SPEC_MMDVFS_ALONE) {
		for (i = 0; i < mmdvfs_clk_num; i++)
			if (clk->user_id == mtk_mmdvfs_clks[i].user_id &&
				mtk_mmdvfs_clks[i].opp < user_opp)
				user_opp = mtk_mmdvfs_clks[i].opp;
		ret = mmdvfs_vcp_ipi_send(FUNC_VOTE_OPP, clk->user_id, user_opp, NULL);
		goto set_rate_end;
	}

	// spec_type != SPEC_MMDVFS_ALONE
	for (i = 0; i < mmdvfs_clk_num; i++)
		if (clk->pwr_id == mtk_mmdvfs_clks[i].pwr_id && mtk_mmdvfs_clks[i].opp < pwr_opp &&
			mtk_mmdvfs_clks[i].spec_type != SPEC_MMDVFS_ALONE)
			pwr_opp = mtk_mmdvfs_clks[i].opp;

	if (pwr_opp == mmdvfs_pwr_opp[clk->pwr_id])
		return 0;
	mmdvfs_pwr_opp[clk->pwr_id] = pwr_opp;


	while (!is_vcp_ready_ex(VCP_A_ID) || !mmdvfs_vcp_cb_ready) {
		if (++retry > 100) {
			ret = -ETIMEDOUT;
			goto set_rate_end;
		}
		usleep_range(1000, 2000);
	}

	if (clk->ipi_type == IPI_MMDVFS_CCU) {
		writel(clk->opp, MEM_VOTE_OPP_USR(clk->user_id));
		if (call_ccu_fp)
			ret = call_ccu_fp(
				ccu_pdev, MTK_CCU_FEATURE_ISPDVFS, /* DVFS_IMG_CLK */ 4,
				(void *)&img_clk, sizeof(img_clk));
	} else
		ret = mmdvfs_vcp_ipi_send(FUNC_VOTE_OPP, clk->user_id, pwr_opp, NULL);

set_rate_end:
	if (ret || (log_level & (1 << log_clk_ops)))
		MMDVFS_ERR(
			"ret:%d retry:%d ready:%d cb_ready:%d user_id:%hhu clk_id:%hhu opp:%hhu rate:%lu opp:%hhu pwr_opp:%hhu user_opp:%hhu img_clk:%u",
			ret, retry, is_vcp_ready_ex(VCP_A_ID), mmdvfs_vcp_cb_ready,
			clk->user_id, clk->clk_id, clk->opp, rate, opp, pwr_opp, user_opp, img_clk);
	return ret;
}

static long mtk_mmdvfs_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *parent_rate)
{
	struct mtk_mmdvfs_clk *clk = container_of(hw, typeof(*clk), clk_hw);

	if (log_level & (1 << log_clk_ops))
		MMDVFS_DBG("clk_id:%hhu opp:%hhu rate:%lu", clk->clk_id, clk->opp, rate);
	return rate;
}

static unsigned long mtk_mmdvfs_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct mtk_mmdvfs_clk *clk = container_of(hw, typeof(*clk), clk_hw);

	if (clk->opp == MAX_OPP)
		return 0;

	if (log_level & (1 << log_clk_ops))
		MMDVFS_DBG("clk_id:%hhu opp:%hhu rate:%lu freq:%u",
			clk->clk_id, clk->opp, parent_rate,
			clk->freqs[clk->freq_num - clk->opp - 1]);
	return clk->freqs[clk->freq_num - clk->opp - 1];
}

static const struct clk_ops mtk_mmdvfs_req_ops = {
	.set_rate	= mtk_mmdvfs_set_rate,
	.round_rate	= mtk_mmdvfs_round_rate,
	.recalc_rate	= mtk_mmdvfs_recalc_rate,
};

int mtk_mmdvfs_camera_notify(const bool enable)
{
	mmdvfs_vcp_ipi_send(FUNC_VMM_CEIL_ENABLE, MAX_OPP, enable ? 1 : 0, NULL);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_camera_notify);

void vmm_notify_work_func(struct work_struct *work)
{
	struct mmdvfs_vmm_notify_work *vmm_notify_work =
		container_of(work, struct mmdvfs_vmm_notify_work, vmm_notify_work);

	mtk_mmdvfs_enable_vcp(vmm_notify_work->enable, VCP_PWR_USR_MMDVFS_GENPD);
	kfree(vmm_notify_work);
}

int mtk_mmdvfs_genpd_notify(const u8 idx, const bool enable)
{
	struct mmdvfs_vmm_notify_work *work;

	if (!mmdvfs_is_init_done())
		return 0;

	mmdvfs_vcp_ipi_send(FUNC_VMM_GENPD_NOTIFY, idx, enable ? 1 : 0, NULL);

	if (!vmm_notify_wq)
		return 0;

	work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (!work)
		return -ENOMEM;

	work->enable = enable;
	INIT_WORK(&work->vmm_notify_work, vmm_notify_work_func);
	queue_work(vmm_notify_wq, &work->vmm_notify_work);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_genpd_notify);

int mtk_mmdvfs_set_avs(const u8 idx, const u32 aging, const u32 fresh)
{
	u32 data[2] = {aging, fresh};
	int ret;

	ret = mmdvfs_vcp_ipi_send(FUNC_VMM_AVS_UPDATE, idx, MAX_OPP, (u32 *)&data);
	MMDVFS_DBG("ret:%d idx:%hhu aging:%u fresh:%u", ret, idx, aging, fresh);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_set_avs);

int vmm_avs_debug_dump(char *buf, const struct kernel_param *kp)
{
	int len = 0;
	u32 avs, i;

	if (!MEM_BASE) {
		MMDVFS_ERR("mmdvfs share memory is not ready");
		return 0;
	}

	// power opp
	len += snprintf(buf + len, PAGE_SIZE - len, "efuse_low:%#x, efuse_high:%#x\n",
		readl(MEM_VMM_EFUSE_LOW), readl(MEM_VMM_EFUSE_HIGH));
	for (i = 0; i < 8; i++)
		len += snprintf(buf + len, PAGE_SIZE - len,
			"opp_level%u: %u\n", i, readl(MEM_VMM_OPP_VOLT(i)));

	i = readl(MEM_REC_VMM_DBG_CNT);
	if (i > 0)
		i = (i - 1) % MEM_REC_CNT_MAX;

	avs = readl(MEM_REC_VMM_AVS(i));
	len += snprintf(buf + len, PAGE_SIZE - len,
		"temp:%#x volt:%u zone:%u opp_level:%u vde_on:%u isp_on:%u\n",
		readl(MEM_REC_VMM_TEMP(i)), readl(MEM_REC_VMM_VOLT(i)), (avs & 0xff),
		((avs >> 8) & 0xff), ((avs >> 16) & 0x1), ((avs >> 17) & 0x1));

	return len;
}

static struct kernel_param_ops vmm_avs_debug_dump_ops = {
	.get = vmm_avs_debug_dump,
};
module_param_cb(avs_debug, &vmm_avs_debug_dump_ops, NULL, 0644);
MODULE_PARM_DESC(avs_debug, "dump avs debug information");

static int mtk_mmdvfs_enable_vmm(const bool enable)
{
	int ret = 0;

	if (!mmdvfs_v3_dev)
		return 0;

	mutex_lock(&mmdvfs_vmm_pwr_mutex);
	if (enable) {
		if (!vmm_power) {
			ret = pm_runtime_resume_and_get(mmdvfs_v3_dev);
			if (ret)
				goto enable_vmm_end;
		}
		vmm_power += 1;
	} else {
		if (!vmm_power) {
			ret = -EINVAL;
			goto enable_vmm_end;
		}
		if (vmm_power == 1)
			pm_runtime_put_sync(mmdvfs_v3_dev);
		vmm_power -= 1;
	}

enable_vmm_end:
	if (ret || (log_level & (1 << log_pwr)))
		MMDVFS_ERR("ret:%d enable:%d vmm_power:%d", ret, enable, vmm_power);
	mutex_unlock(&mmdvfs_vmm_pwr_mutex);
	return ret;
}

static int set_vmm_enable(const char *val, const struct kernel_param *kp)
{
	int enable = 0, ret;

	ret = kstrtoint(val, 0, &enable);
	if (ret) {
		MMDVFS_ERR("failed:%d enable:%d", ret, enable);
		return ret;
	}
	return mtk_mmdvfs_enable_vmm(enable);
}

static struct kernel_param_ops enable_vmm_ops = {
	.set = set_vmm_enable,
};
module_param_cb(enable_vmm, &enable_vmm_ops, NULL, 0644);
MODULE_PARM_DESC(enable_vmm, "enable vmm");

static int mtk_mmdvfs_v3_set_vmm_ceil_step(const bool enable)
{
	MMDVFS_DBG("enable:%u start", enable);
	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMQOS);
	mtk_mmdvfs_camera_notify(enable);
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMQOS);
	MMDVFS_DBG("enable:%u end", enable);
	return 0;
}

int mmdvfs_vmm_ceil_step(const char *val, const struct kernel_param *kp)
{
	int result;
	bool enable;

	result = kstrtobool(val, &enable);
	if (result) {
		MMDVFS_DBG("fail ret:%d\n", result);
		return result;
	}

	mtk_mmdvfs_v3_set_vmm_ceil_step(enable);
	return 0;
}

static struct kernel_param_ops mmdvfs_vmm_ceil_ops = {
	.set = mmdvfs_vmm_ceil_step,
};
module_param_cb(vmm_ceil, &mmdvfs_vmm_ceil_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_ceil, "enable vmm ceiling");

int mtk_mmdvfs_v3_set_force_step(const u16 pwr_idx, const s16 opp)
{
	int *last, ret;

	if (pwr_idx >= PWR_MMDVFS_NUM || opp >= MAX_OPP) {
		MMDVFS_ERR("wrong pwr_idx:%hu opp:%hd", pwr_idx, opp);
		return -EINVAL;
	}

	last = &last_force_step[pwr_idx];

	if (*last == opp)
		return 0;

	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_FORCE);
	if (dpsw_thr > 0 && (*last < 0 || *last >= dpsw_thr) &&
		opp >= 0 && opp < dpsw_thr && pwr_idx == PWR_MMDVFS_VMM)
		mtk_mmdvfs_enable_vmm(true);

	ret = mmdvfs_vcp_ipi_send(FUNC_FORCE_OPP, pwr_idx, opp == -1 ? MAX_OPP : opp, NULL);

	if (dpsw_thr > 0 && *last >= 0 && *last < dpsw_thr &&
		(opp < 0 || opp >= dpsw_thr) && pwr_idx == PWR_MMDVFS_VMM)
		mtk_mmdvfs_enable_vmm(false);
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_FORCE);
	*last = opp;

	if (ret || log_level & (1 << log_adb))
		MMDVFS_DBG("pwr_idx:%hu opp:%hd ret:%d", pwr_idx, opp, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_v3_set_force_step);

static int mmdvfs_set_force_step(const char *val, const struct kernel_param *kp)
{
	u16 idx = 0;
	s16 opp = 0;
	int ret;

	ret = sscanf(val, "%hu %hd", &idx, &opp);
	if (ret != 2 || idx >= PWR_MMDVFS_NUM || opp >= MAX_OPP) {
		MMDVFS_ERR("input failed:%d idx:%hu opp:%hd", ret, idx, opp);
		return -EINVAL;
	}

	ret = mtk_mmdvfs_v3_set_force_step(idx, opp);
	if (ret)
		MMDVFS_ERR("failed:%d idx:%hu opp:%hd", ret, idx, opp);
	return ret;
}

static struct kernel_param_ops mmdvfs_force_step_ops = {
	.set = mmdvfs_set_force_step,
};
module_param_cb(force_step, &mmdvfs_force_step_ops, NULL, 0644);
MODULE_PARM_DESC(force_step, "force mmdvfs to specified step");

int mtk_mmdvfs_v3_set_vote_step(const u16 pwr_idx, const s16 opp)
{
	u32 freq = 0;
	int i, *last, ret = 0;

	if (pwr_idx >= PWR_MMDVFS_NUM || opp >= MAX_OPP) {
		MMDVFS_ERR("failed:%d pwr_idx:%hu opp:%hd", ret, pwr_idx, opp);
		return -EINVAL;
	}

	last = &last_vote_step[pwr_idx];

	if (*last == opp)
		return 0;

	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_VOTE);
	if (dpsw_thr > 0 && (*last < 0 || *last >= dpsw_thr) &&
		opp >= 0 && opp < dpsw_thr && pwr_idx == PWR_MMDVFS_VMM)
		mtk_mmdvfs_enable_vmm(true);

	for (i = mmdvfs_clk_num - 1; i >= 0; i--)
		if (pwr_idx == mtk_mmdvfs_clks[i].pwr_id) {
			if (opp >= mtk_mmdvfs_clks[i].freq_num) {
				MMDVFS_ERR("i:%d invalid opp:%hd freq_num:%hhu",
					i, opp, mtk_mmdvfs_clks[i].freq_num);
				break;
			}

			freq = (opp == -1) ? 0 : mtk_mmdvfs_clks[i].freqs[
				mtk_mmdvfs_clks[i].freq_num - 1 - opp];
			ret = clk_set_rate(mmdvfs_pwr_clk[pwr_idx], freq);
			break;
		}

	if (dpsw_thr > 0 && *last >= 0 && *last < dpsw_thr &&
		(opp < 0 || opp >= dpsw_thr) && pwr_idx == PWR_MMDVFS_VMM)
		mtk_mmdvfs_enable_vmm(false);
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_VOTE);
	*last = opp;

	if (ret || log_level & (1 << log_adb))
		MMDVFS_DBG("pwr_idx:%hu opp:%hd i:%d freq:%u ret:%d", pwr_idx, opp, i, freq, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_v3_set_vote_step);

static int mmdvfs_set_vote_step(const char *val, const struct kernel_param *kp)
{
	int ret;
	u16 idx = 0;
	s16 opp = 0;

	ret = sscanf(val, "%hu %hd", &idx, &opp);
	if (ret != 2 || idx >= PWR_MMDVFS_NUM || opp >= MAX_OPP) {
		MMDVFS_ERR("failed:%d idx:%hu opp:%hd", ret, idx, opp);
		return -EINVAL;
	}

	ret = mtk_mmdvfs_v3_set_vote_step(idx, opp);
	if (ret)
		MMDVFS_ERR("failed:%d idx:%hu opp:%hd", ret, idx, opp);
	return ret;
}

static struct kernel_param_ops mmdvfs_vote_step_ops = {
	.set = mmdvfs_set_vote_step,
};
module_param_cb(vote_step, &mmdvfs_vote_step_ops, NULL, 0644);
MODULE_PARM_DESC(vote_step, "vote mmdvfs to specified step");

int mmdvfs_set_ccu_ipi(const char *val, const struct kernel_param *kp)
{
	int freq = 0, ret = 0, retry = 0;

	ret = kstrtou32(val, 0, &freq);
	if (ret) {
		MMDVFS_ERR("failed:%d freq:%d", ret, freq);
		return ret;
	}

	if (!ccu_pdev)
		return 0;

	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_CCU);
	while (!is_vcp_ready_ex(VCP_A_ID)) {
		if (++retry > 100) {
			MMDVFS_ERR("VCP_A_ID:%d not ready", VCP_A_ID);
			return -ETIMEDOUT;
		}
		usleep_range(1000, 2000);
	}

	mtk_mmdvfs_enable_ccu(true, CCU_PWR_USR_MMDVFS);
	if (call_ccu_fp)
		ret = call_ccu_fp(ccu_pdev, MTK_CCU_FEATURE_ISPDVFS, /* DVFS_IMG_CLK */ 4,
			(void *)&freq, sizeof(freq));
	mtk_mmdvfs_enable_ccu(false, CCU_PWR_USR_MMDVFS);
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_CCU);

	if (ret || log_level & (1 << log_adb))
		MMDVFS_DBG("ret:%d freq:%d", ret, freq);
	return ret;
}

static struct kernel_param_ops mmdvfs_ccu_ipi_ops = {
	.set = mmdvfs_set_ccu_ipi,
};
module_param_cb(ccu_ipi_test, &mmdvfs_ccu_ipi_ops, NULL, 0644);
MODULE_PARM_DESC(ccu_ipi_test, "trigger ccu ipi test");

int mmdvfs_get_vcp_log(char *buf, const struct kernel_param *kp)
{
	int len = 0, ret;

	if (!mmdvfs_is_init_done())
		return 0;

	ret = readl(MEM_LOG_FLAG);
	len += snprintf(buf + len, PAGE_SIZE - len, "MEM_LOG_FLAG:%#x", ret);
	return len;
}

static int mtk_mmdvfs_v3_set_vcp_log(const u32 log)
{
	if (!mmdvfs_is_init_done()) {
		MMDVFS_ERR("mmdvfs is not init done");
		return -1;
	}

	writel(log, MEM_LOG_FLAG);
	return 0;
}

int mmdvfs_set_vcp_log(const char *val, const struct kernel_param *kp)
{
	u32 log = 0;
	int ret;

	ret = kstrtou32(val, 0, &log);
	if (ret || log >= (1 << LOG_NUM)) {
		MMDVFS_ERR("failed:%d log:%#x", ret, log);
		return ret;
	}

	ret = mtk_mmdvfs_v3_set_vcp_log(log);
	if (ret)
		MMDVFS_ERR("set vcp log failed:%d log:%#x", ret, log);

	return ret;
}

static struct kernel_param_ops mmdvfs_set_vcp_log_ops = {
	.get = mmdvfs_get_vcp_log,
	.set = mmdvfs_set_vcp_log,
};
module_param_cb(vcp_log, &mmdvfs_set_vcp_log_ops, NULL, 0644);
MODULE_PARM_DESC(vcp_log, "mmdvfs vcp log");

int mmdvfs_get_vmrc_log(char *buf, const struct kernel_param *kp)
{
	int len = 0, ret;

	if (!mmdvfs_is_init_done())
		return 0;

	ret = readl(MEM_VMRC_LOG_FLAG);
	len += snprintf(buf + len, PAGE_SIZE - len, "MEM_VMRC_LOG_FLAG:%#x", ret);
	return len;
}

static int mtk_mmdvfs_v3_set_vmrc_log(const u32 log)
{
	if (!mmdvfs_is_init_done()) {
		MMDVFS_ERR("mmdvfs is not init done");
		return -1;
	}

	writel(log, MEM_VMRC_LOG_FLAG);
	return 0;
}

int mmdvfs_set_vmrc_log(const char *val, const struct kernel_param *kp)
{
	u32 log = 0;
	int ret;

	ret = kstrtou32(val, 0, &log);
	if (ret || log >= (1 << LOG_NUM)) {
		MMDVFS_ERR("failed:%d log:%#x", ret, log);
		return ret;
	}

	ret = mtk_mmdvfs_v3_set_vmrc_log(log);
	if (ret)
		MMDVFS_ERR("set vmrc log failed:%d log:%#x", ret, log);

	return ret;
}

static const struct kernel_param_ops mmdvfs_set_vmrc_log_ops = {
	.get = mmdvfs_get_vmrc_log,
	.set = mmdvfs_set_vmrc_log,
};
module_param_cb(vmrc_log, &mmdvfs_set_vmrc_log_ops, NULL, 0644);
MODULE_PARM_DESC(vmrc_log, "mmdvfs vmrc log");

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "mmdvfs log level");

struct mmdvfs_mux {
	u8 id;
	const char *name;
	const char *target_name;
	u8 freq_num;
	u64 freq[MAX_OPP];
	u64 rate;
	s8 opp;
	s8 last;
	u8 user_num;
	struct mtk_mux_user *user[MMDVFS_USER_NUM];
};

static struct mmdvfs_mux mmdvfs_mux[MMDVFS_MUX_NUM];
static struct mtk_mux_user mmdvfs_user[MMDVFS_USER_NUM];
static struct clk *mmdvfs_user_clk[MMDVFS_USER_NUM];

static DEFINE_SPINLOCK(mmdvfs_mux_lock);

static rc_enable dpc_fp;
static bool mmdvfs_lp_mode;
static bool mmdvfs_vcp_stop;
static bool mmdvfs_mux_version;
static bool mmdvfs_swrgo;
static bool mmdvfs_swrgo_init;

int mmdvfs_set_lp_mode_by_vcp(const bool enable)
{
	int ret = 0;

	mmdvfs_lp_mode = enable;
	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_DISP);
	ret = mmdvfs_vcp_ipi_send(FUNC_MMDVFS_LP_MODE, enable ? 1 : 0, MAX_OPP, NULL);
	if (ret)
		MMDVFS_ERR("failed:%d enable:%d", ret, enable);
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_DISP);

	return ret;
}
EXPORT_SYMBOL_GPL(mmdvfs_set_lp_mode_by_vcp);

void mmdvfs_rc_enable_set_fp(rc_enable fp)
{
	dpc_fp = fp;
}
EXPORT_SYMBOL_GPL(mmdvfs_rc_enable_set_fp);

int mmdvfs_get_version(void)
{
	return mmdvfs_mux_version;
}
EXPORT_SYMBOL_GPL(mmdvfs_get_version);

int mmdvfs_force_step_by_vcp(const u8 pwr_idx, const s8 opp)
{
	u8 idx = pwr_idx + MMDVFS_USER_VCORE, mux_idx;
	int ret;

	if (!mmdvfs_mux_version || idx >= ARRAY_SIZE(mmdvfs_user)) {
		MMDVFS_ERR("invalid:%d pwr_idx:%hhu idx:%hhu", mmdvfs_mux_version, pwr_idx, idx);
		return -EINVAL;
	}
	mux_idx = mmdvfs_user[idx].target_id;

	if (opp >= MAX_OPP || opp >= mmdvfs_mux[mux_idx].freq_num) {
		MMDVFS_ERR("invalid opp:%hhd idx:%hhu mux:%hhu freq_num:%hhu",
			opp, idx, mux_idx, mmdvfs_mux[mux_idx].freq_num);
		return -EINVAL;
	}

	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_FORCE);
	ret = mmdvfs_vcp_ipi_send(FUNC_FORCE_OPP, pwr_idx, opp, NULL);
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_FORCE);

	if (ret || log_level & (1 << log_adb))
		MMDVFS_DBG("pwr_idx:%hhu idx:%hhu mux_idx:%hhu opp:%hhd",
			pwr_idx, idx, mux_idx, opp);
	return ret;
}
EXPORT_SYMBOL_GPL(mmdvfs_force_step_by_vcp);

int mmdvfs_vote_step_by_vcp(const u8 pwr_idx, const s8 opp)
{
	u8 idx = pwr_idx + MMDVFS_USER_VCORE, mux_idx;
	s8 level;
	int ret;

	if (!mmdvfs_mux_version || idx >= ARRAY_SIZE(mmdvfs_user)) {
		MMDVFS_ERR("invalid:%d pwr_idx:%hhu idx:%hhu", mmdvfs_mux_version, pwr_idx, idx);
		return -EINVAL;
	}
	mux_idx = mmdvfs_user[idx].target_id;

	if (opp >= MAX_OPP || opp >= mmdvfs_mux[mux_idx].freq_num) {
		MMDVFS_ERR("invalid opp:%hhd idx:%hhu mux:%hhu freq_num:%hhu",
			opp, idx, mux_idx, mmdvfs_mux[mux_idx].freq_num);
		return -EINVAL;
	}
	level = mmdvfs_mux[mux_idx].freq_num - 1 - opp;

	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_VOTE);
	ret = clk_set_rate(mmdvfs_user_clk[idx], mmdvfs_mux[mux_idx].freq[level]);
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_VOTE);

	if (ret || log_level & (1 << log_adb))
		MMDVFS_DBG("pwr_idx:%hhu idx:%hhu mux_idx:%hhu opp:%hhd level:%hhd",
			pwr_idx, idx, mux_idx, opp, level);
	return ret;
}
EXPORT_SYMBOL_GPL(mmdvfs_vote_step_by_vcp);

int mmdvfs_set_vcp_test(const char *val, const struct kernel_param *kp)
{
	u8 func, idx, mux_idx;
	s8 opp, level;
	int ret;

	ret = sscanf(val, "%hhu %hhu %hhd", &func, &idx, &opp);
	if (ret != 3) {
		MMDVFS_ERR("input failed:%d func:%hhu idx:%hhu opp:%hhd", ret, func, idx, opp);
		return -EINVAL;
	}

	if (func == TEST_AP_SET_OPP || func == TEST_AP_SET_USER_RATE) {
		if (idx >= MMDVFS_USER_NUM) {
			MMDVFS_ERR("invalid idx:%hhu opp:%hhd", idx, opp);
			return -EINVAL;
		}

		mux_idx = mmdvfs_user[idx].target_id;
		if (opp >= mmdvfs_mux[mux_idx].freq_num) {
			MMDVFS_ERR("invalid opp:%hhd idx:%hhu mux:%hhu freq_num:%hhu",
				opp, idx, mux_idx, mmdvfs_mux[mux_idx].freq_num);
			return -EINVAL;
		}
		level = mmdvfs_mux[mux_idx].freq_num - 1 - opp;
	}

	switch (func) {
	case TEST_AP_SET_OPP:
		ret = mmdvfs_mux_set_opp(mmdvfs_user[idx].name, mmdvfs_mux[mux_idx].freq[level]);
		break;
	case TEST_AP_SET_USER_RATE:
		if (!mmdvfs_user_clk[idx]) {
			MMDVFS_ERR("invalid idx:%hhu opp:%hhd level:%hhd", idx, opp, level);
			return -EINVAL;
		}
		ret = clk_set_rate(mmdvfs_user_clk[idx], mmdvfs_mux[mux_idx].freq[level]);
		break;
	default:
		mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_GENPD);
		ret = mmdvfs_vcp_ipi_send(func, idx, opp, NULL);
		mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_GENPD);
		break;
	}

	if (ret || log_level & (1 << log_adb))
		MMDVFS_DBG("ret:%d func:%hhu idx:%hhu opp:%hhd", ret, func, idx, opp);
	return ret;
}

static const struct kernel_param_ops mmdvfs_vcp_test_ops = {
	.set = mmdvfs_set_vcp_test,
};
module_param_cb(vcp_test, &mmdvfs_vcp_test_ops, NULL, 0644);
MODULE_PARM_DESC(vcp_test, "trigger vcp test");

static inline void mmdvfs_reset_ccu(void)
{
	int i;

	mutex_lock(&mmdvfs_ccu_pwr_mutex);
	for (i = 0; i < CCU_PWR_USR_NUM; i++) {
		if (ccu_pwr_usage[i])
			MMDVFS_ERR("i:%d usage:%d not disable", i, ccu_pwr_usage[i]);
		ccu_pwr_usage[i] = 0;
	}

	if (ccu_power) {
#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG)
		rproc_shutdownx(ccu_rproc, RPROC_UID_MMDVFS);
#else
		rproc_shutdown(ccu_rproc);
#endif
		ccu_power = 0;
	}
	mutex_unlock(&mmdvfs_ccu_pwr_mutex);
}

static inline void mmdvfs_reset_vcp(void)
{
	int i, ret;

	if (mmdvfs_swrgo)
		return;

	mutex_lock(&mmdvfs_vcp_pwr_mutex);
	for (i = 0; i < VCP_PWR_USR_NUM; i++) {
		if (vcp_pwr_usage[i])
			MMDVFS_ERR("i:%d usage:%d not disable", i, vcp_pwr_usage[i]);
		vcp_pwr_usage[i] = 0;
	}
	if (vcp_power) {
		ret = vcp_deregister_feature_ex(MMDVFS_FEATURE_ID);
		if (ret)
			MMDVFS_ERR("failed:%d vcp_power:%d", ret, vcp_power);
		vcp_power = 0;
	}
	mutex_unlock(&mmdvfs_vcp_pwr_mutex);
}

static int mmdvfs_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	int i;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		for (i = 0; i < PWR_MMDVFS_NUM; i++) {
			if (last_vote_step[i] != -1)
				mtk_mmdvfs_v3_set_vote_step(i, -1);

			if (last_force_step[i] != -1)
				mtk_mmdvfs_v3_set_force_step(i, -1);
		}
		mmdvfs_reset_ccu();
		mmdvfs_reset_vcp();
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block mmdvfs_pm_notifier_block = {
	.notifier_call = mmdvfs_pm_notifier,
	.priority = 0,
};

static void disp_pd_swrgo_init(const bool enable)
{
	int ret = 0;

	if (enable && !mmdvfs_swrgo_init) {
		ret = mtk_mmdvfs_enable_vcp(enable, VCP_PWR_USR_MMDVFS_GENPD);
		if (!ret) {
			ret = mmdvfs_vcp_ipi_send(FUNC_SWRGO_INIT, enable ? 1 : 0, MAX_OPP, NULL);
			if (!ret)
				mmdvfs_swrgo_init = true;
		}
	} else if (!enable && mmdvfs_swrgo_init) {
		ret = mmdvfs_vcp_ipi_send(FUNC_SWRGO_INIT, enable ? 1 : 0, MAX_OPP, NULL);
		if (!ret)
			mmdvfs_swrgo_init = false;
		ret = mtk_mmdvfs_enable_vcp(enable, VCP_PWR_USR_MMDVFS_GENPD);
	}
	MMDVFS_DBG("ret:%d enable:%d swrgo:%d", ret, enable, mmdvfs_swrgo_init);
}

void disp_pd_notify_work(struct work_struct *_work)
{
	struct mmdvfs_vmm_notify_work *work = container_of(_work, typeof(*work), vmm_notify_work);

	disp_pd_swrgo_init(work->enable);
	kfree(work);
}

static int disp_pd_callback(struct notifier_block *nb, unsigned long pd_flags, void *data)
{
	struct mmdvfs_vmm_notify_work *work;

	if (!mmdvfs_swrgo || !vmm_notify_wq)
		return 0;

	work = kzalloc(sizeof(*work), GFP_KERNEL);
	if (!work)
		return -ENOMEM;

	switch (pd_flags) {
	case GENPD_NOTIFY_PRE_ON:
		work->enable = true;
		break;
	case GENPD_NOTIFY_OFF:
		work->enable = false;
		break;
	default:
		return 0;
	}

	INIT_WORK(&work->vmm_notify_work, disp_pd_notify_work);
	queue_work(vmm_notify_wq, &work->vmm_notify_work);
	return 0;
}

static void mmdvfs_fmeter_dump(void)
{
	const u8 fmeter_id[13] = {5, 6, 7, 8, 9, 10, 11, 58, 57, 58, 69, 70, 71};
	const u8 fmeter_type[13] = {2, 2, 2, 2, 2, 2, 2, 2, 5, 5, 5, 5, 5};
	int i;

	for (i = 0; i < ARRAY_SIZE(fmeter_id); i++)
		MMDVFS_DBG("i:%d id:%hu type:%hu freq:%u", i, fmeter_id[i], fmeter_type[i],
			mt_get_fmeter_freq(fmeter_id[i], fmeter_type[i]));
}

static int mmdvfs_vcp_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	switch (action) {
	case VCP_EVENT_READY:
		mmdvfs_vcp_ipi_send(FUNC_MMDVFS_INIT, MAX_OPP, MAX_OPP, NULL);
		if (dpc_fp)
			dpc_fp(true, mmdvfs_vcp_stop);
		mmdvfs_vcp_stop = false;
		mmdvfs_vcp_ipi_send(FUNC_MMDVFSRC_INIT, MAX_OPP, MAX_OPP, NULL);
		mmdvfs_vcp_cb_ready = true;
		break;
	case VCP_EVENT_STOP:
		if (dpc_fp)
			dpc_fp(false, true);
		mmdvfs_vcp_stop = true;
		mmdvfs_vcp_cb_ready = false;
		break;
	case VCP_EVENT_SUSPEND:
		if (dpc_fp)
			dpc_fp(false, false);
		if (mmdvfs_swrgo) {
			bool dump = false;
			int i;

			for (i = 0; i < ARRAY_SIZE(mmdvfs_user); i++) {
				u64 freq = mmdvfs_mux[mmdvfs_user[i].target_id].freq[0];

				if (mmdvfs_user[i].rate < freq)
					continue;
				freq = freq ? freq : mmdvfs_mux[mmdvfs_user[i].target_id].freq[1];
				if (mmdvfs_user[i].rate > freq) {
					MMDVFS_DBG("user:%d name:%s rate:%lu", mmdvfs_user[i].id,
						mmdvfs_user[i].name, mmdvfs_user[i].rate);
					mmdvfs_mux_set_opp(mmdvfs_user[i].name, 26000000UL);
					dump = true;
				}
			}
			if (dump)
				mmdvfs_fmeter_dump();
		}
		mmdvfs_vcp_cb_ready = false;
		break;
	}
	return NOTIFY_DONE;
}

static int mmdvfs_force_on_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	mtk_mmdvfs_enable_vcp(!(action == 0), VCP_PWR_USR_SMI);
	return NOTIFY_DONE;
}

static int mmdvfs_vcp_init_thread(void *data)
{
	static struct mtk_ipi_device *vcp_ipi_dev;
	struct iommu_domain *domain;
	int i, retry = 0;

	while (mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_INIT)) {
		if (++retry > 100) {
			MMDVFS_ERR("vcp is not powered on yet");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	retry = 0;
	while (!is_vcp_ready_ex(VCP_A_ID)) {
		if (++retry > 100) {
			MMDVFS_ERR("VCP_A_ID:%d not ready", VCP_A_ID);
			return -ETIMEDOUT;
		}
		usleep_range(1000, 2000);
	}

	retry = 0;
	while (!(vcp_ipi_dev = vcp_get_ipidev())) {
		if (++retry > 100) {
			MMDVFS_ERR("cannot get vcp ipidev");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	mmdvfs_memory_iova = vcp_get_reserve_mem_phys_ex(MMDVFS_MEM_ID);
	domain = iommu_get_domain_for_dev(&vcp_ipi_dev->mrpdev->pdev->dev);
	if (domain)
		mmdvfs_memory_pa = iommu_iova_to_phys(domain, mmdvfs_memory_iova);
	mmdvfs_memory_va = (void *)vcp_get_reserve_mem_virt_ex(MMDVFS_MEM_ID);

	writel_relaxed(mmdvfs_free_run ? 1 : 0, MEM_FREERUN);
	for (i = 0; i < PWR_MMDVFS_NUM; i++) {
		writel_relaxed(MAX_OPP, MEM_FORCE_OPP_PWR(i));
		writel_relaxed(MAX_OPP, MEM_VOTE_OPP_PWR(i));
		writel_relaxed(MAX_OPP, MEM_PWR_OPP(i));
		writel_relaxed(MAX_OPP, MEM_PWR_CUR_GEAR(i));
	}
	for (i = 0; i < USER_NUM; i++) {
		writel_relaxed(MAX_OPP, MEM_VOTE_OPP_USR(i));
		writel_relaxed(MAX_OPP, MEM_MUX_OPP(i));
		writel_relaxed(MAX_OPP, MEM_USR_OPP(i));
	}

	if (mmdvfs_lp_mode)
		writel_relaxed(1, MEM_MMDVFS_LP_MODE);

	mmdvfs_init_done = true;
	MMDVFS_DBG("iova:%pa pa:%pa va:%#lx init_done:%d",
		&mmdvfs_memory_iova, &mmdvfs_memory_pa, (unsigned long)mmdvfs_memory_va, mmdvfs_init_done);

	if (vcp_log_level)
		mtk_mmdvfs_v3_set_vcp_log(vcp_log_level);

	if (vmrc_log_level)
		mtk_mmdvfs_v3_set_vmrc_log(vmrc_log_level);

	if (vmm_ceil_step)
		mtk_mmdvfs_v3_set_vmm_ceil_step(vmm_ceil_step);

	vcp_ready_notifier.notifier_call = mmdvfs_vcp_notifier_callback;
	vcp_A_register_notify_ex(&vcp_ready_notifier);

	force_on_notifier.notifier_call = mmdvfs_force_on_callback;
	mtk_smi_dbg_register_force_on_notifier(&force_on_notifier);

	if (mmdvfs_swrgo) {
		disp_pd_swrgo_init(true);
		for (i = 0; i < ARRAY_SIZE(mmdvfs_user); i++)
			if (mmdvfs_user[i].undo_rate > 26000000UL)
				mmdvfs_mux_set_opp(mmdvfs_user[i].name, mmdvfs_user[i].undo_rate);
	}
	return 0;
}

static int mmdvfs_ccu_init_thread(void *data)
{
	struct device_node *node = (struct device_node *)data, *ccu_node;
	phandle handle;
	int retry = 0;

	if (of_property_read_u32(node, "mediatek,ccu-rproc", &handle) < 0) {
		MMDVFS_ERR("get ccu phandle failed");
		return -EINVAL;
	}

	ccu_node = of_find_node_by_phandle(handle);
	if (!ccu_node) {
		MMDVFS_ERR("find ccu node failed");
		return -EINVAL;
	}

	ccu_pdev = of_find_device_by_node(ccu_node);
	if (!ccu_pdev)
		goto ccu_init_end;

	while (!(ccu_rproc = rproc_get_by_phandle(handle))) {
		if (++retry > 100)
			goto ccu_init_end;
		ssleep(1);
	}

ccu_init_end:
	MMDVFS_DBG("ccu_pdev:%p ccu_rproc:%p", ccu_pdev, ccu_rproc);
	of_node_put(ccu_node);
	return 0;
}

static int mtk_mmdvfs_clk_enable(const u8 clk_idx)
{
	int err;

	if (!mmdvfs_is_init_done())
		return 0;

	err = set_clkmux_memory(clk_idx, true);

	if (err || is_vcp_suspending_ex()) {
		MMDVFS_DBG("clk_idx:%hhu err:%d", clk_idx, err);
		return 0;
	}

	mmdvfs_vcp_ipi_send(FUNC_CLKMUX_ENABLE, clk_idx, true, NULL);
	return 0;
}

static int mtk_mmdvfs_clk_disable(const u8 clk_idx)
{
	int err;

	if (!mmdvfs_is_init_done())
		return 0;

	err = set_clkmux_memory(clk_idx, false);

	if (err || is_vcp_suspending_ex()) {
		MMDVFS_DBG("clk_idx:%hhu err:%d", clk_idx, err);
		return 0;
	}

	mmdvfs_vcp_ipi_send(FUNC_CLKMUX_ENABLE, clk_idx, false, NULL);
	return 0;
}

static const struct of_device_id of_match_mmdvfs_v3[] = {
	{
		.compatible = "mediatek,mtk-mmdvfs-v3",
	}, {}
};

static int mmdvfs_v3_probe(struct platform_device *pdev)
{
	const char *MMDVFS_CLK_NAMES = "mediatek,mmdvfs-clock-names";
	const char *MMDVFS_CLKS = "mediatek,mmdvfs-clocks";
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	struct task_struct *kthr_vcp, *kthr_ccu;
	struct device_node *larbnode;
	struct platform_device *larbdev;
	unsigned int dl_flags = DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS;
	struct clk *clk;
	int i, ret;

	ret = of_property_count_strings(node, MMDVFS_CLK_NAMES);
	if (ret <= 0) {
		MMDVFS_ERR("%s invalid:%d", MMDVFS_CLK_NAMES, ret);
		return ret;
	}
	mmdvfs_clk_num = ret;

	mtk_mmdvfs_clks =
		kcalloc(mmdvfs_clk_num, sizeof(*mtk_mmdvfs_clks), GFP_KERNEL);
	if (!mtk_mmdvfs_clks) {
		MMDVFS_ERR("mtk_mmdvfs_clks without memory");
		return -ENOMEM;
	}

	clk_data = mtk_alloc_clk_data(mmdvfs_clk_num);
	if (!clk_data) {
		MMDVFS_ERR("allocate clk_data failed num:%hhu", mmdvfs_clk_num);
		return -ENOMEM;
	}

	for (i = 0; i < mmdvfs_clk_num; i++) {
		struct device_node *table, *opp = NULL;
		struct of_phandle_args spec;
		struct clk_init_data init = {};
		u8 idx = 0;

		of_property_read_string_index(
			node, MMDVFS_CLK_NAMES, i, &mtk_mmdvfs_clks[i].name);

		ret = of_parse_phandle_with_args(
			node, MMDVFS_CLKS, "#mmdvfs,clock-cells", i, &spec);
		if (ret) {
			MMDVFS_ERR("parse %s i:%d failed:%d",
				MMDVFS_CLKS, i, ret);
			return ret;
		}

		mtk_mmdvfs_clks[i].clk_id = spec.args[0];
		mtk_mmdvfs_clks[i].pwr_id = spec.args[1];
		mtk_mmdvfs_clks[i].user_id = spec.args[2];
		mtk_mmdvfs_clks[i].ipi_type = spec.args[3];
		mtk_mmdvfs_clks[i].spec_type = spec.args[4];
		table = of_find_node_by_phandle(spec.args[5]);
		of_node_put(spec.np);

		do {
			u64 freq;

			opp = of_get_next_available_child(table, opp);
			if (opp) {
				of_property_read_u64(opp, "opp-hz", &freq);
				mtk_mmdvfs_clks[i].freqs[idx] = freq;
				idx += 1;
			}
		} while (opp);
		of_node_put(table);

		mtk_mmdvfs_clks[i].opp = MAX_OPP;
		mtk_mmdvfs_clks[i].freq_num = idx;

		MMDVFS_DBG(
			"i:%d name:%s clk:%hhu pwr:%hhu user:%hhu ipi:%hhu spec:%hhu opp:%hhu freq:%hhu",
			i, mtk_mmdvfs_clks[i].name, mtk_mmdvfs_clks[i].clk_id,
			mtk_mmdvfs_clks[i].pwr_id, mtk_mmdvfs_clks[i].user_id,
			mtk_mmdvfs_clks[i].ipi_type,
			mtk_mmdvfs_clks[i].spec_type,
			mtk_mmdvfs_clks[i].opp, mtk_mmdvfs_clks[i].freq_num);

		if (!IS_ERR_OR_NULL(clk_data->clks[i]))
			continue;

		init.name = mtk_mmdvfs_clks[i].name;
		init.ops = &mtk_mmdvfs_req_ops;
		mtk_mmdvfs_clks[i].clk_hw.init = &init;

		clk = clk_register(NULL, &mtk_mmdvfs_clks[i].clk_hw);
		if (IS_ERR_OR_NULL(clk))
			MMDVFS_ERR("i:%d clk:%s register failed:%d",
				i, mtk_mmdvfs_clks[idx].name, PTR_ERR_OR_ZERO(clk));
		else
			clk_data->clks[i] = clk;
	}

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (ret) {
		MMDVFS_ERR("add clk provider failed:%d", ret);
		mtk_free_clk_data(clk_data);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(mmdvfs_pwr_opp); i++) {
		mmdvfs_pwr_opp[i] = MAX_OPP;

		clk = of_clk_get(node, i);
		if (IS_ERR_OR_NULL(clk))
			MMDVFS_DBG("i:%d clk get failed:%d", i, PTR_ERR_OR_ZERO(clk));
		else
			mmdvfs_pwr_clk[i] = clk;
	}

	if (!vmm_notify_wq)
		vmm_notify_wq = create_singlethread_workqueue("vmm_notify_wq");
	register_pm_notifier(&mmdvfs_pm_notifier_block);

	clkmux_cb.clk_enable = mtk_mmdvfs_clk_enable;
	clkmux_cb.clk_disable = mtk_mmdvfs_clk_disable;
	mtk_clk_register_ipi_callback(&clkmux_cb);

	if (of_property_read_bool(node, "mmdvfs-free-run"))
		mmdvfs_free_run = true;

	mmdvfs_v3_dev = &pdev->dev;
	larbnode = of_parse_phandle(pdev->dev.of_node, "mediatek,larbs", 0);
	if (larbnode) {
		larbdev = of_find_device_by_node(larbnode);
		if (!device_link_add(mmdvfs_v3_dev, &larbdev->dev, dl_flags)) {
			MMDVFS_ERR("add larbdev device link failed");
			return -EINVAL;
		}
		pm_runtime_enable(mmdvfs_v3_dev);
		of_node_put(larbnode);
	}

	of_property_read_s32(node, "mediatek,dpsw-thr", &dpsw_thr);

	for (i = 0; i < PWR_MMDVFS_NUM; i++) {
		last_vote_step[i] = -1;
		last_force_step[i] = -1;
	}

	of_property_read_s32(node, "kernel-log-level", &log_level);
	of_property_read_s32(node, "vcp-log-level", &vcp_log_level);
	of_property_read_s32(node, "vmrc-log-level", &vmrc_log_level);
	of_property_read_s32(node, "vmm-ceil-step", &vmm_ceil_step);

	kthr_vcp = kthread_run(mmdvfs_vcp_init_thread, NULL, "mmdvfs-vcp");
	kthr_ccu = kthread_run(mmdvfs_ccu_init_thread, node, "mmdvfs-ccu");

	return ret;
}

static struct platform_driver clk_mmdvfs_drv = {
	.probe = mmdvfs_v3_probe,
	.driver = {
		.name = "mtk-mmdvfs-v3",
		.owner = THIS_MODULE,
		.of_match_table = of_match_mmdvfs_v3,
	},
};

int mmdvfs_mux_set_opp(const char *name, unsigned long rate)
{
	struct mtk_mux_user *user;
	struct mmdvfs_mux *mux;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mmdvfs_user); i++)
		if (!strncmp(mmdvfs_user[i].name, name, 16))
			break;

	if (i >= ARRAY_SIZE(mmdvfs_user)) {
		MMDVFS_ERR("invalid name:%s rate:%lu", name, rate);
		return -EINVAL;
	}

	user = &mmdvfs_user[i];
	mux = &mmdvfs_mux[user->target_id];

	if (user->undo_rate <= 26000000UL && user->rate == rate)
		return 0;

	user->rate = rate;
	mux->rate = 0ULL;

	for (i = 0; i < mux->user_num; i++)
		if (mux->rate < mux->user[i]->rate)
			mux->rate = mux->user[i]->rate;

	for (i = 0; i < mux->freq_num; i++)
		if (mux->rate <= mux->freq[i])
			break;

	mux->opp = (mux->freq_num - ((i == mux->freq_num) ? (i - 1) : i) - 1);
	if (mux->opp == mux->last)
		return 0;

	if (mmdvfs_swrgo) {
		const u8 vcp_mux_id[MMDVFS_MUX_NUM] = {0, 4, 5, 6, 7, 9, 10, 12};

		if (mux->id >= ARRAY_SIZE(vcp_mux_id)) {
			MMDVFS_ERR("invalid mux_id:%hhu user_id:%hhu", mux->id, user->id);
			return -EINVAL;
		}

		if (!mmdvfs_swrgo_init) {
			MMDVFS_ERR("swrgo:%d not ready", mmdvfs_swrgo_init);
			goto set_opp_end;
		}

		if (dpsw_thr && mux->id >= MMDVFS_MUX_VDE && mux->id <= MMDVFS_MUX_CAM &&
			mux->opp < dpsw_thr && mux->last >= dpsw_thr)
			mtk_mmdvfs_enable_vmm(true);
		ret = mmdvfs_vcp_ipi_send(TEST_SET_MUX, vcp_mux_id[mux->id], mux->opp, NULL);
		if (dpsw_thr && mux->id >= MMDVFS_MUX_VDE && mux->id <= MMDVFS_MUX_CAM &&
			mux->opp >= dpsw_thr && mux->last < dpsw_thr)
			mtk_mmdvfs_enable_vmm(false);
	}
	if (!ret) {
		user->undo_rate = 0UL;
		mux->last = mux->opp;
	}

set_opp_end:
	if (ret)
		user->undo_rate = user->rate;

	if (ret || log_level & (1 << log_clk_ops))
		MMDVFS_ERR(
			"ret:%d name:%s rate:%lu user:%d name:%s rate:%lu undo:%lu mux:%hhu name:%s rate:%llu freq_num:%hhu opp:%hhd last:%hhd",
			ret, name, rate, user->id, user->name, user->rate, user->undo_rate,
			mux->id, mux->name, mux->rate, mux->freq_num, mux->opp, mux->last);
	return 0;
}
EXPORT_SYMBOL(mmdvfs_mux_set_opp);

static int mmdvfs_mux_get_opp(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mmdvfs_mux); i++)
		if (!strncmp(mmdvfs_mux[i].target_name, name, 16))
			break;

	if (i >= ARRAY_SIZE(mmdvfs_mux)) {
		MMDVFS_ERR("invalid name:%s i:%d", name, i);
		return -EINVAL;
	}

	if (log_level & (1 << log_clk_ops))
		MMDVFS_DBG("name:%s mux:%hhu name:%s rate:%llu opp:%hhd", name, mmdvfs_mux[i].id,
			mmdvfs_mux[i].name, mmdvfs_mux[i].rate, mmdvfs_mux[i].opp);

	return mmdvfs_mux[i].opp;
}

static unsigned long mmdvfs_mux_get_rate(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mmdvfs_user); i++)
		if (!strncmp(mmdvfs_user[i].name, name, 16))
			break;

	if (i >= ARRAY_SIZE(mmdvfs_user)) {
		MMDVFS_ERR("invalid name:%s", name);
		return -EINVAL;
	}

	if (log_level & (1 << log_clk_ops))
		MMDVFS_DBG("name:%s user:%d name:%s rate:%lu",
			name, mmdvfs_user[i].id, mmdvfs_user[i].name, mmdvfs_user[i].rate);

	return mmdvfs_user[i].rate;
}

static struct dfs_ops mmdvfs_mux_dfs_ops = {
	.set_opp = mmdvfs_mux_set_opp,
	.get_opp = mmdvfs_mux_get_opp,
	.get_rate = mmdvfs_mux_get_rate,
};

static int mmdvfs_mux_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *data;
	const char *name = NULL;
	int i, j, ret;

	mmdvfs_mux_version = true;
	mmdvfs_swrgo = of_property_read_bool(node, "mediatek,mmdvfs-swrgo");

	for (i = 0; i < ARRAY_SIZE(mmdvfs_mux); i++) {
		struct device_node *table, *opp = NULL;
		struct clk *clk;
		phandle handle;
		u64 freq;

		mmdvfs_mux[i].id = i;

		ret = of_property_read_string_index(node, "clock-names", i, &name);
		if (ret) {
			MMDVFS_ERR("failed:%d i:%d name:%s", ret, i, name);
			return ret;
		}
		mmdvfs_mux[i].name = name;

		clk = of_clk_get(node, i);
		if (IS_ERR_OR_NULL(clk)) {
			MMDVFS_ERR("failed:%d i:%d", PTR_ERR_OR_ZERO(clk), i);
			return PTR_ERR_OR_ZERO(clk);
		}

		name = __clk_get_name(clk);
		if (!name) {
			MMDVFS_ERR("failed:%d name:%s clk:%p", PTR_ERR_OR_ZERO(name), name, clk);
			return PTR_ERR_OR_ZERO(clk);
		}
		mmdvfs_mux[i].target_name = name;

		ret = of_property_read_u32_index(node, "mediatek,mmdvfs-opp-table", i, &handle);
		if (ret) {
			MMDVFS_ERR("failed:%d i:%d handle:%u", ret, i, handle);
			return ret;
		}

		table = of_find_node_by_phandle(handle);
		if (!table)
			return -EINVAL;

		j = 0;
		do {
			opp = of_get_next_available_child(table, opp);
			if (opp) {
				ret = of_property_read_u64(opp, "opp-hz", &freq);
				if (ret) {
					MMDVFS_ERR("failed:%d i:%d freq:%llu", ret, i, freq);
					return ret;
				}
				mmdvfs_mux[i].freq[j] = freq;
				j += 1;
			}
		} while (opp);
		of_node_put(table);

		mmdvfs_mux[i].freq_num = j;
		mmdvfs_mux[i].rate = 26000000UL;
		mmdvfs_mux[i].opp = MAX_OPP;
		mmdvfs_mux[i].last = MAX_OPP;
	}

	for (i = 0; i < ARRAY_SIZE(mmdvfs_user); i++) {
		mmdvfs_user[i].id = i;

		ret = of_property_read_string_index(
			node, "mediatek,mmdvfs-user-names", i, &name);
		if (ret) {
			MMDVFS_ERR("failed:%d i:%d name:%s", ret, i, name);
			return ret;
		}
		mmdvfs_user[i].name = name;

		ret = of_property_read_u32_index(node, "mediatek,mmdvfs-user-target", i, &j);
		if (ret || j >= ARRAY_SIZE(mmdvfs_mux)) {
			MMDVFS_ERR("failed:%d i:%d j:%d", ret, i, j);
			return ret;
		}
		mmdvfs_user[i].target_name = mmdvfs_mux[j].name;
		mmdvfs_user[i].target_id = j;

		mmdvfs_mux[j].user[mmdvfs_mux[j].user_num] = &mmdvfs_user[i];
		mmdvfs_mux[j].user_num += 1;

		mmdvfs_user[i].rate = 26000000UL;
		mmdvfs_user[i].undo_rate = 26000000UL;
		mmdvfs_user[i].ops = &mtk_mux_user_ops;
		mmdvfs_user[i].flags = 0;

		MMDVFS_DBG(
			"user:%2u name:%12s target:%8s mux:%hhu name:%8s target:%12s freq_num:%hhu user_num:%hhu",
			mmdvfs_user[i].id, mmdvfs_user[i].name, mmdvfs_user[i].target_name,
			mmdvfs_mux[j].id, mmdvfs_mux[j].name, mmdvfs_mux[j].target_name,
			mmdvfs_mux[j].freq_num, mmdvfs_mux[j].user_num);
	}

	data = mtk_alloc_clk_data(ARRAY_SIZE(mmdvfs_user));
	if (!data)
		return -ENOMEM;

	ret = mtk_clk_mux_register_user_clks(
		mmdvfs_user, ARRAY_SIZE(mmdvfs_user), &mmdvfs_mux_lock, data, &pdev->dev);
	if (ret) {
		MMDVFS_ERR("failed:%d user:%p size:%lu data:%p",
			ret, mmdvfs_user, ARRAY_SIZE(mmdvfs_user), data);
		return ret;
	}

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, data);
	if (ret) {
		MMDVFS_ERR("failed:%d data:%p", ret, data);
		return ret;
	}

	mtk_clk_mux_register_callback(&mmdvfs_mux_dfs_ops);

	if (!vmm_notify_wq)
		vmm_notify_wq = create_singlethread_workqueue("vmm_notify_wq");

	if (mmdvfs_swrgo) {
		pm_runtime_enable(&pdev->dev);
		disp_pd_notifier.notifier_call = disp_pd_callback;
		ret = dev_pm_genpd_add_notifier(&pdev->dev, &disp_pd_notifier);
	}

	return ret;
}

static int mmdvfs_user_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	const char *name;
	struct clk *clk;
	int i, j, ret;

	ret = of_property_count_strings(node, "clock-names");
	if (ret <= 0) {
		MMDVFS_ERR("clock-names invalid:%d", ret);
		return ret;
	}

	for (i = 0; i < ret; i++) {
		clk = of_clk_get(node, i);
		if (IS_ERR_OR_NULL(clk)) {
			MMDVFS_ERR("failed:%d i:%d", PTR_ERR_OR_ZERO(clk), i);
			return PTR_ERR_OR_ZERO(clk);
		}

		name = __clk_get_name(clk);
		if (!name) {
			MMDVFS_ERR("failed:%d name:%s clk:%p", PTR_ERR_OR_ZERO(name), name, clk);
			return PTR_ERR_OR_ZERO(clk);
		}

		for (j = 0; j < ARRAY_SIZE(mmdvfs_user); j++)
			if (!strncmp(mmdvfs_user[j].name, name, 16)) {
				mmdvfs_user_clk[j] = clk;
				MMDVFS_DBG("user:%2d name:%12s", j, name);
				break;
			}
	}

	return 0;
}

static const struct of_device_id of_match_mmdvfs_mux[] = {
	{
		.compatible = "mediatek,mtk-mmdvfs-mux",
	}, {}
};

static const struct of_device_id of_match_mmdvfs_user[] = {
	{
		.compatible = "mediatek,mtk-mmdvfs-user",
	}, {}
};

static struct platform_driver mmdvfs_mux_drv = {
	.probe = mmdvfs_mux_probe,
	.driver = {
		.name = "mtk-mmdvfs-mux",
		.of_match_table = of_match_mmdvfs_mux,
	},
};

static struct platform_driver mmdvfs_user_drv = {
	.probe = mmdvfs_user_probe,
	.driver = {
		.name = "mtk-mmdvfs-user",
		.of_match_table = of_match_mmdvfs_user,
	},
};

static struct platform_driver * const mmdvfs_drv[] = {
	&clk_mmdvfs_drv,
	&mmdvfs_mux_drv,
	&mmdvfs_user_drv,
};

static int __init clk_mmdvfs_init(void)
{
	return platform_register_drivers(mmdvfs_drv, ARRAY_SIZE(mmdvfs_drv));
}

static void __exit clk_mmdvfs_exit(void)
{
	platform_unregister_drivers(mmdvfs_drv, ARRAY_SIZE(mmdvfs_drv));
}

module_init(clk_mmdvfs_init);
module_exit(clk_mmdvfs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MMDVFS");
MODULE_AUTHOR("MediaTek Inc.");
