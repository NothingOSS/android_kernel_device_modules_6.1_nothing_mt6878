// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include "hl5280.h"
#include "../typec/tcpc/inc/tcpm.h"

#define HL5280_I2C_NAME	"hl5280-driver"

#define HL5280_DEVICE_INFO_VAL           0x49
#define BCT4480_DEVICE_REG_VALUE         0x9

/* Registers Map */
#define HL5280_DEVICE_ID                 0x00
#define HL5280_SWITCH_SETTINGS           0x04
#define HL5280_SWITCH_CONTROL            0x05
#define HL5280_SWITCH_STATUS0            0x06
#define HL5280_SWITCH_STATUS1            0x07
#define HL5280_SLOW_L                    0x08
#define HL5280_SLOW_R                    0x09
#define HL5280_SLOW_MIC                  0x0A
#define HL5280_SLOW_SENSE                0x0B
#define HL5280_SLOW_GND                  0x0C
#define HL5280_DELAY_L_R                 0x0D
#define HL5280_DELAY_L_MIC               0x0E
#define HL5280_DELAY_L_SENSE             0x0F
#define HL5280_DELAY_L_AGND              0x10
#define HL5280_FUN_EN                    0x12
#define HL5280_JACK_STATUS               0x17
#define HL5280_DET_INT_FLAG              0x18
#define HL5280_RESET                     0x1E
#define HL5280_CURRENT_SOURCE_SETTING    0x1F

#define HL5280_SRC_100 0x01
#define HL5280_SRC_400 0x04
#define HL5280_SRC_700 0x07

#define HL5280_HEADSET_P 0x0A
#define HL5280_HEADSET_N 0x11

/* external accdet callback wrapper */
#define EINT_PIN_PLUG_OUT       (0)
#define EINT_PIN_PLUG_IN        (1)
void accdet_eint_callback_wrapper(unsigned int plug_status);

// #undef dev_dbg
// #define dev_dbg dev_info

enum switch_vendor {
	HL5280 = 0,
	BCT4480
};

struct hl5280_priv {
	struct regmap *regmap;
	struct device *dev;
	struct tcpc_device *tcpc_dev;
	struct notifier_block pd_nb;
	atomic_t usbc_mode;
	struct work_struct usbc_analog_work;
	struct blocking_notifier_head hl5280_notifier;
	struct mutex notification_lock;
	unsigned int hs_det_pin;
	enum switch_vendor vendor;
	bool plug_state;
};

struct hl5280_reg_val {
	uint8_t reg;
	uint8_t val;
};

struct hl5280_priv *g_hl_priv = NULL;

static const struct regmap_config hl5280_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = HL5280_CURRENT_SOURCE_SETTING,
};

static const struct hl5280_reg_val hl_reg_i2c_defaults[] = {
	{HL5280_SLOW_L, 0x00},
	{HL5280_SLOW_R, 0x00},
	{HL5280_SLOW_MIC, 0x00},
	{HL5280_SLOW_SENSE, 0x00},
	{HL5280_SLOW_GND, 0x00},
	{HL5280_DELAY_L_R, 0x00},
	{HL5280_DELAY_L_MIC, 0x00},
	{HL5280_DELAY_L_SENSE, 0x00},
	{HL5280_DELAY_L_AGND, 0x09},
	{HL5280_SWITCH_SETTINGS, 0x98},
	{HL5280_SWITCH_CONTROL, 0x18},
};

