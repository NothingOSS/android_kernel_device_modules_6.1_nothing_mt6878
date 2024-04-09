// SPDX-License-Identifier: GPL-2.0
/*
 *  mt6878-mt6369.c  --  mt6878 mt6369 ALSA SoC machine driver
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Shu-wei Hsu <Shu-wei.Hsu@mediatek.com>
 */

#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../common/mtk-afe-platform-driver.h"
#include "mt6878-afe-common.h"
#include "mt6878-afe-clk.h"
#include "mt6878-afe-gpio.h"
#include "../../codecs/mt6369.h"
#include "../common/mtk-sp-spk-amp.h"
/*
 * if need additional control for the ext spk amp that is connected
 * after Lineout Buffer / HP Buffer on the codec, put the control in
 * mt6878_mt6369_spk_amp_event()
 */
#define EXT_SPK_AMP_W_NAME "Ext_Speaker_Amp"
#define MT6369_PROBE_DONE 1
//#define BYPASS_FOR_61_BRINGUP

#if IS_ENABLED(CONFIG_SND_SOC_MT6369_ACCDET) && !defined(BYPASS_FOR_61_BRINGUP)
#include "../../codecs/mt6369-accdet.h"
#endif


static struct snd_soc_card mt6878_mt6369_soc_card;

struct mt6878_compress_info compr_info;

static const char *const mt6878_spk_type_str[] = {MTK_SPK_NOT_SMARTPA_STR,
						  MTK_SPK_RICHTEK_RT5509_STR,
						  MTK_SPK_MEDIATEK_MT6660_STR,
						  MTK_SPK_RICHTEK_RT5512_STR,
						  MTK_SPK_GOODIX_TFA98XX_STR};
static const char *const
	mt6878_spk_i2s_type_str[] = {MTK_SPK_I2S_0_STR,
				     MTK_SPK_I2S_1_STR,
				     MTK_SPK_I2S_2_STR,
				     MTK_SPK_I2S_3_STR,
				     MTK_SPK_I2S_5_STR,
				     MTK_SPK_TINYCONN_I2S_0_STR};

static const struct soc_enum mt6878_spk_type_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6878_spk_type_str),
			    mt6878_spk_type_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6878_spk_i2s_type_str),
			    mt6878_spk_i2s_type_str),
};

static int mt6878_spk_type_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

static int mt6878_spk_i2s_out_type_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_i2s_out_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

static int mt6878_spk_i2s_in_type_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_i2s_in_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

static int mt6878_compress_info_set(struct snd_kcontrol *kcontrol,
				    const unsigned int __user *data,
				    unsigned int size)
{
	if (copy_from_user(&compr_info,
			   data,
			   sizeof(struct mt6878_compress_info))) {
		pr_info("%s() copy fail, data=%p, size=%d\n",
			__func__, data, size);
		return -EFAULT;
	}
	return 0;
}

static int mt6878_compress_info_get(struct snd_kcontrol *kcontrol,
				    unsigned int __user *data, unsigned int size)
{
	struct snd_soc_card *card = &mt6878_mt6369_soc_card;
	struct snd_card *snd_card;
	struct snd_soc_dai_link *dai_link;

	struct snd_device *snd_dev;
	struct snd_compr *compr;
	int ret = 0, i = 0;
	bool found_type = false;
	bool found_name = false;
	bool found_dir = false;

	snd_card = card->snd_card;

	pr_info("i = %d, compr_info->id: %s\n", compr_info.device, compr_info.id);

	list_for_each_entry(snd_dev, &snd_card->devices, list) {
		if ((unsigned int)snd_dev->type == (unsigned int)SNDRV_DEV_COMPRESS) {
			found_type = true;
			compr = snd_dev->device_data;
			if (compr->device == compr_info.device) {
				found_dir = true;
				pr_debug("%s() compr->direction %s\n",
					 __func__,
					 (compr->direction) ? "Capture" : "Playback");
				compr_info.dir = compr->direction;
			}
			for_each_card_prelinks(card, i, dai_link) {
				if (i == compr_info.device) {
					if (dai_link->stream_name != NULL) {
						found_name = true;
						pr_debug("device = %d, dai_link->name: %s\n",
							 i, dai_link->stream_name);
						strscpy(compr_info.id, dai_link->stream_name,
							sizeof(compr_info.id));
					} else
						pr_info("compress_info_get fail\n");
					break;
				}
			}
			break;
		}
	}
	if (copy_to_user(data, &compr_info, sizeof(struct mt6878_compress_info))) {
		pr_info("%s(), copy_to_user fail", __func__);
		ret = -EFAULT;
	}
	if (found_type == false || found_name == false || found_dir == false) {
		pr_info("%s(), Not found! type %d, name %d or dir %d",
			__func__, found_type, found_name, found_dir);
		ret = -EFAULT;
	}
	return ret;
}

static int mt6878_mt6369_spk_amp_event(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;

	dev_info(card->dev, "%s(), event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* spk amp on control */
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* spk amp off control */
		break;
	default:
		break;
	}

	return 0;
};

static const struct snd_soc_dapm_widget mt6878_mt6369_widgets[] = {
	SND_SOC_DAPM_SPK(EXT_SPK_AMP_W_NAME, mt6878_mt6369_spk_amp_event),
};

