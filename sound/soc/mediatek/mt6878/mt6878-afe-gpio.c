// SPDX-License-Identifier: GPL-2.0
/*
 *  mt6878-afe-gpio.c  --  Mediatek 6878 afe gpio ctrl
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Shu-wei Hsu <Shu-wei Hsu@mediatek.com>
 */

#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>

#include "mt6878-afe-common.h"
#include "mt6878-afe-gpio.h"

struct pinctrl *aud_pinctrl;
struct audio_gpio_attr {
	const char *name;
	bool gpio_prepare;
	struct pinctrl_state *gpioctrl;
};

static struct audio_gpio_attr aud_gpios[MT6878_AFE_GPIO_GPIO_NUM] = {
	[MT6878_AFE_GPIO_DAT_MISO0_OFF] = {"aud-dat-miso0-off", false, NULL},
	[MT6878_AFE_GPIO_DAT_MISO0_ON] = {"aud-dat-miso0-on", false, NULL},
	[MT6878_AFE_GPIO_DAT_MISO1_OFF] = {"aud-dat-miso1-off", false, NULL},
	[MT6878_AFE_GPIO_DAT_MISO1_ON] = {"aud-dat-miso1-on", false, NULL},
	[MT6878_AFE_GPIO_DAT_MOSI_OFF] = {"aud-dat-mosi-off", false, NULL},
	[MT6878_AFE_GPIO_DAT_MOSI_ON] = {"aud-dat-mosi-on", false, NULL},
	[MT6878_AFE_GPIO_I2SOUT4_OFF] = {"aud-gpio-i2sout4-off", false, NULL},
	[MT6878_AFE_GPIO_I2SOUT4_ON] = {"aud-gpio-i2sout4-on", false, NULL},
	[MT6878_AFE_GPIO_I2SIN4_OFF] = {"aud-gpio-i2sin4-off", false, NULL},
	[MT6878_AFE_GPIO_I2SIN4_ON] = {"aud-gpio-i2sin4-on", false, NULL},
	[MT6878_AFE_GPIO_VOW_DAT_OFF] = {"vow-dat-miso-off", false, NULL},
	[MT6878_AFE_GPIO_VOW_DAT_ON] = {"vow-dat-miso-on", false, NULL},
	[MT6878_AFE_GPIO_VOW_CLK_OFF] = {"vow-clk-miso-off", false, NULL},
	[MT6878_AFE_GPIO_VOW_CLK_ON] = {"vow-clk-miso-on", false, NULL},
	[MT6878_AFE_GPIO_AP_DMIC_OFF] = {"aud-gpio-ap-dmic-off", false, NULL},
	[MT6878_AFE_GPIO_AP_DMIC_ON] = {"aud-gpio-ap-dmic-on", false, NULL},
	[MT6878_AFE_GPIO_AP_DMIC1_OFF] = {"aud-gpio-ap-dmic1-off", false, NULL},
	[MT6878_AFE_GPIO_AP_DMIC1_ON] = {"aud-gpio-ap-dmic1-on", false, NULL},
	[MT6878_AFE_GPIO_VOW_SCP_DMIC_DAT_OFF] = {"vow-scp-dmic-dat-off", false, NULL},
	[MT6878_AFE_GPIO_VOW_SCP_DMIC_DAT_ON] = {"vow-scp-dmic-dat-on", false, NULL},
	[MT6878_AFE_GPIO_VOW_SCP_DMIC_CLK_OFF] = {"vow-scp-dmic-clk-off", false, NULL},
	[MT6878_AFE_GPIO_VOW_SCP_DMIC_CLK_ON] = {"vow-scp-dmic-clk-on", false, NULL},
};

static DEFINE_MUTEX(gpio_request_mutex);

int mt6878_afe_gpio_init(struct mtk_base_afe *afe)
{
	int ret;
	int i = 0;

	aud_pinctrl = devm_pinctrl_get(afe->dev);
	if (IS_ERR(aud_pinctrl)) {
		ret = PTR_ERR(aud_pinctrl);
		dev_info(afe->dev, "%s(), ret %d, cannot get aud_pinctrl!\n",
			__func__, ret);
		return -ENODEV;
	}

	for (i = 0; i < MT6878_AFE_GPIO_GPIO_NUM; i++) {
		if (aud_gpios[i].name == NULL) {
			dev_info(afe->dev, "%s(), gpio id %d not define!!!\n",
			 __func__, i);
			//AUDIO_AEE("gpio not define");
			//return 0;
		}
	}

	for (i = 0; i < ARRAY_SIZE(aud_gpios); i++) {
		aud_gpios[i].gpioctrl = pinctrl_lookup_state(aud_pinctrl,
					aud_gpios[i].name);
		if (IS_ERR(aud_gpios[i].gpioctrl)) {
			ret = PTR_ERR(aud_gpios[i].gpioctrl);
			dev_info(afe->dev, "%s(), pinctrl_lookup_state %s fail, ret %d\n",
				__func__, aud_gpios[i].name, ret);
		} else
			aud_gpios[i].gpio_prepare = true;
	}

	/* gpio status init */
	mt6878_afe_gpio_request(afe, false, MT6878_DAI_ADDA, 0);
	mt6878_afe_gpio_request(afe, false, MT6878_DAI_ADDA, 1);

	return 0;
}

static int mt6878_afe_gpio_select(struct mtk_base_afe *afe,
				  enum mt6878_afe_gpio type)
{
	int ret = 0;

	if (type >= MT6878_AFE_GPIO_GPIO_NUM) {
		dev_info(afe->dev, "%s(), error, invalid gpio type %d\n",
			__func__, type);
		return -EINVAL;
	}

