// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/version.h>
#include <linux/pm_wakeup.h>
#include <linux/sched/clock.h>

#include "inc/tcpci.h"
#include "inc/mt6370.h"

#if IS_ENABLED(CONFIG_RT_REGMAP)
#include "inc/rt-regmap.h"
#endif /* CONFIG_RT_REGMAP */

#define MT6370_DRV_VERSION	"2.0.8_MTK"

#define MT6370_IRQ_WAKE_TIME	(500) /* ms */

struct mt6370_chip {
	struct i2c_client *client;
	struct device *dev;
#if IS_ENABLED(CONFIG_RT_REGMAP)
	struct rt_regmap_device *m_dev;
#endif /* CONFIG_RT_REGMAP */
	struct tcpc_desc *tcpc_desc;
	struct tcpc_device *tcpc;

	int irq_gpio;
	int irq;
	int chip_id;

	struct mutex irq_lock;
	bool is_suspended;
	bool irq_while_suspended;
};

#if IS_ENABLED(CONFIG_RT_REGMAP)
RT_REG_DECL(TCPC_V10_REG_VID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_DID, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TYPEC_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PD_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_PDIF_REV, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT_MASK, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_STATUS_MASK, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_STATUS_MASK, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TCPC_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_ROLE_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_CC_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_COMMAND, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_MSG_HDR_INFO, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DETECT, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_RX_BYTE_CNT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_BUF_FRAME_TYPE, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_HDR, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DATA, 28, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TRANSMIT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_BYTE_CNT, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TX_HDR, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(TCPC_V10_REG_TX_DATA, 28, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_PHY_CTRL1, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_PHY_CTRL2, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_PHY_CTRL3, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_CLK_CTRL2, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_CLK_CTRL3, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_PRL_FSM_RESET, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_BMC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_BMCIO_RXDZSEL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_MT_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_MT_INT, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_MT_MASK, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_BMCIO_RXDZEN, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_IDLE_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_I2CRST_CTRL, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_SWRESET, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_TTCPC_FILTER, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_DRP_TOGGLE_CYCLE, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_DRP_DUTY_CTRL, 2, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_PHY_CTRL11, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6370_REG_PHY_CTRL12, 1, RT_NORMAL_WR_ONCE, {});

static const rt_register_map_t mt6370_chip_regmap[] = {
	RT_REG(TCPC_V10_REG_VID),
	RT_REG(TCPC_V10_REG_PID),
	RT_REG(TCPC_V10_REG_DID),
	RT_REG(TCPC_V10_REG_TYPEC_REV),
	RT_REG(TCPC_V10_REG_PD_REV),
	RT_REG(TCPC_V10_REG_PDIF_REV),
	RT_REG(TCPC_V10_REG_ALERT),
	RT_REG(TCPC_V10_REG_ALERT_MASK),
	RT_REG(TCPC_V10_REG_POWER_STATUS_MASK),
	RT_REG(TCPC_V10_REG_FAULT_STATUS_MASK),
	RT_REG(TCPC_V10_REG_TCPC_CTRL),
	RT_REG(TCPC_V10_REG_ROLE_CTRL),
	RT_REG(TCPC_V10_REG_FAULT_CTRL),
	RT_REG(TCPC_V10_REG_POWER_CTRL),
	RT_REG(TCPC_V10_REG_CC_STATUS),
	RT_REG(TCPC_V10_REG_POWER_STATUS),
	RT_REG(TCPC_V10_REG_FAULT_STATUS),
	RT_REG(TCPC_V10_REG_COMMAND),
	RT_REG(TCPC_V10_REG_MSG_HDR_INFO),
	RT_REG(TCPC_V10_REG_RX_DETECT),
	RT_REG(TCPC_V10_REG_RX_BYTE_CNT),
	RT_REG(TCPC_V10_REG_RX_BUF_FRAME_TYPE),
	RT_REG(TCPC_V10_REG_RX_HDR),
	RT_REG(TCPC_V10_REG_RX_DATA),
	RT_REG(TCPC_V10_REG_TRANSMIT),
	RT_REG(TCPC_V10_REG_TX_BYTE_CNT),
	RT_REG(TCPC_V10_REG_TX_HDR),
	RT_REG(TCPC_V10_REG_TX_DATA),
	RT_REG(MT6370_REG_PHY_CTRL1),
	RT_REG(MT6370_REG_PHY_CTRL2),
	RT_REG(MT6370_REG_PHY_CTRL3),
	RT_REG(MT6370_REG_CLK_CTRL2),
	RT_REG(MT6370_REG_CLK_CTRL3),
	RT_REG(MT6370_REG_PRL_FSM_RESET),
	RT_REG(MT6370_REG_BMC_CTRL),
	RT_REG(MT6370_REG_BMCIO_RXDZSEL),
	RT_REG(MT6370_REG_MT_STATUS),
	RT_REG(MT6370_REG_MT_INT),
	RT_REG(MT6370_REG_MT_MASK),
	RT_REG(MT6370_REG_BMCIO_RXDZEN),
	RT_REG(MT6370_REG_IDLE_CTRL),
	RT_REG(MT6370_REG_I2CRST_CTRL),
	RT_REG(MT6370_REG_SWRESET),
	RT_REG(MT6370_REG_TTCPC_FILTER),
	RT_REG(MT6370_REG_DRP_TOGGLE_CYCLE),
	RT_REG(MT6370_REG_DRP_DUTY_CTRL),
	RT_REG(MT6370_REG_PHY_CTRL11),
	RT_REG(MT6370_REG_PHY_CTRL12),
};
#define MT6370_CHIP_REGMAP_SIZE ARRAY_SIZE(mt6370_chip_regmap)

#endif /* CONFIG_RT_REGMAP */

static int mt6370_read_device(void *client, u32 reg, int len, void *dst)
{
	struct i2c_client *i2c = client;
	int ret = 0, count = 5;

	while (1) {
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
		if (ret < 0 && count > 1)
			count--;
		else
			break;
		udelay(100);
	}
	return ret;
}

static int mt6370_write_device(void *client, u32 reg, int len, const void *src)
{
	struct i2c_client *i2c = client;
	int ret = 0, count = 5;

	while (1) {
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, len, src);
		if (ret < 0 && count > 1)
			count--;
		else
			break;
		udelay(100);
	}
	return ret;
}

