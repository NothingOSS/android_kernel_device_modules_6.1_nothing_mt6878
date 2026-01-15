// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
//#include "../mediatek/mediatek_v2/mtk_corner_pattern/panel-epd8820-boe.h"
extern int mtk_ddic_dsi_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg, bool blocking);
static atomic_t current_backlight;
#define HSA 4
#define HBP 24
#define HFP 108
#define HBP_30hz 220
#define HFP_30hz 1620

#define VSA 1
#define VBP 55
#define VFP 72
#define VFP_90 912
#define VFP_60 2592
#define VFP_30 72

#define HACT 1080
#define VACT 2392

#define REGFLAG_CMD          0xFFFA
#define REGFLAG_DELAY        0xFFFC
#define REGFLAG_UDELAY       0xFFFB
#define REGFLAG_END_OF_TABLE 0xFFFD

extern unsigned int lcm_now_state;
static struct lcm *g_ctx = NULL;
extern unsigned int is_support_write_key;
extern unsigned long fp_status;
extern unsigned int esd_readreg_per_cycle;
extern unsigned int esd_check_num;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *vddi_en_gpio;
	struct gpio_desc *dvdd_en_gpio;
	struct gpio_desc *vci_en_gpio;
	struct gpio_desc *reset_gpio;

	bool prepared;
	bool enabled;

	int error;

	bool local_hbm_en;
	bool hbm_en;
	bool hbm_wait;
};

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

struct LCM_cmd_setting_table {
	unsigned char count;
	unsigned char para_list[64];
};

static char bl_tb0[] = {0x51, 0x03, 0xFF};
static struct kobject *kobj = NULL;
static struct mtk_ddic_dsi_msg *g_cmd_msg = NULL;

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