	if (!aud_gpios[type].gpio_prepare) {
		dev_info(afe->dev, "%s(), error, gpio type %d not prepared\n",
			 __func__, type);
		return -EIO;
	}

	ret = pinctrl_select_state(aud_pinctrl,
				   aud_gpios[type].gpioctrl);
	if (ret)
		dev_info(afe->dev, "%s(), error, can not set gpio type %d\n",
			__func__, type);

	return ret;
}

static int mt6878_afe_gpio_adda_dl(struct mtk_base_afe *afe, bool enable)
{
	if (enable)
		return mt6878_afe_gpio_select(afe,
					      MT6878_AFE_GPIO_DAT_MOSI_ON);
	else
		return mt6878_afe_gpio_select(afe,
					      MT6878_AFE_GPIO_DAT_MOSI_OFF);
}

static int mt6878_afe_gpio_adda_ul(struct mtk_base_afe *afe, bool enable)
{
	int ret = 0;

	ret = mt6878_afe_gpio_select(afe, enable ?
				     MT6878_AFE_GPIO_DAT_MISO0_ON :
				     MT6878_AFE_GPIO_DAT_MISO0_OFF);
	/* if error happened, skip miso1 select */
	if (ret) {
		dev_info(afe->dev, "%s(), error, can not set enable %d miso gpio\n",
			 __func__, enable);
		return ret;
	}

	ret = mt6878_afe_gpio_select(afe, enable ?
				     MT6878_AFE_GPIO_DAT_MISO1_ON :
				     MT6878_AFE_GPIO_DAT_MISO1_OFF);
	if (ret)
		dev_info(afe->dev, "%s(), error, can not set enable %d miso1 gpio\n",
			 __func__, enable);

	return ret;
}


static int mt6878_afe_gpio_adda_ch56_ul(struct mtk_base_afe *afe, bool enable)
{
	if (enable)
		return mt6878_afe_gpio_select(afe,
					      MT6878_AFE_GPIO_DAT_MISO1_ON);
	else
		return mt6878_afe_gpio_select(afe,
					      MT6878_AFE_GPIO_DAT_MISO1_OFF);
}

int mt6878_afe_gpio_request(struct mtk_base_afe *afe, bool enable,
			    int dai, int uplink)
{
	mutex_lock(&gpio_request_mutex);
	switch (dai) {
	case MT6878_DAI_ADDA:
		if (uplink)
			mt6878_afe_gpio_adda_ul(afe, enable);
		else
			mt6878_afe_gpio_adda_dl(afe, enable);
		break;
	case MT6878_DAI_ADDA_CH56:
		if (uplink)
			mt6878_afe_gpio_adda_ch56_ul(afe, enable);
		break;
	case MT6878_DAI_I2S_IN0:
		break;
	case MT6878_DAI_I2S_OUT0:
		break;
	case MT6878_DAI_I2S_IN4:
	case MT6878_DAI_I2S_OUT4:
		if (enable) {
			mt6878_afe_gpio_select(afe, MT6878_AFE_GPIO_I2SIN4_ON);
			mt6878_afe_gpio_select(afe, MT6878_AFE_GPIO_I2SOUT4_ON);
		} else {
			mt6878_afe_gpio_select(afe, MT6878_AFE_GPIO_I2SIN4_OFF);
			mt6878_afe_gpio_select(afe, MT6878_AFE_GPIO_I2SOUT4_OFF);
		}
		break;
	case MT6878_DAI_VOW:
		if (enable) {
			mt6878_afe_gpio_select(afe,
					       MT6878_AFE_GPIO_VOW_CLK_ON);
			mt6878_afe_gpio_select(afe,
					       MT6878_AFE_GPIO_VOW_DAT_ON);
		} else {
			mt6878_afe_gpio_select(afe,
					       MT6878_AFE_GPIO_VOW_CLK_OFF);
			mt6878_afe_gpio_select(afe,
					       MT6878_AFE_GPIO_VOW_DAT_OFF);
		}
		break;
	case MT6878_DAI_VOW_SCP_DMIC:
		if (enable) {
			mt6878_afe_gpio_select(afe,
					       MT6878_AFE_GPIO_VOW_SCP_DMIC_CLK_ON);
			mt6878_afe_gpio_select(afe,
					       MT6878_AFE_GPIO_VOW_SCP_DMIC_DAT_ON);
		} else {
			mt6878_afe_gpio_select(afe,
					       MT6878_AFE_GPIO_VOW_SCP_DMIC_CLK_OFF);
			mt6878_afe_gpio_select(afe,
					       MT6878_AFE_GPIO_VOW_SCP_DMIC_DAT_OFF);
		}
		break;
	case MT6878_DAI_AP_DMIC:
		if (enable)
			mt6878_afe_gpio_select(afe, MT6878_AFE_GPIO_AP_DMIC_ON);
		else
			mt6878_afe_gpio_select(afe, MT6878_AFE_GPIO_AP_DMIC_OFF);
		break;
	case MT6878_DAI_AP_DMIC_CH34:
		if (enable)
			mt6878_afe_gpio_select(afe, MT6878_AFE_GPIO_AP_DMIC1_ON);
		else
			mt6878_afe_gpio_select(afe, MT6878_AFE_GPIO_AP_DMIC1_OFF);
		break;
	default:
		mutex_unlock(&gpio_request_mutex);
		dev_info(afe->dev, "%s(), invalid dai %d\n", __func__, dai);
		return -EINVAL;
	}
	mutex_unlock(&gpio_request_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(mt6878_afe_gpio_request);

bool mt6878_afe_gpio_is_prepared(enum mt6878_afe_gpio type)
{
	return aud_gpios[type].gpio_prepare;
}
EXPORT_SYMBOL(mt6878_afe_gpio_is_prepared);