static int mt6370_reg_read(struct i2c_client *i2c, u8 reg)
{
	struct mt6370_chip *chip = i2c_get_clientdata(i2c);
	u8 val = 0;
	int ret = 0;

#if IS_ENABLED(CONFIG_RT_REGMAP)
	ret = rt_regmap_block_read(chip->m_dev, reg, 1, &val);
#else
	ret = mt6370_read_device(chip->client, reg, 1, &val);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0) {
		dev_err(chip->dev, "mt6370 reg read fail\n");
		return ret;
	}
	return val;
}

static int mt6370_reg_write(struct i2c_client *i2c, u8 reg, const u8 data)
{
	struct mt6370_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;

#if IS_ENABLED(CONFIG_RT_REGMAP)
	ret = rt_regmap_block_write(chip->m_dev, reg, 1, &data);
#else
	ret = mt6370_write_device(chip->client, reg, 1, &data);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "mt6370 reg write fail\n");
	return ret;
}

static int mt6370_block_read(struct i2c_client *i2c,
			u8 reg, int len, void *dst)
{
	struct mt6370_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#if IS_ENABLED(CONFIG_RT_REGMAP)
	ret = rt_regmap_block_read(chip->m_dev, reg, len, dst);
#else
	ret = mt6370_read_device(chip->client, reg, len, dst);
#endif /* #if IS_ENABLED(CONFIG_RT_REGMAP) */
	if (ret < 0)
		dev_err(chip->dev, "mt6370 block read fail\n");
	return ret;
}

static int mt6370_block_write(struct i2c_client *i2c,
			u8 reg, int len, const void *src)
{
	struct mt6370_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#if IS_ENABLED(CONFIG_RT_REGMAP)
	ret = rt_regmap_block_write(chip->m_dev, reg, len, src);
#else
	ret = mt6370_write_device(chip->client, reg, len, src);
#endif /* #if IS_ENABLED(CONFIG_RT_REGMAP) */
	if (ret < 0)
		dev_err(chip->dev, "mt6370 block write fail\n");
	return ret;
}

static int32_t mt6370_write_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t data)
{
	int ret;

	/* don't need swap */
	ret = mt6370_block_write(client, reg_addr, 2, (uint8_t *)&data);
	return ret;
}

static int32_t mt6370_read_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t *data)
{
	int ret;

	/* don't need swap */
	ret = mt6370_block_read(client, reg_addr, 2, (uint8_t *)data);
	return ret;
}

static inline int mt6370_i2c_write8(
	struct tcpc_device *tcpc, u8 reg, const u8 data)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);

	return mt6370_reg_write(chip->client, reg, data);
}

static inline int mt6370_i2c_write16(
		struct tcpc_device *tcpc, u8 reg, const u16 data)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);

	return mt6370_write_word(chip->client, reg, data);
}

static inline int mt6370_i2c_read8(struct tcpc_device *tcpc, u8 reg)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);

	return mt6370_reg_read(chip->client, reg);
}

static inline int mt6370_i2c_read16(
	struct tcpc_device *tcpc, u8 reg)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);
	u16 data;
	int ret;

	ret = mt6370_read_word(chip->client, reg, &data);
	if (ret < 0)
		return ret;
	return data;
}

#if IS_ENABLED(CONFIG_RT_REGMAP)
static struct rt_regmap_fops mt6370_regmap_fops = {
	.read_device = mt6370_read_device,
	.write_device = mt6370_write_device,
};
#endif /* CONFIG_RT_REGMAP */

static int mt6370_regmap_init(struct mt6370_chip *chip)
{
#if IS_ENABLED(CONFIG_RT_REGMAP)
	struct rt_regmap_properties *props;
	char name[32];
	int len;
	int ret;

	props = devm_kzalloc(chip->dev, sizeof(*props), GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	props->register_num = MT6370_CHIP_REGMAP_SIZE;
	props->rm = mt6370_chip_regmap;

	props->rt_regmap_mode = RT_MULTI_BYTE |
				RT_IO_PASS_THROUGH | RT_DBG_SPECIAL;
	ret = snprintf(name, sizeof(name), "mt6370-%02x",
		chip->client->addr);
	if (ret < 0 || ret >= sizeof(name)) {
		dev_info(chip->dev, "%s-%d, snprintf fail\n",
			__func__, __LINE__);
	}

	len = strlen(name);
	props->name = kzalloc(len+1, GFP_KERNEL);
	props->aliases = kzalloc(len+1, GFP_KERNEL);

	if ((!props->name) || (!props->aliases))
		return -ENOMEM;

	strlcpy((char *)props->name, name, len+1);
	strlcpy((char *)props->aliases, name, len+1);
	props->io_log_en = 0;

	chip->m_dev = rt_regmap_device_register(props,
			&mt6370_regmap_fops, chip->dev, chip->client, chip);
	if (!chip->m_dev) {
		dev_err(chip->dev, "mt6370 chip rt_regmap register fail\n");
		return -EINVAL;
	}
#endif
	return 0;
}

static int mt6370_regmap_deinit(struct mt6370_chip *chip)
{
#if IS_ENABLED(CONFIG_RT_REGMAP)
	rt_regmap_device_unregister(chip->m_dev);
#endif
	return 0;
}

static inline int mt6370_software_reset(struct tcpc_device *tcpc)
{
	int ret = mt6370_i2c_write8(tcpc, MT6370_REG_SWRESET, 1);
#if IS_ENABLED(CONFIG_RT_REGMAP)
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_RT_REGMAP */

	if (ret < 0)
		return ret;
#if IS_ENABLED(CONFIG_RT_REGMAP)
	rt_regmap_cache_reload(chip->m_dev);
#endif /* CONFIG_RT_REGMAP */
	usleep_range(1000, 2000);
	return 0;
}

static inline int mt6370_command(struct tcpc_device *tcpc, uint8_t cmd)
{
	return mt6370_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, cmd);
}

