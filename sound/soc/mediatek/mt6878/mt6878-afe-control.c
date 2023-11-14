// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio Control
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Shu-wei Hsu <Shu-wei Hsu@mediatek.com>
 */

#include "mt6878-afe-common.h"
#include <linux/pm_runtime.h>

#include "../common/mtk-sram-manager.h"

/* don't use this directly if not necessary */
static struct mtk_base_afe *local_afe;

int mt6878_set_local_afe(struct mtk_base_afe *afe)
{
	local_afe = afe;
	return 0;
}

unsigned int mt6878_general_rate_transform(struct device *dev,
		unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_IPM2P0_RATE_8K;
	case 11025:
		return MTK_AFE_IPM2P0_RATE_11K;
	case 12000:
		return MTK_AFE_IPM2P0_RATE_12K;
	case 16000:
		return MTK_AFE_IPM2P0_RATE_16K;
	case 22050:
		return MTK_AFE_IPM2P0_RATE_22K;
	case 24000:
		return MTK_AFE_IPM2P0_RATE_24K;
	case 32000:
		return MTK_AFE_IPM2P0_RATE_32K;
	case 44100:
		return MTK_AFE_IPM2P0_RATE_44K;
	case 48000:
		return MTK_AFE_IPM2P0_RATE_48K;
	case 88200:
		return MTK_AFE_IPM2P0_RATE_88K;
	case 96000:
		return MTK_AFE_IPM2P0_RATE_96K;
	case 176400:
		return MTK_AFE_IPM2P0_RATE_176K;
	case 192000:
		return MTK_AFE_IPM2P0_RATE_192K;
	/* not support 260K */
	case 352800:
		return MTK_AFE_IPM2P0_RATE_352K;
	case 384000:
		return MTK_AFE_IPM2P0_RATE_384K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_IPM2P0_RATE_48K);
		return MTK_AFE_IPM2P0_RATE_48K;
	}
}

static unsigned int pcm_rate_transform(struct device *dev,
				       unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_PCM_RATE_8K;
	case 16000:
		return MTK_AFE_PCM_RATE_16K;
	case 32000:
		return MTK_AFE_PCM_RATE_32K;
	case 48000:
		return MTK_AFE_PCM_RATE_48K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_PCM_RATE_32K);
		return MTK_AFE_PCM_RATE_32K;
	}
}

unsigned int mt6878_rate_transform(struct device *dev,
				   unsigned int rate, int aud_blk)
{
	switch (aud_blk) {
	case MT6878_DAI_PCM_0:
	case MT6878_DAI_PCM_1:
		return pcm_rate_transform(dev, rate);
	default:
		return mt6878_general_rate_transform(dev, rate);
	}
}

