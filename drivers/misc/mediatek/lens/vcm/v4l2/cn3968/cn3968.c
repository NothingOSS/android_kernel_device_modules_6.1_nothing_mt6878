// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#define DRIVER_NAME "cn3968"
#define CN3968_I2C_SLAVE_ADDR 0x18
#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)
#define CN3968_NAME				"cn3968"
#define CN3968_MAX_FOCUS_POS			1023
#define CN3968_ORIGIN_FOCUS_POS		512
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define CN3968_FOCUS_STEPS			1
#define CN3968_SET_POSITION_ADDR		0x03
#define CN3968_CMD_DELAY			0xff
#define CN3968_CTRL_DELAY_US			10000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define CN3968_MOVE_STEPS			100
#define CN3968_MOVE_DELAY_US			5000
/* cn3968 device structure */
struct cn3968_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};
#define VCM_IOC_POWER_ON         _IO('V', BASE_VIDIOC_PRIVATE + 3)
#define VCM_IOC_POWER_OFF        _IO('V', BASE_VIDIOC_PRIVATE + 4)
static inline struct cn3968_device *to_cn3968_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct cn3968_device, ctrls);
}
static inline struct cn3968_device *sd_to_cn3968_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct cn3968_device, sd);
}
struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};
static int cn3968_set_position(struct cn3968_device *cn3968, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&cn3968->sd);
	return i2c_smbus_write_word_data(client, CN3968_SET_POSITION_ADDR,
					 swab16(val));
}
static int cn3968_release(struct cn3968_device *cn3968)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&cn3968->sd);
	diff_dac = CN3968_ORIGIN_FOCUS_POS - cn3968->focus->val;
	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		CN3968_MOVE_STEPS;
	val = cn3968->focus->val;
	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (CN3968_MOVE_STEPS*(-1)) : CN3968_MOVE_STEPS);
		ret = cn3968_set_position(cn3968, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(CN3968_MOVE_DELAY_US,
			     CN3968_MOVE_DELAY_US + 1000);
	}
	// last step to origin
	ret = cn3968_set_position(cn3968, CN3968_ORIGIN_FOCUS_POS);
	if (ret) {
		LOG_INF("%s I2C failure: %d",
			__func__, ret);
		return ret;
	}
	i2c_smbus_write_byte_data(client, 0x02, 0x20);
	LOG_INF("-\n");
	return 0;
}
static int cn3968_init(struct cn3968_device *cn3968)
{
	struct i2c_client *client = v4l2_get_subdevdata(&cn3968->sd);
	int ret = 0;
	char puSendCmdArray[7][2] = {
	{0x02, 0x01}, {0x02, 0x00}, {0xFE, 0xFE},
	{0x06, 0x48}, {0x07, 0x01}, {0x08, 0x35} ,{0xFE, 0xFE},
	};
	unsigned char cmd_number;
	LOG_INF("+\n");
	client->addr = CN3968_I2C_SLAVE_ADDR >> 1;
	//ret = i2c_smbus_read_byte_data(client, 0x02);
	LOG_INF("Check HW version: %x\n", ret);
	for (cmd_number = 0; cmd_number < 7; cmd_number++) {
		if (puSendCmdArray[cmd_number][0] != 0xFE) {
			ret = i2c_smbus_write_byte_data(client,
					puSendCmdArray[cmd_number][0],
					puSendCmdArray[cmd_number][1]);
			if (ret < 0)
				return -1;
		} else {
			udelay(100);
		}
	}
	LOG_INF("-\n");
	return ret;
}
/* Power handling */
static int cn3968_power_off(struct cn3968_device *cn3968)
{
	int ret;
	LOG_INF("+\n");
	ret = cn3968_release(cn3968);
	if (ret)
		LOG_INF("cn3968 release failed!\n");
	ret = regulator_disable(cn3968->vin);
	if (ret)
		return ret;
	ret = regulator_disable(cn3968->vdd);
	if (ret)
		return ret;
	if (cn3968->vcamaf_pinctrl && cn3968->vcamaf_off)
		ret = pinctrl_select_state(cn3968->vcamaf_pinctrl,
					cn3968->vcamaf_off);
	LOG_INF("-\n");
	return ret;
}
static int cn3968_power_on(struct cn3968_device *cn3968)
{
	int ret;
	LOG_INF("+\n");
	ret = regulator_enable(cn3968->vin);
	if (ret < 0)
		return ret;
	ret = regulator_enable(cn3968->vdd);
	if (ret < 0)
		return ret;
	if (cn3968->vcamaf_pinctrl && cn3968->vcamaf_on)
		ret = pinctrl_select_state(cn3968->vcamaf_pinctrl,
					cn3968->vcamaf_on);
	if (ret < 0)
		return ret;
	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(CN3968_CTRL_DELAY_US, CN3968_CTRL_DELAY_US + 100);
	ret = cn3968_init(cn3968);
	if (ret < 0)
		goto fail;
	LOG_INF("-\n");
	return 0;
fail:
	regulator_disable(cn3968->vin);
	regulator_disable(cn3968->vdd);
	if (cn3968->vcamaf_pinctrl && cn3968->vcamaf_off) {
		pinctrl_select_state(cn3968->vcamaf_pinctrl,
				cn3968->vcamaf_off);
	}
	return ret;
}
static int cn3968_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct cn3968_device *cn3968 = to_cn3968_vcm(ctrl);
	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = cn3968_set_position(cn3968, ctrl->val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}