static const struct snd_soc_dapm_route mt6878_mt6369_routes[] = {
	{EXT_SPK_AMP_W_NAME, NULL, "LINEOUT L"},
	{EXT_SPK_AMP_W_NAME, NULL, "Headphone L Ext Spk Amp"},
	{EXT_SPK_AMP_W_NAME, NULL, "Headphone R Ext Spk Amp"},
};
static const struct snd_soc_dapm_route mt6878_mt6369_routes_dummy[] = {};

static const struct snd_kcontrol_new mt6878_mt6369_controls[] = {
	SOC_DAPM_PIN_SWITCH(EXT_SPK_AMP_W_NAME),
	SOC_ENUM_EXT("MTK_SPK_TYPE_GET", mt6878_spk_type_enum[0],
		     mt6878_spk_type_get, NULL),
	SOC_ENUM_EXT("MTK_SPK_I2S_OUT_TYPE_GET", mt6878_spk_type_enum[1],
		     mt6878_spk_i2s_out_type_get, NULL),
	SOC_ENUM_EXT("MTK_SPK_I2S_IN_TYPE_GET", mt6878_spk_type_enum[1],
		     mt6878_spk_i2s_in_type_get, NULL),
	SND_SOC_BYTES_TLV("MTK_COMPRESS_INFO",
			  sizeof(struct mt6878_compress_info),
			  mt6878_compress_info_get, mt6878_compress_info_set),
};

/*
 * define mtk_spk_i2s_mck node in dts when need mclk,
 * BE i2s need assign snd_soc_ops = mt6878_mt6369_i2s_ops
 */
static int mt6878_mt6369_i2s_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int rate = params_rate(params);
	unsigned int mclk_fs_ratio = 128;
	unsigned int mclk_fs = rate * mclk_fs_ratio;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	return snd_soc_dai_set_sysclk(cpu_dai,
				      0, mclk_fs, SND_SOC_CLOCK_OUT);
}

static const struct snd_soc_ops mt6878_mt6369_i2s_ops = {
	.hw_params = mt6878_mt6369_i2s_hw_params,
};

static int mt6878_mt6369_mtkaif_calibration(struct snd_soc_pcm_runtime *rtd)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6878_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_component *codec_component =
		snd_soc_rtdcom_lookup(rtd, CODEC_MT6369_NAME);
	int phase;
	unsigned int monitor = 0;
	int test_done_1, test_done_2;
	int miso0_need_calib, miso1_need_calib;
	int cycle_1, cycle_2;
	int prev_cycle_1, prev_cycle_2;
	int counter;
	int mtkaif_calib_ok;

	dev_info(afe->dev, "%s(), start\n", __func__);

	pm_runtime_get_sync(afe->dev);

	miso0_need_calib = mt6878_afe_gpio_is_prepared(MT6878_AFE_GPIO_DAT_MISO0_ON);
	miso1_need_calib = mt6878_afe_gpio_is_prepared(MT6878_AFE_GPIO_DAT_MISO1_ON);

	mt6878_afe_gpio_request(afe, true, MT6878_DAI_ADDA, 1);
	mt6878_afe_gpio_request(afe, true, MT6878_DAI_ADDA, 0);

	mt6369_mtkaif_calibration_enable(codec_component);

	/* set clock protocol 2 */
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP_CFG0, 0xff, 0xb8);
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP_CFG0, 0xff, 0xb9);

	/* set test type to synchronizer pulse */
	regmap_update_bits(afe_priv->topckgen,
			   CKSYS_AUD_TOP_CFG, 0xffff, 0x4);

	mtkaif_calib_ok = true;
	afe_priv->mtkaif_calibration_num_phase = 42;	/* mt6359: 0 ~ 42 */
	afe_priv->mtkaif_chosen_phase[0] = -1;
	afe_priv->mtkaif_chosen_phase[1] = -1;
	afe_priv->mtkaif_chosen_phase[2] = -1;

	for (phase = 0;
	     phase <= afe_priv->mtkaif_calibration_num_phase &&
	     mtkaif_calib_ok;
	     phase++) {
		mt6369_set_mtkaif_calibration_phase(codec_component,
						    phase, phase, phase);

		regmap_update_bits(afe_priv->topckgen,
				   CKSYS_AUD_TOP_CFG, 0x1, 0x1);

		test_done_1 = miso0_need_calib ? 0 : -1;
		test_done_2 = miso1_need_calib ? 0 : -1;
		cycle_1 = -1;
		cycle_2 = -1;
		counter = 0;
		while (test_done_1 == 0 ||
		       test_done_2 == 0) {
			regmap_read(afe_priv->topckgen,
				    CKSYS_AUD_TOP_MON, &monitor);

			/* get test status */
			if (test_done_1 == 0)
				test_done_1 = (monitor >> 28) & 0x1;
			if (test_done_2 == 0)
				test_done_2 = (monitor >> 29) & 0x1;

			/* get delay cycle */
			if (test_done_1 == 1)
				cycle_1 = monitor & 0xf;
			if (test_done_2 == 1)
				cycle_2 = (monitor >> 4) & 0xf;

			/* handle if never test done */
			if (++counter > 10000) {
				dev_info(afe->dev, "%s(), test fail, cycle_1 %d, cycle_2 %d, monitor 0x%x\n",
					__func__,
					cycle_1, cycle_2, monitor);
				mtkaif_calib_ok = false;
				break;
			}
		}

		if (phase == 0) {
			prev_cycle_1 = cycle_1;
			prev_cycle_2 = cycle_2;
		}

		if (miso0_need_calib &&
		    cycle_1 != prev_cycle_1 &&
		    afe_priv->mtkaif_chosen_phase[0] < 0) {
			afe_priv->mtkaif_chosen_phase[0] = phase - 1;
			afe_priv->mtkaif_phase_cycle[0] = prev_cycle_1;
		}

		if (miso1_need_calib &&
		    cycle_2 != prev_cycle_2 &&
		    afe_priv->mtkaif_chosen_phase[1] < 0) {
			afe_priv->mtkaif_chosen_phase[1] = phase - 1;
			afe_priv->mtkaif_phase_cycle[1] = prev_cycle_2;
		}


		regmap_update_bits(afe_priv->topckgen,
				   CKSYS_AUD_TOP_CFG, 0x1, 0x0);
	}

	mt6369_set_mtkaif_calibration_phase(codec_component,
		(afe_priv->mtkaif_chosen_phase[0] < 0) ?
		0 : afe_priv->mtkaif_chosen_phase[0],
		(afe_priv->mtkaif_chosen_phase[1] < 0) ?
		0 : afe_priv->mtkaif_chosen_phase[1],
		(afe_priv->mtkaif_chosen_phase[2] < 0) ?
		0 : afe_priv->mtkaif_chosen_phase[2]);

	/* disable rx fifo */
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP_CFG0, 0xff, 0xb8);

	mt6369_mtkaif_calibration_disable(codec_component);

	mt6878_afe_gpio_request(afe, false, MT6878_DAI_ADDA, 1);
	mt6878_afe_gpio_request(afe, false, MT6878_DAI_ADDA, 0);


	/* disable syncword if miso pin not prepared */
	if (!miso0_need_calib)
		regmap_update_bits(afe->regmap, AFE_MTKAIF0_RX_CFG2,
				   RG_MTKAIF0_RXIF_SYNC_WORD0_DISABLE_MASK_SFT,
				   0x1 << RG_MTKAIF0_RXIF_SYNC_WORD0_DISABLE_SFT);
	if (!miso1_need_calib)
		regmap_update_bits(afe->regmap, AFE_MTKAIF0_RX_CFG2,
				   RG_MTKAIF0_RXIF_SYNC_WORD1_DISABLE_MASK_SFT,
				   0x1 << RG_MTKAIF0_RXIF_SYNC_WORD1_DISABLE_SFT);

	pm_runtime_put(afe->dev);

	dev_info(afe->dev, "%s(), mtkaif_chosen_phase[0/1]:%d/%d, miso_need_calib[%d/%d]\n",
		 __func__,
		 afe_priv->mtkaif_chosen_phase[0],
		 afe_priv->mtkaif_chosen_phase[1],
		 miso0_need_calib, miso1_need_calib);
