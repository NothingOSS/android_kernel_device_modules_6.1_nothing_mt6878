// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/gpio/consumer.h>

#include "rt4831a.h"

/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
struct rt4831a {
	struct device *dev;
	struct i2c_client *rt4831a_i2c;
	bool pwm_en;
	bool bias_en;
};

static struct rt4831a *rt4831a_left;
static struct rt4831a *rt4831a_right;
static DEFINE_MUTEX(read_lock);
/*****************************************************************************
 * Function Prototype
 *****************************************************************************/

/*****************************************************************************
 * Extern Area
 *****************************************************************************/

static int lcd_bl_write_byte(struct i2c_client *i2c, unsigned char addr, unsigned char value)
{
	int ret = 0;
	unsigned char write_data[2] = {0};

	write_data[0] = addr;
	write_data[1] = value;

	ret = i2c_master_send(i2c, write_data, 2);
	if (ret < 0)
		pr_info("%s i2c write data fail !!\n", __func__);

	return ret;
}

int lcd_bl_set_led_brightness(int value)
{
	pr_info("%s:hyper bl = %d\n", __func__, value);

	if (value < 0) {
		pr_info("%s: invalid value=%d\n", __func__, value);
		return 0;
	}

	if (rt4831a_left && rt4831a_left->pwm_en) {
		if (value > 0) {
			pr_info("%s:left bl = %d\n", __func__, value);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x02, 0xDA);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x11, 0x37);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x15, 0xA0);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x08, 0x1F);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x04, 0x07);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x05, 0xFF);
		} else {
			pr_info("%s:left bl = %d\n", __func__, value);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x04, 0x00);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x05, 0x00);
		}
	}

	if (rt4831a_right && rt4831a_right->pwm_en) {
		if (value > 0) {
			pr_info("%s:right bl = %d\n", __func__, value);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x02, 0xDA);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x11, 0x37);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x15, 0xA0);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x08, 0x1F);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x04, 0x07);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x05, 0xFF);
		} else {
			pr_info("%s:right bl = %d\n", __func__, value);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x04, 0x00);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x05, 0x00);
		}
	}

	return 0;
}
EXPORT_SYMBOL(lcd_bl_set_led_brightness);

int lcd_set_bias(int enable)
{
	pr_info("%s+++, value = %d", __func__, enable);

	if (rt4831a_left && rt4831a_left->bias_en) {
		if (enable) {
			pr_info("%s:left en = %d\n", __func__, enable);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x0C, 0x2A);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x0D, 0x24);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x0E, 0x24);

			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x09, 0x9C);
			mdelay(5);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x09, 0x9E);
		} else {
			pr_info("%s:left en = %d\n", __func__, enable);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x09, 0x9C);
			mdelay(5);
			lcd_bl_write_byte(rt4831a_left->rt4831a_i2c, 0x09, 0x98);
		}
	}

	if (rt4831a_right && rt4831a_right->bias_en) {
		if (enable) {
			pr_info("%s:right en = %d\n", __func__, enable);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x0C, 0x2A);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x0D, 0x24);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x0E, 0x24);

			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x09, 0x9C);
			mdelay(5); /* delay 5ms */
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x09, 0x9E);
		} else {
			pr_info("%s:right en = %d\n", __func__, enable);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x09, 0x9C);
			mdelay(5);
			lcd_bl_write_byte(rt4831a_right->rt4831a_i2c, 0x09, 0x98);
		}
	}
	pr_info("%s---", __func__);

	return 0;
}
EXPORT_SYMBOL(lcd_set_bias);

static int rt4831a_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct device *dev = &client->dev;
	struct rt4831a *rt4831a_client;

	if (!client) {
		pr_info("%s i2c_client is NULL\n", __func__);
		return -EINVAL;
	}

	pr_info("%s, i2c address: %0x\n", __func__, client->addr);

	rt4831a_client = devm_kzalloc(dev, sizeof(*rt4831a_client), GFP_KERNEL);
	if (!rt4831a_client)
		return -ENOMEM;
	rt4831a_client->rt4831a_i2c = client;

	rt4831a_client->bias_en = of_property_read_bool(dev->of_node,
						  "rt4831a,bias-en");
	rt4831a_client->pwm_en = of_property_read_bool(dev->of_node,
						  "rt4831a,pwm-en");

	pr_info("%s, bias_en=%d, pwm_en=%d\n", __func__, rt4831a_client->bias_en, rt4831a_client->pwm_en);

	if (!rt4831a_left) {
		pr_info("probe for left\n");
		rt4831a_left = rt4831a_client;
	} else if (!rt4831a_right) {
		pr_info("probe for right\n");
		rt4831a_right = rt4831a_client;
	}

#ifdef CONFIG_MTK_DISP_NO_LK
	//8866 is initial in lk when have lk, we connot touch it in probe
	if (rt4831a_client->bias_en) {
		//write vsp/vsn reg
		pr_info("%s:bias en\n", __func__);
		ret = lcd_bl_write_byte(rt4831a_client->rt4831a_i2c, 0x0C, 0x2A);
		ret = lcd_bl_write_byte(rt4831a_client->rt4831a_i2c, 0x0D, 0x24);
		ret = lcd_bl_write_byte(rt4831a_client->rt4831a_i2c, 0x0E, 0x24);

		ret = lcd_bl_write_byte(rt4831a_client->rt4831a_i2c, 0x09, 0x9C);
		mdelay(5); /* delay 5ms */
		ret = lcd_bl_write_byte(rt4831a_client->rt4831a_i2c, 0x09, 0x9E);
	}
	//write backlight reg
	if (rt4831a_client->pwm_en) {
		pr_info("%s:pwm en\n", __func__);
		ret = lcd_bl_write_byte(rt4831a_client->rt4831a_i2c, 0x02, 0xDA);
		ret = lcd_bl_write_byte(rt4831a_client->rt4831a_i2c, 0x11, 0x37);
		ret = lcd_bl_write_byte(rt4831a_client->rt4831a_i2c, 0x15, 0xA0);
		ret = lcd_bl_write_byte(rt4831a_client->rt4831a_i2c, 0x08, 0x4F);
	}
#endif

	if (ret < 0)
		pr_info("[%s]:I2C write reg is fail!", __func__);
	else
		pr_info("[%s]:I2C write reg is success!", __func__);

	return ret;
}

static void rt4831a_remove(struct i2c_client *client)
{
	rt4831a_left = NULL;
	rt4831a_right = NULL;
	i2c_unregister_device(client);
}

static const struct i2c_device_id rt4831a_i2c_table[] = {
	{"rt4831a", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, rt4831a_i2c_table);

static const struct of_device_id rt4831a_match[] = {
	{ .compatible = "rt,rt4831a" },
	{},
};
MODULE_DEVICE_TABLE(of, rt4831a_match);

static struct i2c_driver rt4831_driver = {
	.id_table	= rt4831a_i2c_table,
	.probe		= rt4831a_probe,
	.remove		= rt4831a_remove,
	.driver		= {
		.name	= "rt,rt4831a",
		.of_match_table = rt4831a_match,
	},
};
module_i2c_driver(rt4831_driver);

MODULE_AUTHOR("huijuan xie <huijuan.xie@mediatek.com>");
MODULE_DESCRIPTION("Mediatek rt4831a Driver");
MODULE_LICENSE("GPL");