static const struct v4l2_ctrl_ops cn3968_vcm_ctrl_ops = {
	.s_ctrl = cn3968_set_ctrl,
};
static int cn3968_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct cn3968_device *cn3968 = sd_to_cn3968_vcm(sd);
	ret = cn3968_power_on(cn3968);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}
	return 0;
}
static int cn3968_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct cn3968_device *cn3968 = sd_to_cn3968_vcm(sd);
	cn3968_power_off(cn3968);
	return 0;
}
static long cn3968_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	LOG_INF("+\n");
	switch (cmd) {
	case VCM_IOC_POWER_ON:
	{
		// customized area
		LOG_INF("active mode\n");
	}
	break;
	case VCM_IOC_POWER_OFF:
	{
		// customized area
		LOG_INF("stand by mode\n");
	}
	break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	LOG_INF("-\n");
	return ret;
}
static const struct v4l2_subdev_internal_ops cn3968_int_ops = {
	.open = cn3968_open,
	.close = cn3968_close,
};
static struct v4l2_subdev_core_ops cn3968_ops_core = {
	.ioctl = cn3968_ops_core_ioctl,
};
static const struct v4l2_subdev_ops cn3968_ops = {
	.core = &cn3968_ops_core,
};
static void cn3968_subdev_cleanup(struct cn3968_device *cn3968)
{
	v4l2_async_unregister_subdev(&cn3968->sd);
	v4l2_ctrl_handler_free(&cn3968->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&cn3968->sd.entity);
#endif
}
static int cn3968_init_controls(struct cn3968_device *cn3968)
{
	struct v4l2_ctrl_handler *hdl = &cn3968->ctrls;
	const struct v4l2_ctrl_ops *ops = &cn3968_vcm_ctrl_ops;
	v4l2_ctrl_handler_init(hdl, 1);
	cn3968->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, CN3968_MAX_FOCUS_POS, CN3968_FOCUS_STEPS, 0);
	if (hdl->error)
		return hdl->error;
	cn3968->sd.ctrl_handler = hdl;
	return 0;
}
static int cn3968_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct cn3968_device *cn3968;
	int ret;
	LOG_INF("+\n");
	cn3968 = devm_kzalloc(dev, sizeof(*cn3968), GFP_KERNEL);
	if (!cn3968)
		return -ENOMEM;
	cn3968->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(cn3968->vin)) {
		ret = PTR_ERR(cn3968->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}
	cn3968->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(cn3968->vdd)) {
		ret = PTR_ERR(cn3968->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}
	cn3968->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(cn3968->vcamaf_pinctrl)) {
		ret = PTR_ERR(cn3968->vcamaf_pinctrl);
		cn3968->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		cn3968->vcamaf_on = pinctrl_lookup_state(
			cn3968->vcamaf_pinctrl, "vcamaf_on");
		if (IS_ERR(cn3968->vcamaf_on)) {
			ret = PTR_ERR(cn3968->vcamaf_on);
			cn3968->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}
		cn3968->vcamaf_off = pinctrl_lookup_state(
			cn3968->vcamaf_pinctrl, "vcamaf_off");
		if (IS_ERR(cn3968->vcamaf_off)) {
			ret = PTR_ERR(cn3968->vcamaf_off);
			cn3968->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}
	v4l2_i2c_subdev_init(&cn3968->sd, client, &cn3968_ops);
	cn3968->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	cn3968->sd.internal_ops = &cn3968_int_ops;
	ret = cn3968_init_controls(cn3968);
	if (ret)
		goto err_cleanup;
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&cn3968->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;
	cn3968->sd.entity.function = MEDIA_ENT_F_LENS;
#endif
	ret = v4l2_async_register_subdev(&cn3968->sd);
	if (ret < 0)
		goto err_cleanup;
	LOG_INF("-\n");
	return 0;
err_cleanup:
	cn3968_subdev_cleanup(cn3968);
	return ret;
}
static void cn3968_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct cn3968_device *cn3968 = sd_to_cn3968_vcm(sd);
	LOG_INF("+\n");
	cn3968_subdev_cleanup(cn3968);
	LOG_INF("-\n");
}
static const struct i2c_device_id cn3968_id_table[] = {
	{ CN3968_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, cn3968_id_table);
static const struct of_device_id cn3968_of_table[] = {
	{ .compatible = "mediatek,cn3968" },
	{ },
};
MODULE_DEVICE_TABLE(of, cn3968_of_table);
static struct i2c_driver cn3968_i2c_driver = {
	.driver = {
		.name = CN3968_NAME,
		.of_match_table = cn3968_of_table,
	},
	.probe_new  = cn3968_probe,
	.remove = cn3968_remove,
	.id_table = cn3968_id_table,
};
module_i2c_driver(cn3968_i2c_driver);
MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("CN3968 VCM driver");
MODULE_LICENSE("GPL v2");
