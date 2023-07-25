// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

#include "mtk_disp_vidle.h"

#define VDISPDBG(fmt, args...) \
	pr_info("[vdisp] %s:%d " fmt "\n", __func__, __LINE__, ##args)

#define VDISPERR(fmt, args...) \
	pr_info("[vdisp][err] %s:%d " fmt "\n", __func__, __LINE__, ##args)

#define SPM_MML0_PWR_CON 0xE90
#define SPM_MML1_PWR_CON 0xE94
#define SPM_DIS0_PWR_CON 0xE98
#define SPM_DIS1_PWR_CON 0xE9C
#define SPM_OVL0_PWR_CON 0xEA0
#define SPM_OVL1_PWR_CON 0xEA4
#define SPM_RTFF_SAVE_FLAG BIT(27)

#define SPM_ISO_CON_STA 0xF64
#define SPM_ISO_CON_SET 0xF68
#define SPM_ISO_CON_CLR 0xF6C
#define SPM_VDISP_EXT_BUCK_ISO       BIT(0)
#define SPM_AOC_VDISP_SRAM_ISO_DIN   BIT(1)
#define SPM_AOC_VDISP_SRAM_LATCH_ENB BIT(2)

#define VLP_DISP_SW_VOTE_CON 0x410	/* for mminfra pwr on */
#define VLP_DISP_SW_VOTE_SET 0x414
#define VLP_DISP_SW_VOTE_CLR 0x418
#define VLP_MMINFRA_DONE_OFS 0x91c
#define VOTE_RETRY_CNT 2500
#define VOTE_DELAY_US 2
#define POLL_DELAY_US 10
#define TIMEOUT_300MS 300000

/* This id is only for disp internal use */
enum disp_pd_id {
	DISP_PD_DISP_VCORE,
	DISP_PD_DISP1,
	DISP_PD_DISP0,
	DISP_PD_OVL1,
	DISP_PD_OVL0,
	DISP_PD_MML1,
	DISP_PD_MML0,
	DISP_PD_DPTX,
	DISP_PD_NUM,
};

struct mtk_vdisp {
	void __iomem *spm_base;
	void __iomem *vlp_base;
	struct notifier_block rgu_nb;
	struct notifier_block pd_nb;
	enum disp_pd_id pd_id;
};
static struct device *g_dev[DISP_PD_NUM];

static int regulator_event_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct mtk_vdisp *priv = container_of(nb, struct mtk_vdisp, rgu_nb);
	u32 val = 0;
	void __iomem *addr = 0;

	if (event == REGULATOR_EVENT_ENABLE) {
		addr = priv->spm_base + SPM_ISO_CON_CLR;
		writel_relaxed(SPM_VDISP_EXT_BUCK_ISO, addr);
		writel_relaxed(SPM_AOC_VDISP_SRAM_ISO_DIN, addr);
		writel_relaxed(SPM_AOC_VDISP_SRAM_LATCH_ENB, addr);

		// addr = priv->spm_base + SPM_ISO_CON_STA;
		// pr_info("REGULATOR_EVENT_ENABLE (%#llx) ", (u64)readl(addr));
	} else if (event == REGULATOR_EVENT_PRE_DISABLE) {
		addr = priv->spm_base + SPM_MML0_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_MML1_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_DIS0_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_DIS1_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_OVL0_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_OVL1_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = priv->spm_base + SPM_ISO_CON_SET;
		writel_relaxed(SPM_AOC_VDISP_SRAM_LATCH_ENB, addr);
		writel_relaxed(SPM_AOC_VDISP_SRAM_ISO_DIN, addr);
		writel_relaxed(SPM_VDISP_EXT_BUCK_ISO, addr);

		// addr = priv->spm_base + SPM_ISO_CON_STA;
		// pr_info("REGULATOR_EVENT_PRE_DISABLE (%#llx) ", (u64)readl(addr));
	}

	return 0;
}

static void mminfra_hwv_pwr_ctrl(struct mtk_vdisp *priv, bool on)
{
	u32 addr = on ? VLP_DISP_SW_VOTE_SET : VLP_DISP_SW_VOTE_CLR;
	u32 ack = on ? BIT(priv->pd_id) : 0;
	u16 i = 0;
	u32 value = 0;
	int ret = 0;

	/* [0] MMINFRA_DONE_STA
	 * [1] VCP_READY_STA
	 * [2] MMINFRA_DURING_OFF_STA
	 *
	 * Power on flow
	 *   polling 91c & 0x2 = 0x2 (wait 300ms timeout)
	 *   start vote (keep write til vote rg == voting value)
	 *   polling 91c == 0x3 (wait 300ms timeout)
	 *
	 * Power off flow
	 *   polling 91c & 0x2 = 0x2 (wait 300ms timeout)
	 *   start unvote (keep write til vote rg == unvoting value)
	 */

	ret = readl_poll_timeout_atomic(priv->vlp_base + VLP_MMINFRA_DONE_OFS, value,
					(value & 0x2) == 0x2, POLL_DELAY_US, TIMEOUT_300MS);
	if (ret < 0) {
		VDISPERR("failed to wait voter free");
		return;
	}

	writel_relaxed(BIT(priv->pd_id), priv->vlp_base + addr);
	do {
		writel_relaxed(BIT(priv->pd_id), priv->vlp_base + addr);
		if ((readl(priv->vlp_base + VLP_DISP_SW_VOTE_CON) & BIT(priv->pd_id)) == ack)
			break;

		if (i > VOTE_RETRY_CNT) {
			VDISPERR("vlp vote bit(%u) timeout", priv->pd_id);
			return;
		}

		udelay(VOTE_DELAY_US);
		i++;
	} while (1);

	if (on) {
		ret = readl_poll_timeout_atomic(priv->vlp_base + VLP_MMINFRA_DONE_OFS, value,
						value == 0x3, POLL_DELAY_US, TIMEOUT_300MS);
		if (ret < 0)
			VDISPERR("failed to power on mminfra");
	}
}

static int genpd_event_notifier(struct notifier_block *nb,
			  unsigned long event, void *data)
{
	struct mtk_vdisp *priv = container_of(nb, struct mtk_vdisp, pd_nb);

	switch (event) {
	case GENPD_NOTIFY_PRE_OFF:
	case GENPD_NOTIFY_PRE_ON:
		mminfra_hwv_pwr_ctrl(priv, true);
		break;
	case GENPD_NOTIFY_OFF:
	case GENPD_NOTIFY_ON:
		mminfra_hwv_pwr_ctrl(priv, false);
		break;
	default:
		break;
	}

	return 0;
}

static void mtk_vdisp_genpd_put(void)
{
	int i = 0, j = 0;

	for (i = 0; i < DISP_PD_NUM; i++) {
		if (g_dev[i]) {
			pm_runtime_put(g_dev[i]);
			j++;
		}
	}
	VDISPDBG("%d mtcmos has been put", j);
}

static const struct mtk_vdisp_funcs funcs = {
	.genpd_put = mtk_vdisp_genpd_put,
};

static int mtk_vdisp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_vdisp *priv;
	struct regulator *rgu;
	struct resource *res;
	int ret = 0;
	u32 pd_id = 0;

	VDISPDBG("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "SPM_BASE");
	if (res) {
		priv->spm_base = devm_ioremap(dev, res->start, resource_size(res));
		if (!priv->spm_base) {
			VDISPERR("fail to ioremap SPM_BASE: 0x%llx", res->start);
			return -EINVAL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "VLP_BASE");
	if (res) {
		priv->vlp_base = devm_ioremap(dev, res->start, resource_size(res));
		if (!priv->vlp_base) {
			VDISPERR("fail to ioremap VLP_BASE: 0x%llx", res->start);
			return -EINVAL;
		}
	}

	if (of_find_property(dev->of_node, "dis1-shutdown-supply", NULL)) {
		rgu = devm_regulator_get(dev, "dis1-shutdown");
		if (!IS_ERR(rgu)) {
			priv->rgu_nb.notifier_call = regulator_event_notifier;
			ret = devm_regulator_register_notifier(rgu, &priv->rgu_nb);
			if (ret)
				VDISPERR("Failed to register notifier ret(%d)", ret);
		}
	}

	ret = of_property_read_u32(dev->of_node, "disp-pd-id", &pd_id);
	if (ret) {
		VDISPERR("disp-pd-id property read fail(%d)", ret);
		return -ENODEV;
	}

	priv->pd_nb.notifier_call = genpd_event_notifier;
	priv->pd_id = pd_id;
	g_dev[pd_id] = dev;

	if (!pm_runtime_enabled(dev))
		pm_runtime_enable(dev);
	ret = dev_pm_genpd_add_notifier(dev, &priv->pd_nb);
	if (ret)
		VDISPERR("dev_pm_genpd_add_notifier fail(%d)", ret);

	pm_runtime_get(dev);
	mtk_vdisp_register(&funcs);

	return ret;
}

static int mtk_vdisp_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mtk_vdisp_driver_dt_match[] = {
	{.compatible = "mediatek,mt6989-vdisp-ctrl"},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_vdisp_driver_dt_match);

struct platform_driver mtk_vdisp_driver = {
	.probe = mtk_vdisp_probe,
	.remove = mtk_vdisp_remove,
	.driver = {
		.name = "mediatek-vdisp-ctrl",
		.owner = THIS_MODULE,
		.of_match_table = mtk_vdisp_driver_dt_match,
	},
};

static int __init mtk_vdisp_init(void)
{
	VDISPDBG("+");
	platform_driver_register(&mtk_vdisp_driver);
	VDISPDBG("-");
	return 0;
}

static void __exit mtk_vdisp_exit(void)
{
	platform_driver_unregister(&mtk_vdisp_driver);
}

late_initcall(mtk_vdisp_init);
module_exit(mtk_vdisp_exit);
MODULE_AUTHOR("William Yang <William-tw.Yang@mediatek.com>");
MODULE_DESCRIPTION("MTK VDISP driver");
MODULE_SOFTDEP("post:mediatek-drm");
MODULE_LICENSE("GPL");