static void hl5280_usbc_update_settings(struct hl5280_priv *hl_priv,
		u32 switch_control, u32 switch_enable)
{
	if (!hl_priv->regmap) {
		dev_err(hl_priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_write(hl_priv->regmap, HL5280_SWITCH_SETTINGS, 0x80);
	regmap_write(hl_priv->regmap, HL5280_SWITCH_CONTROL, switch_control);
	/* HL5280 chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(hl_priv->regmap, HL5280_SWITCH_SETTINGS, switch_enable);
}

static void hl5280_autoset_switch(struct hl5280_priv *hl_priv)
{
	u32 rc, reg;
	u8 i;

	if (!hl_priv->regmap) {
	        dev_err(hl_priv->dev, "%s: regmap invalid\n", __func__);
	        return;
	}
	/*start auto dectection*/
	regmap_write(hl_priv->regmap, HL5280_FUN_EN, 0x09);
	/*checking auto detection finish*/
	for (i = 0; i < 10; i++) {
		usleep_range(1000, 1050);
		regmap_read(hl_priv->regmap, HL5280_DET_INT_FLAG, &rc);
		regmap_read(hl_priv->regmap, HL5280_FUN_EN, &reg);
		if (((reg & 0x01) == 0) || (rc & 0x04)) {
				dev_err(hl_priv->dev, "%s: auto_det success\n", __func__);
				break;
		}
		if (i == 9) {
			regmap_read(hl_priv->regmap, HL5280_CURRENT_SOURCE_SETTING, &rc);
			if (rc == HL5280_SRC_100) {
				dev_err(hl_priv->dev, "%s: auto_det fail\n", __func__);
				return;
			}
			regmap_write(hl_priv->regmap, HL5280_CURRENT_SOURCE_SETTING, HL5280_SRC_100);
			hl5280_usbc_update_settings(hl_priv, 0x00, 0x9F);
			regmap_write(hl_priv->regmap, HL5280_FUN_EN, 0x09);
			i = 0;
		}
	}
	//for codec 3/4 pole earphone setting
	regmap_read(hl_priv->regmap, HL5280_SWITCH_STATUS1, &rc);
	dev_err(hl_priv->dev, "%s:auto_det reg_0x07 = 0x%#x\n",__func__, rc);
	//manual set switch
	if ((rc != HL5280_HEADSET_P) && (rc != HL5280_HEADSET_N)) {
		regmap_write(hl_priv->regmap, HL5280_SWITCH_SETTINGS, 0x9F);
		rc = (rc & HL5280_HEADSET_P) ? 0x00 : 0x07;
		regmap_write(hl_priv->regmap, HL5280_SWITCH_CONTROL, rc);
		regmap_read(hl_priv->regmap, HL5280_SWITCH_STATUS1, &rc);
		dev_err(hl_priv->dev, "%s: manual_set reg_0x07 = 0x%#x\n",__func__, rc);
	}
}

static int hl5280_usbc_event_changed(struct notifier_block *nb,
					  unsigned long evt, void *ptr)
{
	struct hl5280_priv *hl_priv =
			container_of(nb, struct hl5280_priv, pd_nb);
	struct device *dev;
	struct tcp_notify *noti = ptr;

	if (!hl_priv)
		return -EINVAL;

	dev = hl_priv->dev;
	if (!dev)
		return -EINVAL;

	if (hl_priv->vendor == HL5280) {
		dev_info(dev, "%s: switch chip is HL5280\n", __func__);
	} else {
		dev_err(dev, "%s: switch chip is BCT4480\n", __func__);
    }

	dev_info(dev, "%s: typeC event: %lu\n", __func__, evt);

	switch (evt) {
	case TCP_NOTIFY_TYPEC_STATE:
		dev_info(dev, "%s: old_state: %d, new_state: %d\n",
			__func__, noti->typec_state.old_state, noti->typec_state.new_state);
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO) {
			/* AUDIO plug in */
			dev_info(dev, "%s: audio plug in\n", __func__);
			hl_priv->plug_state = true;
			dev_dbg(dev, "%s: tcpc polarity = %d\n", __func__, noti->typec_state.polarity);
			pm_stay_awake(hl_priv->dev);
			schedule_work(&hl_priv->usbc_analog_work);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			/* AUDIO plug out */
			dev_err(dev, "%s: audio plug out\n", __func__);
			hl_priv->plug_state = false;
			pm_stay_awake(hl_priv->dev);
			schedule_work(&hl_priv->usbc_analog_work);
		}
		else {
			dev_dbg(dev, "%s: ignore tcpc non-audio notification\n", __func__);
		}
		break;
	default:
		break;
	};

	return NOTIFY_OK;
}

static int hl5280_usbc_analog_setup_switches(struct hl5280_priv *hl_priv)
{
	struct device *dev;
	unsigned int switch_status  = 0;
	unsigned int jack_status    = 0;
	int i = 0, reg_val = 0;

	if (!hl_priv)
		return -EINVAL;
	dev = hl_priv->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&hl_priv->notification_lock);

	dev_info(dev, "%s: plug_state %d\n", __func__, hl_priv->plug_state);
	if (hl_priv->plug_state) {
		/* activate switches */
		hl5280_usbc_update_settings(hl_priv, 0x00, 0x9F);
		hl5280_autoset_switch(hl_priv);
		dev_info(dev, "%s: set reg[0x%x] done.\n", __func__, HL5280_FUN_EN);

		accdet_eint_callback_wrapper(EINT_PIN_PLUG_IN);

		regmap_read(hl_priv->regmap, HL5280_JACK_STATUS, &jack_status);
		dev_info(dev, "%s: jack status: 0x%x.\n", __func__, jack_status);
		regmap_read(hl_priv->regmap, HL5280_SWITCH_STATUS0, &switch_status);
		dev_info(dev, "%s: switch status0: 0x%x.\n", __func__, switch_status);
		regmap_read(hl_priv->regmap, HL5280_SWITCH_STATUS1, &switch_status);
		dev_info(dev, "%s: switch status1: 0x%x.\n", __func__, switch_status);
	} else {
		hl5280_usbc_update_settings(hl_priv, 0x18, 0x98);
		accdet_eint_callback_wrapper(EINT_PIN_PLUG_OUT);
	}
	/*test*/
	for (i = 0; i <= 0x1f; i++) {
		regmap_read(hl_priv->regmap, i, &reg_val);
		dev_info(dev, "%s: read reg: %x value: 0x%x\n", __func__, i, reg_val);
	}
	mutex_unlock(&hl_priv->notification_lock);
	return 0;
}