#endif
	return 0;
}

static int mt6878_mt6369_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6878_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_component *codec_component =
		snd_soc_rtdcom_lookup(rtd, CODEC_MT6369_NAME);
	struct snd_soc_dapm_context *dapm = &rtd->card->dapm;
	struct mt6369_codec_ops ops;

	if (!MT6369_PROBE_DONE) {
		dev_info(afe->dev, "%s(), mt6369_probe_done == false, gonna return 0\n", __func__);
		return 0;
	}

	/* set dc component callback function for codec */
	ops.enable_dc_compensation = mt6878_enable_dc_compensation;
	ops.set_lch_dc_compensation = mt6878_set_lch_dc_compensation;
	ops.set_rch_dc_compensation = mt6878_set_rch_dc_compensation;
	ops.adda_dl_gain_control = mt6878_adda_dl_gain_control;
	ops.set_adda_predistortion = mt6878_set_adda_predistortion;
	mt6369_set_codec_ops(codec_component, &ops);

	/* set mtkaif protocol */
	mt6369_set_mtkaif_protocol(codec_component,
				   MTKAIF_PROTOCOL_2_CLK_P2);
	afe_priv->mtkaif_protocol = MTKAIF_PROTOCOL_2_CLK_P2;

	/* mtkaif calibration */
	mt6878_mt6369_mtkaif_calibration(rtd);

	/* disable ext amp connection */
	snd_soc_dapm_disable_pin(dapm, EXT_SPK_AMP_W_NAME);
#if IS_ENABLED(CONFIG_SND_SOC_MT6369_ACCDET) && !defined(BYPASS_FOR_61_BRINGUP)
	mt6369_accdet_init(codec_component, rtd->card);
#endif
	return 0;
}

static int mt6878_i2s_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	dev_info(rtd->dev, "%s(), fix format to 32bit\n", __func__);

	/* fix BE i2s format to 32bit, clean param mask first */
	snd_mask_reset_range(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
			     0, SNDRV_PCM_FORMAT_LAST);

	params_set_format(params, SNDRV_PCM_FORMAT_S32_LE);
	return 0;
}

#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT) && !defined(SKIP_SB_VOW)
static const struct snd_pcm_hardware mt6878_mt6369_vow_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.period_bytes_min = 256,
	.period_bytes_max = 2 * 1024,
	.periods_min = 2,
	.periods_max = 4,
	.buffer_bytes_max = 2 * 2 * 1024,
};

static int mt6878_mt6369_vow_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	int i;

	dev_info(afe->dev, "%s(), start\n", __func__);
	snd_soc_set_runtime_hwparams(substream, &mt6878_mt6369_vow_hardware);

	mt6878_afe_gpio_request(afe, true, MT6878_DAI_VOW, 0);

	/* ASoC will call pm_runtime_get, but vow don't need */
	for_each_rtd_components(rtd, i, component) {
		pm_runtime_put_autosuspend(component->dev);
	}

	return 0;
}

