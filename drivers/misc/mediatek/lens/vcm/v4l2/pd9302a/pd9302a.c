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
#define DRIVER_NAME "pd9302a"
#define PD9302A_I2C_SLAVE_ADDR 0x18
#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)
#define PD9302A_NAME				"pd9302a"
#define PD9302A_MAX_FOCUS_POS			1023
#define PD9302A_ORIGIN_FOCUS_POS		512
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define PD9302A_FOCUS_STEPS			1
#define PD9302A_SET_POSITION_ADDR		0x03
#define PD9302A_CMD_DELAY			0xff
#define PD9302A_CTRL_DELAY_US			10000
/*
 * This acts as the minimum granularity of lens movement.
 * Keep this value power of 2, so the control steps can be
 * uniformly adjusted for gradual lens movement, with desired
 * number of control steps.
 */
#define PD9302A_MOVE_STEPS			100
#define PD9302A_MOVE_DELAY_US			5000
/* pd9302a device structure */
struct pd9302a_device {
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
static inline struct pd9302a_device *to_pd9302a_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct pd9302a_device, ctrls);
}
static inline struct pd9302a_device *sd_to_pd9302a_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct pd9302a_device, sd);
}
struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};
static int pd9302a_set_position(struct pd9302a_device *pd9302a, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&pd9302a->sd);
	return i2c_smbus_write_word_data(client, PD9302A_SET_POSITION_ADDR,
					 swab16(val));
}
static int pd9302a_release(struct pd9302a_device *pd9302a)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;
	struct i2c_client *client = v4l2_get_subdevdata(&pd9302a->sd);
	diff_dac = PD9302A_ORIGIN_FOCUS_POS - pd9302a->focus->val;
	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		PD9302A_MOVE_STEPS;
	val = pd9302a->focus->val;
	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (PD9302A_MOVE_STEPS*(-1)) : PD9302A_MOVE_STEPS);
		ret = pd9302a_set_position(pd9302a, val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
		usleep_range(PD9302A_MOVE_DELAY_US,
			     PD9302A_MOVE_DELAY_US + 1000);
	}
	// last step to origin
	ret = pd9302a_set_position(pd9302a, PD9302A_ORIGIN_FOCUS_POS);
	if (ret) {
		LOG_INF("%s I2C failure: %d",
			__func__, ret);
		return ret;
	}
	i2c_smbus_write_byte_data(client, 0x02, 0x20);
	LOG_INF("-\n");
	return 0;
}
static int pd9302a_init(struct pd9302a_device *pd9302a)
{
	struct i2c_client *client = v4l2_get_subdevdata(&pd9302a->sd);
	int ret = 0;
	char puSendCmdArray[7][2] = {
	{0x02, 0x01}, {0x02, 0x00}, {0xFE, 0xFE},
	{0x02, 0x02}, {0x06, 0x40}, {0x07, 0x60}, {0xFE, 0xFE},
	};
	unsigned char cmd_number;
	LOG_INF("+\n");
	client->addr = PD9302A_I2C_SLAVE_ADDR >> 1;
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
static int pd9302a_power_off(struct pd9302a_device *pd9302a)
{
	int ret;
	LOG_INF("+\n");
	ret = pd9302a_release(pd9302a);
	if (ret)
		LOG_INF("pd9302a release failed!\n");
	ret = regulator_disable(pd9302a->vin);
	if (ret)
		return ret;
	ret = regulator_disable(pd9302a->vdd);
	if (ret)
		return ret;
	if (pd9302a->vcamaf_pinctrl && pd9302a->vcamaf_off)
		ret = pinctrl_select_state(pd9302a->vcamaf_pinctrl,
					pd9302a->vcamaf_off);
	LOG_INF("-\n");
	return ret;
}
static int pd9302a_power_on(struct pd9302a_device *pd9302a)
{
	int ret;
	LOG_INF("+\n");
	ret = regulator_enable(pd9302a->vin);
	if (ret < 0)
		return ret;
	ret = regulator_enable(pd9302a->vdd);
	if (ret < 0)
		return ret;
	if (pd9302a->vcamaf_pinctrl && pd9302a->vcamaf_on)
		ret = pinctrl_select_state(pd9302a->vcamaf_pinctrl,
					pd9302a->vcamaf_on);
	if (ret < 0)
		return ret;
	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(PD9302A_CTRL_DELAY_US, PD9302A_CTRL_DELAY_US + 100);
	ret = pd9302a_init(pd9302a);
	if (ret < 0)
		goto fail;
	LOG_INF("-\n");
	return 0;
fail:
	regulator_disable(pd9302a->vin);
	regulator_disable(pd9302a->vdd);
	if (pd9302a->vcamaf_pinctrl && pd9302a->vcamaf_off) {
		pinctrl_select_state(pd9302a->vcamaf_pinctrl,
				pd9302a->vcamaf_off);
	}
	return ret;
}
static int pd9302a_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct pd9302a_device *pd9302a = to_pd9302a_vcm(ctrl);
	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = pd9302a_set_position(pd9302a, ctrl->val);
		if (ret) {
			LOG_INF("%s I2C failure: %d",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}
static const struct v4l2_ctrl_ops pd9302a_vcm_ctrl_ops = {
	.s_ctrl = pd9302a_set_ctrl,
};
static int pd9302a_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct pd9302a_device *pd9302a = sd_to_pd9302a_vcm(sd);
	ret = pd9302a_power_on(pd9302a);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}
	return 0;
}
static int pd9302a_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct pd9302a_device *pd9302a = sd_to_pd9302a_vcm(sd);
	pd9302a_power_off(pd9302a);
	return 0;
}
static long pd9302a_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
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
static const struct v4l2_subdev_internal_ops pd9302a_int_ops = {
	.open = pd9302a_open,
	.close = pd9302a_close,
};
static struct v4l2_subdev_core_ops pd9302a_ops_core = {
	.ioctl = pd9302a_ops_core_ioctl,
};
static const struct v4l2_subdev_ops pd9302a_ops = {
	.core = &pd9302a_ops_core,
};
static void pd9302a_subdev_cleanup(struct pd9302a_device *pd9302a)
{
	v4l2_async_unregister_subdev(&pd9302a->sd);
	v4l2_ctrl_handler_free(&pd9302a->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&pd9302a->sd.entity);
#endif
}
static int pd9302a_init_controls(struct pd9302a_device *pd9302a)
{
	struct v4l2_ctrl_handler *hdl = &pd9302a->ctrls;
	const struct v4l2_ctrl_ops *ops = &pd9302a_vcm_ctrl_ops;
	v4l2_ctrl_handler_init(hdl, 1);
	pd9302a->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, PD9302A_MAX_FOCUS_POS, PD9302A_FOCUS_STEPS, 0);
	if (hdl->error)
		return hdl->error;
	pd9302a->sd.ctrl_handler = hdl;
	return 0;
}
static int pd9302a_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct pd9302a_device *pd9302a;
	int ret;
	LOG_INF("+\n");
	pd9302a = devm_kzalloc(dev, sizeof(*pd9302a), GFP_KERNEL);
	if (!pd9302a)
		return -ENOMEM;
	pd9302a->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(pd9302a->vin)) {
		ret = PTR_ERR(pd9302a->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}
	pd9302a->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(pd9302a->vdd)) {
		ret = PTR_ERR(pd9302a->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}
	pd9302a->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pd9302a->vcamaf_pinctrl)) {
		ret = PTR_ERR(pd9302a->vcamaf_pinctrl);
		pd9302a->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		pd9302a->vcamaf_on = pinctrl_lookup_state(
			pd9302a->vcamaf_pinctrl, "vcamaf_on");
		if (IS_ERR(pd9302a->vcamaf_on)) {
			ret = PTR_ERR(pd9302a->vcamaf_on);
			pd9302a->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}
		pd9302a->vcamaf_off = pinctrl_lookup_state(
			pd9302a->vcamaf_pinctrl, "vcamaf_off");
		if (IS_ERR(pd9302a->vcamaf_off)) {
			ret = PTR_ERR(pd9302a->vcamaf_off);
			pd9302a->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}
	v4l2_i2c_subdev_init(&pd9302a->sd, client, &pd9302a_ops);
	pd9302a->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	pd9302a->sd.internal_ops = &pd9302a_int_ops;
	ret = pd9302a_init_controls(pd9302a);
	if (ret)
		goto err_cleanup;
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&pd9302a->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;
	pd9302a->sd.entity.function = MEDIA_ENT_F_LENS;
#endif
	ret = v4l2_async_register_subdev(&pd9302a->sd);
	if (ret < 0)
		goto err_cleanup;
	LOG_INF("-\n");
	return 0;
err_cleanup:
	pd9302a_subdev_cleanup(pd9302a);
	return ret;
}
static void pd9302a_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct pd9302a_device *pd9302a = sd_to_pd9302a_vcm(sd);
	LOG_INF("+\n");
	pd9302a_subdev_cleanup(pd9302a);
	LOG_INF("-\n");
}
static const struct i2c_device_id pd9302a_id_table[] = {
	{ PD9302A_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, pd9302a_id_table);
static const struct of_device_id pd9302a_of_table[] = {
	{ .compatible = "mediatek,pd9302a" },
	{ },
};
MODULE_DEVICE_TABLE(of, pd9302a_of_table);
static struct i2c_driver pd9302a_i2c_driver = {
	.driver = {
		.name = PD9302A_NAME,
		.of_match_table = pd9302a_of_table,
	},
	.probe_new  = pd9302a_probe,
	.remove = pd9302a_remove,
	.id_table = pd9302a_id_table,
};
module_i2c_driver(pd9302a_i2c_driver);
MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("PD9302A VCM driver");
MODULE_LICENSE("GPL v2");