extern char panel_name_find[128];
extern char lcm_id[3];
extern void ddic_dsi_read_cmd_test(unsigned int case_num);

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("[LCM]%s +\n", __func__);
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5 * 1000, 6 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(50, 60);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(6 * 1000, 7 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx, 0x90, 0x40);

	//DSC
	lcm_dcs_write_seq_static(ctx, 0x03, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xA3,
		0x12,0x00,0x00,0xAB,0x30,0x80,0x09,0x58,0x04,0x38,
		0x00,0x0D,0x02,0x1C,0x02,0x1C,0x02,0x00,0x02,0x0E,
		0x00,0x20,0x01,0x39,0x00,0x07,0x00,0x0C,0x08,0x00,
		0x07,0xD3,0x18,0x00,0x10,0xF0,0x07,0x10,0x20,0x00,
		0x06,0x0F,0x0F,0x33,0x0E,0x1C,0x2A,0x38,0x46,0x54,
		0x62,0x69,0x70,0x77,0x79,0x7B,0x7D,0x7E,0x02,0x02,
		0x22,0x00,0x2A,0x40,0x2A,0xBE,0x3A,0xFC,0x3A,0xFA,
		0x3A,0xF8,0x3B,0x38,0x3B,0x78,0x3B,0xB6,0x4B,0xB6,
		0x4B,0xF4,0x4B,0xF4,0x6C,0x34,0x84,0x74,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00);

	lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x04, 0x37);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x09, 0x57);
	lcm_dcs_write_seq_static(ctx, 0x43, 0x02, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x44, 0x09, 0x58);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);

	lcm_dcs_write_seq_static(ctx, 0x11);
	usleep_range(120 * 1000, 121 * 1000);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xC9, 0x1F);
	lcm_dcs_write_seq_static(ctx,0xB3,
		0x12,0x00,0x00,0x11,0x00,0x9F,0x00,0x31,0x08,
		0xC6,0x03,0x1A,0x00,0x5A,0x00,0x9F,0x00,0x04,0x02,
		0x00,0x7D,0x0F,0x12,0x14,0x00,0x00,0x02,0xAC);
	lcm_dcs_write_seq_static(ctx, 0xBF, 0x21,0x00,0xFB,0x5A,0xA0,0x06,0x17);

	lcm_dcs_write_seq_static(ctx, 0xE2, 0XEA);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0XC4);
	lcm_dcs_write_seq_static(ctx, 0xCD, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0X05);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x98);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0X21);
	lcm_dcs_write_seq_static(ctx, 0xDD, 0x39);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0XCB);
	lcm_dcs_write_seq_static(ctx, 0xB8, 0x03);

	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x00,0xFF,0x00,0xFF);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0xFB);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x1F,0xFA,0x81,0x00,0x01);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0xa9);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x0C,0x82,0x8A,0x32,0x05,0x10,0x57,0x85,0xDC,0x69,0x37,0x4E);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0xb5);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x00,0x10,0x01,0xC0,0x0E,0x00,0xC0,0x07,0x00,0x3C,0x02,0x00,0x14);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0xc2);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0xFF,0xF7,0xFE,0x3F,0xF1,0xFF,0x4F,0xF9,0xFF,0xCB,0xFE,0x1F,0xEC);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0xcf);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x00,0x03,0x1D,0x4B,0x06,0x40,0x70,0x87,0xD0,0x96,0x0A,0x8C);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0xdb);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0xe3);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0xeb);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x00,0x20,0x20,0x00,0x06,0x10,0x02,0x14);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0xf3);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x0A,0x20,0x1C,0xFA,0x06,0x00,0x00,0x00);

	lcm_dcs_write_seq_static(ctx, 0x29);
	usleep_range(10 * 1000, 11 * 1000);

	pr_info("[LCM]%s -\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;
	pr_info("[LCM]%s -\n", __func__);

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("[LCM]%s +\n", __func__);
	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	usleep_range(20 * 1000, 21 * 1000);
	lcm_dcs_write_seq_static(ctx, 0x10);
	usleep_range(80 * 1000, 81 * 1000);

	ctx->error = 0;
	ctx->prepared = false;
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(1 * 1000, 2 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->dvdd_en_gpio = devm_gpiod_get(ctx->dev, "dvdd-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->dvdd_en_gpio, 0);
	usleep_range(1 * 1000, 2 * 1000);
	devm_gpiod_put(ctx->dev, ctx->dvdd_en_gpio);

	ctx->vci_en_gpio = devm_gpiod_get(ctx->dev, "vci-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vci_en_gpio, 0);
	usleep_range(1 * 1000, 2 * 1000);
	devm_gpiod_put(ctx->dev, ctx->vci_en_gpio);

	ctx->vddi_en_gpio = devm_gpiod_get(ctx->dev, "vddi-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddi_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddi_en_gpio);

	ctx->local_hbm_en = false;
	ctx->hbm_en = false;

	lcm_now_state = 0;
	pr_info("[LCM]%s -\n", __func__);

	return 0;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
	pr_info("[LCM]%s +\n", __func__);
	if (ctx->prepared)
		return 0;

	ctx->vddi_en_gpio = devm_gpiod_get(ctx->dev, "vddi-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddi_en_gpio, 1);
	usleep_range(1 * 1000, 2 * 1000);
	devm_gpiod_put(ctx->dev, ctx->vddi_en_gpio);

	ctx->vci_en_gpio = devm_gpiod_get(ctx->dev, "vci-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vci_en_gpio, 1);
	usleep_range(1 * 1000, 2 * 1000);
	devm_gpiod_put(ctx->dev, ctx->vci_en_gpio);

	ctx->dvdd_en_gpio = devm_gpiod_get(ctx->dev, "dvdd-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->dvdd_en_gpio, 1);
	usleep_range(1 * 1000, 2 * 1000);
	devm_gpiod_put(ctx->dev, ctx->dvdd_en_gpio);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	pr_info("[LCM]%s -\n", __func__);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("[LCM]%s +\n", __func__);

	if (ctx->prepared)
		return 0;

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	lcm_now_state = 0;
	pr_info("[LCM]%s -\n", __func__);

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;
	pr_info("[LCM]%s -\n", __func__);

	return 0;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3];
	unsigned char id[3] = {0x00, 0x80, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0)
		pr_info("%s error\n", __func__);

	DDPINFO("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	DDPINFO("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}

static struct LCM_setting_table lhbm_on_cmd_tb[] = {
	{REGFLAG_CMD, 2, {0x86, 0x10}},
	{REGFLAG_CMD, 9, {0xAE, 0x82, 0x1C, 0xA0, 0x08, 0x00, 0x80, 0x08, 0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lhbm_off_cmd_tb[] = {
	{REGFLAG_CMD, 2, {0x86, 0x10}},
	{REGFLAG_CMD, 9, {0xAE, 0x02, 0x1C, 0xA0, 0x08, 0x00, 0x80, 0x08, 0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int panel_lhbm_set_cmdq(struct drm_panel *panel, void *dsi, dcs_write_gce cb,
		void *handle, bool en)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int i = 0;

	if (!cb) {
		pr_info("[LCM]cb is null, %s return -1\n", __func__);
		return 0;
	}

	if (ctx->local_hbm_en == en)
		goto done;

	if (en) {
		for (i = 0; i < sizeof(lhbm_on_cmd_tb)/sizeof(struct LCM_setting_table); i++)
			cb(dsi, handle, lhbm_on_cmd_tb[i].para_list, lhbm_on_cmd_tb[i].count);
		pr_info("[LCM], open lhbm!\n");
	} else {
		for (i = 0; i < sizeof(lhbm_off_cmd_tb)/sizeof(struct LCM_setting_table); i++)
			cb(dsi, handle, lhbm_off_cmd_tb[i].para_list, lhbm_off_cmd_tb[i].count);
		pr_info("[LCM], close lhbm!\n");
	}

	ctx->local_hbm_en = en;
	mtk_panel_proc_local_hbm(ctx->local_hbm_en);
done:
	return 0;
}

static void panel_lhbm_get_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);

	*state = ctx->local_hbm_en;
}

static struct LCM_setting_table lcm_aod_high_mode[] = {
	/* aod 60nit*/
	{REGFLAG_CMD, 2, {0x90, 0x41} },
	{REGFLAG_CMD, 5, {0x51, 0x00, 0x00, 0x0F, 0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_aod_middle_mode[] = {
	/* aod 30nit*/
	{REGFLAG_CMD, 2, {0x90, 0x41} },
	{REGFLAG_CMD, 5, {0x51, 0x00, 0x00, 0x0B, 0x2E} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_aod_low_mode[] = {
	/* aod 5nit*/
	{REGFLAG_CMD, 2, {0x90, 0x41} },
	{REGFLAG_CMD, 5, {0x51, 0x00, 0x00, 0x00, 0x03} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

extern int last_bl_level ;
static int lcm_setbacklight_cmdq(void *dsi,
		dcs_write_gce cb, void *handle, unsigned int level)
{
	int i;
	if (!cb) {
		pr_info("[LCM]cb is null, %s return -1\n", __func__);
		return -1;
	}
	if (level > 4095)
		level = 4095;

	bl_tb0[1] = (level >> 8) & 0x0F;
	bl_tb0[2] = level & 0xFF;

	if (level != 0) {
		last_bl_level = level;
		atomic_set(&current_backlight, level);
	}
	pr_info("%s+, bl_tb0[1]=0x%x, bl_tb0[2]=0x%x, last_bl_level=%d, level=%d\n", __func__,
		bl_tb0[1], bl_tb0[2], last_bl_level, level);
	if (lcm_now_state && (level != 0)) {
		// AOD mode, set aod backlight
		if (level >= 780) {
			for (i = 0; i < sizeof(lcm_aod_high_mode)/sizeof(struct LCM_setting_table); i++)
				cb(dsi, handle, lcm_aod_high_mode[i].para_list, lcm_aod_high_mode[i].count);
		} else if (level < 780 && level >= 280) {
			for (i = 0; i < sizeof(lcm_aod_middle_mode)/sizeof(struct LCM_setting_table); i++)
				cb(dsi, handle, lcm_aod_middle_mode[i].para_list, lcm_aod_middle_mode[i].count);
		} else if (level < 280) {
			for (i = 0; i < sizeof(lcm_aod_low_mode)/sizeof(struct LCM_setting_table); i++)
				cb(dsi, handle, lcm_aod_low_mode[i].para_list, lcm_aod_low_mode[i].count);
		}
		return 0;
	}
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	return 0;
}
/**
static unsigned long panel_doze_get_mode_flags(struct drm_panel *panel,
	int doze_en)
{
	unsigned long mode_flags;

	if (doze_en) {
		mode_flags = MIPI_DSI_MODE_LPM
		       | MIPI_DSI_MODE_NO_EOT_PACKET
		       | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	} else {
		mode_flags = MIPI_DSI_MODE_VIDEO
		       | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
		       | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
		       | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	}

	return mode_flags;
}
**/

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	lcm_now_state = 1;

	pr_info("[LCM]%s-\n", __func__);
	return 0;
}

static int panel_doze_enable_start(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	pr_info("[LCM]%s+\n", __func__);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	lcm_now_state = 0;

	pr_info("[LCM]%s-\n", __func__);
	return 0;
}

static int panel_doze_area(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	return 0;
}

static struct mtk_panel_params ext_params_120hz = {
	.change_fps_by_vfp_send_cmd = 1,
	.data_rate = 1038,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.platform_esdbl_rec = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	//.lp_perline_en = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2392,
		.pic_width = 1080,
		.slice_height = 13,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 313,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2048,
		.slice_bpg_offset = 2003,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0x86, 0x10} },
		.doze_dis_cmd_table[0] = {0, 2, {0x90, 0x40} },
		.bl_update_cmd_table[0] = {0, 3, {0x51, 0x03, 0x09} },
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.change_fps_by_vfp_send_cmd = 1,
	.data_rate = 1038,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.platform_esdbl_rec = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	//.lp_perline_en = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2392,
		.pic_width = 1080,
		.slice_height = 13,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 313,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2048,
		.slice_bpg_offset = 2003,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.dfps_cmd_table[0] = {0, 2, {0x86, 0x11} },
		.doze_dis_cmd_table[0] = {0, 2, {0x90, 0x40} },
		.bl_update_cmd_table[0] = {0, 3, {0x51, 0x03, 0x09} },
	},
};

static struct mtk_panel_params ext_params_60hz = {
	.change_fps_by_vfp_send_cmd = 1,
	.data_rate = 1038,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.platform_esdbl_rec = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	//.lp_perline_en = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2392,
		.pic_width = 1080,
		.slice_height = 13,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 313,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2048,
		.slice_bpg_offset = 2003,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
		.dfps_cmd_table[0] = {0, 2, {0x86, 0x12} },
		.doze_dis_cmd_table[0] = {0, 2, {0x90, 0x40} },
		.bl_update_cmd_table[0] = {0, 3, {0x51, 0x03, 0x09} },
	},
};

static struct mtk_panel_params ext_params_30hz = {
	.change_fps_by_vfp_send_cmd = 1,
	.data_rate = 1038,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.platform_esdbl_rec = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	//.lp_perline_en = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2392,
		.pic_width = 1080,
		.slice_height = 13,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 313,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2048,
		.slice_bpg_offset = 2003,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 30,
		.doze_en_cmd_table[0] = {0, 2, {0x90, 0x41} },
		.aod_high_cmd_table[0] = {0, 5, {0x51, 0x00, 0x00, 0x0F, 0xFF} },          //AOD 60nit
		.aod_middle_cmd_table[0] = {0, 5, {0x51, 0x00, 0x00, 0x0B, 0x2E} },   //AOD 30nit
		.aod_low_cmd_table[0] = {0, 5, {0x51, 0x00, 0x00, 0x00, 0x03} },            //AOD 5nit
	},
};
/*
static int panel_doze_post_disp_on(struct drm_panel *panel,
		void *dsi, dcs_write_gce cb, void *handle)
{
	int cmd = 0x29;
	if (cb) {
		pr_err("%s:%d write 0x29 for post display on\n", __func__, __LINE__);
		cb(dsi, handle, &cmd, 1);
	}
	return 0;
}
*/

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
			struct drm_connector *connector,
			struct mtk_panel_params **ext_param,
			unsigned int mode)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (!m) {
		pr_err("[LCM]%s:%d invalid display_mode\n", __func__, __LINE__);
		return ret;
	}
	if (drm_mode_vrefresh(m) == 120) {
		*ext_param = &ext_params_120hz;
	} else if (drm_mode_vrefresh(m) == 90) {
		*ext_param = &ext_params_90hz;
	} else if (drm_mode_vrefresh(m) == 60) {
		*ext_param = &ext_params_60hz;
	} else if (drm_mode_vrefresh(m) == 30) {
		*ext_param = &ext_params_30hz;
	} else
		ret = 1;

	return ret;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (!m) {
		pr_err("[LCM]%s:%d invalid display_mode\n", __func__, __LINE__);
		return ret;
	}
	if (drm_mode_vrefresh(m) == 120){
		ext->params = &ext_params_120hz;
	} else if (drm_mode_vrefresh(m) == 90){
		ext->params = &ext_params_90hz;
	} else if (drm_mode_vrefresh(m) == 60){
		ext->params = &ext_params_60hz;
	} else if (drm_mode_vrefresh(m) == 30){
		ext->params = &ext_params_30hz;
	} else {
		ret = 1;
	}

//	if (ext->params->dyn_fps.vact_timing_fps != 30) {
//		ext->params->dyn_fps.doze_dis_cmd_table[1].para_list[1] = (last_level >> 8) & 0x0F;
//		ext->params->dyn_fps.doze_dis_cmd_table[1].para_list[2] = last_level & 0xFF;
//	}

	return ret;
}

static struct mtk_panel_funcs ext_funcs = {
	.panel_poweron = lcm_panel_poweron,
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,

	.ata_check = panel_ata_check,
	.hbm_set_cmdq = NULL,
	.hbm_get_state = NULL,
	.hbm_get_wait_state = NULL,
	.hbm_set_wait_state = NULL,
	.lhbm_set_cmdq = panel_lhbm_set_cmdq,
	.lhbm_get_state = panel_lhbm_get_state,

	/* add for ramless AOD */
	.doze_get_mode_flags = NULL,//panel_doze_get_mode_flags,
	.doze_enable = panel_doze_enable,
	.doze_enable_start = panel_doze_enable_start,
	.doze_area = panel_doze_area,
	.doze_disable = panel_doze_disable,
	.doze_post_disp_on = NULL,//panel_doze_post_disp_on,
	.set_aod_light_mode = NULL,

	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
};

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static const struct drm_display_mode switch_mode_120hz = {
	.clock = 367718,
	.hdisplay = HACT,
	.hsync_start = HACT + HFP,
	.hsync_end = HACT + HFP + HSA,
	.htotal = HACT + HFP + HSA + HBP,
	.vdisplay = VACT,
	.vsync_start = VACT + VFP,
	.vsync_end = VACT + VFP + VSA,
	.vtotal = VACT + VFP + VSA + VBP,
};

static const struct drm_display_mode switch_mode_90hz = {
	.clock = 367718,
	.hdisplay = HACT,
	.hsync_start = HACT + HFP,
	.hsync_end = HACT + HFP + HSA,
	.htotal = HACT + HFP + HSA + HBP,
	.vdisplay = VACT,
	.vsync_start = VACT + VFP_90,
	.vsync_end = VACT + VFP_90 + VSA,
	.vtotal = VACT + VFP_90 + VSA + VBP,
};

static const struct drm_display_mode switch_mode_60hz = {
	.clock = 367718,
	.hdisplay = HACT,
	.hsync_start = HACT + HFP,
	.hsync_end = HACT + HFP + HSA,
	.htotal = HACT + HFP + HSA + HBP,
	.vdisplay = VACT,
	.vsync_start = VACT + VFP_60,
	.vsync_end = VACT + VFP_60 + VSA,
	.vtotal = VACT + VFP_60 + VSA + VBP,
};

static const struct drm_display_mode switch_mode_30hz = {
	.clock = 221054,
	.hdisplay = HACT,
	.hsync_start = HACT + HFP_30hz,
	.hsync_end = HACT + HFP_30hz + HSA,
	.htotal = HACT + HFP_30hz + HSA + HBP_30hz,
	.vdisplay = VACT,
	.vsync_start = VACT + VFP_30,
	.vsync_end = VACT + VFP_30 + VSA,
	.vtotal = VACT + VFP_30 + VSA + VBP,
};

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;
	struct drm_display_mode *mode_3;

	mode = drm_mode_duplicate(connector->dev, &switch_mode_120hz);
	if (!mode) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_120hz.hdisplay, switch_mode_120hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_120hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode_1 = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode_1) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_60hz.hdisplay, switch_mode_60hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);

	mode_2 = drm_mode_duplicate(connector->dev, &switch_mode_90hz);
	if (!mode_2) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_90hz.hdisplay, switch_mode_90hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_90hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_2);

	mode_3 = drm_mode_duplicate(connector->dev, &switch_mode_30hz);
	if (!mode_3) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_30hz.hdisplay, switch_mode_30hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_30hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_3);
	mode_3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_3);

	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 157;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static ssize_t aod_area_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t aod_area_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return 0;
}
static DEVICE_ATTR_RW(aod_area);

static struct attribute *aod_area_sysfs_attrs[] = {
	&dev_attr_aod_area.attr,
	NULL,
};

static struct attribute_group aod_area_sysfs_attr_group = {
	.attrs = aod_area_sysfs_attrs,
};

static ssize_t fp_status_show(struct kobject* kodjs,struct kobj_attribute *attr,char *buf)
{
	int count = 0;
	count = sprintf(buf, "hbm state: %d\n", g_ctx->local_hbm_en);
	return count;
}

static ssize_t fp_status_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count)
{
	int ret = 0;
	unsigned char tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg = g_cmd_msg;

	ret = kstrtoul(buf, 0, &fp_status);
	if (ret < 0) {
		return ret;
	}

	if (g_ctx->prepared) {
		if (!fp_status) {
			if (g_ctx->local_hbm_en == false) {
				pr_info("[%s] lhbm has been closed\n", __func__);
				return count;
			}
			cmd_msg->channel = 0;
			cmd_msg->flags = 0;
			cmd_msg->tx_cmd_num = 1;
			cmd_msg->tx_buf[0] = tx;
			cmd_msg->type[0] = 0x15;
			tx[0] = 0x86;
			tx[1] = 0x10;
			cmd_msg->tx_len[0] = 2;
			mtk_ddic_dsi_send_cmd(cmd_msg, 0);
			cmd_msg->type[0] = 0x39;
			tx[0] = 0xAE;
			tx[1] = 0x02;
			tx[2] = 0x1C;
			tx[3] = 0xA0;
			tx[4] = 0x08;
			tx[5] = 0x00;
			tx[6] = 0x80;
			tx[7] = 0x08;
			tx[8] = 0x00;
			cmd_msg->tx_len[0] = 9;
			mtk_ddic_dsi_send_cmd(cmd_msg, 0);
			g_ctx->local_hbm_en = false;
			mtk_panel_proc_local_hbm(g_ctx->local_hbm_en);
			pr_info("[LCM] node close lhbm!\n");
		}
	} else {
		pr_err("[LCM] %s panel not prepared\n", __func__);
	}

	return count;
}

static ssize_t displayid_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
	int count = 0;
	unsigned int display_id = 0;

	if (g_ctx->prepared) {
		ddic_dsi_read_cmd_test(7);
		ddic_dsi_read_cmd_test(8);
		ddic_dsi_read_cmd_test(9);
	}

	display_id = lcm_id[0] << 16 | lcm_id[1] << 8 | lcm_id[2];
	pr_info("[%s] display_id:0x%06x!!!\n",__func__, display_id);
	count = sprintf(buf, "%06x\n", display_id);

	return count;
}

static ssize_t brightnessid_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
	int count = 0, level = 0;
	level = atomic_read(&current_backlight);
	pr_info("[%s] display_bl:%d!!!\n",__func__, level);
	count = sprintf(buf, "%d\n", level);

	return count;
}