/*
 * hl5280_reg_notifier - register notifier block with fsa driver
 *
 * @nb - notifier block of hl5280
 * @node - phandle node to hl5280 device
 *
 * Returns 0 on success, or error code
 */
int hl5280_reg_notifier(struct notifier_block *nb,
			 struct device_node *node)
{
	int rc = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct hl5280_priv *hl_priv;

	if (!client)
		return -EINVAL;

	hl_priv = (struct hl5280_priv *)i2c_get_clientdata(client);
	if (!hl_priv)
		return -EINVAL;

	rc = blocking_notifier_chain_register
				(&hl_priv->hl5280_notifier, nb);
	if (rc)
		return rc;

	/*
	 * as part of the init sequence check if there is a connected
	 * USB C analog adapter
	 */
	dev_dbg(hl_priv->dev, "%s: verify if USB adapter is already inserted\n",
		__func__);
	rc = hl5280_usbc_analog_setup_switches(hl_priv);

	return rc;
}
EXPORT_SYMBOL(hl5280_reg_notifier);

/*
 * hl5280_unreg_notifier - unregister notifier block with fsa driver
 *
 * @nb - notifier block of hl5280
 * @node - phandle node to hl5280 device
 *
 * Returns 0 on pass, or error code
 */
int hl5280_unreg_notifier(struct notifier_block *nb,
				 struct device_node *node)
{
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct hl5280_priv *hl_priv;

	if (!client)
		return -EINVAL;

	hl_priv = (struct hl5280_priv *)i2c_get_clientdata(client);
	if (!hl_priv)
		return -EINVAL;

	hl5280_usbc_update_settings(hl_priv, 0x18, 0x98);
	return blocking_notifier_chain_unregister
					(&hl_priv->hl5280_notifier, nb);
}
EXPORT_SYMBOL(hl5280_unreg_notifier);

static int hl5280_validate_display_port_settings(struct hl5280_priv *hl_priv)
{
	u32 switch_status = 0;

	regmap_read(hl_priv->regmap, HL5280_SWITCH_STATUS1, &switch_status);

	if ((switch_status != 0x23) && (switch_status != 0x1C)) {
		dev_err(hl_priv->dev, "%s: AUX SBU1/2 switch status is invalid = %u\n",
				__func__, switch_status);
		return -EIO;
	}

	return 0;
}
/*
 * hl5280_switch_event - configure FSA switch position based on event
 *
 * @node - phandle node to hl5280 device
 * @event - hl_function enum
 *
 * Returns int on whether the switch happened or not
 */
