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

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

/* RROT register offset */

/* SMI offset */
#define SMI_LARB_NON_SEC_CON		0x380

#define RROT_LABEL_TOTAL		0

struct rrot_data {
};

static const struct rrot_data mt6989_rrot_data = {
};

struct mml_comp_rrot {
	struct mml_comp comp;
	const struct rrot_data *data;
	int idx;

	struct device *dev;	/* for dmabuf to iova */
	/* smi register to config sram/dram mode */
	phys_addr_t smi_larb_con;

	u8 input_idx;
};

/* meta data for each different frame config */
struct rrot_frame_data {
	u32 pixel_acc;		/* pixel accumulation */
	u32 datasize;		/* qos data size in bytes */
};

static inline struct rrot_frame_data *rrot_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_comp_rrot *comp_to_rrot(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_rrot, comp);
}

static s32 rrot_prepare(struct mml_comp *comp, struct mml_task *task,
			struct mml_comp_config *ccfg)
{
	return 0;
}

static s32 rrot_buf_map(struct mml_comp *comp, struct mml_task *task,
			const struct mml_path_node *node)
{
	//struct mml_comp_rrot *rrot = comp_to_rrot(comp);

	return 0;
}

static s32 rrot_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	//struct mml_comp_rrot *rrot = comp_to_rrot(comp);

	return 0;
}

static void rrot_buf_unprepare(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	//struct mml_comp_rrot *rrot = comp_to_rrot(comp);
}

s32 rrot_tile_prepare(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg,
	struct tile_func_block *func,
	union mml_tile_data *data)
{
	return 0;
}

static const struct mml_comp_tile_ops rrot_tile_ops = {
	.prepare = rrot_tile_prepare,
};

static u32 rrot_get_label_count(struct mml_comp *comp, struct mml_task *task,
				struct mml_comp_config *ccfg)
{
	return RROT_LABEL_TOTAL;
}

static s32 rrot_config_frame(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	return 0;
}

static s32 rrot_config_tile(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg, u32 idx)
{
	return 0;
}

static s32 rrot_wait(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg, u32 idx)
{
	return 0;
}

static s32 rrot_post(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	return 0;
}

static s32 rrot_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	return 0;
}

static const struct mml_comp_config_ops rrot_cfg_ops = {
	.prepare = rrot_prepare,
	.buf_map = rrot_buf_map,
	.buf_prepare = rrot_buf_prepare,
	.buf_unprepare = rrot_buf_unprepare,
	.get_label_count = rrot_get_label_count,
	.frame = rrot_config_frame,
	.tile = rrot_config_tile,
	.wait = rrot_wait,
	.post = rrot_post,
	.reframe = rrot_reconfig_frame,
};

static u32 rrot_datasize_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	struct rrot_frame_data *rrot_frm = rrot_frm_data(ccfg);

	return rrot_frm->datasize;
}

static u32 rrot_format_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	return task->config->info.dest[ccfg->node->out_idx].data.format;
}

static void rrot_task_done(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
}

static const struct mml_comp_hw_ops rrot_hw_ops = {
	.pw_enable = &mml_comp_pw_enable,
	.pw_disable = &mml_comp_pw_disable,
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.qos_datasize_get = &rrot_datasize_get,
	.qos_format_get = &rrot_format_get,
	.qos_set = &mml_comp_qos_set,
	.qos_clear = &mml_comp_qos_clear,
	.task_done = rrot_task_done,
};

static void rrot_debug_dump(struct mml_comp *comp)
{
}

static void rrot_reset(struct mml_comp *comp, struct mml_frame_config *cfg, u32 pipe)
{
}

static const struct mml_comp_debug_ops rrot_debug_ops = {
	.dump = &rrot_debug_dump,
	.reset = &rrot_reset,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_rrot *rrot = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &rrot->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_rrot *rrot = dev_get_drvdata(dev);

	mml_unregister_comp(master, &rrot->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_rrot *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_rrot *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);
	priv->dev = dev;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
	if (ret)
		dev_err(dev, "fail to config rrot dma mask %d\n", ret);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* init larb for smi and mtcmos */
	ret = mml_comp_init_larb(&priv->comp, dev);
	if (ret) {
		if (ret == -EPROBE_DEFER)
			return ret;
		dev_err(dev, "fail to init component %u larb ret %d\n",
			priv->comp.id, ret);
	}

	/* get index of rrot by alias */
	priv->idx = of_alias_get_id(dev->of_node, "mml-rrot");

	/* assign ops */
	priv->comp.tile_ops = &rrot_tile_ops;
	priv->comp.config_ops = &rrot_cfg_ops;
	priv->comp.hw_ops = &rrot_hw_ops;
	priv->comp.debug_ops = &rrot_debug_ops;

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

const struct of_device_id mml_rrot_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6989-mml_rrot",
		.data = &mt6989_rrot_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_rrot_driver_dt_match);

struct platform_driver mml_rrot_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-rrot",
		.owner = THIS_MODULE,
		.of_match_table = mml_rrot_driver_dt_match,
	},
};

//module_platform_driver(mml_rrot_driver);

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML RROT driver");
MODULE_LICENSE("GPL");