static struct LCM_cmd_setting_table set_backlight_cmd[] = {
	{3, {0x51, 0x0D, 0xBB} }
};

static ssize_t brightnessid_store(struct device *dev,
           struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, i, j;
	unsigned int state;
	unsigned char tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg = g_cmd_msg;

	ret = kstrtouint(buf, 10, &state);
	if (ret < 0) {
		goto err;
	}
	pr_info("[%s] brightness level:%d\n", __func__, state);
	set_backlight_cmd[0].para_list[1] = (state >> 8) & 0x0F;
	set_backlight_cmd[0].para_list[2] = (state) & 0xFF;

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->type[0] = 0x39;

	for (i = 0; i < sizeof(set_backlight_cmd)/sizeof(struct LCM_cmd_setting_table); i++) {
		for (j = 0; j < set_backlight_cmd[i].count; j++) {
			tx[j] = set_backlight_cmd[i].para_list[j];
		}
		cmd_msg->tx_len[0] = set_backlight_cmd[i].count;
		mtk_ddic_dsi_send_cmd(cmd_msg, 0);
	}

	atomic_set(&current_backlight, state & 0x0FFF);
err:
	return size;
}

static ssize_t backlight_reg_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
        int count = 0;

        pr_info("%s+, bl_tb0[1]=0x%x, bl_tb0[2]=0x%x \n", __func__,
                        bl_tb0[1], bl_tb0[2]);
        count = sprintf(buf, "bl_tb0[1]=0x%x, bl_tb0[2]=0x%x\n",bl_tb0[1], bl_tb0[2] );

        return count;
}