int hl5280_switch_event(struct device_node *node,
			 enum hl_function event)
{
	int switch_control = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct hl5280_priv *hl_priv;

	if (!client)
		return -EINVAL;

	hl_priv = (struct hl5280_priv *)i2c_get_clientdata(client);
	if (!hl_priv)
		return -EINVAL;
	if (!hl_priv->regmap)
		return -EINVAL;

	pr_info("%s - switch event: %d\n", __func__, event);
	switch (event) {
	case HL_MIC_GND_SWAP:
		regmap_read(hl_priv->regmap, HL5280_SWITCH_CONTROL,
				&switch_control);
		if ((switch_control & 0x07) == 0x07)
			switch_control = 0x0;
		else
			switch_control = 0x7;
		hl5280_usbc_update_settings(hl_priv, switch_control, 0x9F);
		break;
	case HL_USBC_ORIENTATION_CC1:
		hl5280_usbc_update_settings(hl_priv, 0x18, 0xF8);
		return hl5280_validate_display_port_settings(hl_priv);
	case HL_USBC_ORIENTATION_CC2:
		hl5280_usbc_update_settings(hl_priv, 0x78, 0xF8);
		return hl5280_validate_display_port_settings(hl_priv);
	case HL_USBC_DISPLAYPORT_DISCONNECTED:
		hl5280_usbc_update_settings(hl_priv, 0x18, 0x98);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(hl5280_switch_event);

static int hl5280_parse_dt(struct hl5280_priv *hl_priv,
	struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	int ret = 0;

	if (dNode == NULL) {
		pr_err("%s: device node is NULL\n", __func__);
		return -ENODEV;
	}

	return ret;
}

static void hl5280_usbc_analog_work_fn(struct work_struct *work)
{
	struct hl5280_priv *hl_priv =
		container_of(work, struct hl5280_priv, usbc_analog_work);

	if (!hl_priv) {
		pr_err("%s: fsa container invalid\n", __func__);
		return;
	}
	hl5280_usbc_analog_setup_switches(hl_priv);
	pm_relax(hl_priv->dev);
}

static void hl5280_update_reg_defaults(struct hl5280_priv *hl_priv)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(hl_reg_i2c_defaults); i++)
		regmap_write(hl_priv->regmap, hl_reg_i2c_defaults[i].reg,
				   hl_reg_i2c_defaults[i].val);
}

static ssize_t fregdump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i, rc, ret = 0;
	struct hl5280_priv *hl_priv = g_hl_priv;

	if (!hl_priv) {
		pr_err("%s: fsa priv invalid\n", __func__);
		return ret;
	}
	mutex_lock(&hl_priv->notification_lock);
	for (i = 0 ; i <= 0x1F; i++) {
		ret = regmap_read(hl_priv->regmap, (uint8_t)i, &rc);
		dev_info(hl_priv->dev, "read Reg_0x%02x=0x%02x\n", (uint8_t)i, rc);
	}
	mutex_unlock(&hl_priv->notification_lock);

	return ret;
}
DEVICE_ATTR(fregdump, S_IRUGO, fregdump_show, NULL);

