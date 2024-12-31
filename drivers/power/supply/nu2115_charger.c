#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include "charger_class.h"
#include "nu2115_charger.h"

#define NU2115_DEVID		0x90

#define NU2115_NAME "nu2115_cp"
char slave_charger_info_3[32] = {"unknown"};
EXPORT_SYMBOL(slave_charger_info_3);

enum {
	NU2115_MASTER,
	NU2115_SLAVE,
	NU2115_STANDALONE,
};

enum {
	NU2115_ADC_IBUS = 0,
	NU2115_ADC_VBUS,
	NU2115_ADC_VAC1,
	NU2115_ADC_VAC2,
	NU2115_ADC_VOUT,
	NU2115_ADC_VBAT,
	NU2115_ADC_IBAT,
	NU2115_ADC_TSBUS,
	NU2115_ADC_TSBAT,
	NU2115_ADC_TDIE,
};

static const char *nu2115_adc_name[] = {
	[NU2115_ADC_IBUS] = "ibus",
	[NU2115_ADC_VBUS] = "vbus",
	[NU2115_ADC_VAC1] = "vac1",
	[NU2115_ADC_VAC2] = "vac2",
	[NU2115_ADC_VOUT] = "vout",
	[NU2115_ADC_VBAT] = "vbat",
	[NU2115_ADC_IBAT] = "ibat",
	[NU2115_ADC_TSBUS] = "tsbus",
	[NU2115_ADC_TSBAT] = "tsbat",
	[NU2115_ADC_TDIE] = "tdie",
};


//static const u32 irqmask_default = ~(u32)BIT(NU2115_IRQFLAG_ADC_DONE);

struct nu2115_cfg {
	/* household */
	unsigned int ac_ovp;
	unsigned int bat_ovp;
	unsigned int bat_ovp_alm;
	unsigned int bat_ocp;
	unsigned int bat_ocp_alm;
	unsigned int bat_ucp_alm;
	unsigned int bus_ovp;
	unsigned int bus_ovp_alm;
	unsigned int bus_ocp;
	unsigned int bus_ocp_alm;
	unsigned int fsw_set;
	unsigned int ibat_sns_res;
	unsigned int ss_timeout;
	int tdie_dis;
	int tsbus_dis;
	int tsbat_dis;
	int pmid2out_uvp;
	int pmid2out_ovp;
	/* watchdog */
	bool watchdog_dis;
	unsigned int watchdog;
};

struct nu2115 {
	struct i2c_client *client;
	struct device *dev;
	struct nu2115_cfg cfg;

	struct mutex rw_lock;
	struct mutex ops_lock;
	struct mutex adc_lock;
	struct mutex irq_lock;

	u32 irqmask;

	struct charger_device *chg_dev;
	struct charger_properties chg_prop;

	struct dentry *debugfs;
	u8 debug_addr;
	int irq_gpio;
	int irq;
};

static int __nu2115_read(struct nu2115 *chip, u8 reg, u8 *val)
{
	struct i2c_client *client = chip->client;
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		return ret;

	*val = (ret & 0xFF);

	return ret < 0 ? ret : 0;
}

static int nu2115_read_device(void *client, u32 addr, int len, void *dst)
{
	struct i2c_client *i2c = (struct i2c_client *)client;

	return i2c_smbus_read_i2c_block_data(i2c, addr, len, dst);
}


static int __nu2115_write(struct nu2115 *chip, u8 reg, u8 val)
{
	struct i2c_client *client = chip->client;
	s32 ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);

	return ret < 0 ? ret : 0;
}

static inline int __nu2115_i2c_read_block(struct nu2115 *chip, u8 reg,
					  u32 len, u8 *data)
{
	int ret;

	ret = nu2115_read_device(chip->client, reg, len, data);

	return ret;
}

static int nu2115_i2c_read_block(struct nu2115 *chip, u8 reg, u32 len,
				 u8 *data)
{
	int ret;

	mutex_lock(&chip->rw_lock);
	ret = __nu2115_i2c_read_block(chip, reg, len, data);
	mutex_unlock(&chip->rw_lock);

	return ret;
}

static int __nu2115_update_bits(struct nu2115 *chip, u8 reg, u8 mask, u8 val)
{
	u8 tmp;
	s32 ret = 0;

	mutex_lock(&chip->rw_lock);

	ret = __nu2115_read(chip, reg, &tmp);
	if (ret < 0)
		goto out;

	tmp = (val & mask) | (tmp & (~mask));
	ret = __nu2115_write(chip, reg, tmp);

out:
	mutex_unlock(&chip->rw_lock);

	return ret;
}

static int __nu2115_set_bits(struct nu2115 *chip, u8 reg, u8 bits)
{
	return __nu2115_update_bits(chip, reg, bits, 0xFF);
}

static int __nu2115_clr_bits(struct nu2115 *chip, u8 reg, u8 bits)
{
	return __nu2115_update_bits(chip, reg, bits, 0);
}

static bool __nu2115_check_bits(struct nu2115 *chip, u8 reg, u8 bits)
{
	u8 val;
	int ret;

	ret = __nu2115_read(chip, reg, &val);
	if (ret < 0)
		return false;

	return (val & bits) ? true : false;
}

static int __nu2115_get_bits(struct nu2115 *chip, u8 reg, u8 mask, u8 offset)
{
	u8 val;
	int ret;

	ret = __nu2115_read(chip, reg, &val);
	if (ret < 0)
		return false;

	return ((val & mask) >> offset);
}

/*
static int __nu2115_read_word(struct nu2115 *chip, u8 reg, u16 *val)
{
	struct i2c_client *client = chip->client;
	s32 ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0)
		return ret;

	*val = (u16)ret;

	return 0;
}
*/

static int __nu2115_write_word(struct nu2115 *chip, u8 reg, u16 val)
{
	struct i2c_client *client = chip->client;
	s32 ret;

	ret = i2c_smbus_write_word_data(client, reg, val);

	return ret < 0 ? ret : 0;
}

static void __maybe_unused __nu2115_dump_register(struct nu2115 *chip)
{
	u8 reg, val;
	int ret;

	for (reg = NU2115_REG_00; reg <= NU2115_REG_36; reg++) {
		ret = __nu2115_read(chip, reg, &val);
		if (ret < 0) {
			dev_err(chip->dev, "[DUMP] 0x%02x = error\n", reg);
			continue;
		}
		dev_err(chip->dev, "[DUMP] 0x%02x = 0x%02x\n", reg, val);
	}
}

static int __nu2115_set_bat_ovp(struct nu2115 *chip, unsigned int uV)
{
	u8 val = (uV - NU2115_BAT_OVP_MIN_UV)
			/ NU2115_BAT_OVP_STEP_UV;

	if (uV < NU2115_BAT_OVP_MIN_UV)
		val = 0x0;
	if (uV > NU2115_BAT_OVP_MAX_UV)
		val = 0x7F;
    dev_info(chip->dev, "%s %d(0x%02X)\n", __func__, uV, val);
	return __nu2115_update_bits(chip, NU2115_REG_00,
				     NU2115_BAT_OVP_MASK, val);
}

static int __nu2115_set_bat_ovp_alm(struct nu2115 *chip, unsigned int uV)
{
	u8 val = (uV - NU2115_BAT_OVP_ALM_MIN_UV)
			/ NU2115_BAT_OVP_ALM_STEP_UV;

	if (uV < NU2115_BAT_OVP_ALM_MIN_UV)
		val = 0x0;
	if (uV > NU2115_BAT_OVP_ALM_MAX_UV)
		val = 0x7F;
    dev_info(chip->dev, "%s %d(0x%02X)\n", __func__, uV, val);
	return __nu2115_update_bits(chip, NU2115_REG_01,
				     NU2115_BAT_OVP_ALM_MASK, val);
}