static struct kobj_attribute hbm_mode_attr = __ATTR(fp_status, 0664, fp_status_show, fp_status_store);
static DEVICE_ATTR(displayid, 0664, displayid_show, NULL);
static DEVICE_ATTR(brightnessid, 0664, brightnessid_show, brightnessid_store);
static DEVICE_ATTR(backlight_reg, 0664, backlight_reg_show, NULL);

static struct attribute *displayid_attributes[] = {
	&dev_attr_displayid.attr,
	&dev_attr_brightnessid.attr,
	&dev_attr_backlight_reg.attr,
	NULL
};

static struct attribute_group displayid_attribute_group = {
	.attrs = displayid_attributes,
};

static const struct of_device_id displayid_of_match[] = {
	{.compatible = "mediatek,display_id",},
	{},
};
MODULE_DEVICE_TABLE(of, displayid_of_match);

static int displayid_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = sysfs_create_group(&pdev->dev.kobj, &displayid_attribute_group);
	if (ret < 0) {
		pr_err("[%s] sysfs_create_group failed\n",__func__);
	}
	pr_info("%s is OK!!!\n",__func__);

	return 0;
}

static struct platform_driver displayid_driver = {
	.probe = displayid_probe,
	.driver = {
		.name = "displayid",
		.of_match_table = displayid_of_match,
	},
};

