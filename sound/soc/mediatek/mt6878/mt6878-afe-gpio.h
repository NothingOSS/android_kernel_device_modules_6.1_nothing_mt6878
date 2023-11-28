/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6878-afe-gpio.h  --  Mediatek 6878 afe gpio ctrl definition
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Shu-wei Hsu <Shu-wei Hsu@mediatek.com>
 */

#ifndef _MT6878_AFE_GPIO_H_
#define _MT6878_AFE_GPIO_H_

enum mt6878_afe_gpio {
	MT6878_AFE_GPIO_DAT_MISO0_OFF,
	MT6878_AFE_GPIO_DAT_MISO0_ON,
	MT6878_AFE_GPIO_DAT_MISO1_OFF,
	MT6878_AFE_GPIO_DAT_MISO1_ON,
	MT6878_AFE_GPIO_DAT_MOSI_OFF,
	MT6878_AFE_GPIO_DAT_MOSI_ON,
	MT6878_AFE_GPIO_I2SIN4_OFF,
	MT6878_AFE_GPIO_I2SIN4_ON,
	MT6878_AFE_GPIO_I2SOUT4_OFF,
	MT6878_AFE_GPIO_I2SOUT4_ON,
	MT6878_AFE_GPIO_VOW_DAT_OFF,
	MT6878_AFE_GPIO_VOW_DAT_ON,
	MT6878_AFE_GPIO_VOW_CLK_OFF,
	MT6878_AFE_GPIO_VOW_CLK_ON,
	MT6878_AFE_GPIO_AP_DMIC_OFF,
	MT6878_AFE_GPIO_AP_DMIC_ON,
	MT6878_AFE_GPIO_AP_DMIC1_OFF,
	MT6878_AFE_GPIO_AP_DMIC1_ON,
	MT6878_AFE_GPIO_VOW_SCP_DMIC_DAT_OFF,
	MT6878_AFE_GPIO_VOW_SCP_DMIC_DAT_ON,
	MT6878_AFE_GPIO_VOW_SCP_DMIC_CLK_OFF,
	MT6878_AFE_GPIO_VOW_SCP_DMIC_CLK_ON,
	MT6878_AFE_GPIO_GPIO_NUM
};

struct mtk_base_afe;

int mt6878_afe_gpio_init(struct mtk_base_afe *afe);
int mt6878_afe_gpio_request(struct mtk_base_afe *afe, bool enable,
			    int dai, int uplink);
bool mt6878_afe_gpio_is_prepared(enum mt6878_afe_gpio type);

#endif