static int __nu2115_set_bat_ocp(struct nu2115 *chip, unsigned int uA)
{
	u8 val = (uA - NU2115_BAT_OCP_MIN_UA)
			/ NU2115_BAT_OCP_STEP_UA;

	if (uA < NU2115_BAT_OCP_MIN_UA)
		val = 0x0;
	if (uA > NU2115_BAT_OCP_MAX_UA)
		val = 0x7F;
    dev_info(chip->dev, "%s %d(0x%02X)\n", __func__, uA, val);
	return __nu2115_update_bits(chip, NU2115_REG_02,
				     NU2115_BAT_OCP_MASK, val);
}

static int __nu2115_set_bat_ocp_alm(struct nu2115 *chip, unsigned int uA)
{
	u8 val = (uA - NU2115_BAT_OCP_ALM_MIN_UA)
			/ NU2115_BAT_OCP_ALM_STEP_UA;

	if (uA < NU2115_BAT_OCP_ALM_MIN_UA)
		val = 0x0;
	if (uA > NU2115_BAT_OCP_ALM_MAX_UA)
		val = 0x7F;
    	dev_dbg(chip->dev, "%s %d(0x%02X)\n", __func__, uA, val);
	return __nu2115_update_bits(chip, NU2115_REG_03,
				     NU2115_BAT_OCP_ALM_MASK, val);
}

static int __nu2115_set_bat_ucp_alm(struct nu2115 *chip, unsigned int uA)
{
	u8 val = (uA - NU2115_BAT_UCP_ALM_MIN_UA)
			/ NU2115_BAT_UCP_ALM_STEP_UA;

	if (uA < NU2115_BAT_UCP_ALM_MIN_UA)
		val = 0x0;
	if (uA > NU2115_BAT_UCP_ALM_MAX_UA)
		val = 0x3F;

	return __nu2115_update_bits(chip, NU2115_REG_04,
				     NU2115_BAT_UCP_ALM_MASK, val);
}

static int __nu2115_set_ac1_ovp(struct nu2115 *chip, unsigned int uV)
{
	u8 val = (uV - NU2115_AC1_OVP_MIN_UV) / NU2115_AC1_OVP_STEP_UV;

	if (uV < NU2115_AC1_OVP_MIN_UV)
		val = 0x0;
	if (uV > NU2115_AC1_OVP_MAX_UV)
		val = 0x7;

	return __nu2115_update_bits(chip, NU2115_REG_05,
				     NU2115_AC1_OVP_MASK, val);
}

static int __nu2115_set_ac2_ovp(struct nu2115 *chip, unsigned int uV)
{
	u8 val = (uV - NU2115_AC2_OVP_MIN_UV) / NU2115_AC2_OVP_STEP_UV;

	if (uV < NU2115_AC2_OVP_MIN_UV)
		val = 0x0;
	if (uV > NU2115_AC2_OVP_MAX_UV)
		val = 0x7;

	return __nu2115_update_bits(chip, NU2115_REG_06,
				     NU2115_AC2_OVP_MASK, val);
}

static int __nu2115_set_bus_ovp(struct nu2115 *chip, unsigned int uV)
{
	u8 val = (uV - NU2115_BUS_OVP_MIN_UV)
			/ NU2115_BUS_OVP_STEP_UV;

	if (uV < NU2115_BUS_OVP_MIN_UV)
		val = 0x0;
	if (uV > NU2115_BUS_OVP_MAX_UV)
		val = 0x3F;

    	dev_info(chip->dev, "%s %d(0x%02X)\n", __func__, uV, val);
	return __nu2115_update_bits(chip, NU2115_REG_07,
				     NU2115_BUS_OVP_MASK, val);
}


static int __nu2115_set_bus_ovp_alm(struct nu2115 *chip, unsigned int uV)
{
	u8 val = (uV - NU2115_BUS_OVP_ALM_MIN_UV)
			/ NU2115_BUS_OVP_ALM_STEP_UV;

	if (uV < NU2115_BUS_OVP_ALM_MIN_UV)
		val = 0x0;
	if (uV > NU2115_BUS_OVP_ALM_MAX_UV)
		val = 0x3F;
    	dev_dbg(chip->dev, "%s %d(0x%02X)\n", __func__, uV, val);
	return __nu2115_update_bits(chip, NU2115_REG_08,
				     NU2115_BUS_OVP_ALM_MASK, val);
}

static int __nu2115_set_bus_ocp(struct nu2115 *chip, unsigned int uA)
{
	u8 val = (uA - NU2115_BUS_OCP_MIN_UA)
			/ NU2115_BUS_OCP_STEP_UA;

	if (uA < NU2115_BUS_OCP_MIN_UA)
		val = 0x0;
	if (uA > NU2115_BUS_OCP_MAX_UA)
		val = 0x0F;

	dev_info(chip->dev, "%s %d(0x%02X)\n", __func__, uA, val);

	return __nu2115_update_bits(chip, NU2115_REG_09,
				     NU2115_BUS_OCP_MASK, val);
}

static int __nu2115_set_bus_ocp_alm(struct nu2115 *chip, unsigned int uA)
{
	u8 val = (uA - NU2115_BUS_OCP_ALM_MIN_UA)
			/ NU2115_BUS_OCP_ALM_STEP_UA;

	if (uA < NU2115_BUS_OCP_ALM_MIN_UA)
		val = 0x0;
	if (uA > NU2115_BUS_OCP_ALM_MAX_UA)
		val = 0x1F;

    	dev_dbg(chip->dev, "%s %d(0x%02X)\n", __func__, uA, val);
	return __nu2115_update_bits(chip, NU2115_REG_0A,
				     NU2115_BUS_OCP_ALM_MASK, val);
}

/*
static int __nu2115_set_fsw(struct nu2115 *chip, unsigned int hz)
{
	const unsigned int fsw_set[] = {
		200000, 300000, 400000, 500000, 600000, 700000, 800000, 900000
	};
	u8 val;

	for (val = 0; val < ARRAY_SIZE(fsw_set) - 1; val++) {
		if (hz <= fsw_set[val])
			break;
	}

	return __nu2115_update_bits(chip, NU2115_REG_0D,
				     NU2115_FSW_SET_MASK,
				     val << NU2115_FSW_SET_SHIFT);
}
*/

static int __nu2115_enable_watchdog(struct nu2115 *chip, bool en)
{
	return (en ? __nu2115_clr_bits : __nu2115_set_bits)
			(chip, NU2115_REG_0D, NU2115_WATCHDOG_DIS_MASK);
}

static int __nu2115_set_watchdog(struct nu2115 *chip, unsigned int usec)
{
	const unsigned int watchdog[] = {
		500000, 1000000, 5000000, 30000000
	};
	u8 val;

	for (val = 0; val < ARRAY_SIZE(watchdog) - 1; val++) {
		if (usec <= watchdog[val])
			break;
	}

	return __nu2115_update_bits(chip, NU2115_REG_0D,
				     NU2115_WATCHDOG_MASK, val);
}

static bool __nu2115_is_chg_enabled(struct nu2115 *chip)
{
	return __nu2115_check_bits(chip, NU2115_REG_0E,
				    NU2115_CHG_EN_MASK);
}

static int __nu2115_enable_chg(struct nu2115 *chip, bool en)
{
	return (en ? __nu2115_set_bits : __nu2115_clr_bits)
			(chip, NU2115_REG_0E, NU2115_CHG_EN_MASK);
}

static bool __nu2115_is_adc_enabled(struct nu2115 *chip)
{
	return __nu2115_check_bits(chip, NU2115_REG_15,
				    NU2115_ADC_EN_MASK);
}

static int __nu2115_enable_adc(struct nu2115 *chip, bool en)
{
	return (en ? __nu2115_set_bits : __nu2115_clr_bits)
			(chip, NU2115_REG_15, NU2115_ADC_EN_MASK);
}

static int __nu2115_set_adc_fn_dis(struct nu2115 *chip, u16 adc_fn_dis)
{
	u16 val = ((adc_fn_dis & 0xFF) << 8) | ((adc_fn_dis >> 8) & 0xFF);

	return __nu2115_write_word(chip, NU2115_REG_15, val);
}


