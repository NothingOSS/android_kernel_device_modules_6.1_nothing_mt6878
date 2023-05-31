// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>


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

struct mtk_vdisp {
	void __iomem *spm_base;
	struct notifier_block nb;
};
static struct mtk_vdisp *g_priv;

static int regulator_event_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	u32 val = 0;
	void __iomem *addr = 0;

	if (event == REGULATOR_EVENT_ENABLE) {
		addr = g_priv->spm_base + SPM_ISO_CON_CLR;
		writel_relaxed(SPM_VDISP_EXT_BUCK_ISO, addr);
		writel_relaxed(SPM_AOC_VDISP_SRAM_ISO_DIN, addr);
		writel_relaxed(SPM_AOC_VDISP_SRAM_LATCH_ENB, addr);

		// addr = g_priv->spm_base + SPM_ISO_CON_STA;
		// pr_info("REGULATOR_EVENT_ENABLE (%#llx) ", (u64)readl(addr));
	} else if (event == REGULATOR_EVENT_PRE_DISABLE) {
		addr = g_priv->spm_base + SPM_MML0_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = g_priv->spm_base + SPM_MML1_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = g_priv->spm_base + SPM_DIS0_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = g_priv->spm_base + SPM_DIS1_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = g_priv->spm_base + SPM_OVL0_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = g_priv->spm_base + SPM_OVL1_PWR_CON;
		val = readl_relaxed(addr);
		val &= ~SPM_RTFF_SAVE_FLAG;
		writel_relaxed(val, addr);

		addr = g_priv->spm_base + SPM_ISO_CON_SET;
		writel_relaxed(SPM_AOC_VDISP_SRAM_LATCH_ENB, addr);
		writel_relaxed(SPM_AOC_VDISP_SRAM_ISO_DIN, addr);
		writel_relaxed(SPM_VDISP_EXT_BUCK_ISO, addr);

		// addr = g_priv->spm_base + SPM_ISO_CON_STA;
		// pr_info("REGULATOR_EVENT_PRE_DISABLE (%#llx) ", (u64)readl(addr));
	}

	return 0;
}

static int mtk_vdisp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_vdisp *priv;
	struct regulator *rgu;
	struct resource *res;
	int ret = 0;

	VDISPDBG("+");
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	g_priv = priv;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		VDISPERR("fail to get resource SPM_BASE");
		return -EINVAL;
	}
	priv->spm_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!priv->spm_base) {
		VDISPERR("fail to ioremap SPM_BASE: 0x%llx", res->start);
		return -EINVAL;
	}

	rgu = devm_regulator_get(dev, "dis1-shutdown");
	if (IS_ERR(rgu)) {
		VDISPERR("devm_regulator_get dis1-shutdown-supply fail");
		return PTR_ERR(rgu);
	}

	priv->nb.notifier_call = regulator_event_notifier;
	ret = devm_regulator_register_notifier(rgu, &priv->nb);
	if (ret)
		VDISPERR("Failed to register notifier ret(%d)", ret);

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

module_init(mtk_vdisp_init);
module_exit(mtk_vdisp_exit);
MODULE_AUTHOR("William Yang <William-tw.Yang@mediatek.com>");
MODULE_DESCRIPTION("MTK VDISP driver");
MODULE_SOFTDEP("pre:vcp");
MODULE_SOFTDEP("post:mtk-scpsys-mt6989");
MODULE_LICENSE("GPL");
