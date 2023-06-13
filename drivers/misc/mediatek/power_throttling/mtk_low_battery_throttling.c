// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "mtk_low_battery_throttling.h"
#include "pmic_lbat_service.h"
#include "pmic_lvsys_notify.h"

#define LBCB_MAX_NUM 16
#define THD_VOLTS_LENGTH 20
#define POWER_INT0_VOLT 3400
#define POWER_INT1_VOLT 3250
#define POWER_INT2_VOLT 3100
#define POWER_INT3_VOLT 2700


struct lbat_intr_tbl {
	unsigned int volt_thd;
	unsigned int lt_en;
	unsigned int lt_lv;
	unsigned int ht_en;
	unsigned int ht_lv;
};

struct low_bat_thl_priv {
	unsigned int *thd_volts;
	unsigned int low_bat_thl_temp_volt_thd;
	enum LOW_BATTERY_LVSYS_STATUS lvsys_status;
	int thd_volts_size;
	int low_bat_thl_level;
	int low_bat_thl_stop;
	struct lbat_user *lbat_pt;
	struct lbat_intr_tbl *lbat_intr_info;
	struct mutex lock;
};

struct low_battery_callback_table {
	void (*lbcb)(enum LOW_BATTERY_LEVEL_TAG, void *data);
	void *data;
};

static unsigned int *volt_l_thd, *volt_h_thd;
static struct low_bat_thl_priv *low_bat_thl_data;
static struct low_battery_callback_table lbcb_tb[LBCB_MAX_NUM] = { {0}, {0} };

static int rearrange_volt(struct lbat_intr_tbl *intr_info, unsigned int *volt_l,
	unsigned int *volt_h, unsigned int num)
{
	unsigned int idx_l = 0, idx_h = 0, idx_t = 0, i;
	unsigned int volt_l_next, volt_h_next;

	for (i = 0; i < num - 1; i++) {
		if (volt_l[i] < volt_l[i+1] || volt_h[i] < volt_h[i+1]) {
			pr_notice("[%s] i=%d volt_l(%d, %d) volt_h(%d, %d) error\n",
				__func__, i, volt_l[i], volt_l[i+1], volt_h[i], volt_h[i+1]);
			return -EINVAL;
		}
	}
	for (i = 0; i < num * 2; i++) {
		volt_l_next = (idx_l < num) ? volt_l[idx_l] : 0;
		volt_h_next = (idx_h < num) ? volt_h[idx_h] : 0;
		if (volt_l_next > volt_h_next && volt_l_next > 0) {
			intr_info[idx_t].volt_thd = volt_l_next;
			intr_info[idx_t].lt_en = 1;
			intr_info[idx_t].lt_lv = idx_l + 1;
			idx_l++;
			idx_t++;
		} else if (volt_l_next == volt_h_next && volt_l_next > 0) {
			intr_info[idx_t].volt_thd = volt_l_next;
			intr_info[idx_t].lt_en = 1;
			intr_info[idx_t].lt_lv = idx_l + 1;
			intr_info[idx_t].ht_en = 1;
			intr_info[idx_t].ht_lv = idx_h;
			idx_l++;
			idx_h++;
			idx_t++;
		} else if (volt_h_next > 0) {
			intr_info[idx_t].volt_thd = volt_h_next;
			intr_info[idx_t].ht_en = 1;
			intr_info[idx_t].ht_lv = idx_h;
			idx_h++;
			idx_t++;
		} else
			break;
	}
	for (i = 0; i < idx_t; i++) {
		pr_info("[%s] intr_info[%d] = (%d, trig l[%d %d] h[%d %d])\n",
				__func__, i, intr_info[i].volt_thd, intr_info[i].lt_en,
				intr_info[i].lt_lv, intr_info[i].ht_en, intr_info[i].ht_lv);
	}
	return idx_t;
}

