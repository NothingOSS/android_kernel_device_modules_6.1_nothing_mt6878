// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

static int mm_fake_eng_probe(struct platform_device *pdev)
{
	pr_notice("%s for smmu fake dev", __func__);
	return 0;
}

static int mm_fake_eng_remove(struct platform_device *pdev)
{
	pr_notice("%s for smmu fake dev", __func__);
	return 0;
}

static const struct of_device_id mm_fake_eng_of_ids[] = {
	{.compatible = "mediatek,smmu-share-group"},
	{}
};
MODULE_DEVICE_TABLE(of, mm_fake_eng_of_ids);

static struct platform_driver mm_fake_eng_drv = {
	.probe = mm_fake_eng_probe,
	.remove = mm_fake_eng_remove,
	.driver = {
		.name = "mm_fake_eng",
		.of_match_table = mm_fake_eng_of_ids,
	},
};
module_platform_driver(mm_fake_eng_drv);

MODULE_LICENSE("GPL");

