/*
 * dio8016-regulator.c - Regulator device driver for DIO8016
 * Copyright (C) 2021  DIOO Semiconductor Ltd.
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

#define DIO8016_CHIPID			0x00
#define DIO8016_ILIMIT			0x01
#define DIO8016_RDIS			0x02

#define DIO8016_LDO1_DVO1		0x03
#define DIO8016_LDO2_DVO2		0x04
#define DIO8016_LDO3_AVO1		0x05
#define DIO8016_LDO4_AVO2		0x06

#define DIO8016_LDO1_LDO2_SEQ1	0x0a
#define DIO8016_LDO3_LDO4_SEQ2	0x0b

#define DIO8016_LDO_EN			0x0e
#define DIO8016_SEQ_C			0x0f
#define DIO8016_ILMIT_COAR		0x10

#define DIO8016_MAX_REG_NO		0x10
#define DIO8016_VSEL_MASK		0xff

#define DEVICE_ID				0x04

enum dio8016_regulators {
	DIO8016_REGULATOR_LDO1 = 0,
	DIO8016_REGULATOR_LDO2,
	DIO8016_REGULATOR_LDO3,
	DIO8016_REGULATOR_LDO4,
	DIO8016_MAX_REGULATORS,
};

/* ldo1~4  0-min current,1- max current */
/*
 *	LDO1	1500000, 1700000
 *	LDO2	1500000, 1700000
 *	LDO3	400000, 600000
 *	LDO4	400000, 600000
 */
static const unsigned int dio8016_crtable1[] = {1500000, 1700000};
static const unsigned int dio8016_crtable2[] = {1500000, 1700000};
static const unsigned int dio8016_crtable3[] = {400000, 600000};
static const unsigned int dio8016_crtable4[] = {400000, 600000};

struct dio8016 {
	struct 	device *dev;
	struct 	regmap *regmap;
	struct 	regulator_dev *rdev;
	struct 	gpio_desc *en_gpio;
	int 	min_dropout_uv;
	int 	ldo_vout[4];
	int 	ldo_en;
};

static const struct regulator_ops dio8016_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_current_limit  = regulator_set_current_limit_regmap,
	.enable				= regulator_enable_regmap,
	.disable			= regulator_disable_regmap,
	.is_enabled			= regulator_is_enabled_regmap,
};

#define DIO8016_DESC(_id, _match, _supply, _min, _max, _step, 				\
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
		.ops				= &dio8016_reg_ops,							\
		.curr_table 		= _curr_table,								\
		.linear_min_sel 	= _minsel,									\
		.owner				= THIS_MODULE,								\
	}

static const struct regulator_desc dio8016_reg[] = {
	DIO8016_DESC(DIO8016_REGULATOR_LDO1, "ldo1", "dvin", 600, 2130, 6,
		     	DIO8016_LDO1_DVO1, DIO8016_VSEL_MASK, DIO8016_LDO_EN, BIT(0),
			 	BIT(0), 0,	dio8016_crtable1, DIO8016_ILIMIT, BIT(0), 0),

	DIO8016_DESC(DIO8016_REGULATOR_LDO2, "ldo2", "dvin", 600, 2130, 6,
		     	DIO8016_LDO2_DVO2, DIO8016_VSEL_MASK, DIO8016_LDO_EN, BIT(1),
			 	BIT(1), 0, dio8016_crtable2, DIO8016_ILIMIT, BIT(1), 0),

	DIO8016_DESC(DIO8016_REGULATOR_LDO3, "ldo3", "avin", 1200, 4387.5, 12.5,
		     	DIO8016_LDO3_AVO1, DIO8016_VSEL_MASK, DIO8016_LDO_EN, BIT(2),
			 	BIT(2), 0, dio8016_crtable3, DIO8016_ILIMIT, BIT(2), 0),

	DIO8016_DESC(DIO8016_REGULATOR_LDO4, "ldo4", "avin", 1200, 4387.5, 12.5,
		     	DIO8016_LDO4_AVO2, DIO8016_VSEL_MASK, DIO8016_LDO_EN, BIT(3),
			 	BIT(3), 0, dio8016_crtable4, DIO8016_ILIMIT, BIT(3), 0),
};


static const struct regmap_range dio8016_writeable_ranges[] = {
	regmap_reg_range(DIO8016_ILIMIT, DIO8016_ILMIT_COAR),
};

static const struct regmap_range dio8016_readable_ranges[] = {
	regmap_reg_range(DIO8016_CHIPID, DIO8016_ILMIT_COAR),
};

static const struct regmap_range dio8016_volatile_ranges[] = {
	regmap_reg_range(DIO8016_ILIMIT, DIO8016_ILMIT_COAR),
};

static const struct regmap_access_table dio8016_writeable_table = {
	.yes_ranges   = dio8016_writeable_ranges,
	.n_yes_ranges = ARRAY_SIZE(dio8016_writeable_ranges),
};

static const struct regmap_access_table dio8016_readable_table = {
	.yes_ranges   = dio8016_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(dio8016_readable_ranges),
};

static const struct regmap_access_table dio8016_volatile_table = {
	.yes_ranges   = dio8016_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(dio8016_volatile_ranges),
};

