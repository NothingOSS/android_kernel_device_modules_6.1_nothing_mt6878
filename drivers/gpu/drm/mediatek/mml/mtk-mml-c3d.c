// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/math64.h>
#include <soc/mediatek/smi.h>

#include "mtk-mml-driver.h"
#include "mtk-mml-tile.h"
#include "mtk-mml-sys.h"
#include "mtk-mml-mmp.h"
#include "mtk-mml-dle-adaptor.h"
#include "tile_driver.h"
#include "tile_mdp_func.h"

/* C3D register offset */

#define C3D_LABEL_TOTAL		0

struct c3d_data {
};

static const struct c3d_data mt6989_c3d_data = {
};

struct mml_comp_c3d {
	struct mml_comp comp;
	const struct c3d_data *data;
};

/* meta data for each different frame config */
struct c3d_frame_data {
};

static inline struct c3d_frame_data *c3d_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_comp_c3d *comp_to_c3d(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_c3d, comp);
}

static s32 c3d_prepare(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	return 0;
}

s32 c3d_tile_prepare(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg,
	struct tile_func_block *func,
	union mml_tile_data *data)
{
	return 0;
}

static const struct mml_comp_tile_ops c3d_tile_ops = {
	.prepare = c3d_tile_prepare,
};

static u32 c3d_get_label_count(struct mml_comp *comp, struct mml_task *task,
				 struct mml_comp_config *ccfg)
{
	return C3D_LABEL_TOTAL;
}

static s32 c3d_config_frame(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	return 0;
}

static s32 c3d_config_tile(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 idx)
{
	return 0;
}

static s32 c3d_wait(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 idx)
{
	return 0;
}

static s32 c3d_post(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	return 0;
}

static s32 c3d_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	return 0;
}

static const struct mml_comp_config_ops c3d_cfg_ops = {
	.prepare = c3d_prepare,
	.get_label_count = c3d_get_label_count,
	.frame = c3d_config_frame,
	.tile = c3d_config_tile,
	.wait = c3d_wait,
	.post = c3d_post,
	.reframe = c3d_reconfig_frame,
};

static void c3d_debug_dump(struct mml_comp *comp)
{
}

static void c3d_reset(struct mml_comp *comp, struct mml_frame_config *cfg, u32 pipe)
{
}

static const struct mml_comp_debug_ops c3d_debug_ops = {
	.dump = &c3d_debug_dump,
	.reset = &c3d_reset,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_c3d *c3d = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &c3d->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_c3d *c3d = dev_get_drvdata(dev);

	mml_unregister_comp(master, &c3d->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_c3d *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_c3d *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* assign ops */
	priv->comp.tile_ops = &c3d_tile_ops;
	priv->comp.config_ops = &c3d_cfg_ops;
	priv->comp.debug_ops = &c3d_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return 0;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_c3d_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6989-mml_c3d",
		.data = &mt6989_c3d_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_c3d_driver_dt_match);

struct platform_driver mml_c3d_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-c3d",
		.owner = THIS_MODULE,
		.of_match_table = mml_c3d_driver_dt_match,
	},
};

//module_platform_driver(mml_c3d_driver);

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML C3D driver");
MODULE_LICENSE("GPL");