int register_low_battery_notify(low_battery_callback lb_cb,
				enum LOW_BATTERY_PRIO_TAG prio_val, void *data)
{
	if (prio_val >= LBCB_MAX_NUM) {
		pr_notice("[%s] prio_val=%d, out of boundary\n",
			  __func__, prio_val);
		return -EINVAL;
	}

	if (lbcb_tb[prio_val].lbcb != 0)
		pr_info("[%s] Notice: LBCB has been registered\n", __func__);

	lbcb_tb[prio_val].lbcb = lb_cb;
	lbcb_tb[prio_val].data = data;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if (!low_bat_thl_data) {
		pr_info("[%s] Failed to create low_bat_thl_data\n", __func__);
		return 3;
	}

	if (low_bat_thl_data->low_bat_thl_level && lbcb_tb[prio_val].lbcb) {
		lbcb_tb[prio_val].lbcb(low_bat_thl_data->low_bat_thl_level, lbcb_tb[prio_val].data);
		pr_info("[%s] notify lv=%d\n", __func__, low_bat_thl_data->low_bat_thl_level);
	}
	return 3;
}
EXPORT_SYMBOL(register_low_battery_notify);

void exec_throttle(unsigned int level)
{
	int i;

	if (!low_bat_thl_data) {
		pr_info("[%s] Failed to create low_bat_thl_data\n", __func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(lbcb_tb); i++) {
		if (lbcb_tb[i].lbcb)
			lbcb_tb[i].lbcb(low_bat_thl_data->low_bat_thl_level, lbcb_tb[i].data);
	}

	pr_info("[%s] low_battery_level = %d\n", __func__, level);
}

void exec_low_battery_throttle(unsigned int thd, int int_type, int int_status)
{
	int i = 0, cur_lv = 0;
	struct lbat_intr_tbl *info;

	if (!low_bat_thl_data)
		return;
	if (low_bat_thl_data->low_bat_thl_stop == 1) {
		pr_info("[%s] low_bat_thl_stop=%d\n",
			__func__, low_bat_thl_data->low_bat_thl_stop);
		return;
	}

	mutex_lock(&low_bat_thl_data->lock);
	if (int_type == LVSYS && low_bat_thl_data->lvsys_status == DEACTIVATE)
		thd = low_bat_thl_data->low_bat_thl_temp_volt_thd;
	cur_lv = low_bat_thl_data->low_bat_thl_level;
	for (i = 0; i < low_bat_thl_data->thd_volts_size; i++) {
		if (thd == low_bat_thl_data->thd_volts[i]) {
			info = &(low_bat_thl_data->lbat_intr_info[i]);
			if (info->ht_en == 1 && cur_lv > info->ht_lv)
				low_bat_thl_data->low_bat_thl_level = info->ht_lv;
			else if (info->lt_en == 1 && cur_lv < info->lt_lv)
				low_bat_thl_data->low_bat_thl_level = info->lt_lv;
			break;
		}
	}

	if (i == low_bat_thl_data->thd_volts_size) {
		pr_notice("[%s] wrong threshold=%d\n", __func__, thd);
		mutex_unlock(&low_bat_thl_data->lock);
		return;
	}

	if (cur_lv == low_bat_thl_data->low_bat_thl_level) {
		pr_notice("[%s] same level\n", __func__);
		mutex_unlock(&low_bat_thl_data->lock);
		return;
	}

	exec_throttle(low_bat_thl_data->low_bat_thl_level);

	pr_info("[%s] thd=%d cl=%d pl=%d, ht[%d %d] lt[%d %d] new_l=%d, INT type=%d\n",
		__func__, thd, cur_lv, i, info->ht_en, info->ht_lv,
		info->lt_en, info->lt_lv, low_bat_thl_data->low_bat_thl_level, int_type);

	if (int_type == LVSYS && int_status == DEACTIVATE)
		low_bat_thl_data->lvsys_status = DEACTIVATE;

	mutex_unlock(&low_bat_thl_data->lock);
}

void exec_low_battery_callback(unsigned int thd)
{
	int size;

	if (!low_bat_thl_data) {
		pr_info("[%s] low_bat_thl_data not allocate\n", __func__);
		return;
	}

	size = low_bat_thl_data->thd_volts_size;
	if (thd != low_bat_thl_data->thd_volts[size - 1]) {
		mutex_lock(&low_bat_thl_data->lock);
		low_bat_thl_data->low_bat_thl_temp_volt_thd = thd;
		mutex_unlock(&low_bat_thl_data->lock);
		if (low_bat_thl_data->lvsys_status != ACTIVATE)
			exec_low_battery_throttle(thd, LVBAT, DEACTIVATE);
	}
}

/*****************************************************************************
 * low battery protect UT
 ******************************************************************************/
static ssize_t low_battery_protect_ut_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	dev_dbg(dev, "low_bat_thl_level=%d\n",
		low_bat_thl_data->low_bat_thl_level);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_level);
}

