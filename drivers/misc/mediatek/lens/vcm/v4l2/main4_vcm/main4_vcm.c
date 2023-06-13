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

#define DRIVER_NAME "main4_vcm"
#define MAIN4_VCM_I2C_SLAVE_ADDR 0x18

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define MAIN4_VCM_NAME				"main4_vcm"
#define MAIN4_VCM_MAX_FOCUS_POS			1023
/*
 * This sets the minimum granularity for the focus positions.
 * A value of 1 gives maximum accuracy for a desired focus position
 */
#define MAIN4_VCM_FOCUS_STEPS			1

#define REGULATOR_MAXSIZE			16

static const char * const ldo_names[] = {
	"vin",
	"vdd",
};

/* main4_vcm device structure */
struct main4_vcm_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
	struct regulator *ldo[REGULATOR_MAXSIZE];
};

#define I2CCOMM_MAXSIZE 4

/* I2C format */
struct stVCM_I2CFormat {
	/* Register address */
	uint8_t Addr[I2CCOMM_MAXSIZE];
	uint8_t AddrNum;

	/* Register Data */
	uint8_t CtrlData[I2CCOMM_MAXSIZE];
	uint8_t BitR[I2CCOMM_MAXSIZE];
	uint8_t Mask1[I2CCOMM_MAXSIZE];
	uint8_t BitL[I2CCOMM_MAXSIZE];
	uint8_t Mask2[I2CCOMM_MAXSIZE];
	uint8_t DataNum;
};

#define I2CSEND_MAXSIZE 4

struct VcmDriverConfig {
	// Init param
	unsigned int ctrl_delay_us;
	char wr_table[16][3];

	// Per-frame param
	unsigned int slave_addr;
	uint8_t I2CSendNum;
	struct stVCM_I2CFormat I2Cfmt[I2CSEND_MAXSIZE];

	// Uninit param
	unsigned int origin_focus_pos;
	unsigned int move_steps;
	unsigned int move_delay_us;
	char wr_rls_table[8][3];

	// Capacity
	int32_t vcm_bits;
	int32_t af_calib_bits;
};

struct mtk_vcm_info {
	struct VcmDriverConfig *p_vcm_info;
};

struct VcmDriverConfig g_vcmconfig;

/* Control commnad */
#define VIDIOC_MTK_S_LENS_INFO _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct mtk_vcm_info)

static inline struct main4_vcm_device *to_main4_vcm_vcm(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct main4_vcm_device, ctrls);
}

static inline struct main4_vcm_device *sd_to_main4_vcm_vcm(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct main4_vcm_device, sd);
}

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static void register_setting(struct i2c_client *client, char table[][3], int table_size)
{
	int ret = 0, read_count = 0, i = 0, j = 0;

	for (i = 0; i < table_size; ++i) {

		LOG_INF("table[%d] = [0x%x 0x%x 0x%x]\n",
			i, table[i][0],
			table[i][1],
			table[i][2]);

		if (table[i][0] == 0)
			break;

		// write command
		if (table[i][0] == 0x1) {

			// write register
			ret = i2c_smbus_write_byte_data(client,
					table[i][1],
					table[i][2]);
			if (ret < 0) {
				LOG_INF(
					"i2c write fail: %d, table[%d] = [0x%x 0x%x 0x%x]\n",
					ret, i, table[i][0],
					table[i][1],
					table[i][2]);
			}

		// read command
		} else if (table[i][0] == 0x2) {
			read_count = 0;

			// read register and check value
			do {
				if (read_count > 10) {
					LOG_INF(
						"timeout, i2c read fail: %d, table[%d] = [0x%x 0x%x 0x%x]\n",
						ret, i, table[i][0],
						table[i][1],
						table[i][2]);
					break;
				}
				ret = i2c_smbus_read_byte_data(client,
					table[i][1]);
				read_count++;
			} while (ret != table[i][2]);

		// delay command
		} else if (table[i][0] == 0x3) {

			LOG_INF("delay time: %dms\n", table[i][1]);
			mdelay(table[i][1]);

		// read command w/o check result value
		} else if (table[i][0] == 0x4) {

			// read register
			for (j = 0; j < table[i][2]; ++j) {
				ret = i2c_smbus_read_byte_data(client,
					table[i][1]);
				if (ret < 0)
					LOG_INF(
						"i2c read fail: %d, table[%d] = [0x%x 0x%x 0x%x]\n",
						ret, i, table[i][0],
						table[i][1],
						table[i][2]);
				else
					LOG_INF(
						"table[%d] = [0x%x 0x%x 0x%x], result value is 0x%x",
						i, table[i][0], table[i][1],
						table[i][2], ret);
			}

		} else {
			// reserved
		}
		udelay(100);
	}
}