static int mt6370_init_alert_mask(struct tcpc_device *tcpc)
{
	uint16_t mask;
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);

	mask = TCPC_V10_REG_ALERT_CC_STATUS | TCPC_V10_REG_ALERT_POWER_STATUS;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	/* Need to handle RX overflow */
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS | TCPC_V10_REG_ALERT_TX_DISCARDED
			| TCPC_V10_REG_ALERT_TX_FAILED
			| TCPC_V10_REG_ALERT_RX_HARD_RST
			| TCPC_V10_REG_ALERT_RX_STATUS
			| TCPC_V10_REG_RX_OVERFLOW;
#endif

	mask |= TCPC_REG_ALERT_FAULT;

	return mt6370_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, mask);
}

static int mt6370_init_power_status_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask = TCPC_V10_REG_POWER_STATUS_VBUS_PRES;

	return mt6370_i2c_write8(tcpc,
			TCPC_V10_REG_POWER_STATUS_MASK, mask);
}

static int mt6370_init_fault_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask =
		TCPC_V10_REG_FAULT_STATUS_VCONN_OV |
		TCPC_V10_REG_FAULT_STATUS_VCONN_OC;

	return mt6370_i2c_write8(tcpc,
			TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

static int mt6370_init_mt_mask(struct tcpc_device *tcpc)
{
	uint8_t mt_mask = MT6370_REG_M_WAKEUP | MT6370_REG_M_VBUS_80;

	return mt6370_i2c_write8(tcpc, MT6370_REG_MT_MASK, mt_mask);
}

static irqreturn_t mt6370_intr_handler(int irq, void *data)
{
	struct mt6370_chip *chip = data;

	mutex_lock(&chip->irq_lock);
	if (chip->is_suspended) {
		dev_notice(chip->dev, "%s irq while suspended\n", __func__);
		chip->irq_while_suspended = true;
		disable_irq_nosync(chip->irq);
		mutex_unlock(&chip->irq_lock);
		return IRQ_NONE;
	}
	mutex_unlock(&chip->irq_lock);

	pm_wakeup_event(chip->dev, MT6370_IRQ_WAKE_TIME);

	tcpci_lock_typec(chip->tcpc);
	tcpci_alert(chip->tcpc);
	tcpci_unlock_typec(chip->tcpc);

	return IRQ_HANDLED;
}

static int mt6370_init_alert(struct tcpc_device *tcpc)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);
	int ret = 0;
	char *name = NULL;

	/* Clear Alert Mask & Status */
	mt6370_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, 0);
	mt6370_write_word(chip->client, TCPC_V10_REG_ALERT, 0xffff);

	name = devm_kasprintf(chip->dev, GFP_KERNEL, "%s-IRQ",
			      chip->tcpc_desc->name);
	if (!name)
		return -ENOMEM;

	dev_info(chip->dev, "%s name = %s, gpio = %d\n",
			    __func__, chip->tcpc_desc->name, chip->irq_gpio);

	ret = devm_gpio_request(chip->dev, chip->irq_gpio, name);
	if (ret < 0) {
		dev_notice(chip->dev, "%s request GPIO fail(%d)\n",
				      __func__, ret);
		return ret;
	}

	ret = gpio_direction_input(chip->irq_gpio);
	if (ret < 0) {
		dev_notice(chip->dev, "%s set GPIO fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = gpio_to_irq(chip->irq_gpio);
	if (ret < 0) {
		dev_notice(chip->dev, "%s gpio to irq fail(%d)",
				      __func__, ret);
		return ret;
	}
	chip->irq = ret;

	dev_info(chip->dev, "%s IRQ number = %d\n", __func__, chip->irq);

	device_init_wakeup(chip->dev, true);
	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					mt6370_intr_handler,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					name, chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s request irq fail(%d)\n",
				      __func__, ret);
		return ret;
	}
	enable_irq_wake(chip->irq);

	return 0;
}

