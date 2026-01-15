/*
 * et5924a-regulator.c - Regulator device driver for ET5924A
 * Copyright (C) 2021  ETEK Semiconductor Ltd.
 *
 */

#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/kernel.h>

#define ET5924A_CHIPID			0x00
#define ET5924A_ILIMIT			0x01
#define ET5924A_RDIS			0x02

#define ET5924A_LDO1_DVO1		0x03
#define ET5924A_LDO2_DVO2		0x04
#define ET5924A_LDO3_AVO1		0x05
#define ET5924A_LDO4_AVO2		0x06

#define ET5924A_LDO1_LDO2_SEQ1	0x0a
#define ET5924A_LDO3_LDO4_SEQ2	0x0b

#define ET5924A_LDO_EN			0x0e
#define ET5924A_SEQ_C			0x0f
#define ET5924A_ILMIT_COAR		0x10

#define ET5924A_MAX_REG_NO		0x10
#define ET5924A_VSEL_MASK		0xff

#define DEVICE_ID				0x0A


enum et5924a_regulators {
	ET5924A_REGULATOR_LDO1 = 0,
	ET5924A_REGULATOR_LDO2,
	ET5924A_REGULATOR_LDO3,
	ET5924A_REGULATOR_LDO4,
	ET5924A_MAX_REGULATORS,
};

/* ldo1~4  0-min current,1- max current */
/*
 *	LDO1	1550000, 2000000
 *	LDO2	1250000, 1900000
 *	LDO3	400000, 1000000
 *	LDO4	400000, 1000000
 */
static const unsigned int et5924a_crtable1[] = {1550000, 3000000};
static const unsigned int et5924a_crtable2[] = {1250000, 1900000};
static const unsigned int et5924a_crtable3[] = {400000, 1000000};
static const unsigned int et5924a_crtable4[] = {400000, 1000000};

struct et5924a {
	struct 	device *dev;
	struct 	regmap *regmap;
	struct 	regulator_dev *rdev;
	struct 	gpio_desc *en_gpio;
	int 	min_dropout_uv;
	int 	ldo_vout[4];
	int 	ldo_en;
};

static const struct regulator_ops et5924a_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_current_limit  = regulator_set_current_limit_regmap,
	.enable				= regulator_enable_regmap,
	.disable			= regulator_disable_regmap,
	.is_enabled			= regulator_is_enabled_regmap,
};

#define ET5924A_DESC(_id, _match, _supply, _min, _max, _step, 				\
					_vreg, _vmask, _ereg, _emask, 							\
					_enval, _disval, _curr_table, _creg, _cmask, _minsel)	\
	{																	\
		.id					= (_id),									\
		.name				= (_match),									\
		.of_match			= of_match_ptr(_match),						\
		.supply_name		= (_supply),								\
		.min_uV				= (_min) * 1000,							\
		.uV_step			= (_step) * 1000,							\
		.n_voltages			= (((_max) - (_min)) / (_step) + _minsel),	\
		.n_current_limits 	= ARRAY_SIZE(_curr_table),					\
		.regulators_node 	= of_match_ptr("regulators"),				\
		.type				= REGULATOR_VOLTAGE,						\
		.vsel_reg			= (_vreg),									\
		.vsel_mask			= (_vmask),									\
		.csel_reg			= (_creg),									\
		.csel_mask			= (_cmask),									\
		.enable_reg			= (_ereg),									\
		.enable_mask		= (_emask),									\
		.enable_val     	= (_enval),									\
		.disable_val    	= (_disval),								\
		.ops				= &et5924a_reg_ops,							\
		.curr_table 		= _curr_table,								\
		.linear_min_sel 	= _minsel,									\
		.owner				= THIS_MODULE,								\
	}