static int __nu2115_set_ss_timeout(struct nu2115 *chip, unsigned int us)
{
	const unsigned int timeout[] = {
		0, 12500, 25000, 50000, 100000, 400000, 1500000, 100000000
	};
	u8 val;

	for (val = 0; val < ARRAY_SIZE(timeout); val++) {
		if (us <= timeout[val])
			break;
	}

	return __nu2115_update_bits(chip, NU2115_REG_2E,
				     NU2115_SS_TIMEOUT_SET_MASK,
				     val << NU2115_SS_TIMEOUT_SET_SHIFT);
}


static int __nu2115_set_ibat_sns_res(struct nu2115 *chip, unsigned int ohm)
{
	return ((ohm == 5) ? __nu2115_set_bits : __nu2115_clr_bits)
			(chip, NU2115_REG_2E,
			 NU2115_SET_IBAT_SNS_RES_MASK);
}

static int __nu2115_convert_adc(struct nu2115 *chip, int channel, u16 val)
{
	s16 sval = (s16)val;
	int conv_val = 0;

	switch (channel) {
	case NU2115_ADC_IBUS:
	case NU2115_ADC_VBUS:
	case NU2115_ADC_VAC1:
	case NU2115_ADC_VAC2:
	case NU2115_ADC_VOUT:
	case NU2115_ADC_VBAT:
		/* in micro volt */
		conv_val = sval * 1000;
		dev_dbg(chip->dev, "%s adc: %duV (0x%04x)\n",
				nu2115_adc_name[channel], conv_val, val);
		break;
	case NU2115_ADC_IBAT:
		/* in micro amp */
		conv_val = sval * 1000;
		dev_dbg(chip->dev, "%s adc: %duA (0x%04x)\n",
				nu2115_adc_name[channel], conv_val, val);
		break;
	case NU2115_ADC_TDIE:
		/* in degreeC */
		conv_val = sval >> 1;
		dev_dbg(chip->dev, "%s adc: %d.%cdegC (0x%04x)\n",
				nu2115_adc_name[channel],
				conv_val, (sval & 0x1 ? '5' : '0'), val);
		break;
	case NU2115_ADC_TSBAT:
	case NU2115_ADC_TSBUS:
		/* in percent */
		conv_val = sval * 100 / 1024;
		dev_dbg(chip->dev, "%s adc: %d%% (0x%04x)\n",
				nu2115_adc_name[channel], conv_val, val);
		break;
	}

	return conv_val;
}

static int nu2115_get_adc_data(struct nu2115 *chip, int channel,  int *result)
{
	int ret;
	u16 temp;
	u8 data[2];

	if (channel < NU2115_ADC_IBUS || channel > NU2115_ADC_TDIE)
		return -EINVAL;

	ret = __nu2115_set_bits(chip, NU2115_REG_15, NU2115_ADC_EN_MASK);
	usleep_range(12000,15000);

	switch (channel) {
	case NU2115_ADC_IBUS:
		ret = nu2115_i2c_read_block(chip, NU2115_REG_17, 2, data);
	    if (ret < 0)
		    return ret;
		break;
	case NU2115_ADC_VBUS:
		ret = nu2115_i2c_read_block(chip, NU2115_REG_19, 2, data);
	    if (ret < 0)
		    return ret;
		break;
	case NU2115_ADC_VAC1:
		ret = nu2115_i2c_read_block(chip, NU2115_REG_1B, 2, data);
	    if (ret < 0)
		    return ret;
		break;
    case NU2115_ADC_VAC2:
		ret = nu2115_i2c_read_block(chip, NU2115_REG_1D, 2, data);
	    if (ret < 0)
		    return ret;
		break;
	case NU2115_ADC_VOUT:
		ret = nu2115_i2c_read_block(chip, NU2115_REG_1F, 2, data);
	    if (ret < 0)
		    return ret;
		break;
	case NU2115_ADC_VBAT:
		ret = nu2115_i2c_read_block(chip, NU2115_REG_21, 2, data);
	    if (ret < 0)
		    return ret;
		break;
	case NU2115_ADC_IBAT:
		ret = nu2115_i2c_read_block(chip, NU2115_REG_23, 2, data);
	    if (ret < 0)
		    return ret;
		break;
	case NU2115_ADC_TSBUS:
		ret = nu2115_i2c_read_block(chip, NU2115_REG_25, 2, data);
	    if (ret < 0)
		    return ret;
		break;
	case NU2115_ADC_TSBAT:
		ret = nu2115_i2c_read_block(chip, NU2115_REG_27, 2, data);
	    if (ret < 0)
		    return ret;
		break;
	case NU2115_ADC_TDIE:
		ret = nu2115_i2c_read_block(chip, NU2115_REG_29, 2, data);
	    if (ret < 0)
		    return ret;
		break;
	default:
		dev_err(chip->dev, "failed to enable adc\n");
		break;
	}

	temp = (data[0] << 8) + data[1];

	*result = __nu2115_convert_adc(chip, channel, temp);

	dev_dbg(chip->dev, "get %s adc, reg: %2x, channel:%d, val:%4x\n",
				nu2115_adc_name[channel], (NU2115_REG_17 + (channel << 1)),
				channel, temp);

	return 0;
}


static int __nu2115_get_adc(struct nu2115 *chip, int channel, int *data)
{
	if (channel < NU2115_ADC_IBUS || channel > NU2115_ADC_TDIE)
		return -EINVAL;

	mutex_lock(&chip->adc_lock);
	nu2115_get_adc_data(chip, channel, data);
	mutex_unlock(&chip->adc_lock);

	return 0;
}

/*
 * set intterupt bit
 */
static int __nu2115_set_irqmask(struct nu2115 *chip, u8 addr, u8 mask)
{
	int ret;
	u8 val;

	ret = __nu2115_read(chip, addr, &val);
	if (ret)
		return ret;

	val |= mask;

	ret = __nu2115_write(chip, addr, val);

	return ret;
}

/*
 * check stat and flag regs for IC status
 */