int mt6370_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask)
{
	int ret;
	uint16_t mask_t1;
	uint8_t mask_t2;

	mask_t1 = mask;
	if (mask_t1) {
		ret = mt6370_i2c_write16(tcpc, TCPC_V10_REG_ALERT, mask_t1);
		if (ret < 0)
			return ret;
	}

	mask_t2 = mask >> 16;
	if (mask_t2) {
		ret = mt6370_i2c_write8(tcpc, MT6370_REG_MT_INT, mask_t2);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int mt6370_set_clock_gating(struct tcpc_device *tcpc,
									bool en)
{
	int ret = 0;

#if CONFIG_TCPC_CLOCK_GATING
	int i = 0;
	uint8_t clk2 = MT6370_REG_CLK_DIV_600K_EN
		| MT6370_REG_CLK_DIV_300K_EN | MT6370_REG_CLK_CK_300K_EN;
	uint8_t clk3 = MT6370_REG_CLK_DIV_2P4M_EN;

	if (!en) {
		clk2 |=
			MT6370_REG_CLK_BCLK2_EN | MT6370_REG_CLK_BCLK_EN;
		clk3 |=
			MT6370_REG_CLK_CK_24M_EN | MT6370_REG_CLK_PCLK_EN;
	}

	if (en) {
		for (i = 0; i < 2; i++)
			ret = mt6370_alert_status_clear(tcpc,
				TCPC_REG_ALERT_RX_ALL_MASK);
	}

	if (ret == 0)
		ret = mt6370_i2c_write8(tcpc, MT6370_REG_CLK_CTRL2, clk2);
	if (ret == 0)
		ret = mt6370_i2c_write8(tcpc, MT6370_REG_CLK_CTRL3, clk3);
#endif	/* CONFIG_TCPC_CLOCK_GATING */

	return ret;
}

static inline int mt6370_init_cc_params(
			struct tcpc_device *tcpc, uint8_t cc_res)
{
	int rv = 0;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#if CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC
	uint8_t en, sel;

	if (cc_res == TYPEC_CC_VOLT_SNK_DFT) { /* 0.55 */
		en = 1;
		sel = 0x81;
	} else { /* 0.4 & 0.7 */
		en = 0;
		sel = 0x80;
	}

	rv = mt6370_i2c_write8(tcpc, MT6370_REG_BMCIO_RXDZEN, en);
	if (rv == 0)
		rv = mt6370_i2c_write8(tcpc, MT6370_REG_BMCIO_RXDZSEL, sel);
#endif	/* CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC */
#endif	/* CONFIG_USB_POWER_DELIVERY */

	return rv;
}

static int mt6370_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	bool retry_discard_old = false;
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);

	MT6370_INFO("\n");

	if (sw_reset) {
		ret = mt6370_software_reset(tcpc);
		if (ret < 0)
			return ret;
	}

	/* For No-GoodCRC Case (0x70) */
	mt6370_i2c_write8(tcpc, MT6370_REG_PHY_CTRL2, 0x38);
	mt6370_i2c_write8(tcpc, MT6370_REG_PHY_CTRL3, 0x82);
	mt6370_i2c_write8(tcpc, MT6370_REG_PHY_CTRL11, 0xfc);
	mt6370_i2c_write8(tcpc, MT6370_REG_PHY_CTRL12, 0x50);

#if CONFIG_TCPC_I2CRST_EN
	mt6370_i2c_write8(tcpc,
		MT6370_REG_I2CRST_CTRL,
		MT6370_REG_I2CRST_SET(true, 0x0f));
#endif	/* CONFIG_TCPC_I2CRST_EN */

	/* UFP Both RD setting */
	/* DRP = 0, RpVal = 0 (Default), Rd, Rd */
	mt6370_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL,
		TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));

	if (chip->chip_id == MT6370_DID_A) {
		mt6370_i2c_write8(tcpc, TCPC_V10_REG_FAULT_CTRL,
			TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV);
	}

	/*
	 * CC Detect Debounce : 26.7*val us
	 * Transition window count : spec 12~20us, based on 2.4MHz
	 * DRP Toggle Cycle : 51.2 + 6.4*val ms
	 * DRP Duty Ctrl : dcSRC / 1024
	 */

	mt6370_i2c_write8(tcpc, MT6370_REG_TTCPC_FILTER, 10);
	mt6370_i2c_write8(tcpc, MT6370_REG_DRP_TOGGLE_CYCLE, 0);
	mt6370_i2c_write16(tcpc, MT6370_REG_DRP_DUTY_CTRL, TCPC_NORMAL_RP_DUTY);

	/* RX/TX Clock Gating (Auto Mode)*/
	if (!sw_reset)
		mt6370_set_clock_gating(tcpc, true);

	if (!(tcpc->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD))
		retry_discard_old = true;

	/* For BIST, Change Transition Toggle Counter (Noise) from 3 to 7 */
	mt6370_i2c_write8(tcpc, MT6370_REG_PHY_CTRL1,
		MT6370_REG_PHY_CTRL1_SET(retry_discard_old, 7, 0, 1));

	tcpci_alert_status_clear(tcpc, 0xffffffff);

	mt6370_init_power_status_mask(tcpc);
	mt6370_init_alert_mask(tcpc);
	mt6370_init_fault_mask(tcpc);
	mt6370_init_mt_mask(tcpc);

	/* CK_300K from 320K, SHIPPING off, AUTOIDLE enable, TIMEOUT = 6.4ms */
	mt6370_i2c_write8(tcpc, MT6370_REG_IDLE_CTRL,
		MT6370_REG_IDLE_SET(0, 1, 1, 0));
	mdelay(1);

	return 0;
}

static inline int mt6370_fault_status_vconn_ov(struct tcpc_device *tcpc)
{
	int ret;

	ret = mt6370_i2c_read8(tcpc, MT6370_REG_BMC_CTRL);
	if (ret < 0)
		return ret;

	ret &= ~MT6370_REG_DISCHARGE_EN;
	return mt6370_i2c_write8(tcpc, MT6370_REG_BMC_CTRL, ret);
}

static inline int mt6370_fault_status_vconn_oc(struct tcpc_device *tcpc)
{
	const uint8_t mask =
		TCPC_V10_REG_FAULT_STATUS_VCONN_OV;

	return mt6370_i2c_write8(tcpc,
		TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

int mt6370_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OV)
		mt6370_fault_status_vconn_ov(tcpc);
	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OC)
		mt6370_fault_status_vconn_oc(tcpc);

	return mt6370_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);
}

int mt6370_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
	int ret;
	uint8_t v2;

	ret = mt6370_i2c_read16(tcpc, TCPC_V10_REG_ALERT_MASK);
	if (ret < 0)
		return ret;
	*mask = (uint16_t) ret;

	ret = mt6370_i2c_read8(tcpc, MT6370_REG_MT_MASK);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*mask |= v2 << 16;

	return 0;
}

int mt6370_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
	int ret;
	uint8_t v2;

	ret = mt6370_i2c_read16(tcpc, TCPC_V10_REG_ALERT);
	if (ret < 0)
		return ret;

	*alert = (uint16_t) ret;

	ret = mt6370_i2c_read8(tcpc, MT6370_REG_MT_INT);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*alert |= v2 << 16;

	return 0;
}

static int mt6370_get_power_status(
		struct tcpc_device *tcpc, uint16_t *pwr_status)
{
	int ret;

	ret = mt6370_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
	if (ret < 0)
		return ret;

	*pwr_status = 0;

	if (ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

	ret = mt6370_i2c_read8(tcpc, MT6370_REG_MT_STATUS);
	if (ret < 0)
		return ret;

	if (ret & MT6370_REG_VBUS_80)
		*pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;

	return 0;
}

int mt6370_get_fault_status(struct tcpc_device *tcpc, uint8_t *status)
{
	int ret;

	ret = mt6370_i2c_read8(tcpc, TCPC_V10_REG_FAULT_STATUS);
	if (ret < 0)
		return ret;
	*status = (uint8_t) ret;
	return 0;
}

static int mt6370_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	int status, role_ctrl, cc_role;
	bool act_as_sink, act_as_drp;