static const struct regulator_desc et5924a_reg[] = {
	ET5924A_DESC(ET5924A_REGULATOR_LDO1, "ldo1", "dvin", 600, 2130, 6,
		     	ET5924A_LDO1_DVO1, ET5924A_VSEL_MASK, ET5924A_LDO_EN, BIT(0),
			 	BIT(0), 0,	et5924a_crtable1, ET5924A_ILIMIT, BIT(0), 0),

	ET5924A_DESC(ET5924A_REGULATOR_LDO2, "ldo2", "dvin", 600, 2130, 6,
		     	ET5924A_LDO2_DVO2, ET5924A_VSEL_MASK, ET5924A_LDO_EN, BIT(1),
			 	BIT(1), 0, et5924a_crtable2, ET5924A_ILIMIT, BIT(1), 0),

	ET5924A_DESC(ET5924A_REGULATOR_LDO3, "ldo3", "avin", 1200, 4387.5, 12.5,
		     	ET5924A_LDO3_AVO1, ET5924A_VSEL_MASK, ET5924A_LDO_EN, BIT(2),
			 	BIT(2), 0, et5924a_crtable3, ET5924A_ILIMIT, BIT(2), 0),

	ET5924A_DESC(ET5924A_REGULATOR_LDO4, "ldo4", "avin", 1200, 4387.5, 12.5,
		     	ET5924A_LDO4_AVO2, ET5924A_VSEL_MASK, ET5924A_LDO_EN, BIT(3),
			 	BIT(3), 0, et5924a_crtable4, ET5924A_ILIMIT, BIT(3), 0),
};


static const struct regmap_range et5924a_writeable_ranges[] = {
	regmap_reg_range(ET5924A_ILIMIT, ET5924A_ILMIT_COAR),
};

static const struct regmap_range et5924a_readable_ranges[] = {
	regmap_reg_range(ET5924A_CHIPID, ET5924A_ILMIT_COAR),
};

static const struct regmap_range et5924a_volatile_ranges[] = {
	regmap_reg_range(ET5924A_ILIMIT, ET5924A_ILMIT_COAR),
};

static const struct regmap_access_table et5924a_writeable_table = {
	.yes_ranges   = et5924a_writeable_ranges,
	.n_yes_ranges = ARRAY_SIZE(et5924a_writeable_ranges),
};

static const struct regmap_access_table et5924a_readable_table = {
	.yes_ranges   = et5924a_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(et5924a_readable_ranges),
};

static const struct regmap_access_table et5924a_volatile_table = {
	.yes_ranges   = et5924a_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(et5924a_volatile_ranges),
};

static const struct regmap_config et5924a_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = ET5924A_MAX_REG_NO,
	.wr_table = &et5924a_writeable_table,
	.rd_table = &et5924a_readable_table,
	.cache_type = REGCACHE_RBTREE,
	.volatile_table = &et5924a_volatile_table,
};


static void et5924a_reset(struct et5924a *et5924a)
{
	gpiod_set_value_cansleep(et5924a->en_gpio, 0);	//set L
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(et5924a->en_gpio, 1);	//set H
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(et5924a->en_gpio, 0);	//set L
	usleep_range(10000, 11000);
}


static int et5924a_check_device_id(struct regmap *regmap, unsigned int reg, unsigned int expected_id)
{
    unsigned int device_id;
    int ret;

    // 读取设备ID
    ret = regmap_read(regmap, reg, &device_id);
    if (ret < 0) {
        pr_info("Failed to read device ID: %d\n", ret);
        return ret;
    }

    // 检查设备ID
    if (device_id != expected_id) {
        pr_info("Invalid device ID: 0x%02X, expected 0x%02X\n", device_id, expected_id);
        return -ENODEV;
    }

    // 设备匹配成功
    pr_info("Device (ID: 0x%02X) detected\n", device_id);

    return 0;
}