static const struct regmap_config dio8016_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = DIO8016_MAX_REG_NO,
	.wr_table = &dio8016_writeable_table,
	.rd_table = &dio8016_readable_table,
	.cache_type = REGCACHE_RBTREE,
	.volatile_table = &dio8016_volatile_table,
};

static void dio8016_reset(struct dio8016 *dio8016)
{
	gpiod_set_value_cansleep(dio8016->en_gpio, 1);	//set H
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(dio8016->en_gpio, 0);	//set L
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(dio8016->en_gpio, 1);	//set H
	usleep_range(10000, 11000);
}


static int dio8016_check_device_id(struct regmap *regmap, unsigned int reg, unsigned int expected_id)
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


static int dio8016_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	const struct regulator_desc *regulators;
	struct dio8016 *dio8016;
	int ret, i, discharge;

	dio8016 = devm_kzalloc(dev, sizeof(struct dio8016), GFP_KERNEL);
	if (!dio8016)
		return -ENOMEM;

	dio8016->en_gpio = devm_gpiod_get(dev, "en", GPIOD_OUT_HIGH);
	if (IS_ERR(dio8016->en_gpio)) {
		ret = PTR_ERR(dio8016->en_gpio);
		dev_err(dev, "failed to request reset GPIO: %d\n", ret);
		return ret;
	}

	dio8016_reset(dio8016);

	regulators = dio8016_reg;

	i2c_set_clientdata(client, dio8016);
	dio8016->dev = dev;
	dio8016->regmap = devm_regmap_init_i2c(client, &dio8016_regmap_config);
	if (IS_ERR(dio8016->regmap)) {
		ret = PTR_ERR(dio8016->regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}

	ret = dio8016_check_device_id(dio8016->regmap,DIO8016_CHIPID,DEVICE_ID);
	if(ret < 0){
		return ret;
	}

	regmap_write(dio8016->regmap, DIO8016_RDIS, 0x0F);

	regmap_read(dio8016->regmap, DIO8016_RDIS, &discharge);
	pr_info("dio8016 RDIS%d value is 0x%x\n", DIO8016_RDIS, discharge);


	config.dev = &client->dev;
	config.regmap = dio8016->regmap;

	/* Instantiate the regulators */
	for (i = 0; i < DIO8016_MAX_REGULATORS; i++) {
		rdev = devm_regulator_register(&client->dev,
					       &regulators[i], &config);
		if (IS_ERR(rdev)) {
			dev_err(&client->dev,
				"failed to register %d regulator\n", i);
			return PTR_ERR(rdev);
		}
	}

	/*Inital related mask for interrupt here*/
	for (i = 0; i < ARRAY_SIZE(dio8016->ldo_vout); i++)
	{
		regmap_read(dio8016->regmap, DIO8016_LDO1_DVO1 + i,
			&dio8016->ldo_vout[i]);
		pr_info("dio8016 ET_LDO%d value is 0x%x\n", i, dio8016->ldo_vout[i]);
	}

	return 0;
}

static void dio8016_regulator_shutdown(struct i2c_client *client)
{
	struct dio8016 *dio8016 = i2c_get_clientdata(client);

	if (system_state == SYSTEM_POWER_OFF)
		regmap_write(dio8016->regmap, DIO8016_LDO_EN, 0x80);
}

static int __maybe_unused dio8016_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct dio8016 *dio8016 = i2c_get_clientdata(client);
	int i;

	regmap_read(dio8016->regmap, DIO8016_LDO_EN, &dio8016->ldo_en);
	for (i = 0; i < ARRAY_SIZE(dio8016->ldo_vout); i++)
		regmap_read(dio8016->regmap, DIO8016_LDO1_DVO1 + i,
			    &dio8016->ldo_vout[i]);

	return 0;
}

static int __maybe_unused dio8016_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct dio8016 *dio8016 = i2c_get_clientdata(client);
	int i;

	for (i = 0; i < ARRAY_SIZE(dio8016->ldo_vout); i++)
		regmap_write(dio8016->regmap, DIO8016_LDO1_DVO1 + i,
			     dio8016->ldo_vout[i]);
	regmap_write(dio8016->regmap, DIO8016_LDO_EN, dio8016->ldo_en);

	return 0;
}

static SIMPLE_DEV_PM_OPS(dio8016_pm_ops, dio8016_suspend, dio8016_resume);

static const struct i2c_device_id dio8016_i2c_id[] = {
	{ "dio8016", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, dio8016_i2c_id);

static const struct of_device_id dio8016_of_match[] = {
	{ .compatible = "dioo,dio8016" },
	{}
};
MODULE_DEVICE_TABLE(of, dio8016_of_match);

static struct i2c_driver dio8016_i2c_driver = {
	.driver = {
		.name = "dio8016",
		.of_match_table = of_match_ptr(dio8016_of_match),
		.pm = &dio8016_pm_ops,
	},
	.id_table 	= dio8016_i2c_id,
	.probe		= dio8016_i2c_probe,
	.shutdown 	= dio8016_regulator_shutdown,
};

module_i2c_driver(dio8016_i2c_driver);

MODULE_DESCRIPTION("DIO8016 regulator driver");
MODULE_LICENSE("GPL");