	status = mt6370_i2c_read8(tcpc, TCPC_V10_REG_CC_STATUS);
	if (status < 0)
		return status;

	role_ctrl = mt6370_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
	if (role_ctrl < 0)
		return role_ctrl;

	if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
		*cc1 = TYPEC_CC_DRP_TOGGLING;
		*cc2 = TYPEC_CC_DRP_TOGGLING;
		return 0;
	}

	*cc1 = TCPC_V10_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_V10_REG_CC_STATUS_CC2(status);

	act_as_drp = TCPC_V10_REG_ROLE_CTRL_DRP & role_ctrl;

	if (act_as_drp) {
		act_as_sink = TCPC_V10_REG_CC_STATUS_DRP_RESULT(status);
	} else {
		if (tcpc->typec_polarity)
			cc_role = TCPC_V10_REG_CC_STATUS_CC2(role_ctrl);
		else
			cc_role = TCPC_V10_REG_CC_STATUS_CC1(role_ctrl);
		if (cc_role == TYPEC_CC_RP)
			act_as_sink = false;
		else
			act_as_sink = true;
	}

	/*
	 * If status is not open, then OR in termination to convert to
	 * enum tcpc_cc_voltage_status.
	 */

	if (*cc1 != TYPEC_CC_VOLT_OPEN)
		*cc1 |= (act_as_sink << 2);

	if (*cc2 != TYPEC_CC_VOLT_OPEN)
		*cc2 |= (act_as_sink << 2);

	mt6370_init_cc_params(tcpc,
		(uint8_t)tcpc->typec_polarity ? *cc2 : *cc1);

	return 0;
}

static int mt6370_enable_vsafe0v_detect(
	struct tcpc_device *tcpc, bool enable)
{
	int ret = mt6370_i2c_read8(tcpc, MT6370_REG_MT_MASK);

	if (ret < 0)
		return ret;

	if (enable)
		ret |= MT6370_REG_M_VBUS_80;
	else
		ret &= ~MT6370_REG_M_VBUS_80;

	return mt6370_i2c_write8(tcpc, MT6370_REG_MT_MASK, (uint8_t) ret);
}

static int mt6370_set_cc(struct tcpc_device *tcpc, int pull)
{
	int ret;
	uint8_t data;
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull), pull1, pull2;

	MT6370_INFO("pull = 0x%02X\n", pull);
	pull = TYPEC_CC_PULL_GET_RES(pull);
	if (pull == TYPEC_CC_DRP) {
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(
				1, rp_lvl, TYPEC_CC_RD, TYPEC_CC_RD);

		ret = mt6370_i2c_write8(
			tcpc, TCPC_V10_REG_ROLE_CTRL, data);

		if (ret == 0) {
			mt6370_enable_vsafe0v_detect(tcpc, false);
			ret = mt6370_command(tcpc, TCPM_CMD_LOOK_CONNECTION);
		}
	} else {
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
		if (pull == TYPEC_CC_RD && tcpc->pd_wait_pr_swap_complete)
			mt6370_init_cc_params(tcpc, TYPEC_CC_VOLT_SNK_DFT);
#endif	/* CONFIG_USB_POWER_DELIVERY */

		pull1 = pull2 = pull;

		if (pull == TYPEC_CC_RP &&
			tcpc->typec_state == typec_attached_src) {
			if (tcpc->typec_polarity)
				pull1 = TYPEC_CC_OPEN;
			else
				pull2 = TYPEC_CC_OPEN;
		}
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull1, pull2);
		ret = mt6370_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
	}

	return 0;
}

static int mt6370_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	int data;

	if (polarity < 0 || polarity > 1)
		return -EOVERFLOW;

	data = mt6370_init_cc_params(tcpc,
		tcpc->typec_remote_cc[polarity]);
	if (data)
		return data;

	data = mt6370_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT;
	data |= polarity ? TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT : 0;

	return mt6370_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}

static int mt6370_set_vconn(struct tcpc_device *tcpc, int enable)
{
	int rv;
	int data;

	data = mt6370_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_POWER_CTRL_VCONN;
	data |= enable ? TCPC_V10_REG_POWER_CTRL_VCONN : 0;

	rv = mt6370_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data);
	if (rv < 0)
		return rv;

	if (enable)
		mt6370_init_fault_mask(tcpc);

	return rv;
}

static int mt6370_is_vsafe0v(struct tcpc_device *tcpc)
{
	int rv = mt6370_i2c_read8(tcpc, MT6370_REG_MT_STATUS);

	if (rv < 0)
		return rv;

	return (rv & MT6370_REG_VBUS_80) != 0;
}

static int mt6370_set_low_power_mode(
		struct tcpc_device *tcpc, bool en, int pull)
{
	int ret = 0;
	uint8_t data;

	ret = mt6370_i2c_write8(tcpc, MT6370_REG_IDLE_CTRL,
		MT6370_REG_IDLE_SET(0, 1, en ? 0 : 1, 0));
	if (ret < 0)
		return ret;
	ret = mt6370_enable_vsafe0v_detect(tcpc, !en);
	if (ret < 0)
		return ret;
	if (en) {
		data = MT6370_REG_BMCIO_LPEN;

		if (TYPEC_CC_PULL_GET_RES(pull) == TYPEC_CC_RP)
			data |= MT6370_REG_BMCIO_LPRPRD;

#if CONFIG_TYPEC_CAP_NORP_SRC
		data |= MT6370_REG_BMCIO_BG_EN | MT6370_REG_VBUS_DET_EN;
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
	} else {
		data = MT6370_REG_BMCIO_BG_EN |
			MT6370_REG_VBUS_DET_EN | MT6370_REG_BMCIO_OSC_EN;
	}

	return mt6370_i2c_write8(tcpc, MT6370_REG_BMC_CTRL, data);
}

static int mt6370_tcpc_deinit(struct tcpc_device *tcpc)
{
#if IS_ENABLED(CONFIG_RT_REGMAP)
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);
#endif /* CONFIG_RT_REGMAP */