static ssize_t low_battery_protect_ut_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val = 0;
	char cmd[21];

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (strncmp(cmd, "Utest", 5))
		return -EINVAL;

	if (val > LOW_BATTERY_LEVEL_3) {
		dev_info(dev, "wrong number (%d)\n", val);
		return size;
	}

	low_bat_thl_data->low_bat_thl_level = val;
	dev_info(dev, "your input is %d\n", val);
	exec_throttle(val);
	return size;
}
static DEVICE_ATTR_RW(low_battery_protect_ut);

/*****************************************************************************
 * low battery protect stop
 ******************************************************************************/
static ssize_t low_battery_protect_stop_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	dev_dbg(dev, "low_bat_thl_stop=%d\n",
		low_bat_thl_data->low_bat_thl_stop);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_stop);
}

static ssize_t low_battery_protect_stop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int val = 0;
	char cmd[21];

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (strncmp(cmd, "stop", 4))
		return -EINVAL;

	if ((val != 0) && (val != 1))
		val = 0;

	low_bat_thl_data->low_bat_thl_stop = val;
	dev_info(dev, "low_bat_thl_stop=%d\n",
		 low_bat_thl_data->low_bat_thl_stop);
	return size;
}
static DEVICE_ATTR_RW(low_battery_protect_stop);

/*****************************************************************************
 * low battery protect level
 ******************************************************************************/
static ssize_t low_battery_protect_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_dbg(dev, "low_bat_thl_level=%d\n",
		low_bat_thl_data->low_bat_thl_level);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_level);
}

static ssize_t low_battery_protect_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	dev_dbg(dev, "low_bat_thl_level = %d\n",
		low_bat_thl_data->low_bat_thl_level);
	return size;
}

static DEVICE_ATTR_RW(low_battery_protect_level);
static void dump_thd_volts(struct device *dev, unsigned int *thd_volts, unsigned int size)
{
	int i, r = 0;
	char str[128] = "";
	size_t len = sizeof(str) - 1;

	for (i = 0; i < size; i++) {
		r += snprintf(str + r, len - r, "%s%d mV", i ? ", " : "", thd_volts[i]);
		if (r >= len)
			return;
	}
	dev_notice(dev, "%s Done\n", str);
}

static int low_battery_vsys_notifier_call(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	int thd, size = 0;

	size = low_bat_thl_data->thd_volts_size;
	event = event & ~(1 << 15);
	pr_notice("[%s] lvsys thd = %lu\n", __func__, event);

	if (event == low_bat_thl_data->thd_volts[size - 1]) {
		if (size > 0 && low_bat_thl_data->thd_volts) {
			thd = low_bat_thl_data->thd_volts[size - 1];
			mutex_lock(&low_bat_thl_data->lock);
			low_bat_thl_data->lvsys_status = ACTIVATE;
			mutex_unlock(&low_bat_thl_data->lock);
			exec_low_battery_throttle(thd, LVSYS, ACTIVATE);
		}
	} else if (event == low_bat_thl_data->thd_volts[size - 2]) {
		mutex_lock(&low_bat_thl_data->lock);
		thd = low_bat_thl_data->low_bat_thl_temp_volt_thd;
		mutex_unlock(&low_bat_thl_data->lock);
		exec_low_battery_throttle(thd, LVSYS, DEACTIVATE);
	}

	return NOTIFY_DONE;
}