int displayid_node_init(void)
{
	return platform_driver_register(&displayid_driver);
}

int lcm_sys_node_init(void)
{
	int ret = 0;

	kobj = kobject_create_and_add("panel_feature", NULL);
	if (kobj == NULL) {
		return -ENOMEM;
	}

	ret = sysfs_create_file(kobj, &hbm_mode_attr.attr);
	if (ret < 0) {
		pr_err("[%s] sysfs_create_group failed\n",__func__);
		return -1;
	}

	pr_info("[%s] is OK!!!\n", __func__);
	return 0;
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_err("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	g_cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (!g_cmd_msg)
		return -ENOMEM;
	else
		memset(g_cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	g_ctx = ctx;

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);

	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ret = of_property_read_u32(dev->of_node, "is-support-write-key", &is_support_write_key);
	if (ret)
		pr_warn("[LCM] no is-support-write-key property set\n");
	else
		pr_info("[LCM] is-support-write-key = %d\n", is_support_write_key);

	ret = of_property_read_u32(dev->of_node, "esd-readreg-per-cycle", &esd_readreg_per_cycle);
	if (ret)
		pr_warn("[LCM] no esd-readreg-per-cycle property set\n");
	else
		pr_info("[LCM] esd-readreg-per-cycle = %d\n", esd_readreg_per_cycle);

	ret = of_property_read_u32(dev->of_node, "esd-check-num", &esd_check_num);
	if (ret)
		pr_warn("[LCM] no esd-check-num property set\n");
	else
		pr_info("[LCM] esd-check-num = %d\n", esd_check_num);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs,
			DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	ret = mtk_panel_ext_create(dev, &ext_params_60hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

	ret = sysfs_create_group(&dev->kobj, &aod_area_sysfs_attr_group);
	if (ret)
		return ret;

	ctx->local_hbm_en = false;
	ctx->hbm_en = false;
	lcm_sys_node_init();
	displayid_node_init();
	atomic_set(&current_backlight, 3515);

	snprintf(panel_name_find, sizeof(panel_name_find), "epd8820_boe");

	pr_info("%s-\n", __func__);
	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "epd8820,boe", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "epd8820,boe",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Linus Wallei <linus.walleij@linaro.org>");
MODULE_DESCRIPTION("MIPI-DSI epd8820 Panel Driver");
MODULE_LICENSE("GPL v2");