static int et5924a_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	const struct regulator_desc *regulators;
	struct et5924a *et5924a;
	int ret, i;

	et5924a = devm_kzalloc(dev, sizeof(struct et5924a), GFP_KERNEL);
	if (!et5924a)
		return -ENOMEM;

	et5924a->en_gpio = devm_gpiod_get(dev, "en", GPIOD_OUT_LOW);
	if (IS_ERR(et5924a->en_gpio)) {
		ret = PTR_ERR(et5924a->en_gpio);
		dev_err(dev, "failed to request reset GPIO: %d\n", ret);
		return ret;
	}

	et5924a_reset(et5924a);

	regulators = et5924a_reg;

	i2c_set_clientdata(client, et5924a);
	et5924a->dev = dev;

	et5924a->regmap = devm_regmap_init_i2c(client, &et5924a_regmap_config);
	if (IS_ERR(et5924a->regmap)) {
		ret = PTR_ERR(et5924a->regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}

	ret = et5924a_check_device_id(et5924a->regmap,ET5924A_CHIPID,DEVICE_ID);
	if(ret < 0){
		return ret;
	}

	config.dev = &client->dev;
	config.regmap = et5924a->regmap;

	/* Instantiate the regulators */
	for (i = 0; i < ET5924A_MAX_REGULATORS; i++) {
		rdev = devm_regulator_register(&client->dev,
					       &regulators[i], &config);
		if (IS_ERR(rdev)) {
			dev_err(&client->dev,
				"failed to register %d regulator\n", i);
			return PTR_ERR(rdev);
		}
	}

	/*Inital related mask for interrupt here*/
	for (i = 0; i < ARRAY_SIZE(et5924a->ldo_vout); i++)
	{
		regmap_read(et5924a->regmap, ET5924A_LDO1_DVO1 + i,
			&et5924a->ldo_vout[i]);
		pr_info("et5924a ET_LDO%d value is 0x%x\n", i, et5924a->ldo_vout[i]);
	}

	return 0;
}

static void et5924a_regulator_shutdown(struct i2c_client *client)
{
	struct et5924a *et5924a = i2c_get_clientdata(client);

	if (system_state == SYSTEM_POWER_OFF)
		regmap_write(et5924a->regmap, ET5924A_LDO_EN, 0x80);
}

static int __maybe_unused et5924a_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct et5924a *et5924a = i2c_get_clientdata(client);
	int i;

	regmap_read(et5924a->regmap, ET5924A_LDO_EN, &et5924a->ldo_en);
	for (i = 0; i < ARRAY_SIZE(et5924a->ldo_vout); i++)
		regmap_read(et5924a->regmap, ET5924A_LDO1_DVO1 + i,
			    &et5924a->ldo_vout[i]);

	return 0;
}

static int __maybe_unused et5924a_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct et5924a *et5924a = i2c_get_clientdata(client);
	int i;

	for (i = 0; i < ARRAY_SIZE(et5924a->ldo_vout); i++)
		regmap_write(et5924a->regmap, ET5924A_LDO1_DVO1 + i,
			     et5924a->ldo_vout[i]);
	regmap_write(et5924a->regmap, ET5924A_LDO_EN, et5924a->ldo_en);

	return 0;
}

static SIMPLE_DEV_PM_OPS(et5924a_pm_ops, et5924a_suspend, et5924a_resume);

static const struct i2c_device_id et5924a_i2c_id[] = {
	{ "et5924a", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, et5924a_i2c_id);

static const struct of_device_id et5924a_of_match[] = {
	{ .compatible = "etek,et5924" },
	{}
};
MODULE_DEVICE_TABLE(of, et5924a_of_match);

static struct i2c_driver et5924a_i2c_driver = {
	.driver = {
		.name = "et5924a",
		.of_match_table = of_match_ptr(et5924a_of_match),
		.pm = &et5924a_pm_ops,
	},
	.id_table 	= et5924a_i2c_id,
	.probe		= et5924a_i2c_probe,
	.shutdown 	= et5924a_regulator_shutdown,
};

module_i2c_driver(et5924a_i2c_driver);

MODULE_DESCRIPTION("ET5924A regulator driver");
MODULE_LICENSE("GPL");