static struct notifier_block lbat_vsys_notifier = {
	.notifier_call = low_battery_vsys_notifier_call,
};

static int check_duplicate(unsigned int *volt_thd)
{
	int i, j;

	for (i = 0; i < LOW_BATTERY_LEVEL_NUM - 1; i++) {
		for (j = i + 1; j < LOW_BATTERY_LEVEL_NUM - 1; j++) {
			if (volt_thd[i] == volt_thd[j]) {
				pr_notice("[%s] volt_thd duplicate = %d\n", __func__, volt_thd[i]);
				return -1;
			}
		}
	}
	return 0;
}

static int low_battery_throttling_probe(struct platform_device *pdev)
{
	int ret, i;
	struct low_bat_thl_priv *priv;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *gauge_np = pdev->dev.parent->of_node;
	int vol_l_size, vol_h_size, vol_t_size;
	int lvsys_thd_enable, vbat_thd_enable;
	char thd_volts_l[THD_VOLTS_LENGTH] = "thd-volts-l";
	char thd_volts_h[THD_VOLTS_LENGTH] = "thd-volts-h";
	int bat_type = 0;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, priv);

	gauge_np = of_find_node_by_name(gauge_np, "mtk-gauge");
	if (!gauge_np)
		dev_notice(&pdev->dev, "get mtk-gauge node fail\n");
	else {
		ret = of_property_read_u32(gauge_np, "bat_type", &bat_type);
		if (ret)
			dev_notice(&pdev->dev, "get bat_type fail\n");

		if (bat_type == 1) {
			strncpy(thd_volts_l, "thd-volts-l-2s", THD_VOLTS_LENGTH);
			strncpy(thd_volts_h, "thd-volts-h-2s", THD_VOLTS_LENGTH);
		}
	}

	vol_l_size = of_property_count_elems_of_size(np, thd_volts_l, sizeof(u32));
	if (vol_l_size != 3) {
		strncpy(thd_volts_l, "thd-volts-l", THD_VOLTS_LENGTH);
		vol_l_size = of_property_count_elems_of_size(np, thd_volts_l, sizeof(u32));
	}
	if (vol_l_size != 3) {
		dev_notice(&pdev->dev, "[%s] Wrong thd-volts-l\n", __func__);
		return -ENODATA;
	}

	vol_h_size = of_property_count_elems_of_size(np, thd_volts_h, sizeof(u32));
	if (vol_h_size != 3) {
		strncpy(thd_volts_h, "thd-volts-h", THD_VOLTS_LENGTH);
		vol_h_size = of_property_count_elems_of_size(np, thd_volts_h, sizeof(u32));
	}
	if (vol_h_size != 3) {
		dev_notice(&pdev->dev, "[%s] Wrong thd-volts-h\n", __func__);
		return -ENODATA;
	}

	ret = of_property_read_u32(np, "lvsys-thd-enable", &lvsys_thd_enable);
	if (ret) {
		dev_notice(&pdev->dev,
			"[%s] failed to get lvsys-thd-enable ret=%d\n", __func__, ret);
		lvsys_thd_enable = 0;
	}

	ret = of_property_read_u32(np, "vbat-thd-enable", &vbat_thd_enable);
	if (ret) {
		dev_notice(&pdev->dev,
			"[%s] failed to get vbat-thd-enable ret=%d\n", __func__, ret);
		vbat_thd_enable = 1;
	}

	dev_notice(&pdev->dev, "bat_type = %d\n", bat_type);
	dev_notice(&pdev->dev, "lvsys_thd_enable = %d, vbat_thd_enable = %d\n",
			lvsys_thd_enable, vbat_thd_enable);

	priv->thd_volts_size = vol_l_size + vol_h_size;
	priv->lbat_intr_info = devm_kmalloc_array(&pdev->dev, priv->thd_volts_size,
		sizeof(struct lbat_intr_tbl), GFP_KERNEL);
	if (!priv->lbat_intr_info)
		return -ENOMEM;

	volt_l_thd = devm_kmalloc_array(&pdev->dev, priv->thd_volts_size,
							sizeof(u32), GFP_KERNEL);
	volt_h_thd = devm_kmalloc_array(&pdev->dev, priv->thd_volts_size,
							sizeof(u32), GFP_KERNEL);
	ret = of_property_read_u32_array(np, thd_volts_l, volt_l_thd, vol_l_size);
	ret |= of_property_read_u32_array(np, thd_volts_h, volt_h_thd, vol_h_size);
	ret |= check_duplicate(volt_l_thd);
	ret |= check_duplicate(volt_h_thd);

	if (ret) {
		dev_notice(&pdev->dev,
			"[%s] failed to get correct thd-volt ret=%d\n", __func__, ret);
		priv->thd_volts_size = LOW_BATTERY_LEVEL_NUM;
		priv->thd_volts = devm_kmalloc_array(&pdev->dev, priv->thd_volts_size,
						sizeof(u32), GFP_KERNEL);
		if (!priv->thd_volts)
			return -ENOMEM;
		priv->thd_volts[0] = POWER_INT0_VOLT;
		priv->thd_volts[1] = POWER_INT1_VOLT;
		priv->thd_volts[2] = POWER_INT2_VOLT;
		priv->thd_volts[3] = POWER_INT3_VOLT;
	} else {
		vol_t_size = rearrange_volt(priv->lbat_intr_info,
			volt_l_thd, volt_h_thd, vol_l_size);
		if (vol_t_size <= 0) {
			dev_notice(&pdev->dev, "[%s] Failed to rearrange_volt\n", __func__);
			return -ENODATA;
		}
		priv->thd_volts_size = vol_t_size;
		priv->thd_volts = devm_kmalloc_array(&pdev->dev, priv->thd_volts_size,
					sizeof(u32), GFP_KERNEL);
		if (!priv->thd_volts)
			return -ENOMEM;

		for (i = 0; i < vol_t_size; i++)
			priv->thd_volts[i] = priv->lbat_intr_info[i].volt_thd;
		priv->low_bat_thl_temp_volt_thd = priv->thd_volts[0];
	}

	if (vbat_thd_enable)
		priv->lbat_pt = lbat_user_register_ext("power throttling", priv->thd_volts,
							priv->thd_volts_size - 1,
							exec_low_battery_callback);

	if (IS_ERR(priv->lbat_pt)) {
		ret = PTR_ERR(priv->lbat_pt);
		if (ret != -EPROBE_DEFER) {
			dev_notice(&pdev->dev, "[%s] error ret=%d\n", __func__, ret);
		}
		return ret;
	}

	dump_thd_volts(&pdev->dev, priv->thd_volts, priv->thd_volts_size);
	ret = device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_ut);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_stop);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_level);
	if (ret) {
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);
		return ret;
	}

	priv->lvsys_status = DEACTIVATE;
	mutex_init(&priv->lock);
	if (lvsys_thd_enable)
		ret = lvsys_register_notifier(&lbat_vsys_notifier);

	if (ret)
		dev_notice(&pdev->dev, "lvsys_register_notifier error ret=%d\n", ret);

	low_bat_thl_data = priv;
	return 0;
}

static const struct of_device_id low_bat_thl_of_match[] = {
	{ .compatible = "mediatek,low_battery_throttling", },
	{ },
};
MODULE_DEVICE_TABLE(of, low_bat_thl_of_match);

static struct platform_driver low_battery_throttling_driver = {
	.driver = {
		.name = "low_battery_throttling",
		.of_match_table = low_bat_thl_of_match,
	},
	.probe = low_battery_throttling_probe,
};

module_platform_driver(low_battery_throttling_driver);
MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MTK low battery throttling driver");
MODULE_LICENSE("GPL");