int mt6878_dai_set_priv(struct mtk_base_afe *afe, int id,
			int priv_size, const void *priv_data)
{
	struct mt6878_afe_private *afe_priv = afe->platform_priv;
	void *temp_data;

	temp_data = devm_kzalloc(afe->dev,
				 priv_size,
				 GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	if (priv_data)
		memcpy(temp_data, priv_data, priv_size);

	if (id < 0 || id >= MT6878_DAI_NUM) {
		dev_info(afe->dev, "%s(), invalid DAI id %d\n", __func__, id);
		return -EINVAL;
	}

	afe_priv->dai_priv[id] = temp_data;

	return 0;
}

/* DC compensation */
int mt6878_enable_dc_compensation(bool enable)
{
	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_info(local_afe->dev, "%s(), status suspended\n", __func__);


	pm_runtime_get_sync(local_afe->dev);
	regmap_update_bits(local_afe->regmap,
			   AFE_ADDA_DL_SDM_DCCOMP_CON,
			   AFE_DL_AUD_DC_COMP_EN_MASK_SFT,
			   (enable ? 1 : 0) << AFE_DL_AUD_DC_COMP_EN_SFT);
	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6878_enable_dc_compensation);

int mt6878_set_lch_dc_compensation(int value)
{
	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_info(local_afe->dev, "%s(), status suspended\n", __func__);

	pm_runtime_get_sync(local_afe->dev);

	/* reset toggle bit
	 * toggle bit only trigger from 0->1
	 * need to reset to 0 before setting dccomp value
	 */
	regmap_update_bits(local_afe->regmap,
			   AFE_ADDA_DL_SDM_DCCOMP_CON,
			   AFE_DL_DCCOMP_SYNC_TOGGLE_MASK_SFT,
			   0x0 << AFE_DL_DCCOMP_SYNC_TOGGLE_SFT);
	regmap_write(local_afe->regmap,
		     AFE_ADDA_DL_DC_COMP_CFG0,
		     value);
	/* toggle sdm */
	regmap_update_bits(local_afe->regmap,
			   AFE_ADDA_DL_SDM_DCCOMP_CON,
			   AFE_DL_DCCOMP_SYNC_TOGGLE_MASK_SFT,
			   0x1 << AFE_DL_DCCOMP_SYNC_TOGGLE_SFT);

	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6878_set_lch_dc_compensation);

int mt6878_set_rch_dc_compensation(int value)
{
	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_info(local_afe->dev, "%s(), status suspended\n", __func__);

	pm_runtime_get_sync(local_afe->dev);

	/* reset toggle bit
	 * toggle bit only trigger from 0->1
	 * need to reset to 0 before setting dccomp value
	 */
	regmap_update_bits(local_afe->regmap,
			   AFE_ADDA_DL_SDM_DCCOMP_CON,
			   AFE_DL_DCCOMP_SYNC_TOGGLE_MASK_SFT,
			   0x0 << AFE_DL_DCCOMP_SYNC_TOGGLE_SFT);
	regmap_write(local_afe->regmap,
		     AFE_ADDA_DL_DC_COMP_CFG1,
		     value);
	/* toggle sdm */
	regmap_update_bits(local_afe->regmap,
			   AFE_ADDA_DL_SDM_DCCOMP_CON,
			   AFE_DL_DCCOMP_SYNC_TOGGLE_MASK_SFT,
			   0x1 << AFE_DL_DCCOMP_SYNC_TOGGLE_SFT);

	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6878_set_rch_dc_compensation);

int mt6878_adda_dl_gain_control(bool mute)
{
	unsigned int dl_gain_ctl;

	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_info(local_afe->dev, "%s(), status suspended\n", __func__);

	pm_runtime_get_sync(local_afe->dev);

	if (mute)
		dl_gain_ctl = MTK_AFE_ADDA_DL_GAIN_MUTE;
	else
		dl_gain_ctl = 0xf74ff74f; //MTK_AFE_ADDA_DL_GAIN_NORMAL for platform before 6878

	regmap_update_bits(local_afe->regmap,
			   AFE_ADDA_DL_SRC_CON1,
			   AFE_DL_GAIN2_CTL_PRE_MASK_SFT,
			   dl_gain_ctl << AFE_DL_GAIN2_CTL_PRE_SFT);

	dev_info(local_afe->dev, "%s(), adda_dl_gain %x\n",
		 __func__, dl_gain_ctl);

	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6878_adda_dl_gain_control);

struct audio_swpm_data mt6878_aud_get_power_scenario(void)
{
	struct audio_swpm_data test;

	test.adda_mode = 0;
	test.afe_on = 1;
	test.channel_num = 4;
	test.input_device = 0;
	test.output_device = 2;
	test.sample_rate = 2;
	test.user_case = 1;
	return test;
}
EXPORT_SYMBOL_GPL(mt6878_aud_get_power_scenario);

int mt6878_set_adda_predistortion(int hp_impedance)
{
	unsigned int read_predis_con0 = 0;
	unsigned int read_predis_con1 = 0;

	dev_info(local_afe->dev, "%s()++\n", __func__);

	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_info(local_afe->dev, "%s(), status suspended\n", __func__);

	pm_runtime_get_sync(local_afe->dev);

	if (hp_impedance == 0) {
		dev_info(local_afe->dev, "%s(), clean predistortion\n", __func__);
		regmap_write(local_afe->regmap, AFE_ADDA_DL_PREDIS_CON0, 0);
		regmap_write(local_afe->regmap, AFE_ADDA_DL_PREDIS_CON1, 0);
		goto exit;
	}

	if (hp_impedance < 30) { /* 16 Ohm */
		regmap_write(local_afe->regmap,
			   AFE_ADDA_DL_PREDIS_CON0,
			   0x800E0000);
		regmap_write(local_afe->regmap,
			   AFE_ADDA_DL_PREDIS_CON1,
			   0x800E0000);
	} else {              /* 32 Ohm */
		regmap_write(local_afe->regmap,
			   AFE_ADDA_DL_PREDIS_CON0,
			   0x80090000);
		regmap_write(local_afe->regmap,
			   AFE_ADDA_DL_PREDIS_CON1,
			   0x80090000);
	}

exit:
	regmap_read(local_afe->regmap, AFE_ADDA_DL_PREDIS_CON0, &read_predis_con0);
	regmap_read(local_afe->regmap, AFE_ADDA_DL_PREDIS_CON1, &read_predis_con1);

	dev_info(local_afe->dev, "%s(), AFE_ADDA_DL_PREDIS_CON0=0x%x, AFE_ADDA_DL_PREDIS_CON1=0x%x\n",
		 __func__, read_predis_con0, read_predis_con1);

	pm_runtime_put(local_afe->dev);
	dev_info(local_afe->dev, "%s()--\n", __func__);
	return 0;
}
EXPORT_SYMBOL(mt6878_set_adda_predistortion);