static int main4_vcm_set_position(struct main4_vcm_device *main4_vcm, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&main4_vcm->sd);
	int ret = -1;
	char puSendCmd[3] = {0};
	unsigned int nArrayIndex = 0;
	int i = 0, j = 0;
	int retry = 3;

	for (i = 0; i < g_vcmconfig.I2CSendNum; ++i, nArrayIndex = 0) {
		int nCommNum = g_vcmconfig.I2Cfmt[i].AddrNum +
			g_vcmconfig.I2Cfmt[i].DataNum;

		// Fill address
		for (j = 0; j < g_vcmconfig.I2Cfmt[i].AddrNum; ++j) {
			if (nArrayIndex >= ARRAY_SIZE(puSendCmd)) {
				LOG_INF("nArrayIndex(%d) exceeds the size of puSendCmd(%d)\n",
					nArrayIndex, (int)ARRAY_SIZE(puSendCmd));
				return -1;
			}
			puSendCmd[nArrayIndex] = g_vcmconfig.I2Cfmt[i].Addr[j];
			++nArrayIndex;
		}

		// Fill data
		for (j = 0; j < g_vcmconfig.I2Cfmt[i].DataNum; ++j) {
			if (nArrayIndex >= ARRAY_SIZE(puSendCmd)) {
				LOG_INF("nArrayIndex(%d) exceeds the size of puSendCmd(%d)\n",
					nArrayIndex, (int)ARRAY_SIZE(puSendCmd));
				return -1;
			}
			puSendCmd[nArrayIndex] =
				((((val >> g_vcmconfig.I2Cfmt[i].BitR[j]) &
				g_vcmconfig.I2Cfmt[i].Mask1[j]) <<
				g_vcmconfig.I2Cfmt[i].BitL[j]) &
				g_vcmconfig.I2Cfmt[i].Mask2[j]) |
				g_vcmconfig.I2Cfmt[i].CtrlData[j];
			++nArrayIndex;
		}

		while (retry-- > 0) {
			ret = i2c_master_send(client, puSendCmd, nCommNum);
			if (ret >= 0)
				break;
		}

		if (ret < 0) {
			LOG_INF(
				"puSendCmd I2C failure i2c_master_send: %d, I2Cfmt[%d].AddrNum/DataNum: %d/%d\n",
				ret, i, g_vcmconfig.I2Cfmt[i].AddrNum,
				g_vcmconfig.I2Cfmt[i].DataNum);
			return ret;
		}
	}

	return ret;
}

static int main4_vcm_release(struct main4_vcm_device *main4_vcm)
{
	int ret, val;
	int diff_dac = 0;
	int nStep_count = 0;
	int i = 0;

	struct i2c_client *client = v4l2_get_subdevdata(&main4_vcm->sd);

	diff_dac = g_vcmconfig.origin_focus_pos - main4_vcm->focus->val;

	nStep_count = (diff_dac < 0 ? (diff_dac*(-1)) : diff_dac) /
		g_vcmconfig.move_steps;

	val = main4_vcm->focus->val;

	for (i = 0; i < nStep_count; ++i) {
		val += (diff_dac < 0 ? (g_vcmconfig.move_steps*(-1)) :
			g_vcmconfig.move_steps);

		ret = main4_vcm_set_position(main4_vcm, val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d\n",
				__func__, ret);
			return ret;
		}
		usleep_range(g_vcmconfig.move_delay_us,
			g_vcmconfig.move_delay_us + 1000);
	}

	// last step to origin
	ret = main4_vcm_set_position(main4_vcm, g_vcmconfig.origin_focus_pos);
	if (ret < 0) {
		LOG_INF("%s I2C failure: %d\n",
			__func__, ret);
		return ret;
	}

	register_setting(client, g_vcmconfig.wr_rls_table, 8);

	return 0;
}