static void mt6878_mt6369_vow_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	int i;

	dev_info(afe->dev, "%s(), end\n", __func__);
	mt6878_afe_gpio_request(afe, false, MT6878_DAI_VOW, 0);

	/* restore to fool ASoC */
	for_each_rtd_components(rtd, i, component) {
		pm_runtime_get_sync(component->dev);
	}
}

static const struct snd_soc_ops mt6878_mt6369_vow_ops = {
	.startup = mt6878_mt6369_vow_startup,
	.shutdown = mt6878_mt6369_vow_shutdown,
};
#endif  // #if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)

/* FE */
SND_SOC_DAILINK_DEFS(playback0,
	DAILINK_COMP_ARRAY(COMP_CPU("DL0")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback1,
	DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback2,
	DAILINK_COMP_ARRAY(COMP_CPU("DL2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback3,
	DAILINK_COMP_ARRAY(COMP_CPU("DL3")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback4,
	DAILINK_COMP_ARRAY(COMP_CPU("DL4")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback5,
	DAILINK_COMP_ARRAY(COMP_CPU("DL5")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback6,
	DAILINK_COMP_ARRAY(COMP_CPU("DL6")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback7,
	DAILINK_COMP_ARRAY(COMP_CPU("DL7")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback8,
	DAILINK_COMP_ARRAY(COMP_CPU("DL8")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback23,
	DAILINK_COMP_ARRAY(COMP_CPU("DL23")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback24,
	DAILINK_COMP_ARRAY(COMP_CPU("DL24")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback25,
	DAILINK_COMP_ARRAY(COMP_CPU("DL25")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback_24ch,
	DAILINK_COMP_ARRAY(COMP_CPU("DL_24CH")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture0,
	DAILINK_COMP_ARRAY(COMP_CPU("UL0")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture1,
	DAILINK_COMP_ARRAY(COMP_CPU("UL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture2,
	DAILINK_COMP_ARRAY(COMP_CPU("UL2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture3,
	DAILINK_COMP_ARRAY(COMP_CPU("UL3")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture4,
	DAILINK_COMP_ARRAY(COMP_CPU("UL4")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture5,
	DAILINK_COMP_ARRAY(COMP_CPU("UL5")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture6,
	DAILINK_COMP_ARRAY(COMP_CPU("UL6")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture7,
	DAILINK_COMP_ARRAY(COMP_CPU("UL7")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture8,
	DAILINK_COMP_ARRAY(COMP_CPU("UL8")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture9,
	DAILINK_COMP_ARRAY(COMP_CPU("UL9")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture10,
	DAILINK_COMP_ARRAY(COMP_CPU("UL10")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture24,
	DAILINK_COMP_ARRAY(COMP_CPU("UL24")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture25,
	DAILINK_COMP_ARRAY(COMP_CPU("UL25")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture_cm0,
	DAILINK_COMP_ARRAY(COMP_CPU("UL_CM0")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture_cm1,
	DAILINK_COMP_ARRAY(COMP_CPU("UL_CM1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture_etdm_in1,
	DAILINK_COMP_ARRAY(COMP_CPU("UL_ETDM_IN1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture_etdm_in2,
	DAILINK_COMP_ARRAY(COMP_CPU("UL_ETDM_IN2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture_etdm_in4,
	DAILINK_COMP_ARRAY(COMP_CPU("UL_ETDM_IN4")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* hostless */
SND_SOC_DAILINK_DEFS(hostless_lpbk,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless LPBK DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_fm,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless FM DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_speech,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless Speech DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_bt,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless BT DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_sph_echo_ref,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_Sph_Echo_Ref_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_spk_init,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_Spk_Init_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_adda_dl_i2s_out,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_ADDA_DL_I2S_OUT DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_src0,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_SRC_0_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_src2,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_SRC_2_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_hw_src_0_out,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_HW_SRC_0_OUT_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_hw_src_0_in,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_HW_SRC_0_IN_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_hw_src_1_out,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_HW_SRC_1_OUT_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_hw_src_1_in,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_HW_SRC_1_IN_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_hw_src_2_out,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_HW_SRC_2_OUT_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_hw_src_2_in,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_HW_SRC_2_IN_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_src_bargein,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_SRC_Bargein_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* BE */
SND_SOC_DAILINK_DEFS(adda,
	DAILINK_COMP_ARRAY(COMP_CPU("ADDA")),
	DAILINK_COMP_ARRAY(COMP_CODEC(DEVICE_MT6369_NAME,
				      "mt6369-snd-codec-aif1")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(adda_ch34,
	DAILINK_COMP_ARRAY(COMP_CPU("ADDA_CH34")),
	DAILINK_COMP_ARRAY(COMP_CODEC(DEVICE_MT6369_NAME,
				      "mt6369-snd-codec-aif2")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(ap_dmic,
	DAILINK_COMP_ARRAY(COMP_CPU("AP_DMIC")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(ap_dmic_ch34,
	DAILINK_COMP_ARRAY(COMP_CPU("AP_DMIC_CH34")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2sin1,
	DAILINK_COMP_ARRAY(COMP_CPU("I2SIN1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2sin2,
	DAILINK_COMP_ARRAY(COMP_CPU("I2SIN2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2sin4,
	DAILINK_COMP_ARRAY(COMP_CPU("I2SIN4")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2sout1,
	DAILINK_COMP_ARRAY(COMP_CPU("I2SOUT1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2sout2,
	DAILINK_COMP_ARRAY(COMP_CPU("I2SOUT2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2sout4,
	DAILINK_COMP_ARRAY(COMP_CPU("I2SOUT4")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_gain0,
	DAILINK_COMP_ARRAY(COMP_CPU("HW Gain 0")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_gain1,
	DAILINK_COMP_ARRAY(COMP_CPU("HW Gain 1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_gain2,
	DAILINK_COMP_ARRAY(COMP_CPU("HW Gain 2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_gain3,
	DAILINK_COMP_ARRAY(COMP_CPU("HW Gain 3")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_src0,
	DAILINK_COMP_ARRAY(COMP_CPU("HW_SRC_0")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_src1,
	DAILINK_COMP_ARRAY(COMP_CPU("HW_SRC_1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_src2,
	DAILINK_COMP_ARRAY(COMP_CPU("HW_SRC_2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(connsys_i2s,
	DAILINK_COMP_ARRAY(COMP_CPU("CONNSYS_I2S")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(pcm0,
	DAILINK_COMP_ARRAY(COMP_CPU("PCM 0")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(pcm1,
	DAILINK_COMP_ARRAY(COMP_CPU("PCM 1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* hostless */
SND_SOC_DAILINK_DEFS(hostless_ul1,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL1 DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_ul2,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL2 DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_ul3,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL3 DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_ul4,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL4 DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_dsp_dl,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_DSP_DL DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_hw_gain_aaudio,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless HW Gain AAudio DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_src_aaudio,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless SRC AAudio DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
#if IS_ENABLED(CONFIG_DEVICE_MODULES_SND_SOC_MTK_BTCVSD) && !defined(SKIP_SB_BTCVSD)
SND_SOC_DAILINK_DEFS(btcvsd,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("18830000.mtk-btcvsd-snd")));
#endif
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT) && !defined(SKIP_SB_VOW)
SND_SOC_DAILINK_DEFS(vow,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(DEVICE_MT6369_NAME,
				      "mt6369-snd-codec-vow")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
#endif
#if IS_ENABLED(CONFIG_MTK_ULTRASND_PROXIMITY) && !defined(SKIP_SB_ULTRA)
SND_SOC_DAILINK_DEFS(ultra,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-scp-ultra")));
#endif
#if (IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) && IS_ENABLED(CONFIG_SND_SOC_MTK_OFFLOAD) \
	&& !defined(SKIP_SB_OFFLOAD))
SND_SOC_DAILINK_DEFS(dspoffload,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_offload_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("mt-soc-offload-common")));
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) && !defined(SKIP_SB_DSP)
SND_SOC_DAILINK_DEFS(dspvoip,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_voip_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspprimary,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_primary_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspdeepbuf,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_deepbuf_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspfast,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_fast_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspspatializer,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_spatializer_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspplayback,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_Playback_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspcapture1,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_capture_ul1_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspcallfinal,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_call_final_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspktv,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_ktv_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspcaptureraw,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_capture_raw_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspfmadsp,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_fm_adsp_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspa2dp,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_a2dp_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspbledl,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_bledl_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspbleul,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_bleul_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspbtdl,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_btdl_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspbtul,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_btul_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspulproc,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_ulproc_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspechodl,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_echodl_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspusbdl,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_usbdl_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspusbul,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_usbul_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspmddl,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_mddl_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
SND_SOC_DAILINK_DEFS(dspmdul,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_mdul_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-audio-dsp")));
#endif

static struct snd_soc_dai_link mt6878_mt6369_dai_links[] = {
	/* Front End DAI links */
	{
		.name = "Playback_1",
		.stream_name = "Playback_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback0),
	},
	{
		.name = "Playback_12",
		.stream_name = "Playback_12",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback1),
	},
	{
		.name = "Playback_2",
		.stream_name = "Playback_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback2),
	},
	{
		.name = "Playback_3",
		.stream_name = "Playback_3",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback3),
	},
	{
		.name = "Playback_5",
		.stream_name = "Playback_5",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback4),
	},
	{
		.name = "Playback_8",
		.stream_name = "Playback_8",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback5),
	},
	{
		.name = "Playback_6",
		.stream_name = "Playback_6",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback6),
	},
	{
		.name = "Playback_7",
		.stream_name = "Playback_7",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback7),
	},
	{
		.name = "Playback_9",
		.stream_name = "Playback_9",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback8),
	},
	{
		.name = "Playback_13",
		.stream_name = "Playback_13",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback23),
	},
	{
		.name = "Playback_4",
		.stream_name = "Playback_4",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback24),
	},
	{
		.name = "Playback_25",
		.stream_name = "Playback_25",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback25),
	},
	{
		.name = "Playback_11",
		.stream_name = "Playback_11",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback_24ch),
	},
	{
		.name = "Capture_1",
		.stream_name = "Capture_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture9),
	},
	{
		.name = "Capture_2",
		.stream_name = "Capture_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture1),
	},
	{
		.name = "Capture_3",
		.stream_name = "Capture_3",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture0),
	},
	{
		.name = "Capture_4",
		.stream_name = "Capture_4",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture3),
	},
	{
		.name = "Capture_5",
		.stream_name = "Capture_5",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture7),
	},
	{
		.name = "Capture_6",
		.stream_name = "Capture_6",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture4),
	},
	{
		.name = "Capture_7",
		.stream_name = "Capture_7",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture2),
	},
	{
		.name = "Capture_8",
		.stream_name = "Capture_8",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture5),
	},
	{
		.name = "Capture_9",
		.stream_name = "Capture_9",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture_cm0),
	},
	{
		.name = "Capture_10",
		.stream_name = "Capture_10",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture_cm1),
	},
	{
		.name = "Capture_11",
		.stream_name = "Capture_11",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture10),
	},
	{
		.name = "Capture_12",
		.stream_name = "Capture_12",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture6),
	},
	{
		.name = "Capture_13",
		.stream_name = "Capture_13",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture25),
	},
	{
		.name = "Capture_Mono_1",
		.stream_name = "Capture_Mono_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture8),
	},
	{
		.name = "Capture_Mono_2",
		.stream_name = "Capture_Mono_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture24),
	},
	{
		.name = "Capture_ETDM_In1",
		.stream_name = "Capture_ETDM_In1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture_etdm_in1),
	},
	{
		.name = "Capture_ETDM_In2",
		.stream_name = "Capture_ETDM_In2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture_etdm_in2),
	},
	{
		.name = "Capture_ETDM_In4",
		.stream_name = "Capture_ETDM_In4",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture_etdm_in4),
	},
	{
		.name = "Hostless_LPBK",
		.stream_name = "Hostless_LPBK",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_lpbk),
	},
	{
		.name = "Hostless_FM",
		.stream_name = "Hostless_FM",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_fm),
	},
	{
		.name = "Hostless_Speech",
		.stream_name = "Hostless_Speech",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_speech),
	},
	{
		.name = "Hostless_BT",
		.stream_name = "Hostless_BT",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_bt),
	},
	{
		.name = "Hostless_Sph_Echo_Ref",
		.stream_name = "Hostless_Sph_Echo_Ref",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_sph_echo_ref),
	},
	{
		.name = "Hostless_Spk_Init",
		.stream_name = "Hostless_Spk_Init",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_spk_init),
	},
	{
		.name = "Hostless_ADDA_DL_I2S_OUT",
		.stream_name = "Hostless_ADDA_DL_I2S_OUT",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_adda_dl_i2s_out),
	},
	{
		.name = "Hostless_SRC_1",
		.stream_name = "Hostless_SRC_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_src0),
	},
	{
		.name = "Hostless_SRC_3",
		.stream_name = "Hostless_SRC_3",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_src2),
	},
	{
		.name = "Hostless_HW_SRC_1_OUT",
		.stream_name = "Hostless_HW_SRC_1_OUT",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_hw_src_0_out),
	},
	{
		.name = "Hostless_HW_SRC_1_IN",
		.stream_name = "Hostless_HW_SRC_1_IN",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_hw_src_0_in),
	},
	{
		.name = "Hostless_HW_SRC_2_OUT",
		.stream_name = "Hostless_HW_SRC_2_OUT",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_hw_src_1_out),
	},
	{
		.name = "Hostless_HW_SRC_2_IN",
		.stream_name = "Hostless_HW_SRC_2_IN",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_hw_src_1_in),
	},
	{
		.name = "Hostless_HW_SRC_3_OUT",
		.stream_name = "Hostless_HW_SRC_3_OUT",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_hw_src_2_out),
	},
	{
		.name = "Hostless_HW_SRC_3_IN",
		.stream_name = "Hostless_HW_SRC_3_IN",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_hw_src_2_in),
	},
	{
		.name = "Hostless_SRC_Bargein",
		.stream_name = "Hostless_SRC_Bargein",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_src_bargein),
	},
	/* Back End DAI links */
	{
		.name = "Primary Codec",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.init = mt6878_mt6369_init,
		SND_SOC_DAILINK_REG(adda),
	},
	{
		.name = "Primary Codec CH34",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adda_ch34),
	},
	{
		.name = "AP_DMIC",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(ap_dmic),
	},
	{
		.name = "AP_DMIC_CH34",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(ap_dmic_ch34),
	},
	{
		.name = "I2SIN1",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt6878_mt6369_i2s_ops,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6878_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(i2sin1),
	},
	{
		.name = "I2SIN2",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt6878_mt6369_i2s_ops,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6878_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(i2sin2),
	},
	{
		.name = "I2SIN4",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt6878_mt6369_i2s_ops,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = mt6878_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(i2sin4),
	},
	{
		.name = "I2SOUT1",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt6878_mt6369_i2s_ops,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6878_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(i2sout1),
	},
	{
		.name = "I2SOUT2",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt6878_mt6369_i2s_ops,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6878_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(i2sout2),
	},
	{
		.name = "I2SOUT4",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt6878_mt6369_i2s_ops,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = mt6878_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(i2sout4),
	},
	{
		.name = "HW Gain 0",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_gain0),
	},
	{
		.name = "HW Gain 1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_gain1),
	},
	{
		.name = "HW Gain 2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_gain2),
	},
	{
		.name = "HW Gain 3",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_gain3),
	},
	{
		.name = "HW_SRC_0",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_src0),
	},
	{
		.name = "HW_SRC_1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_src1),
	},
	{
		.name = "HW_SRC_2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_src2),
	},
	{
		.name = "CONNSYS_I2S",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(connsys_i2s),
	},
	{
		.name = "PCM 0",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(pcm0),
	},
	{
		.name = "PCM 1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(pcm1),
	},
	/* dummy BE for ul memif to record from dl memif */
	{
		.name = "Hostless_UL1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_ul2),
	},
	{
		.name = "Hostless_UL2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_ul1),
	},
	{
		.name = "Hostless_UL3",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_ul3),
	},
	{
		.name = "Hostless_UL4",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_ul4),
	},
	{
		.name = "Hostless_DSP_DL",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_dsp_dl),
	},
	{
		.name = "Hostless_HW_Gain_AAudio",
		.stream_name = "Hostless_HW_Gain_AAudio",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_hw_gain_aaudio),
	},
	{
		.name = "Hostless_SRC_AAudio",
		.stream_name = "Hostless_SRC_AAudio",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_src_aaudio),
	},
	/* BTCVSD */
#if IS_ENABLED(CONFIG_DEVICE_MODULES_SND_SOC_MTK_BTCVSD)  && !defined(SKIP_SB_BTCVSD)
	{
		.name = "BTCVSD",
		.stream_name = "BTCVSD",
		SND_SOC_DAILINK_REG(btcvsd),
	},
#endif
	/* VoW */
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT) && !defined(SKIP_SB_VOW)
	{
		.name = "VOW_Capture",
		.stream_name = "VOW_Capture",
		.ignore_suspend = 1,
		.ops = &mt6878_mt6369_vow_ops,
		SND_SOC_DAILINK_REG(vow),
	},
#endif
#if IS_ENABLED(CONFIG_MTK_ULTRASND_PROXIMITY) && !defined(SKIP_SB_ULTRA)
	{
		.name = "SCP_ULTRA_Playback",
		.stream_name = "SCP_ULTRA_Playback",
		SND_SOC_DAILINK_REG(ultra),
	},
#endif
#if (IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) && IS_ENABLED(CONFIG_SND_SOC_MTK_OFFLOAD) \
	&& !defined(SKIP_SB_OFFLOAD))
	{
		.name = "Offload_Playback",
		.stream_name = "Offload_Playback",
		SND_SOC_DAILINK_REG(dspoffload),
	},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) && !defined(SKIP_SB_DSP)
	{
		.name = "DSP_Playback_Voip",
		.stream_name = "DSP_Playback_Voip",
		SND_SOC_DAILINK_REG(dspvoip),
	},
	{
		.name = "DSP_Playback_Primary",
		.stream_name = "DSP_Playback_Primary",
		SND_SOC_DAILINK_REG(dspprimary),
	},
	{
		.name = "DSP_Playback_DeepBuf",
		.stream_name = "DSP_Playback_DeepBuf",
		SND_SOC_DAILINK_REG(dspdeepbuf),
	},
	{
		.name = "DSP_Playback_Fast",
		.stream_name = "DSP_Playback_Fast",
		SND_SOC_DAILINK_REG(dspfast),
	},
	{
		.name = "DSP_Playback_Spatializer",
		.stream_name = "DSP_Playback_Spatializer",
		SND_SOC_DAILINK_REG(dspspatializer),
	},
	{
		.name = "DSP_Playback_Playback",
		.stream_name = "DSP_Playback_Playback",
		SND_SOC_DAILINK_REG(dspplayback),
	},
	{
		.name = "DSP_Capture_Ul1",
		.stream_name = "DSP_Capture_Ul1",
		SND_SOC_DAILINK_REG(dspcapture1),
	},
	{
		.name = "DSP_Call_Final",
		.stream_name = "DSP_Call_Final",
		SND_SOC_DAILINK_REG(dspcallfinal),
	},
	{
		.name = "DSP_Playback_Ktv",
		.stream_name = "DSP_Playback_Ktv",
		SND_SOC_DAILINK_REG(dspktv),
	},
	{
		.name = "DSP_Capture_Raw",
		.stream_name = "DSP_Capture_Raw",
		SND_SOC_DAILINK_REG(dspcaptureraw),
	},
	{
		.name = "DSP_Playback_Fm_Adsp",
		.stream_name = "DSP_Playback_Fm_Adsp",
		SND_SOC_DAILINK_REG(dspfmadsp),
	},
	{
		.name = "DSP_Playback_A2DP",
		.stream_name = "DSP_Playback_A2DP",
		SND_SOC_DAILINK_REG(dspa2dp),
	},
	{
		.name = "DSP_Playback_BLEDL",
		.stream_name = "DSP_Playback_BLEDL",
		SND_SOC_DAILINK_REG(dspbledl),
	},
	{
		.name = "DSP_Capture_BLE",
		.stream_name = "DSP_Capture_BLE",
		SND_SOC_DAILINK_REG(dspbleul),
	},
	{
		.name = "DSP_Playback_BT",
		.stream_name = "DSP_Playback_BT",
		SND_SOC_DAILINK_REG(dspbtdl),
	},
	{
		.name = "DSP_Capture_BT",
		.stream_name = "DSP_Capture_BT",
		SND_SOC_DAILINK_REG(dspbtul),
	},
	{
		.name = "DSP_Capture_Process",
		.stream_name = "DSP_Capture_Process",
		SND_SOC_DAILINK_REG(dspulproc),
	},
	{
		.name = "DSP_Playback_Echoref",
		.stream_name = "DSP_Playback_Echoref",
		SND_SOC_DAILINK_REG(dspechodl),
	},
	{
		.name = "DSP_Playback_USB",
		.stream_name = "DSP_Playback_USB",
		SND_SOC_DAILINK_REG(dspusbdl),
	},
	{
		.name = "DSP_Capture_USB",
		.stream_name = "DSP_Capture_USB",
		SND_SOC_DAILINK_REG(dspusbul),
	},
	{
		.name = "DSP_Capture_MDDL",
		.stream_name = "DSP_Capture_MDDL",
		SND_SOC_DAILINK_REG(dspmddl),
	},
	{
		.name = "DSP_Playback_MDUL",
		.stream_name = "DSP_Playback_MDUL",
		SND_SOC_DAILINK_REG(dspmdul),
	},
#endif
};

static struct snd_soc_card mt6878_mt6369_soc_card = {
	.name = "mt6878-mt6369",
	.owner = THIS_MODULE,
	.dai_link = mt6878_mt6369_dai_links,
	.num_links = ARRAY_SIZE(mt6878_mt6369_dai_links),

	.controls = mt6878_mt6369_controls,
	.num_controls = ARRAY_SIZE(mt6878_mt6369_controls),
	.dapm_widgets = mt6878_mt6369_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6878_mt6369_widgets),
	.dapm_routes = mt6878_mt6369_routes,
	.num_dapm_routes = ARRAY_SIZE(mt6878_mt6369_routes),
};

static int mt6878_mt6369_bypass_primary_codec(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt6878_mt6369_soc_card;
	int i;
	struct snd_soc_dai_link *dai_link;

	dev_info(&pdev->dev, "%s() successfully start\n", __func__);
	for_each_card_prelinks(card, i, dai_link) {
		if (strcmp(dai_link->name, "Primary Codec") == 0) {
			dai_link->codecs->name = "snd-soc-dummy";
			dai_link->codecs->dai_name = "snd-soc-dummy-dai";
			dai_link->init = NULL;
			dev_info(&pdev->dev, "%s() Primary Codec modified\n", __func__);
		} else if (strcmp(dai_link->name, "Primary Codec CH34") == 0) {
			dai_link->codecs->name = "snd-soc-dummy";
			dai_link->codecs->dai_name = "snd-soc-dummy-dai";
			dev_info(&pdev->dev, "%s() Primary Codec CH34 modified\n", __func__);
		} else if (strcmp(dai_link->name, "VOW_Capture") == 0) {
			dai_link->codecs->name = "snd-soc-dummy";
			dai_link->codecs->dai_name = "snd-soc-dummy-dai";
			dev_info(&pdev->dev, "%s() VOW_Capture modified\n", __func__);
		}
	}
	card->dapm_routes = mt6878_mt6369_routes_dummy;
	card->num_dapm_routes = ARRAY_SIZE(mt6878_mt6369_routes_dummy);
	return 0;
}

static int mt6878_mt6369_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt6878_mt6369_soc_card;
	struct device_node *platform_node, *spk_node;
	int ret, i;
	struct snd_soc_dai_link *dai_link;

	dev_info(&pdev->dev, "%s() successfully start\n", __func__);

	/* update speaker type */
	ret = mtk_spk_update_info(card, pdev);
	if (ret) {
		dev_info(&pdev->dev, "%s(), mtk_spk_update_info error\n",
			__func__);
		return -EINVAL;
	}

	/* get platform node */
	platform_node = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,platform", 0);
	if (!platform_node) {
		dev_info(&pdev->dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

	/* get speaker codec node */
	spk_node = of_get_child_by_name(pdev->dev.of_node,
					"mediatek,speaker-codec");
	if (!spk_node) {
		dev_info(&pdev->dev,
			"spk_node of_get_child_by_name fail\n");
		//return -EINVAL;
	}

	for_each_card_prelinks(card, i, dai_link) {
		if (!dai_link->platforms->name)
			dai_link->platforms->of_node = platform_node;

		if (!strcmp(dai_link->name, "Speaker Codec")) {
			ret = snd_soc_of_get_dai_link_codecs(
						&pdev->dev, spk_node, dai_link);
			if (ret < 0) {
				dev_info(&pdev->dev,
					"Speaker Codec get_dai_link fail: %d\n", ret);
				return -EINVAL;
			}
		} else if (!strcmp(dai_link->name, "Speaker Codec Ref")) {
			ret = snd_soc_of_get_dai_link_codecs(
						&pdev->dev, spk_node, dai_link);
			if (ret < 0) {
				dev_info(&pdev->dev,
					"Speaker Codec Ref get_dai_link fail: %d\n", ret);
				return -EINVAL;
			}
		}
	}

	/* codec probe fail, bypass codec driver */
	if (!MT6369_PROBE_DONE)
		mt6878_mt6369_bypass_primary_codec(pdev);

	card->dev = &pdev->dev;

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret)
		dev_info(&pdev->dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
	else
		dev_info(&pdev->dev, "%s snd_soc_register_card pss %d\n",
				__func__, ret);
	return ret;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mt6878_mt6369_dt_match[] = {
	{.compatible = "mediatek,mt6878-mt6369-sound",},
	{}
};
#endif

static const struct dev_pm_ops mt6878_mt6369_pm_ops = {
	.poweroff = snd_soc_poweroff,
	.restore = snd_soc_resume,
};

static struct platform_driver mt6878_mt6369_driver = {
	.driver = {
		.name = "mt6878-mt6369",
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = mt6878_mt6369_dt_match,
#endif
		.pm = &mt6878_mt6369_pm_ops,
	},
	.probe = mt6878_mt6369_dev_probe,
};

module_platform_driver(mt6878_mt6369_driver);

/* Module information */
MODULE_DESCRIPTION("MT6878 mt6369 ALSA SoC machine driver");
MODULE_AUTHOR("Shane Chien <shane.chien@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("mt6878 mt6369 soc card");