#if CONFIG_TCPC_SHUTDOWN_CC_DETACH
	mt6370_set_cc(tcpc, TYPEC_CC_OPEN);

	mt6370_i2c_write8(tcpc,
		MT6370_REG_I2CRST_CTRL,
		MT6370_REG_I2CRST_SET(true, 4));
#else
	mt6370_i2c_write8(tcpc, MT6370_REG_SWRESET, 1);
#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */
#if IS_ENABLED(CONFIG_RT_REGMAP)
	rt_regmap_cache_reload(chip->m_dev);
#endif /* CONFIG_RT_REGMAP */

	return 0;
}

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
static int mt6370_set_msg_header(
	struct tcpc_device *tcpc, uint8_t power_role, uint8_t data_role)
{
	uint8_t msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(
		data_role, power_role);

	return mt6370_i2c_write8(
		tcpc, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int mt6370_protocol_reset(struct tcpc_device *tcpc)
{
	mt6370_i2c_write8(tcpc, MT6370_REG_PRL_FSM_RESET, 0);
	mdelay(1);
	mt6370_i2c_write8(tcpc, MT6370_REG_PRL_FSM_RESET, 1);
	return 0;
}

static int mt6370_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
	int ret = 0;

	if (enable)
		ret = mt6370_set_clock_gating(tcpc, false);

	if (ret == 0)
		ret = mt6370_i2c_write8(tcpc, TCPC_V10_REG_RX_DETECT, enable);

	if ((ret == 0) && (!enable)) {
		mt6370_protocol_reset(tcpc);
		ret = mt6370_set_clock_gating(tcpc, true);
	}

	return ret;
}

static int mt6370_get_message(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);
	int rv = 0;
	uint8_t cnt = 0;
	uint8_t buf[4];

	rv = mt6370_block_read(chip->client, TCPC_V10_REG_RX_BYTE_CNT, 4, buf);
	if (rv < 0)
		return rv;

	cnt = buf[0];
	*frame_type = buf[1];
	*msg_head = le16_to_cpu(*(uint16_t *)&buf[2]);

	/* TCPC 1.0 ==> no need to subtract the size of msg_head */
	if (cnt > 3) {
		cnt -= 3; /* MSG_HDR */
		rv = mt6370_block_read(chip->client, TCPC_V10_REG_RX_DATA, cnt,
				       payload);
	}

	return rv;
}

static int mt6370_set_bist_carrier_mode(
	struct tcpc_device *tcpc, uint8_t pattern)
{
	/* Don't support this function */
	return 0;
}

/* transmit count (1byte) + message header (2byte) + data object (7*4) */
#define MT6370_TRANSMIT_MAX_SIZE (1+sizeof(uint16_t) + sizeof(uint32_t)*7)

#if CONFIG_USB_PD_RETRY_CRC_DISCARD
static int mt6370_retransmit(struct tcpc_device *tcpc)
{
	return mt6370_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, TCPC_TX_SOP));
}
#endif

static int mt6370_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	int data_cnt, packet_cnt;
	uint8_t temp[MT6370_TRANSMIT_MAX_SIZE];

	if (type < TCPC_TX_HARD_RESET) {
		data_cnt = sizeof(uint32_t) * PD_HEADER_CNT(header);
		packet_cnt = data_cnt + sizeof(uint16_t);

		temp[0] = packet_cnt;
		memcpy(temp+1, (uint8_t *)&header, 2);
		if (data_cnt > 0)
			memcpy(temp+3, (uint8_t *)data, data_cnt);

		rv = mt6370_block_write(chip->client,
				TCPC_V10_REG_TX_BYTE_CNT,
				packet_cnt+1, (uint8_t *)temp);
		if (rv < 0)
			return rv;
	}

	rv = mt6370_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, type));
	return rv;
}

static int mt6370_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	int data;

	data = mt6370_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE;
	data |= en ? TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE : 0;

	return mt6370_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

#if CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#if CONFIG_TCPC_FORCE_DISCHARGE_IC
static int mt6370_set_force_discharge(struct tcpc_device *tcpc, bool en, int mv)
{
	int data;

	data = mt6370_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_FORCE_DISC_EN;
	data |= en ? TCPC_V10_REG_FORCE_DISC_EN : 0;

	return mt6370_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data);
}
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */

static struct tcpc_ops mt6370_tcpc_ops = {
	.init = mt6370_tcpc_init,
	.alert_status_clear = mt6370_alert_status_clear,
	.fault_status_clear = mt6370_fault_status_clear,
	.get_alert_mask = mt6370_get_alert_mask,
	.get_alert_status = mt6370_get_alert_status,
	.get_power_status = mt6370_get_power_status,
	.get_fault_status = mt6370_get_fault_status,
	.get_cc = mt6370_get_cc,
	.set_cc = mt6370_set_cc,
	.set_polarity = mt6370_set_polarity,
	.set_vconn = mt6370_set_vconn,
	.deinit = mt6370_tcpc_deinit,

	.is_vsafe0v = mt6370_is_vsafe0v,

	.set_low_power_mode = mt6370_set_low_power_mode,

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	.set_msg_header = mt6370_set_msg_header,
	.set_rx_enable = mt6370_set_rx_enable,
	.protocol_reset = mt6370_protocol_reset,
	.get_message = mt6370_get_message,
	.transmit = mt6370_transmit,
	.set_bist_test_mode = mt6370_set_bist_test_mode,
	.set_bist_carrier_mode = mt6370_set_bist_carrier_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#if CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = mt6370_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#if CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#if CONFIG_TCPC_FORCE_DISCHARGE_IC
	.set_force_discharge = mt6370_set_force_discharge,
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */
};

static int mt_parse_dt(struct mt6370_chip *chip, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	pr_info("%s\n", __func__);

#if !IS_ENABLED(CONFIG_MTK_GPIO) || IS_ENABLED(CONFIG_MTK_GPIOLIB_STAND)
	ret = of_get_named_gpio(np, "mt6370pd,intr-gpio", 0);
	if (ret < 0)
		ret = of_get_named_gpio(np, "mt6370pd,intr_gpio", 0);

	if (ret < 0)
		pr_err("%s no intr_gpio info\n", __func__);
	else
		chip->irq_gpio = ret;
#else
	ret = of_property_read_u32(np, "mt6370pd,intr-gpio-num", &chip->irq_gpio) ?
	      of_property_read_u32(np, "mt6370pd,intr_gpio_num", &chip->irq_gpio) : 0;
	if (ret < 0)
		pr_err("%s no intr_gpio info\n", __func__);
#endif /* !CONFIG_MTK_GPIO || CONFIG_MTK_GPIOLIB_STAND */
	return ret < 0 ? ret : 0;
}