static int main4_vcm_init(struct main4_vcm_device *main4_vcm)
{
	struct i2c_client *client = v4l2_get_subdevdata(&main4_vcm->sd);
	int ret = 0;

	LOG_INF("+\n");

	client->addr = g_vcmconfig.slave_addr >> 1;

	register_setting(client, g_vcmconfig.wr_table, 16);

	LOG_INF("-\n");

	return ret;
}

/* Power handling */
static int main4_vcm_power_off(struct main4_vcm_device *main4_vcm)
{
	int ret = 0;
	int ldo_num = 0;
	int i = 0;

	LOG_INF("+\n");

	ret = main4_vcm_release(main4_vcm);
	if (ret)
		LOG_INF("main4_vcm release failed!\n");

	ldo_num = ARRAY_SIZE(ldo_names);
	if (ldo_num > REGULATOR_MAXSIZE)
		ldo_num = REGULATOR_MAXSIZE;
	for (i = 0; i < ldo_num; i++) {
		if (main4_vcm->ldo[i]) {
			ret = regulator_disable(main4_vcm->ldo[i]);
			if (ret < 0)
				LOG_INF("cannot disable %d regulator\n", i);
		}
	}

	if (main4_vcm->vcamaf_pinctrl && main4_vcm->vcamaf_off)
		ret = pinctrl_select_state(main4_vcm->vcamaf_pinctrl,
					main4_vcm->vcamaf_off);

	return ret;
}