static void nu2115_check_status_flags(struct nu2115 *chip)
{
    int ret;
	u8 sf_reg[26] = {0};

	dev_err(chip->dev,"---------%s---------\n", __func__);

	ret = nu2115_i2c_read_block(chip, NU2115_REG_05, 2, &sf_reg[0]);
	if (ret > 0)
	{
	    if (sf_reg[0] & NU2115_AC1_OVP_STAT_MASK)
			dev_err(chip->dev,"REG_05:%x, AC1_OVP_STAT\n", sf_reg[0]);

		if (sf_reg[0] & NU2115_AC1_OVP_FLAG_MASK)
			dev_err(chip->dev,"REG_05:%x, AC1_OVP_FLAG\n", sf_reg[0]);

		if (sf_reg[1] & NU2115_AC2_OVP_STAT_MASK)
			dev_err(chip->dev,"REG_06:%x, AC2_OVP_STAT\n", sf_reg[1]);

		if (sf_reg[1] & NU2115_AC2_OVP_FLAG_MASK)
			dev_err(chip->dev,"REG_06:%x, AC2_OVP_FLAG\n", sf_reg[1]);
	}

	ret = nu2115_i2c_read_block(chip, NU2115_REG_09, 11, &sf_reg[2]);

	if (ret > 0)
	{
	    if (sf_reg[2] & NU2115_IBUS_UCP_RISE_FLAG_MASK)
			dev_err(chip->dev,"REG_09:%x, IBUS_UCP_RISE_FLAG\n", sf_reg[2]);

		if (sf_reg[3] & NU2115_IBUS_UCP_FALL_FLAG_MASK) {
			//if (chip->chg_dev)
			//	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_IBUSUCP_FALL);
			dev_err(chip->dev,"REG_0A:%x, IBUS_UCP_FALL_FLAG\n", sf_reg[3]);
		}

		if (sf_reg[4] & NU2115_VOUT_OVP_STAT_MASK)
			dev_err(chip->dev,"REG_0B:%x, VOUT_OVP_STAT\n", sf_reg[4]);
		if (sf_reg[4] & NU2115_VOUT_OVP_FLAG_MASK) {
			if (chip->chg_dev)
				charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_VOUTOVP);
			dev_err(chip->dev,"REG_0B:%x, VOUT_OVP_FLAG\n", sf_reg[4]);
		}

		if (sf_reg[5] & NU2115_TSD_FLAG_MASK)
			dev_err(chip->dev,"REG_0C:%x, TSD_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & NU2115_TSD_STAT_MASK)
			dev_err(chip->dev,"REG_0C:%x, TSD_STAT\n", sf_reg[5]);
		if (sf_reg[5] & NU2115_VBUS_ERRORLO_FLAG_MASK)
			dev_err(chip->dev,"REG_0C:%x, VBUS_ERRLO_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & NU2115_VBUS_ERRORHI_FLAG_MASK)
			dev_err(chip->dev,"REG_0C:%x, VBUS_ERRHI_FLAG\n", sf_reg[5]);
        if (sf_reg[5] & NU2115_SS_TIMEOUT_FLAG_MASK)
			dev_err(chip->dev,"REG_0C:%x, SS_TIMEOUT_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & NU2115_CONV_SWITCHING_STAT_MASK)
			dev_err(chip->dev,"REG_0C:%x, CONV_ACTIVE_STAT\n", sf_reg[5]);
		if (sf_reg[5] & NU2115_PIN_DIAG_FALL_FLAG_MASK)
			dev_err(chip->dev,"REG_0C:%x, PIN_DIAG_FAIL_FLAG\n", sf_reg[5]);

		if (sf_reg[6] & NU2115_WD_TIMEOUT_FLAG_MASK)
			dev_err(chip->dev,"REG_0D:%x, WD_TIMEOUT_FLAG\n", sf_reg[6]);

		if (sf_reg[8] & NU2115_BAT_OVP_ALM_STAT_MASK)
			dev_err(chip->dev,"REG_0F:%x, BAT_OVP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & NU2115_BAT_OCP_ALM_STAT_MASK)
			dev_err(chip->dev,"REG_0F:%x, BAT_OCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & NU2115_BUS_OVP_ALM_STAT_MASK)
			dev_err(chip->dev,"REG_0F:%x, BUS_OVP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & NU2115_BUS_OCP_ALM_STAT_MASK)
			dev_err(chip->dev,"REG_0F:%x, BUS_OCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & NU2115_BAT_UCP_ALM_STAT_MASK)
			dev_err(chip->dev,"REG_0F:%x, BAT_UCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & NU2115_VBAT_INSERT_STAT_MASK)
			dev_err(chip->dev,"REG_0F:%x, VBAT_INSERT_STAT\n", sf_reg[8]);
		if (sf_reg[8] & NU2115_ADC_DONE_STAT_MASK)
			dev_err(chip->dev,"REG_0F:%x, ADC_DONE_STAT\n", sf_reg[8]);

		if (sf_reg[9] & NU2115_BAT_OVP_ALM_FLAG_MASK) {
			//if (chip->chg_dev)
			//	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_VBATOVP_ALARM);
			dev_err(chip->dev,"REG_10:%x, BAT_OVP_ALM_FLAG\n", sf_reg[9]);
		}
		if (sf_reg[9] & NU2115_BAT_OCP_ALM_FLAG_MASK)
			dev_err(chip->dev,"REG_10:%x, BAT_OCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & NU2115_BUS_OVP_ALM_FLAG_MASK) {
			//if (chip->chg_dev)
			//	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_VBUSOVP_ALARM);
			dev_err(chip->dev,"REG_10:%x, BUS_OVP_ALM_FLAG\n", sf_reg[9]);
		}
		if (sf_reg[9] & NU2115_BUS_OCP_ALM_FLAG_MASK)
			dev_err(chip->dev,"REG_10:%x, BUS_OCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & NU2115_BAT_UCP_ALM_FLAG_MASK)
			dev_err(chip->dev,"REG_10:%x, BAT_UCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & NU2115_VBAT_INSERT_FLAG_MASK)
			dev_err(chip->dev,"REG_10:%x, VBAT_INSERT_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & NU2115_ADC_DONE_FLAG_MASK)
			dev_err(chip->dev,"REG_10:%x, ADC_DONE_FLAG\n", sf_reg[9]);

		if (sf_reg[11] & NU2115_BAT_OVP_FLT_STAT_MASK)
			dev_err(chip->dev,"REG_12:%x, BAT_OVP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & NU2115_BAT_OCP_FLT_STAT_MASK)
			dev_err(chip->dev,"REG_12:%x, BAT_OCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & NU2115_BUS_OVP_FLT_STAT_MASK)
			dev_err(chip->dev,"REG_12:%x, BUS_OVP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & NU2115_BUS_OCP_FLT_STAT_MASK)
			dev_err(chip->dev,"REG_12:%x, BUS_OCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & NU2115_BUS_RCP_FLT_STAT_MASK)
			dev_err(chip->dev,"REG_12:%x, BUS_RCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & NU2115_TS_ALM_STAT_MASK)
			dev_err(chip->dev,"REG_12:%x, TS_ALM_STAT\n", sf_reg[11]);
		if (sf_reg[11] & NU2115_TS_FLT_STAT_MASK)
			dev_err(chip->dev,"REG_12:%x, TS_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & NU2115_TDIE_ALM_STAT_MASK)
			dev_err(chip->dev,"REG_12:%x, TDIE_ALM_STAT\n", sf_reg[11]);

		if (sf_reg[12] & NU2115_BAT_OVP_FLT_FLAG_MASK) {
			if (chip->chg_dev)
				charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_BAT_OVP);
			dev_err(chip->dev,"REG_13:%x, BAT_OVP_FLT_FLAG\n", sf_reg[12]);
		}
		if (sf_reg[12] & NU2115_BAT_OCP_FLT_FLAG_MASK) {
			if (chip->chg_dev)
				charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_IBATOCP);
			dev_err(chip->dev,"REG_13:%x, BAT_OCP_FLT_FLAG\n", sf_reg[12]);
		}
		if (sf_reg[12] & NU2115_BUS_OVP_FLT_FLAG_MASK) {
			if (chip->chg_dev)
				charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
			dev_err(chip->dev,"REG_13:%x, BUS_OVP_FLT_FLAG\n", sf_reg[12]);
		}
		if (sf_reg[12] & NU2115_BUS_OCP_FLT_FLAG_MASK) {
			if (chip->chg_dev)
				charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_IBUSOCP);
			dev_err(chip->dev,"REG_13:%x, BUS_OCP_FLT_FLAG\n", sf_reg[12]);
		}
		if (sf_reg[12] & NU2115_BUS_RCP_FLT_FLAG_MASK)
			dev_err(chip->dev,"REG_13:%x, BUS_RCP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & NU2115_TS_ALM_FLAG_MASK)
			dev_err(chip->dev,"REG_13:%x, TS_ALM_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & NU2115_TS_FLT_FLAG_MASK)
			dev_err(chip->dev,"REG_13:%x, TS_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & NU2115_TDIE_ALM_FLAG_MASK)
			dev_err(chip->dev,"REG_13:%x, TDIE_ALM_FLAG\n", sf_reg[12]);
	}

	ret = nu2115_i2c_read_block(chip, NU2115_REG_2E, 10, &sf_reg[13]);
	if (ret > 0)
	{
		if (sf_reg[14] & NU2115_VAC1PRESENT_STAT_MASK)
		    dev_err(chip->dev,"REG_2F:%x, VAC1PRESENT_STAT\n", sf_reg[14]);
		if (sf_reg[14] & NU2115_VAC1PRESENT_FLAG_MASK)
		    dev_err(chip->dev,"REG_2F:%x, VAC1PRESENT_FLAG\n", sf_reg[14]);
		if (sf_reg[14] & NU2115_VAC2PRESENT_STAT_MASK)
		    dev_err(chip->dev,"REG_2F:%x, VAC2PRESENT_STAT\n", sf_reg[14]);
		if (sf_reg[14] & NU2115_VAC2PRESENT_FLAG_MASK)
		    dev_err(chip->dev,"REG_2F:%x, VAC2PRESENT_FLAG\n", sf_reg[14]);

		if (sf_reg[15] & NU2115_ACRB1_STAT_MASK)
		    dev_err(chip->dev,"REG_30:%x, ACRB1_STAT\n", sf_reg[15]);
		if (sf_reg[15] & NU2115_ACRB1_FLAG_MASK)
		    dev_err(chip->dev,"REG_30:%x, ACRB1_FLAG\n", sf_reg[15]);
		if (sf_reg[15] & NU2115_ACRB2_STAT_MASK)
		    dev_err(chip->dev,"REG_30:%x, ACRB2_STAT\n", sf_reg[15]);
		if (sf_reg[15] & NU2115_ACRB2_FLAG_MASK)
		    dev_err(chip->dev,"REG_30:%x, ACRB2_FLAG\n", sf_reg[15]);

        if (sf_reg[17] & NU2115_PMID2VOUT_UVP_FLAG_MASK)
		    dev_err(chip->dev,"REG_32:%x, PMID2VOUT_UVP_FLAG\n", sf_reg[17]);

		if (sf_reg[17] & NU2115_PMID2VOUT_OVP_FLAG_MASK)
		    dev_err(chip->dev,"REG_32:%x, PMID2VOUT_OVP_FLAG\n", sf_reg[17]);

		if (sf_reg[19] & NU2115_POWER_NG_FLAG_MASK)
		    dev_err(chip->dev,"REG_34:%x, POWER_NG_FLAG\n", sf_reg[19]);

		if (sf_reg[21] & NU2115_VBUS_PRESENT_STAT_MASK)
		    dev_err(chip->dev,"REG_36:%x, VBUS_PRESENT_STAT\n", sf_reg[21]);
		if (sf_reg[21] & NU2115_VBUS_PRESENT_FLAG_MASK)
		    dev_err(chip->dev,"REG_36:%x, VBUS_PRESENT_FLAG\n", sf_reg[21]);
	}

}


static irqreturn_t nu2115_irq_handler(int irq, void *data)
{
	struct nu2115 *chip = data;

	dev_err(chip->dev, "nu2115_irq_handler do\n");
	mutex_lock(&chip->irq_lock);
	nu2115_check_status_flags(chip);
	mutex_unlock(&chip->irq_lock);

	return IRQ_HANDLED;
}

/* charger interface */
static int nu2115_enable_chg(struct charger_device *chg_dev, bool en)
{
	struct nu2115 *chip = charger_get_data(chg_dev);
	int ret;

    dev_err(chip->dev, "nu2115 enable: %d\n", en);
	if (!en) {
		ret = __nu2115_set_irqmask(chip, NU2115_REG_14, 0x7);
		if (ret < 0)
			dev_err(chip->dev, "failed to set irqmask\n");

		ret = __nu2115_enable_chg(chip, false);
		if (ret < 0) {
			dev_err(chip->dev, "failed to disable chg\n");
			return ret;
		}

		mutex_lock(&chip->adc_lock);

		ret = __nu2115_enable_adc(chip, false);
		if (ret < 0) {
			dev_err(chip->dev, "failed to disable adc\n");
			return ret;
		}

		mutex_unlock(&chip->adc_lock);

		goto out;
	}

	nu2115_check_status_flags(chip);

	ret = __nu2115_set_irqmask(chip, NU2115_REG_14, 0x7);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set irqmask\n");
		return ret;
	}

	ret = __nu2115_enable_adc(chip, true);
	if (ret < 0) {
		dev_err(chip->dev, "failed to enable adc\n");
		return ret;
	}
	if (en && (__nu2115_is_adc_enabled(chip) == true))
	{
	    __nu2115_enable_adc(chip, false);
	    ret = __nu2115_enable_adc(chip, true);
	}

	ret = __nu2115_enable_chg(chip, true);
	if (ret < 0) {
		dev_err(chip->dev, "failed to enable chg\n");
		return ret;
	}

	if (en && !__nu2115_is_chg_enabled(chip)) {
		dev_err(chip->dev, "chg not enabled\n");
		return -EIO;
	}

	dev_err(chip->dev, "nu2115 enable ok: %d\n", en);

out:
	if (chip->cfg.watchdog_dis)
		return 0;

	ret = __nu2115_enable_watchdog(chip, false);
	if (ret < 0) {
		dev_err(chip->dev, "failed to enable watchdog\n");
		return ret;
	}

	return 0;
}

static int nu2115_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
	struct nu2115 *chip = charger_get_data(chg_dev);

	*en = __nu2115_is_chg_enabled(chip);

	return 0;
}

//static int nu2115_set_chg_mode(struct charger_device *chg_dev, int mode)
//{
//	struct nu2115 *chip = charger_get_data(chg_dev);
//	int ret = 0;
//
//	if (mode)
//	{
//		ret = __nu2115_update_bits(chip, NU2115_REG_0E,
//				     NU2115_CHG_MODE_MASK,
//				     0x20);
//		dev_err(chip->dev, "enable bapass mode\n");
//	}
//	else
//	{
//		ret = __nu2115_update_bits(chip, NU2115_REG_0E,
//				     NU2115_CHG_MODE_MASK,
//				     0);
//		dev_err(chip->dev, "disable bapass mode\n");
//	}
//
//	return ret;
//}
//
//static int nu2115_get_chg_mode(struct charger_device *chg_dev, int *mode)
//{
//	struct nu2115 *chip = charger_get_data(chg_dev);
//
//	*mode = __nu2115_check_bits(chip, NU2115_REG_0E,
//				    NU2115_CHG_MODE_MASK);
//
//	return 0;
//}

static int nu2115_get_adc(struct charger_device *chg_dev,
			   enum adc_channel chan, int *min, int *max)
{
	struct nu2115 *chip = charger_get_data(chg_dev);
	int channel;
	int ret;

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		channel = NU2115_ADC_VBUS;
		break;
	case ADC_CHANNEL_VBAT:
		channel = NU2115_ADC_VBAT;
		break;
	case ADC_CHANNEL_IBUS:
		channel = NU2115_ADC_IBUS;
		break;
	case ADC_CHANNEL_IBAT:
		channel = NU2115_ADC_IBAT;
		break;
	case ADC_CHANNEL_TEMP_JC:
		channel = NU2115_ADC_TDIE;
		break;
	case ADC_CHANNEL_VOUT:
		channel = NU2115_ADC_VOUT;
		break;
	default:
		return -ENOTSUPP;
	}

	if (!min || !max)
		return -EINVAL;

	ret = __nu2115_get_adc(chip, channel, max);
    //dev_err(chip->dev, "%s------max=%d\n",__func__,*max);
	if (ret < 0)
		*max = 0;

	*min = *max;

	return ret;
}

static int nu2115_get_adc_accuracy(struct charger_device *chg_dev,
				    enum adc_channel chan, int *min, int *max)
{
	switch (chan) {
	case ADC_CHANNEL_VBUS:
		*min = 35000;
		*max = 35000;
		break;
	case ADC_CHANNEL_VBAT:
		*min = 20000;
		*max = 20000;
		break;
	case ADC_CHANNEL_IBUS:
		*min = 150000;
		*max = 150000;
		break;
	case ADC_CHANNEL_IBAT:
		*min = 200000;
		*max = 200000;
		break;
	case ADC_CHANNEL_TEMP_JC:
		*min = 4;
		*max = 4;
		break;
	case ADC_CHANNEL_VOUT:
		*min = 20000;
		*max = 20000;
		break;
	default:
		*min = 0;
		*max = 0;
		break;
	}

	return 0;
}

static int nu2115_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
	struct nu2115 *chip = charger_get_data(chg_dev);

	return __nu2115_set_bus_ovp(chip, uV);
}

static int nu2115_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
//	struct nu2115 *chip = charger_get_data(chg_dev);
//
//	/* uA will be 110% of target */
//	__nu2115_set_bus_ocp_alm(chip, uA / 110 * 100);
//
//	return __nu2115_set_bus_ocp(chip, uA);
	return 0;
}

static int nu2115_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{
	struct nu2115 *chip = charger_get_data(chg_dev);

	return __nu2115_set_bat_ovp(chip, uV);
}

static int nu2115_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	struct nu2115 *chip = charger_get_data(chg_dev);
	int ret;

	mutex_lock(&chip->ops_lock);

	ret = __nu2115_set_bat_ovp_alm(chip, uV);

	mutex_unlock(&chip->ops_lock);

	return ret;
}

static int nu2115_reset_vbatovp_alarm(struct charger_device *chg_dev)
{
	struct nu2115 *chip = charger_get_data(chg_dev);

	mutex_lock(&chip->ops_lock);

	__nu2115_set_bits(chip, NU2115_REG_01,
			NU2115_BAT_OVP_ALM_DIS_MASK);
	__nu2115_clr_bits(chip, NU2115_REG_01,
			NU2115_BAT_OVP_ALM_DIS_MASK);

	mutex_unlock(&chip->ops_lock);

	return 0;
}

static int nu2115_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	struct nu2115 *chip = charger_get_data(chg_dev);
	int ret;

	mutex_lock(&chip->ops_lock);

	ret = __nu2115_set_bus_ovp_alm(chip, uV);

	mutex_unlock(&chip->ops_lock);

	return ret;
}

#define VBUS_ERROR_LO		BIT(2)

static int nu2115_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
	struct nu2115 *chip = charger_get_data(chg_dev);
	int ret;
	u8 stat = 0;
	*err = false;
	return 0;

	ret = __nu2115_read(chip, NU2115_REG_35, &stat);
	if (!ret)
	{
		dev_err(chip->dev,"nu2115_is_vbuslowerr,NU2115_REG_0A: 0x%02X\n", stat);

		if (stat & VBUS_ERROR_LO)
			*err = true;
	}

	return 0;
}

static int nu2115_reset_vbusovp_alarm(struct charger_device *chg_dev)
{
	struct nu2115 *chip = charger_get_data(chg_dev);

	mutex_lock(&chip->ops_lock);

	__nu2115_set_bits(chip, NU2115_REG_08,
			NU2115_BUS_OVP_ALM_DIS_MASK);
	__nu2115_clr_bits(chip, NU2115_REG_08,
			NU2115_BUS_OVP_ALM_DIS_MASK);

	mutex_unlock(&chip->ops_lock);

	return 0;
}

static int nu2115_init_chip(struct charger_device *chg_dev)
{
	return 0;
}

static int nu2115_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{
//	struct nu2115 *chip = charger_get_data(chg_dev);
//
//	/* uA will be 110% of target */
//	__nu2115_set_bat_ocp_alm(chip, uA / 110 * 100);
//
//	return __nu2115_set_bat_ocp(chip, uA);
	return 0;
}

static int nu2115_get_ovpgate_state(struct nu2115 *chip)
{
	int acdrv1;

	acdrv1 = __nu2115_get_bits(chip, NU2115_REG_30,
			NU2115_EN_ACDRV1_MASK, NU2115_EN_ACDRV1_SHIFT);

	return acdrv1;
}

static int nu2115_get_wpcgate_state(struct nu2115 *chip)
{
	int acdrv2;

	acdrv2 = __nu2115_get_bits(chip, NU2115_REG_30,
			NU2115_EN_ACDRV2_MASK, NU2115_EN_ACDRV2_SHIFT);

	return acdrv2;
}

static int nu2115_get_mode_state(struct nu2115 *chip)
{
	int mode;

	mode = __nu2115_get_bits(chip, NU2115_REG_0E,
			NU2115_CHG_MODE_MASK, NU2115_CHG_MODE_SHIFT);
	return mode == 0 ? 2 : 1;
}

static int nu2115_get_cp_status(struct charger_device *chg_dev, u32 evt)
{
	int val = 0;
	struct nu2115 *chip = charger_get_data(chg_dev);

	switch(evt){
	case CP_DEV_REVISION:
		val = NU2115_DEVID;
		break;
	case CP_DEV_OVPGATE:
		val = nu2115_get_ovpgate_state(chip);
		break;
	case CP_DEV_WPCGATE:
		val = nu2115_get_wpcgate_state(chip);
		break;
	case CP_DEV_WORKMODE:
		val = nu2115_get_mode_state(chip);
		break;
	default:
		break;
	}
	return val;
}

static int nu2115_dump_registers(struct charger_device *chg_dev)
{
	struct nu2115 *chip = charger_get_data(chg_dev);

	__nu2115_dump_register(chip);
	return 0;
}

static int nu2115_enable_vac_otgovp(struct charger_device *chg_dev, bool en)
{
	struct nu2115 *chip = charger_get_data(chg_dev);
	int ret = 0;

	dev_err(chip->dev, "%s\n", __func__);
	if (en) {
		ret += __nu2115_update_bits(chip, NU2115_REG_2F, NU2115_EN_OTG_MASK,
								NU2115_OTG_ENABLE << NU2115_EN_OTG_SHIFT);
		ret += __nu2115_update_bits(chip, NU2115_REG_2F, NU2115_DIS_ACDRV_MASK,
								NU2115_DIS_ACDRV_DISABLE << NU2115_DIS_ACDRV_SHIFT);
		ret += __nu2115_update_bits(chip, NU2115_REG_2F, NU2115_DIS_ACDRV_MASK,
								NU2115_DIS_ACDRV_ENABLE << NU2115_DIS_ACDRV_SHIFT);
		ret += __nu2115_update_bits(chip, NU2115_REG_30, NU2115_EN_ACDRV1_MASK,
								NU2115_ACDRV1_ENABLE << NU2115_EN_ACDRV1_SHIFT);
		if (ret < 0)
			dev_err(chip->dev, "nu2115 enable acdrv1 fail!\n");
	} else {
		ret += __nu2115_update_bits(chip, NU2115_REG_30, NU2115_EN_ACDRV1_MASK,
								NU2115_ACDRV1_DISABLE << NU2115_EN_ACDRV1_SHIFT);
		ret += __nu2115_update_bits(chip, NU2115_REG_2F, NU2115_EN_OTG_MASK,
								NU2115_OTG_DISABLE << NU2115_EN_OTG_SHIFT);
		if (ret < 0)
			dev_err(chip->dev, "nu2115 disable acdrv1 fail!\n");
	}

	return ret;
}

static const struct charger_ops nu2115_chg_ops = {
	.enable = nu2115_enable_chg,
	.is_enabled = nu2115_is_chg_enabled,
	.get_adc = nu2115_get_adc,
	.set_vbusovp = nu2115_set_vbusovp,
	.set_ibusocp = nu2115_set_ibusocp,
	.set_vbatovp = nu2115_set_vbatovp,
	.set_ibatocp = nu2115_set_ibatocp,
	.init_chip = nu2115_init_chip,
	.set_vbatovp_alarm = nu2115_set_vbatovp_alarm,
	.reset_vbatovp_alarm = nu2115_reset_vbatovp_alarm,
	.set_vbusovp_alarm = nu2115_set_vbusovp_alarm,
	.reset_vbusovp_alarm = nu2115_reset_vbusovp_alarm,
	.is_vbuslowerr = nu2115_is_vbuslowerr,
	.get_adc_accuracy = nu2115_get_adc_accuracy,
//	.set_chg_mode = nu2115_set_chg_mode,
//	.get_chg_mode = nu2115_get_chg_mode,
	.dump_registers = nu2115_dump_registers,
	.get_cp_status	= nu2115_get_cp_status,
	.enable_vac_otgovp = nu2115_enable_vac_otgovp,
};

/* debugfs interface */
static int debugfs_get_data(void *data, u64 *val)
{
	struct nu2115 *chip = data;
	int ret;
	u8 temp;

	ret = __nu2115_read(chip, chip->debug_addr, &temp);
	if (ret)
		return -EAGAIN;

	*val = temp;

	return 0;
}

static int debugfs_set_data(void *data, u64 val)
{
	struct nu2115 *chip = data;
	int ret;
	u8 temp;

	temp = (u8)val;
	ret = __nu2115_write(chip, chip->debug_addr, temp);
	if (ret)
		return -EAGAIN;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(data_debugfs_ops,
	debugfs_get_data, debugfs_set_data, "0x%02llx\n");

static int dump_debugfs_show(struct seq_file *m, void *start)
{
	struct nu2115 *chip = m->private;
	u8 reg, val;
	int ret;

	for (reg = NU2115_REG_00; reg <= NU2115_REG_36; reg++) {
		ret = __nu2115_read(chip, reg, &val);
		if (ret) {
			seq_printf(m, "0x%02x = error\n", reg);
			continue;
		}

		seq_printf(m, "0x%02x = 0x%02x\n", reg, val);
	}

	return 0;
}

static int dump_debugfs_open(struct inode *inode, struct file *file)
{
	struct nu2115 *chip = inode->i_private;

	return single_open(file, dump_debugfs_show, chip);
}

static const struct file_operations dump_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= dump_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int create_debugfs_entries(struct nu2115 *chip)
{
	struct dentry *ent;

	chip->debugfs = debugfs_create_dir(chip->chg_prop.alias_name, NULL);
	if (!chip->debugfs) {
		dev_err(chip->dev, "failed to create debugfs\n");
		return -ENODEV;
	}

	debugfs_create_x8("addr", S_IFREG | S_IWUSR | S_IRUGO,
		chip->debugfs, &chip->debug_addr);

	ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
		chip->debugfs, chip, &data_debugfs_ops);
	if (!ent)
		dev_err(chip->dev, "failed to create data debugfs\n");

	ent = debugfs_create_file("dump", S_IFREG | S_IRUGO,
		chip->debugfs, chip, &dump_debugfs_ops);
	if (!ent)
		dev_err(chip->dev, "failed to create dump debugfs\n");

	return 0;
}

static int nu2115_charger_device_register(struct nu2115 *chip)
{
	chip->chg_prop.alias_name = "nu2115_standalone";
	chip->chg_dev = charger_device_register("primary_dvchg",
			chip->dev, chip, &nu2115_chg_ops, &chip->chg_prop);
	if (!chip->chg_dev)
		return -EINVAL;

	return 0;
}

static int nu2115_irq_init(struct nu2115 *chip)
{
	int ret = 0;

    if (gpio_is_valid(chip->irq_gpio)) {
        ret = gpio_request_one(chip->irq_gpio, GPIOF_DIR_IN,"nu2115_irq");
        if (ret) {
            dev_err(chip->dev,"failed to request nu2115_irq\n");
            return -EINVAL;
        }
        chip->irq = gpio_to_irq(chip->irq_gpio);
        if (chip->irq < 0) {
            dev_err(chip->dev,"nu2115 failed to gpio_to_irq\n");
            return -EINVAL;
        }
    } else {
        dev_err(chip->dev,"nu2115 irq gpio not provided\n");
        return -EINVAL;
    }

	if (chip->irq) {
		ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
						nu2115_irq_handler,
						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"nu2115_irq", chip);
		if (ret) {
			dev_err(chip->dev, "failed to request irq %d\n", chip->irq);
			return ret;
		}
		enable_irq_wake(chip->irq);
	}

	return ret;
}

static int nu2115_hw_init(struct nu2115 *chip)
{
	int ret = 0;

	__nu2115_set_ss_timeout(chip, chip->cfg.ss_timeout);

	ret = __nu2115_set_bat_ucp_alm(chip, 50000);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set bat_ucp_alm\n");
		return ret;
	}

	ret = __nu2115_set_ac1_ovp(chip, chip->cfg.ac_ovp);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set ac_ovp\n");
		return ret;
	}
	ret = __nu2115_set_ac2_ovp(chip, chip->cfg.ac_ovp);

	ret = __nu2115_set_bat_ovp(chip, chip->cfg.bat_ovp);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set bat_ovp\n");
		return ret;
	}

	ret = __nu2115_set_bat_ovp_alm(chip, chip->cfg.bat_ovp_alm);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set bat_ovp_alm\n");
		return ret;
	}

	ret = __nu2115_set_bat_ocp(chip, chip->cfg.bat_ocp);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set bat_ocp\n");
		return ret;
	}

	ret = __nu2115_set_bat_ocp_alm(chip, chip->cfg.bat_ocp_alm);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set bat_ocp_alm\n");
		return ret;
	}

	ret = __nu2115_set_bus_ovp(chip, chip->cfg.bus_ovp);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set bus_ovp\n");
		return ret;
	}

	ret = __nu2115_set_bus_ovp_alm(chip, chip->cfg.bus_ovp_alm);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set bus_ovp_alm\n");
		return ret;
	}

	ret = __nu2115_set_bus_ocp(chip, chip->cfg.bus_ocp);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set bus_ocp\n");
		return ret;
	}

	ret = __nu2115_set_bus_ocp_alm(chip, chip->cfg.bus_ocp_alm);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set bus_ocp_alm\n");
		return ret;
	}

	//ret = __nu2115_set_fsw(chip, 500000);
	/*
	ret = __nu2115_set_fsw(chip, 400000);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set fsw\n");
		return ret;
	}
	*/

	ret = __nu2115_set_watchdog(chip, 500000);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set watchdog\n");
		return ret;
	}

	ret = __nu2115_enable_watchdog(chip, false);
	if (ret < 0) {
		dev_err(chip->dev, "failed to disable watchdog\n");
		return ret;
	}

	ret = __nu2115_set_adc_fn_dis(chip, 0x06);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set adc channel\n");
		return ret;
	}

	ret = __nu2115_set_ibat_sns_res(chip, 2);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set ibat_sns_res\n");
		return ret;
	}

	ret = __nu2115_update_bits(chip, NU2115_REG_0E,
			NU2115_TDIE_DIS_MASK,
			(chip->cfg.tdie_dis) << NU2115_TDIE_DIS_SHIFT);
	if (ret < 0) {
		dev_err(chip->dev, "failed to dis tdie!\n");
		return ret;
	}

	ret = __nu2115_update_bits(chip, NU2115_REG_0E,
			NU2115_TSBAT_DIS_MASK,
			(chip->cfg.tsbat_dis) << NU2115_TSBAT_DIS_SHIFT);
	if (ret < 0) {
		dev_err(chip->dev, "failed to dis tsbat!\n");
		return ret;
	}

	ret = __nu2115_update_bits(chip, NU2115_REG_0E,
			NU2115_TSBUS_DIS_MASK,
			(chip->cfg.tsbus_dis) << NU2115_TSBUS_DIS_SHIFT);
	if (ret < 0) {
		dev_err(chip->dev, "failed to dis tsbus!\n");
		return ret;
	}

	ret = __nu2115_update_bits(chip, NU2115_REG_32,
			NU2115_PMID2VOUT_UVP_MASK,
			(chip->cfg.pmid2out_uvp) << NU2115_PMID2VOUT_UVP_SHIFT);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set pmid2out_uvp!\n");
		return ret;
	}

	ret = __nu2115_update_bits(chip, NU2115_REG_32,
			NU2115_PMID2VOUT_OVP_MASK,
			(chip->cfg.pmid2out_ovp) << NU2115_PMID2VOUT_OVP_SHIFT);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set pmid2out_ovp!\n");
		return ret;
	}

	ret = __nu2115_set_irqmask(chip, NU2115_REG_14, 0x7);
	if (ret < 0) {
		dev_err(chip->dev, "failed to set mask\n");
		return ret;
	}

	/* clear irqs */
	nu2115_check_status_flags(chip);
	__nu2115_dump_register(chip);

	return ret;
}