static int mt6370_tcpcdev_init(struct mt6370_chip *chip, struct device *dev)
{
	struct tcpc_desc *desc;
	struct tcpc_device *tcpc = NULL;
	struct device_node *np = dev->of_node;
	u32 val, len;
	const char *name = "default";

	dev_info(dev, "%s\n", __func__);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	if (of_property_read_u32(np, "mt-tcpc,role-def", &val) >= 0 ||
	    of_property_read_u32(np, "mt-tcpc,role_def", &val) >= 0) {
		if (val >= TYPEC_ROLE_NR)
			desc->role_def = TYPEC_ROLE_DRP;
		else
			desc->role_def = val;
	} else {
		dev_info(dev, "use default Role DRP\n");
		desc->role_def = TYPEC_ROLE_DRP;
	}

	if (of_property_read_u32(np, "mt-tcpc,rp-level", &val) >= 0 ||
	    of_property_read_u32(np, "mt-tcpc,rp_level", &val) >= 0) {
		switch (val) {
		case TYPEC_RP_DFT:
		case TYPEC_RP_1_5:
		case TYPEC_RP_3_0:
			desc->rp_lvl = val;
			break;
		default:
			desc->rp_lvl = TYPEC_RP_DFT;
			break;
		}
	}

#if CONFIG_TCPC_VCONN_SUPPLY_MODE
	if (of_property_read_u32(np, "mt-tcpc,vconn-supply", &val) >= 0 ||
	    of_property_read_u32(np, "mt-tcpc,vconn_supply", &val) >= 0) {
		if (val >= TCPC_VCONN_SUPPLY_NR)
			desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
		else
			desc->vconn_supply = val;
	} else {
		dev_info(dev, "use default VconnSupply\n");
		desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	if (of_property_read_string(np, "mt-tcpc,name",
				(char const **)&name) < 0) {
		dev_info(dev, "use default name\n");
	}

	len = strlen(name);
	desc->name = kzalloc(len+1, GFP_KERNEL);
	if (!desc->name)
		return -ENOMEM;

	strlcpy((char *)desc->name, name, len+1);

	chip->tcpc_desc = desc;

	tcpc = tcpc_device_register(dev, desc, &mt6370_tcpc_ops, chip);
	if (IS_ERR_OR_NULL(tcpc))
		return -EINVAL;
	chip->tcpc = tcpc;

#if CONFIG_USB_PD_DISABLE_PE
	tcpc->disable_pe = of_property_read_bool(np, "mt-tcpc,disable-pe") ||
				 of_property_read_bool(np, "mt-tcpc,disable_pe");
#endif	/* CONFIG_USB_PD_DISABLE_PE */

#if CONFIG_USB_PD_RETRY_CRC_DISCARD
	tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#if CONFIG_USB_PD_REV30
	tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;

	if (tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
		dev_info(dev, "PD_REV30\n");
	else
		dev_info(dev, "PD_REV20\n");
#endif	/* CONFIG_USB_PD_REV30 */
	tcpc->tcpc_flags |= TCPC_FLAGS_ALERT_V10;

	return 0;
}

#define MEDIATEK_6370_VID	0x29cf
#define MEDIATEK_6370_PID	0x5081

static inline int mt6370_check_revision(struct i2c_client *client)
{
	u16 vid, pid, did;
	int ret;
	u8 data = 1;

	ret = i2c_smbus_read_i2c_block_data(client,
			TCPC_V10_REG_VID, 2, (u8 *)&vid);
	if (ret < 0) {
		dev_err(&client->dev, "read chip ID fail\n");
		return -EIO;
	}

	if (vid != MEDIATEK_6370_VID) {
		pr_info("%s failed, VID=0x%04x\n", __func__, vid);
		return -ENODEV;
	}

	ret = i2c_smbus_read_i2c_block_data(client,
			TCPC_V10_REG_PID, 2, (u8 *)&pid);
	if (ret < 0) {
		dev_err(&client->dev, "read product ID fail\n");
		return -EIO;
	}

	/* add MT6371 chip TCPC pid check for compatible */
	if (pid != MEDIATEK_6370_PID && pid != 0x5101 && pid != 0x6372) {
		pr_info("%s failed, PID=0x%04x\n", __func__, pid);
		return -ENODEV;
	}

	ret = i2c_smbus_write_i2c_block_data(client,
			MT6370_REG_SWRESET, 1, (u8 *)&data);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);

	ret = i2c_smbus_read_i2c_block_data(client,
			TCPC_V10_REG_DID, 2, (u8 *)&did);
	if (ret < 0) {
		dev_err(&client->dev, "read device ID fail\n");
		return -EIO;
	}

	return did;
}

static int mt6370_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct mt6370_chip *chip;
	int ret = 0, chip_id;
	bool use_dt = client->dev.of_node;

	pr_info("%s\n", __func__);
	if (i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BYTE_DATA))
		pr_info("I2C functionality : OK...\n");
	else
		pr_info("I2C functionality check : failuare...\n");

	chip_id = mt6370_check_revision(client);
	if (chip_id < 0)
		return chip_id;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (use_dt) {
		ret = mt_parse_dt(chip, &client->dev);
		if (ret < 0)
			return ret;
	} else {
		dev_err(&client->dev, "no dts node\n");
		return -ENODEV;
	}
	chip->dev = &client->dev;
	chip->client = client;
	i2c_set_clientdata(client, chip);
	chip->chip_id = chip_id;
	pr_info("mt6370_chipID = 0x%0x\n", chip_id);
	mutex_init(&chip->irq_lock);
	chip->is_suspended = false;
	chip->irq_while_suspended = false;

	ret = mt6370_regmap_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "mt6370 regmap init fail\n");
		goto err_regmap_init;
	}

	ret = mt6370_tcpcdev_init(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "mt6370 tcpc dev init fail\n");
		goto err_tcpc_reg;
	}

	ret = mt6370_init_alert(chip->tcpc);
	if (ret < 0) {
		pr_err("mt6370 init alert fail\n");
		goto err_irq_init;
	}

	pr_info("%s probe OK!\n", __func__);
	return 0;

