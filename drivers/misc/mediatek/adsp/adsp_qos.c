// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/interconnect.h>
#include <linux/of_address.h>
#include "adsp_platform_driver.h"
#include "adsp_qos.h"

static struct adsp_qos_control qos_ctrl;

int adsp_qos_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	int ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg_hrt");
	if (unlikely(!res)) {
		pr_info("%s get resource IFRBUS_AO fail.\n", __func__);
		return 0;
	}

	qos_ctrl.cfg_hrt = devm_ioremap(dev, res->start,
					resource_size(res));
	if (unlikely(!qos_ctrl.cfg_hrt)) {
		pr_warn("%s get ioremap IFRBUS_AO fail: 0x%llx\n",
			__func__, res->start);
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "hrt-ctrl-bits",
				   &qos_ctrl.hrt_bits);
	if (ret) {
		pr_warn("%s get hrt_bits fail:%s.\n", __func__, "hrt-ctrl-bits");
		return -ENODEV;
	}
	/* emi hrt setting */
	writel(qos_ctrl.hrt_bits, qos_ctrl.cfg_hrt);

	qos_ctrl.icc_hrt_path = of_icc_get(dev, "icc-hrt-bw");
	if (IS_ERR_OR_NULL(qos_ctrl.icc_hrt_path)) {
		pr_warn("%s get icc_hrt_path fail:%s.\n", __func__, "icc-hrt-bw");
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(adsp_qos_probe);

void set_adsp_icc_bw(uint32_t bw_mbps)
{
	//pr_info("%s %d Mbps\n", __func__, bw_mbps);
	icc_set_bw(qos_ctrl.icc_hrt_path, MBps_to_icc(bw_mbps), 0);
}
EXPORT_SYMBOL(set_adsp_icc_bw);

void clear_adsp_icc_bw(void)
{
	icc_set_bw(qos_ctrl.icc_hrt_path, 0, 0);
}
EXPORT_SYMBOL(clear_adsp_icc_bw);