static int nu2115_parse_dt(struct nu2115 *chip)
{
	struct device_node *np = chip->dev->of_node;
	int ret;

	if (!np)
		return -ENODEV;

	ret = of_property_read_u32(np, "ac_ovp", &chip->cfg.ac_ovp);
	if (ret)
		chip->cfg.ac_ovp = 12000000;
	ret = of_property_read_u32(np, "bat_ovp", &chip->cfg.bat_ovp);
	if (ret)
		chip->cfg.bat_ovp = 4700000;
	ret = of_property_read_u32(np, "bat_ovp_alm", &chip->cfg.bat_ovp_alm);
	if (ret)
		chip->cfg.bat_ovp_alm = 4600000;
	ret = of_property_read_u32(np, "bat_ocp", &chip->cfg.bat_ocp);
	if (ret)
		chip->cfg.bat_ocp = 8000000;
	ret = of_property_read_u32(np, "bat_ocp_alm", &chip->cfg.bat_ocp_alm);
	if (ret)
		chip->cfg.bat_ocp_alm = 7800000;
	ret = of_property_read_u32(np, "bat_ucp_alm", &chip->cfg.bat_ucp_alm);
	if (ret)
		chip->cfg.bat_ucp_alm = 50000;
	ret = of_property_read_u32(np, "bus_ovp", &chip->cfg.bus_ovp);
	if (ret)
		chip->cfg.bat_ovp = 11000000;
	ret = of_property_read_u32(np, "bus_ovp_alm", &chip->cfg.bus_ovp_alm);
	if (ret)
		chip->cfg.bus_ovp_alm = 10800000;
	ret = of_property_read_u32(np, "bus_ocp", &chip->cfg.bus_ocp);
	if (ret)
		chip->cfg.bat_ocp = 4000000;
	ret = of_property_read_u32(np, "bus_ocp_alm", &chip->cfg.bus_ocp_alm);
	if (ret)
		chip->cfg.bus_ocp_alm = 3800000;
	ret = of_property_read_u32(np, "ss_timeout", &chip->cfg.ss_timeout);
	if (ret)
		chip->cfg.ss_timeout = 1500000;
	ret = of_property_read_u32(np, "fsw_set", &chip->cfg.fsw_set);
	if (ret)
		chip->cfg.fsw_set = 500000;
	ret = of_property_read_u32(np, "ibat_sns_res", &chip->cfg.ibat_sns_res);
	if (ret)
		chip->cfg.ibat_sns_res = 2;
	ret = of_property_read_u32(np, "tdie_dis", &chip->cfg.tdie_dis);
	if (ret)
		chip->cfg.tdie_dis = 1;
	ret = of_property_read_u32(np, "tsbus_dis", &chip->cfg.tsbus_dis);
	if (ret)
		chip->cfg.tsbus_dis = 1;
	ret = of_property_read_u32(np, "tsbat_dis", &chip->cfg.tsbat_dis);
	if (ret)
		chip->cfg.tsbat_dis = 1;
	ret = of_property_read_u32(np, "pmid2out_uvp", &chip->cfg.pmid2out_uvp);
	if (ret)
		chip->cfg.pmid2out_uvp = 3;
	ret = of_property_read_u32(np, "pmid2out_ovp", &chip->cfg.pmid2out_ovp);
	if (ret)
		chip->cfg.pmid2out_ovp = 3;
	chip->cfg.watchdog_dis = of_property_read_bool(np, "watchdog_dis");
	ret = of_property_read_u32(np, "watchdog", &chip->cfg.watchdog);
	if (ret)
		chip->cfg.watchdog = 30000000;
	if (!chip->cfg.watchdog)
		chip->cfg.watchdog_dis = true;

    chip->irq_gpio = of_get_named_gpio(np, "nu2115,intr", 0);
    if (!gpio_is_valid(chip->irq_gpio)) {
        dev_err(chip->dev,"%s, fail to valid gpio : %d\n",__func__, chip->irq_gpio);
        return -EINVAL;
    }

	return 0;
}