err_irq_init:
	tcpc_device_unregister(chip->dev, chip->tcpc);
err_tcpc_reg:
	mt6370_regmap_deinit(chip);
err_regmap_init:
	mutex_destroy(&chip->irq_lock);
	return ret;
}

static void mt6370_i2c_remove(struct i2c_client *client)
{
	struct mt6370_chip *chip = i2c_get_clientdata(client);

	if (chip) {
		tcpc_device_unregister(chip->dev, chip->tcpc);
		mt6370_regmap_deinit(chip);
		mutex_destroy(&chip->irq_lock);
	}
}

#if CONFIG_PM
static int mt6370_i2c_suspend(struct device *dev)
{
	struct mt6370_chip *chip = dev_get_drvdata(dev);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	if (chip->tcpc->pd_wait_hard_reset_complete) {
		dev_info(dev, "%s WAITING HRESET - NO SUSPEND\n", __func__);
		return -EAGAIN;
	}
#endif

	dev_info(dev, "%s irq_gpio = %d\n",
		      __func__, gpio_get_value(chip->irq_gpio));

	mutex_lock(&chip->irq_lock);
	chip->is_suspended = true;
	mutex_unlock(&chip->irq_lock);

	synchronize_irq(chip->irq);

	return 0;
}

static int mt6370_i2c_resume(struct device *dev)
{
	struct mt6370_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s irq_gpio = %d\n",
		      __func__, gpio_get_value(chip->irq_gpio));

	mutex_lock(&chip->irq_lock);
	if (chip->irq_while_suspended) {
		enable_irq(chip->irq);
		chip->irq_while_suspended = false;
	}
	chip->is_suspended = false;
	mutex_unlock(&chip->irq_lock);

	return 0;
}

static void mt6370_shutdown(struct i2c_client *client)
{
	struct mt6370_chip *chip = i2c_get_clientdata(client);

	/* Please reset IC here */
	if (chip != NULL) {
		if (chip->irq)
			disable_irq(chip->irq);
		tcpm_shutdown(chip->tcpc);
	} else {
		i2c_smbus_write_byte_data(
			client, MT6370_REG_SWRESET, 0x01);
	}
}

#if IS_ENABLED(CONFIG_PM_RUNTIME)
static int mt6370_pm_suspend_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: suspending...\n");
	return 0;
}

static int mt6370_pm_resume_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: resuming...\n");
	return 0;
}
#endif /* CONFIG_PM_RUNTIME */


static const struct dev_pm_ops mt6370_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			mt6370_i2c_suspend,
			mt6370_i2c_resume)
#if IS_ENABLED(CONFIG_PM_RUNTIME)
	SET_RUNTIME_PM_OPS(
		mt6370_pm_suspend_runtime,
		mt6370_pm_resume_runtime,
		NULL
	)
#endif /* CONFIG_PM_RUNTIME */
};
#define MT6370_PM_OPS	(&mt6370_pm_ops)
#else
#define MT6370_PM_OPS	(NULL)
#endif /* CONFIG_PM */

static const struct i2c_device_id mt6370_id_table[] = {
	{"mt6370_typec", 0},
	{"mt6371_typec", 0},
	{"mt6372_typec", 0},
	{"rt5081_typec", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, mt6370_id_table);

static const struct of_device_id rt_match_table[] = {
	{.compatible = "mediatek,mt6370_typec",},
	{.compatible = "mediatek,mt6371_typec",},
	{.compatible = "mediatek,mt6372_typec",},
	{.compatible = "richtek,rt5081_typec",},
	{},
};

static struct i2c_driver mt6370_driver = {
	.driver = {
		.name = "mt6370_typec",
		.owner = THIS_MODULE,
		.of_match_table = rt_match_table,
		.pm = MT6370_PM_OPS,
	},
	.probe = mt6370_i2c_probe,
	.remove = mt6370_i2c_remove,
	.shutdown = mt6370_shutdown,
	.id_table = mt6370_id_table,
};

static int __init mt6370_init(void)
{
	struct device_node *np;

	pr_info("%s (%s)\n", __func__, MT6370_DRV_VERSION);
	np = of_find_node_by_name(NULL, "mt6370-typec");
	pr_info("%s mt6370-typec node %s\n", __func__,
		np == NULL ? "not found" : "found");

	return i2c_add_driver(&mt6370_driver);
}
subsys_initcall(mt6370_init);

static void __exit mt6370_exit(void)
{
	i2c_del_driver(&mt6370_driver);
}
module_exit(mt6370_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT6370 TCPC Driver");
MODULE_VERSION(MT6370_DRV_VERSION);

/**** Release Note ****
 * 2.0.8_MTK
 * (1) Revise suspend/resume flow for IRQ
 *
 * 2.0.7_MTK
 * (1) Revise IRQ handling
 *
 * 2.0.6_MTK
 * (1) Update tTCPCfilter to 267us
 *
 * 2.0.5_MTK
 * (1) Utilize rt-regmap to reduce I2C accesses
 *
 * 2.0.4_MTK
 * (1) Mask vSafe0V IRQ before entering low power mode
 * (2) Disable auto idle mode before entering low power mode
 * (3) Reset Protocol FSM and clear RX alerts twice before clock gating
 *
 * 2.0.3_MTK
 * (1) Move down the shipping off
 *
 * 2.0.2_MTK
 * (1) Single Rp as Attatched.SRC for Ellisys TD.4.9.4
 * (2) Fix Rx Noise for MQP
 *
 * 2.0.1_MTK
 *  First released PD3.0 Driver on MTK platform
 */