static int main4_vcm_power_on(struct main4_vcm_device *main4_vcm)
{
	int ret = 0;
	int ldo_num = 0;
	int i = 0;

	LOG_INF("+\n");

	ldo_num = ARRAY_SIZE(ldo_names);
	if (ldo_num > REGULATOR_MAXSIZE)
		ldo_num = REGULATOR_MAXSIZE;
	for (i = 0; i < ldo_num; i++) {
		if (main4_vcm->ldo[i]) {
			ret = regulator_enable(main4_vcm->ldo[i]);
			if (ret < 0)
				LOG_INF("cannot enable %d regulator\n", i);
		}
	}

	if (main4_vcm->vcamaf_pinctrl && main4_vcm->vcamaf_on)
		ret = pinctrl_select_state(main4_vcm->vcamaf_pinctrl,
					main4_vcm->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(g_vcmconfig.ctrl_delay_us, g_vcmconfig.ctrl_delay_us + 100);

	return ret;
}

static int main4_vcm_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct main4_vcm_device *main4_vcm = to_main4_vcm_vcm(ctrl);

	if (ctrl->id == V4L2_CID_FOCUS_ABSOLUTE) {
		LOG_INF("pos(%d)\n", ctrl->val);
		ret = main4_vcm_set_position(main4_vcm, ctrl->val);
		if (ret < 0) {
			LOG_INF("%s I2C failure: %d\n",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct v4l2_ctrl_ops main4_vcm_vcm_ctrl_ops = {
	.s_ctrl = main4_vcm_set_ctrl,
};

static int main4_vcm_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;
	struct main4_vcm_device *main4_vcm = sd_to_main4_vcm_vcm(sd);

	LOG_INF("+\n");

	ret = main4_vcm_power_on(main4_vcm);
	if (ret < 0) {
		LOG_INF("power on fail, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int main4_vcm_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct main4_vcm_device *main4_vcm = sd_to_main4_vcm_vcm(sd);

	LOG_INF("+\n");

	main4_vcm_power_off(main4_vcm);

	return 0;
}

static long main4_vcm_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct main4_vcm_device *main4_vcm = sd_to_main4_vcm_vcm(sd);

	switch (cmd) {

	case VIDIOC_MTK_S_LENS_INFO:
	{
		struct mtk_vcm_info *info = arg;

		if (copy_from_user(&g_vcmconfig,
				   (void *)info->p_vcm_info,
				   sizeof(struct VcmDriverConfig)) != 0) {
			LOG_INF("VIDIOC_MTK_S_LENS_INFO copy_from_user failed\n");
			ret = -EFAULT;
			break;
		}

		LOG_INF("slave_addr: 0x%x\n", g_vcmconfig.slave_addr);

		ret = main4_vcm_init(main4_vcm);
		if (ret < 0)
			LOG_INF("init error\n");
	}
	break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static const struct v4l2_subdev_internal_ops main4_vcm_int_ops = {
	.open = main4_vcm_open,
	.close = main4_vcm_close,
};

static const struct v4l2_subdev_core_ops main4_vcm_ops_core = {
	.ioctl = main4_vcm_ops_core_ioctl,
};

static const struct v4l2_subdev_ops main4_vcm_ops = {
	.core = &main4_vcm_ops_core,
};

static void main4_vcm_subdev_cleanup(struct main4_vcm_device *main4_vcm)
{
	v4l2_async_unregister_subdev(&main4_vcm->sd);
	v4l2_ctrl_handler_free(&main4_vcm->ctrls);
#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&main4_vcm->sd.entity);
#endif
}

static int main4_vcm_init_controls(struct main4_vcm_device *main4_vcm)
{
	struct v4l2_ctrl_handler *hdl = &main4_vcm->ctrls;
	const struct v4l2_ctrl_ops *ops = &main4_vcm_vcm_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 1);

	main4_vcm->focus = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE,
			  0, MAIN4_VCM_MAX_FOCUS_POS, MAIN4_VCM_FOCUS_STEPS, 0);

	if (hdl->error)
		return hdl->error;

	main4_vcm->sd.ctrl_handler = hdl;

	return 0;
}

static int main4_vcm_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct main4_vcm_device *main4_vcm;
	int ret;
	int ldo_num;
	int i;

	LOG_INF("+\n");

	main4_vcm = devm_kzalloc(dev, sizeof(*main4_vcm), GFP_KERNEL);
	if (!main4_vcm)
		return -ENOMEM;

	ldo_num = ARRAY_SIZE(ldo_names);
	if (ldo_num > REGULATOR_MAXSIZE)
		ldo_num = REGULATOR_MAXSIZE;
	for (i = 0; i < ldo_num; i++) {
		main4_vcm->ldo[i] = devm_regulator_get(dev, ldo_names[i]);
		if (IS_ERR(main4_vcm->ldo[i])) {
			LOG_INF("cannot get %s regulator\n", ldo_names[i]);
			main4_vcm->ldo[i] = NULL;
		}
	}

	main4_vcm->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(main4_vcm->vcamaf_pinctrl)) {
		ret = PTR_ERR(main4_vcm->vcamaf_pinctrl);
		main4_vcm->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		main4_vcm->vcamaf_on = pinctrl_lookup_state(
			main4_vcm->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(main4_vcm->vcamaf_on)) {
			ret = PTR_ERR(main4_vcm->vcamaf_on);
			main4_vcm->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		main4_vcm->vcamaf_off = pinctrl_lookup_state(
			main4_vcm->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(main4_vcm->vcamaf_off)) {
			ret = PTR_ERR(main4_vcm->vcamaf_off);
			main4_vcm->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&main4_vcm->sd, client, &main4_vcm_ops);
	main4_vcm->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	main4_vcm->sd.internal_ops = &main4_vcm_int_ops;

	ret = main4_vcm_init_controls(main4_vcm);
	if (ret)
		goto err_cleanup;

#if IS_ENABLED(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&main4_vcm->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	main4_vcm->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&main4_vcm->sd);
	if (ret < 0)
		goto err_cleanup;

	return 0;

err_cleanup:
	main4_vcm_subdev_cleanup(main4_vcm);
	return ret;
}

static void main4_vcm_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct main4_vcm_device *main4_vcm = sd_to_main4_vcm_vcm(sd);

	LOG_INF("+\n");

	main4_vcm_subdev_cleanup(main4_vcm);
}

static const struct i2c_device_id main4_vcm_id_table[] = {
	{ MAIN4_VCM_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, main4_vcm_id_table);

static const struct of_device_id main4_vcm_of_table[] = {
	{ .compatible = "mediatek,main4_vcm" },
	{ },
};
MODULE_DEVICE_TABLE(of, main4_vcm_of_table);

static struct i2c_driver main4_vcm_i2c_driver = {
	.driver = {
		.name = MAIN4_VCM_NAME,
		.of_match_table = main4_vcm_of_table,
	},
	.probe_new  = main4_vcm_probe,
	.remove = main4_vcm_remove,
	.id_table = main4_vcm_id_table,
};

module_i2c_driver(main4_vcm_i2c_driver);

MODULE_AUTHOR("Po-Hao Huang <Po-Hao.Huang@mediatek.com>");
MODULE_DESCRIPTION("MAIN4_VCM VCM driver");
MODULE_LICENSE("GPL");