static void determine_initial_status(struct nu2115 *chip)
{
	if (chip->client->irq)
		nu2115_irq_handler(chip->client->irq, chip);
}

static bool nu2115_detect_device(struct nu2115 *chip)
{
	int ret;
	u8 val;

	ret = __nu2115_read(chip, NU2115_REG_31, &val);
	dev_err(chip->dev, "nu2115_detect_device---val=0x%x\n",val);
	if (val == NU2115_DEVID)
		return true;
	else
		return false;
}

static int nu2115_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct nu2115 *chip;
	int ret = 0;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}
	i2c_set_clientdata(client, chip);
	chip->client = client;
	//chip->client->addr = 0x66;
	chip->dev = &client->dev;
	mutex_init(&chip->rw_lock);
	mutex_init(&chip->ops_lock);
	mutex_init(&chip->adc_lock);
	mutex_init(&chip->irq_lock);

	if(!nu2115_detect_device(chip)){
		dev_err(&client->dev, "nu2115_detect_device failed.\n");
		return ret;
	}

	ret = nu2115_parse_dt(chip);
	if (ret){
		dev_err(&client->dev, "nu2115_parse_dt failed.\n");
		return ret;
	}

	ret = nu2115_hw_init(chip);
	if (ret){
		dev_err(&client->dev, "nu2115_hw_init failed.\n");
		return ret;
	}
	ret = nu2115_irq_init(chip);
	if (ret){
		dev_err(&client->dev, "nu2115_irq_init failed.\n");
		return ret;
	}

    determine_initial_status(chip);

	ret = nu2115_charger_device_register(chip);
	if (ret){
		dev_err(&client->dev, "nu2115_charger_device_register failed.\n");
		return ret;
	}

	create_debugfs_entries(chip);
	/* update cp chg hw info  */
	memset(slave_charger_info_3, 0, sizeof(slave_charger_info_3));
	memcpy(slave_charger_info_3, NU2115_NAME,
		strlen(NU2115_NAME) > (sizeof(slave_charger_info_3) - 1) ?
		(sizeof(slave_charger_info_3) - 1) : strlen(NU2115_NAME));
	dev_err(&client->dev, "nu2115_charger probe OK.\n");

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int nu2115_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nu2115 *chip = i2c_get_clientdata(client);

	if (device_may_wakeup(dev))
		disable_irq_wake(chip->irq);
	enable_irq(chip->irq);
	__nu2115_enable_adc(chip, true);
	dev_info(chip->dev, "Resume successfully!");

	return 0;
}