static int hl5280_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	struct hl5280_priv *hl_priv;
	int rc = 0;
	unsigned int reg_value = 0;
	uint8_t tcpc_attach_state = 0;

	hl_priv = devm_kzalloc(&i2c->dev, sizeof(*hl_priv),
				GFP_KERNEL);
	if (!hl_priv)
		return -ENOMEM;

	hl_priv->dev = &i2c->dev;

	hl5280_parse_dt(hl_priv, &i2c->dev);

	hl_priv->regmap = devm_regmap_init_i2c(i2c, &hl5280_regmap_config);
	if (IS_ERR_OR_NULL(hl_priv->regmap)) {
		dev_err(hl_priv->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		if (!hl_priv->regmap) {
			rc = -EINVAL;
			goto err_data;
		}
		rc = PTR_ERR(hl_priv->regmap);
		goto err_data;
	}

	regmap_read(hl_priv->regmap, HL5280_DEVICE_ID, &reg_value);
	if (HL5280_DEVICE_INFO_VAL == reg_value) {
		hl_priv->vendor = HL5280;
		dev_info(hl_priv->dev, "%s: switch chip is HL5280\n", __func__);
	} else if (BCT4480_DEVICE_REG_VALUE == reg_value) {
		hl_priv->vendor = BCT4480;
		dev_info(hl_priv->dev, "%s: switch chip is BCT4480\n", __func__);
	} else {
		hl_priv->vendor = HL5280;
		dev_info(hl_priv->dev, "%s: switch chip is HL5280 or other[0x%x]\n", __func__, reg_value);
	}

	hl5280_update_reg_defaults(hl_priv);

	hl_priv->plug_state = false;
	hl_priv->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!hl_priv->tcpc_dev) {
		pr_err("%s get tcpc device type_c_port0 fail\n", __func__);
		goto err_data;
	}

	hl_priv->pd_nb.notifier_call = hl5280_usbc_event_changed;
	hl_priv->pd_nb.priority = 0;
	rc = register_tcp_dev_notifier(hl_priv->tcpc_dev, &hl_priv->pd_nb, TCP_NOTIFY_TYPE_ALL);
	if (rc < 0) {
		pr_err("%s: register tcpc notifer fail\n", __func__);
		goto err_data;
	}

	mutex_init(&hl_priv->notification_lock);
	i2c_set_clientdata(i2c, hl_priv);

	INIT_WORK(&hl_priv->usbc_analog_work,
		  hl5280_usbc_analog_work_fn);

	hl_priv->hl5280_notifier.rwsem =
		(struct rw_semaphore)__RWSEM_INITIALIZER
		((hl_priv->hl5280_notifier).rwsem);
	hl_priv->hl5280_notifier.head = NULL;

	device_create_file(hl_priv->dev, &dev_attr_fregdump);
	/* In case of audio accessory pluged in before this driver probe */
	tcpc_attach_state = tcpm_inquire_typec_attach_state(hl_priv->tcpc_dev);
	if (unlikely(tcpc_attach_state == TYPEC_ATTACHED_AUDIO)) {
		dev_info(hl_priv->dev, "%s: audio plug in before probe\n", __func__);
		hl_priv->plug_state = true;
		pm_stay_awake(hl_priv->dev);
		schedule_work(&hl_priv->usbc_analog_work);
	}

	g_hl_priv = hl_priv;
	return 0;

err_data:
	devm_kfree(&i2c->dev, hl_priv);
	return rc;
}

static void hl5280_remove(struct i2c_client *i2c)
{
	struct hl5280_priv *hl_priv =
			(struct hl5280_priv *)i2c_get_clientdata(i2c);

	if (!hl_priv)
		return;

	hl5280_usbc_update_settings(hl_priv, 0x18, 0x98);
	cancel_work_sync(&hl_priv->usbc_analog_work);
	pm_relax(hl_priv->dev);
	mutex_destroy(&hl_priv->notification_lock);
	dev_set_drvdata(&i2c->dev, NULL);
	device_remove_file(hl_priv->dev, &dev_attr_fregdump);
}

static const struct of_device_id hl5280_i2c_dt_match[] = {
	{
		.compatible = "mediatek,hl5280-audioswitch",
	},
	{}
};

static struct i2c_driver hl5280_i2c_driver = {
	.driver = {
		.name = HL5280_I2C_NAME,
		.of_match_table = hl5280_i2c_dt_match,
	},
	.probe = hl5280_probe,
	.remove = hl5280_remove,
};

static int __init hl5280_init(void)
{
	int rc;

	rc = i2c_add_driver(&hl5280_i2c_driver);
	if (rc)
		pr_err("hl5280: Failed to register I2C driver: %d\n", rc);

	return rc;
}

static void __exit hl5280_exit(void)
{
	i2c_del_driver(&hl5280_i2c_driver);
}

late_initcall_sync(hl5280_init);
module_exit(hl5280_exit);

MODULE_DESCRIPTION("HL5280 I2C driver");
MODULE_LICENSE("GPL v2");