static int nu2115_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nu2115 *chip = i2c_get_clientdata(client);

	if (device_may_wakeup(dev))
		enable_irq_wake(chip->irq);
	disable_irq(chip->irq);
	__nu2115_enable_adc(chip, false);
	dev_info(chip->dev, "Suspend successfully!");

	return 0;
}

static const struct dev_pm_ops nu2115_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(nu2115_suspend, nu2115_resume)
};
#endif

static void nu2115_remove(struct i2c_client *client)
{
	struct nu2115 *chip = i2c_get_clientdata(client);

	charger_device_unregister(chip->chg_dev);
}

static void nu2115_shutdown(struct i2c_client *client)
{
	struct nu2115 *chip = i2c_get_clientdata(client);

	if (!chip)
		return;

	__nu2115_enable_adc(chip, false);
}

static const struct of_device_id nu2115_of_match[] = {
	{ .compatible = "cp,nu2115", },
	{ },
};

static const struct i2c_device_id nu2115_i2c_id[] = {
	{ .name = "nu2115", },
	{ },
};

static struct i2c_driver nu2115_driver = {
	.probe = nu2115_probe,
	.remove = nu2115_remove,
	.shutdown   = nu2115_shutdown,
	.driver = {
		.name = "nu2115",
		.of_match_table = nu2115_of_match,
#ifdef CONFIG_PM_SLEEP
		.pm = &nu2115_pm,
#endif
	},
	.id_table = nu2115_i2c_id,
};
module_i2c_driver(nu2115_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("bing");
MODULE_DESCRIPTION("NuVolta NU2115 Switched Cap Fast Charger");
MODULE_VERSION("1.0");
