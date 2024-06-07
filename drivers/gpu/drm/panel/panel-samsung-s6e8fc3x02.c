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
#include "../mediatek/mediatek_v2/mtk_corner_pattern/panel-samsung-s6e8fc3x02_rc.h"
extern int mtk_ddic_dsi_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg, bool blocking);
unsigned int g_level = 0;

#define HSA 16
#define HBP 24
#define HFP 173
#define HFP_30hz 2040

#define VSA 2
#define VBP 10
#define VFP 20
#define VFP_60 2452

#define HACT 1080
#define VACT 2400

#define REGFLAG_CMD          0xFFFA
#define REGFLAG_DELAY        0xFFFC
#define REGFLAG_UDELAY       0xFFFB
#define REGFLAG_END_OF_TABLE 0xFFFD

unsigned int lcm_now_state;
EXPORT_SYMBOL(lcm_now_state);
unsigned int lcm_vdc_state = 0;
unsigned int lcm_hbmoff_state = 0;
static struct lcm *g_ctx = NULL;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *vddi_en_gpio;
	struct gpio_desc *vci_en_gpio;
	struct gpio_desc *reset_gpio;

	bool prepared;
	bool enabled;

	int error;

	bool hbm_en;
	bool hbm_wait;
	bool hbm_stat;
};

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

struct LCM_mtk_setting_table {
	unsigned int count;
	unsigned char para_list[256];
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

static int panel_send_pack_cmd(void *dsi, struct LCM_mtk_setting_table *table,
			unsigned int lcm_cmd_count, dcs_write_gce_pack cb, void *handle)
{
	unsigned int i = 0;
	struct mtk_ddic_dsi_cmd send_cmd_to_ddic;

	if (lcm_cmd_count > MAX_TX_CMD_NUM_PACK) {
		pr_info("%s,out of mtk_ddic_dsi_cmd\n", __func__);
		return 0;
	}

	for (i = 0; i < lcm_cmd_count; i++) {
		send_cmd_to_ddic.mtk_ddic_cmd_table[i].cmd_num = table[i].count;
		send_cmd_to_ddic.mtk_ddic_cmd_table[i].para_list = table[i].para_list;
	}
	send_cmd_to_ddic.is_hs = 1;
	send_cmd_to_ddic.is_package = 1;
	send_cmd_to_ddic.cmd_count = lcm_cmd_count;

	cb(dsi, handle, &send_cmd_to_ddic);

	return 0;
}

static struct LCM_mtk_setting_table cmd_tb[] = {
	{2, {0x53, 0x20}},
	{3, {0x51, 0x0F, 0xFF}},
	{2, {0xF7, 0x0B}},
};

static struct LCM_mtk_setting_table elvss_cmd_tb[] = {
	{3, {0xF0, 0x5A, 0x5A}},
	{4, {0xB0, 0x00, 0x0C, 0xB2}},
	{2, {0xB2, 0x20}},
	{3, {0xF0, 0xA5, 0xA5}},
};

static struct LCM_mtk_setting_table vdc_on_cmd_tb[] = {
	{3, {0xF0, 0x5A, 0x5A}},
	{4, {0xB0, 0x00, 0x89, 0xB1}},
	{2, {0xB1, 0x0D}},
	{3, {0xF0, 0xA5, 0xA5}},
};

static struct LCM_mtk_setting_table vdc_off_cmd_tb[] = {
	{3, {0xF0, 0x5A, 0x5A}},
	{4, {0xB0, 0x00, 0x89, 0xB1}},
	{2, {0xB1, 0x2D}},
	{3, {0xF0, 0xA5, 0xA5}},
};

static struct LCM_mtk_setting_table hbm_cmd_tb[] = {
	{2, {0x53, 0xE0}},
	{3, {0x51, 0x05, 0xB6}},
	{2, {0xF7, 0x0B}},
};

static int samsung_set_backlight_pack(void *dsi, dcs_write_gce_pack cb,
		void *handle, unsigned int level)
{
	int cmd_cnt = 0;

	if (!cb) {
		pr_info("[LCM]cb is null, %s return -1\n", __func__);
		return -1;
	}

	if (level > 4096)
		level = 4096;

	if (lcm_hbmoff_state == 1) {
		cmd_cnt = sizeof(elvss_cmd_tb) / sizeof(struct LCM_mtk_setting_table);
		panel_send_pack_cmd(dsi, elvss_cmd_tb, cmd_cnt, cb, handle);
		pr_info("%s set elvssdly off\n", __func__);
		lcm_hbmoff_state = 0;
	}

	if (level > 4090 && !lcm_vdc_state) {
		cmd_cnt = sizeof(vdc_on_cmd_tb) / sizeof(struct LCM_mtk_setting_table);
		panel_send_pack_cmd(dsi, vdc_on_cmd_tb, cmd_cnt, cb, handle);
		lcm_vdc_state = 1;
		pr_info("%s set vdc_on\n", __func__);
	} else if (lcm_vdc_state && level <= 4090) {
		cmd_cnt = sizeof(vdc_off_cmd_tb) / sizeof(struct LCM_mtk_setting_table);
		panel_send_pack_cmd(dsi, vdc_off_cmd_tb, cmd_cnt, cb, handle);
		lcm_vdc_state = 0;
		pr_info("%s set vdc_off\n", __func__);
	}

	if (level >= 417 && level <= 424)
		level = 425;

	bl_tb0[1] = (level >> 8) & 0xFF;
	bl_tb0[2] = level & 0xFF;

	g_level = level;

	if (g_ctx->hbm_stat == true || g_ctx->hbm_en == true) {
		pr_info("%s+, hbm mode set_level=%d\n", __func__, level);
		return 0;
	}

	pr_info("%s, bl_tb0[1]=0x%x, bl_tb0[2]=0x%x, bl_level:%d\n", __func__, bl_tb0[1], bl_tb0[2], level);

	if (level > 0x7FF) {
		level = level - 0x800;
		hbm_cmd_tb[1].para_list[1] = (level >> 8) & 0xf;
		hbm_cmd_tb[1].para_list[2] = level & 0xff;
		cmd_cnt = sizeof(hbm_cmd_tb) / sizeof(struct LCM_mtk_setting_table);
		panel_send_pack_cmd(dsi, hbm_cmd_tb, cmd_cnt, cb, handle);
	} else {
		cmd_tb[1].para_list[1] = bl_tb0[1];
		cmd_tb[1].para_list[2] = bl_tb0[2];
		cmd_cnt = sizeof(cmd_tb) / sizeof(struct LCM_mtk_setting_table);
		panel_send_pack_cmd(dsi, cmd_tb, cmd_cnt, cb, handle);
	}

	return 0;
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

static int lcm_panel_check_data(struct lcm *ctx, u8 reg, u8 *reg_value)
{
	ssize_t ret;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	ret = mipi_dsi_dcs_read(dsi, reg, reg_value, sizeof(*reg_value));
	if(ret <= 0) {
		pr_err("%s error, reg=0x%02x\n", __func__, reg);
		return ret;
	}

	return 0;
}

char panel_name_find[128] = "lcd unknow";
EXPORT_SYMBOL(panel_name_find);
extern char lcm_id[3];
extern void ddic_dsi_read_cmd_test(unsigned int case_num);
void lcm_panel_get_data(void)
{
	ddic_dsi_read_cmd_test(7);
	ddic_dsi_read_cmd_test(8);
	ddic_dsi_read_cmd_test(9);

	if (lcm_id[0] != 0)
		snprintf(panel_name_find, sizeof(panel_name_find), "samsung_s6e8fc3x02");
}
EXPORT_SYMBOL(lcm_panel_get_data);

static void lcm_panel_init(struct lcm *ctx)
{
	unsigned int bl_level = 0xff;
	u8 reg_value;
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 11 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 11 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_panel_check_data(ctx, 0xDC, &reg_value);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	pr_info("[LCM]0xDC reg_value=0x%x\n", reg_value);

	lcm_dcs_write_seq_static(ctx, 0x9F, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x11);
	usleep_range(20 * 1000, 21 * 1000);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x01, 0x31);
	if(reg_value == 0x01) {  //96.0MHz
		lcm_dcs_write_seq_static(ctx, 0xDF, 0x09, 0x30, 0x95, 0x44, 0x36, 0x44, 0x36);
	} else {  //91.8MHz
		lcm_dcs_write_seq_static(ctx, 0xDF, 0x09, 0x30, 0x95, 0x41, 0x3A, 0x41, 0x3A);
	}
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x9E,
		0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
		0x04, 0x38, 0x00, 0x28, 0x02, 0x1C, 0x02, 0x1C,
		0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0xDD,
		0x00, 0x07, 0x00, 0x0C, 0x02, 0x77, 0x02, 0x8B,
		0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
		0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
		0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
		0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
		0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
		0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
		0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
		0x00);

// Flat Gamma Control
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x89, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x2D);

	lcm_dcs_write_seq_static(ctx, 0x60, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);

// VINT Voltage control
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x11, 0xFE);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);

// Dimming Setting
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0D, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0C, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x20);
	if (g_level>0x7FF) {
		bl_level = g_level - 0x800;
		lcm_dcs_write_seq_static(ctx, 0x53, 0xE0);
        } else {
		bl_level = g_level;
		lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	}
	bl_tb0[1] = (bl_level>>8)&0xf;
	bl_tb0[2] = (bl_level)&0xff;
	lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));

	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);

// Err FG Setting
	lcm_dcs_write_seq_static(ctx, 0xED, 0x01, 0xCD, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xE1, 0x83);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x1F);

	usleep_range(100 * 1000, 101 * 1000);
	lcm_dcs_write_seq_static(ctx, 0x29);

	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);
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

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("[LCM]%s ++\n", __func__);
	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(20);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(160);

	ctx->error = 0;
	ctx->prepared = false;
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(1 * 1000, 5 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	//usleep_range(10 * 1000, 15 * 1000);
	ctx->hbm_en = false;
	ctx->hbm_stat = false;
  	lcm_now_state = 0;
	ctx->vci_en_gpio = devm_gpiod_get(ctx->dev, "vci-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vci_en_gpio, 0);
	usleep_range(1 * 1000, 5 * 1000);
	devm_gpiod_put(ctx->dev, ctx->vci_en_gpio);

	ctx->vddi_en_gpio = devm_gpiod_get(ctx->dev, "vddi-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddi_en_gpio, 0);
	//usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->vddi_en_gpio);
	pr_info("[LCM]%s --\n", __func__);

	return 0;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->prepared)
		return 0;
	ctx->vddi_en_gpio = devm_gpiod_get(ctx->dev, "vddi-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddi_en_gpio, 1);
	usleep_range(1 * 1000, 5 * 1000);
	devm_gpiod_put(ctx->dev, ctx->vddi_en_gpio);

	ctx->vci_en_gpio = devm_gpiod_get(ctx->dev, "vci-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vci_en_gpio, 1);
	usleep_range(1 * 1000, 5 * 1000);
	devm_gpiod_put(ctx->dev, ctx->vci_en_gpio);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("[LCM]%s ++\n", __func__);
	lcm_panel_poweron(panel);

	if (ctx->prepared)
		return 0;

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	lcm_now_state = 0;
	pr_info("[LCM]%s --\n", __func__);

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
	unsigned char id[3] = {0xb3, 0x2, 0x1};
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

static int lcm_setbacklight_cmdq(void *dsi,
		dcs_write_gce cb, void *handle, unsigned int level)
{

	pr_info("%s\n", __func__);

	if (!cb) {
		pr_info("[LCM]cb is null, lcm_setbacklight_cmdq return -1\n");
		return -1;
	}

	bl_tb0[1] = (level >> 8) & 0xFF;
	bl_tb0[2] = level & 0xFF;

	return 0;
}

static struct LCM_mtk_setting_table elvssdly_cmd_tb[] = {
	{3, {0xF0, 0x5A, 0x5A}},
	{4, {0xB0, 0x00, 0x0C, 0xB2}},
	{2, {0xB2, 0x30}},
	{2, {0x53, 0xE0}},
	{3, {0x51, 0x05, 0xF4}},
	{2, {0xF7, 0x0B}},
	{3, {0xF0, 0xA5, 0xA5}},
};

static struct LCM_mtk_setting_table hbm_code_cmd_tb[] = {
	{2, {0x53, 0xE0}},
	{3, {0x51, 0x05, 0xF4}},
	{2, {0xF7, 0x0B}},
};

static struct LCM_mtk_setting_table backlight_code_cmd_tb[] = {
	{2, {0x53, 0x20}},
	{3, {0x51, 0x05, 0xF4}},
	{2, {0xF7, 0x0B}},
};

static int panel_set_hbm_pack(struct drm_panel *panel, void *dsi, dcs_write_gce_pack cb,
		void *handle, bool en)
{
		struct lcm *ctx = panel_to_lcm(panel);
	unsigned int level = 0xff;
	int cmd_cnt = 0;

	if (!cb) {
		pr_info("[LCM]cb is null, %s return -1\n", __func__);
		return 0;
	}

	if (ctx->hbm_en == en)
		goto done;

	if (en) {
		cmd_cnt = sizeof(elvssdly_cmd_tb) / sizeof(struct LCM_mtk_setting_table);
		panel_send_pack_cmd(dsi, elvssdly_cmd_tb, cmd_cnt, cb, handle);
		pr_info("[LCM]hbm, set to hbm bl!\n");
	} else {
		lcm_hbmoff_state = 1;
		if (g_level > 0x7FF) {
			level = g_level - 0x800;
			hbm_code_cmd_tb[1].para_list[1] = (level>>8)&0xf;
			hbm_code_cmd_tb[1].para_list[2] = (level)&0xff;
			cmd_cnt = sizeof(hbm_code_cmd_tb) / sizeof(struct LCM_mtk_setting_table);
			panel_send_pack_cmd(dsi, hbm_code_cmd_tb, cmd_cnt, cb, handle);
			pr_info("[LCM]hbm, restore hbm bl:%d\n",g_level);
		} else {
			backlight_code_cmd_tb[1].para_list[1]  = (g_level>>8)&0xf;
			backlight_code_cmd_tb[1].para_list[2]  = (g_level)&0xff;
			cmd_cnt = sizeof(backlight_code_cmd_tb) / sizeof(struct LCM_mtk_setting_table);
			panel_send_pack_cmd(dsi, backlight_code_cmd_tb, cmd_cnt, cb, handle);
			pr_info("[LCM]hbm, restore normal bl:%d\n",g_level);
		}
	}

	ctx->hbm_en = en;
	ctx->hbm_wait = true;
	mtk_panel_proc_hbm(ctx->hbm_en);
	pr_info("%s- level =%d ctx->hbm_en =%d\n", __func__,g_level,ctx->hbm_en);
done:
	return 0;
}

static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
			      dcs_write_gce cb, void *handle, bool en)
{

	pr_info("%s\n", __func__);

	if (!cb) {
		pr_info("[LCM]cb is null, panel_hbm_set_cmdq return -1\n");
		return 0;
	}

	return 0;
}

static void panel_hbm_get_state(struct drm_panel *panel, bool *state)
{
	struct lcm *ctx = panel_to_lcm(panel);

	*state = ctx->hbm_en;
}

static void panel_hbm_get_wait_state(struct drm_panel *panel, bool *wait)
{
	struct lcm *ctx = panel_to_lcm(panel);

	*wait = ctx->hbm_wait;
}

static bool panel_hbm_set_wait_state(struct drm_panel *panel, bool wait)
{
	struct lcm *ctx = panel_to_lcm(panel);
	bool old = ctx->hbm_wait;

	ctx->hbm_wait = wait;
	return old;
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
	pr_info("[LCM]%s+\n", __func__);
	lcm_now_state = 1;
	return 0;
}

static int panel_doze_enable_start(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
/*	char display_off[] = {0x28};
	char display_on[] = {0x29};
	char aod_tb00[] = {0xF0, 0x5A, 0x5A};
	char aod_tb01[] = {0x53, 0x20};
	char aod_tb02[] = {0x51, 0x03, 0xFF};
	char aod_tb03[] = {0x60, 0x01};
	char aod_tb04[] = {0xB0, 0x01, 0x17, 0xB2};
	char aod_tb05[] = {0xB2, 0x01};
	char aod_tb06[] = {0xB0, 0x00, 0x03, 0xF4};
	char aod_tb07[] = {0xF4, 0x6C};
	char aod_tb08[] = {0xB0, 0x00, 0x1B, 0xF4};
	char aod_tb09[] = {0xF4, 0x83};
	char aod_tb0a[] = {0xB0, 0x00, 0x1D, 0xF4};
	char aod_tb0b[] = {0xF4, 0x12};
	char aod_tb0c[] = {0xB0, 0x00, 0x11, 0xB2};
	char aod_tb0d[] = {0xB2, 0x1F};
	char aod_tb0e[] = {0xB7, 0x01};
	char aod_tb0f[] = {0xB0, 0x00, 0x86, 0xB1};
	char aod_tb10[] = {0xB1, 0x01};
	char aod_tb11[] = {0xB0, 0x00, 0x88, 0xB1};
	char aod_tb12[] = {0xB1, 0x26};
	char aod_tb13[] = {0xB0, 0x00, 0x18, 0xB6};
	char aod_tb14[] = {0xB6, 0x81};
	char aod_tb15[] = {0xB0, 0x00, 0x2D, 0xB6};
	char aod_tb16[] = {0xB6, 0x01};
	char aod_tb17[] = {0xB0, 0x00, 0xBC, 0xB5};
	char aod_tb18[] = {0xB5, 0x25};
	char aod_tb19[] = {0xC3, 0x33};
	char aod_tb1a[] = {0xB0, 0x00, 0x91, 0xC3};
	char aod_tb1b[] = {0xC3, 0x0B, 0x00, 0x02};
	char aod_tb1c[] = {0xB0, 0x00, 0xA3, 0xC3};
	char aod_tb1d[] = {0xC3, 0x6D, 0x24, 0x00, 0x00, 0x04, 0x3C, 0x44, 0x3C};
	char aod_tb1e[] = {0xB0, 0x00, 0xB6, 0xC3};
	char aod_tb1f[] = {0xC3, 0x06, 0x79};
	char aod_tb20[] = {0xB0, 0x00, 0xEE, 0xC3};
	char aod_tb21[] = {0xC3, 0x44, 0x44, 0x44};
	char aod_tb22[] = {0xB0, 0x00, 0xF3, 0xC3};
	char aod_tb23[] = {0xC3, 0x09, 0x09};
	char aod_tb24[] = {0xB0, 0x00, 0x32, 0xF6};
	char aod_tb25[] = {0xF6, 0x81, 0x41};
	char aod_tb26[] = {0xB0, 0x00, 0x2F, 0xF6};
	char aod_tb27[] = {0xF6, 0x86, 0x81};
	char aod_tb28[] = {0xB0, 0x00, 0x39, 0xF6};
	char aod_tb29[] = {0xF6, 0x41};
	char aod_tb2a[] = {0xB0, 0x01, 0x61, 0xB2};
	char aod_tb2b_5[] = {0xB2, 0x01, 0x08, 0xC0};
	char aod_tb2b_30[] = {0xB2, 0x01, 0x04, 0xD0};
	char aod_tb2b_60[] = {0xB2, 0x01, 0x00, 0x28};

	char aod_tb2c[] = {0xB0, 0x00, 0x03, 0xCD};
	char aod_tb2d[] = {0xCD, 0x34};
	char aod_tb2e[] = {0xF7, 0x0B};
	char aod_tb2f[] = {0xF0, 0xA5, 0xA5};

	pr_info("[LCM]panel_doze_enable_start+\n");
	if (!cb) {
		pr_info("[LCM]cb is null, panel_doze_enable return -1\n");
		return -1;
	}
	cb(dsi, handle, display_off, ARRAY_SIZE(display_off));
	mdelay(17);

	cb(dsi, handle, aod_tb00, ARRAY_SIZE(aod_tb00));
	cb(dsi, handle, aod_tb01, ARRAY_SIZE(aod_tb01));
	cb(dsi, handle, aod_tb02, ARRAY_SIZE(aod_tb02));
	cb(dsi, handle, aod_tb03, ARRAY_SIZE(aod_tb03));
	cb(dsi, handle, aod_tb04, ARRAY_SIZE(aod_tb04));
	cb(dsi, handle, aod_tb05, ARRAY_SIZE(aod_tb05));
	cb(dsi, handle, aod_tb06, ARRAY_SIZE(aod_tb06));
	cb(dsi, handle, aod_tb07, ARRAY_SIZE(aod_tb07));
	cb(dsi, handle, aod_tb08, ARRAY_SIZE(aod_tb08));
	cb(dsi, handle, aod_tb09, ARRAY_SIZE(aod_tb09));
	cb(dsi, handle, aod_tb0a, ARRAY_SIZE(aod_tb0a));
	cb(dsi, handle, aod_tb0b, ARRAY_SIZE(aod_tb0b));
	cb(dsi, handle, aod_tb0c, ARRAY_SIZE(aod_tb0c));
	cb(dsi, handle, aod_tb0d, ARRAY_SIZE(aod_tb0d));
	cb(dsi, handle, aod_tb0e, ARRAY_SIZE(aod_tb0e));
	cb(dsi, handle, aod_tb0f, ARRAY_SIZE(aod_tb0f));
	cb(dsi, handle, aod_tb10, ARRAY_SIZE(aod_tb10));
	cb(dsi, handle, aod_tb11, ARRAY_SIZE(aod_tb11));
	cb(dsi, handle, aod_tb12, ARRAY_SIZE(aod_tb12));
	cb(dsi, handle, aod_tb13, ARRAY_SIZE(aod_tb13));
	cb(dsi, handle, aod_tb14, ARRAY_SIZE(aod_tb14));
	cb(dsi, handle, aod_tb15, ARRAY_SIZE(aod_tb15));
	cb(dsi, handle, aod_tb16, ARRAY_SIZE(aod_tb16));
	cb(dsi, handle, aod_tb17, ARRAY_SIZE(aod_tb17));
	cb(dsi, handle, aod_tb18, ARRAY_SIZE(aod_tb18));
	cb(dsi, handle, aod_tb19, ARRAY_SIZE(aod_tb19));
	cb(dsi, handle, aod_tb1a, ARRAY_SIZE(aod_tb1a));
	cb(dsi, handle, aod_tb1b, ARRAY_SIZE(aod_tb1b));
	cb(dsi, handle, aod_tb1c, ARRAY_SIZE(aod_tb1c));
	cb(dsi, handle, aod_tb1d, ARRAY_SIZE(aod_tb1d));
	cb(dsi, handle, aod_tb1e, ARRAY_SIZE(aod_tb1e));
	cb(dsi, handle, aod_tb1f, ARRAY_SIZE(aod_tb1f));
	cb(dsi, handle, aod_tb20, ARRAY_SIZE(aod_tb20));
	cb(dsi, handle, aod_tb21, ARRAY_SIZE(aod_tb21));
	cb(dsi, handle, aod_tb22, ARRAY_SIZE(aod_tb22));
	cb(dsi, handle, aod_tb23, ARRAY_SIZE(aod_tb23));
	cb(dsi, handle, aod_tb24, ARRAY_SIZE(aod_tb24));
	cb(dsi, handle, aod_tb25, ARRAY_SIZE(aod_tb25));
	cb(dsi, handle, aod_tb26, ARRAY_SIZE(aod_tb26));
	cb(dsi, handle, aod_tb27, ARRAY_SIZE(aod_tb27));
	cb(dsi, handle, aod_tb28, ARRAY_SIZE(aod_tb28));
	cb(dsi, handle, aod_tb29, ARRAY_SIZE(aod_tb29));
	cb(dsi, handle, aod_tb2a, ARRAY_SIZE(aod_tb2a));

	if (g_level >= 200)
		cb(dsi, handle, aod_tb2b_60, ARRAY_SIZE(aod_tb2b_60));
	else if (g_level > 100)
		cb(dsi, handle, aod_tb2b_30, ARRAY_SIZE(aod_tb2b_30));
	else
		cb(dsi, handle, aod_tb2b_5, ARRAY_SIZE(aod_tb2b_5));
	cb(dsi, handle, aod_tb2c, ARRAY_SIZE(aod_tb2c));
	cb(dsi, handle, aod_tb2d, ARRAY_SIZE(aod_tb2d));
	cb(dsi, handle, aod_tb2e, ARRAY_SIZE(aod_tb2e));
	cb(dsi, handle, aod_tb2f, ARRAY_SIZE(aod_tb2f));

	mdelay(17);
	cb(dsi, handle, display_on, ARRAY_SIZE(display_on));
	pr_info("[LCM]panel_doze_enable_start-\n");
*/
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{

	lcm_now_state = 0;
/*
	unsigned int bl_level = 0x74;
	char display_off[] = {0x28};
	char display_on[] = {0x29};
	char aod_tb00[] = {0xF0, 0x5A, 0x5A};
	char aod_tb01[] = {0x53, 0x20};
	//char aod_tb02[] = {0x51, 0x00, 0x74};
	char aod_tb03[] = {0x60, 0x01};
	char aod_tb04[] = {0xB0, 0x01, 0x17, 0xB2};
	char aod_tb05[] = {0xB2, 0x00};
	char aod_tb06[] = {0xB0, 0x00, 0x03, 0xF4};
	char aod_tb07[] = {0xF4, 0x4A};
	char aod_tb08[] = {0xB0, 0x00, 0x1B, 0xF4};
	char aod_tb09[] = {0xF4, 0x9A};
	char aod_tb0a[] = {0xB0, 0x00, 0x1D, 0xF4};
	char aod_tb0b[] = {0xF4, 0x1C};
	char aod_tb0c[] = {0xB0, 0x00, 0x11, 0xB2};
	char aod_tb0d[] = {0xB2, 0x16};
	char aod_tb0e[] = {0xB7, 0x02};
	char aod_tb0f[] = {0xB0, 0x00, 0x86, 0xB1};
	char aod_tb10[] = {0xB1, 0x02};
	char aod_tb11[] = {0xB0, 0x00, 0x88, 0xB1};
	char aod_tb12[] = {0xB1, 0x27};
	char aod_tb13[] = {0xB0, 0x00, 0x18, 0xB6};
	char aod_tb14[] = {0xB6, 0x82};
	char aod_tb15[] = {0xB0, 0x00, 0x2D, 0xB6};
	char aod_tb16[] = {0xB6, 0x0E};
	char aod_tb17[] = {0xB0, 0x00, 0xBC, 0xB5};
	char aod_tb18[] = {0xB5, 0x26};
	char aod_tb19[] = {0xC3, 0x22};
	char aod_tb1a[] = {0xB0, 0x00, 0x91, 0xC3};
	char aod_tb1b[] = {0xC3, 0x07, 0x00, 0x06};
	char aod_tb1c[] = {0xB0, 0x00, 0xA3, 0xC3};
	char aod_tb1d[] = {0xC3, 0x2A, 0x24, 0x00, 0x00, 0x07, 0x16, 0x22, 0x20};
	char aod_tb1e[] = {0xB0, 0x00, 0xB6, 0xC3};
	char aod_tb1f[] = {0xC3, 0x0C, 0x1A};
	char aod_tb20[] = {0xB0, 0x00, 0xEE, 0xC3};
	char aod_tb21[] = {0xC3, 0x04, 0x04, 0x04};
	char aod_tb22[] = {0xB0, 0x00, 0xF3, 0xC3};
	char aod_tb23[] = {0xC3, 0x1C, 0x1C};
	char aod_tb24[] = {0xB0, 0x00, 0x32, 0xF6};
	char aod_tb25[] = {0xF6, 0x45, 0x20};
	char aod_tb26[] = {0xB0, 0x00, 0x2F, 0xF6};
	char aod_tb27[] = {0xF6, 0x48, 0x4A};
	char aod_tb28[] = {0xB0, 0x00, 0x39, 0xF6};
	char aod_tb29[] = {0xF6, 0x01};
	char aod_tb2a[] = {0xB0, 0x01, 0x61, 0xB2};
	char aod_tb2b[] = {0xB2, 0x00};
	char aod_tb2c[] = {0xB0, 0x00, 0x03, 0xCD};
	char aod_tb2d[] = {0xCD, 0x14};
	char aod_tb2e[] = {0xF7, 0x0B};
	char aod_tb2f[] = {0xF0, 0xA5, 0xA5};
	lcm_now_state = 0;
	bl_level = g_level;
	bl_tb0[1] = (bl_level>>8)&0xf;
	bl_tb0[2] = (bl_level)&0xff;
	pr_info("[LCM]panel_doze_disable+\n");
	if (!cb) {
		pr_info("[LCM]cb is null, panel_doze_enable return -1\n");
		return -1;
	}
	cb(dsi, handle, display_off, ARRAY_SIZE(display_off));
	mdelay(17);

	cb(dsi, handle, aod_tb00, ARRAY_SIZE(aod_tb00));
	cb(dsi, handle, aod_tb01, ARRAY_SIZE(aod_tb01));
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	cb(dsi, handle, aod_tb03, ARRAY_SIZE(aod_tb03));
	cb(dsi, handle, aod_tb04, ARRAY_SIZE(aod_tb04));
	cb(dsi, handle, aod_tb05, ARRAY_SIZE(aod_tb05));
	cb(dsi, handle, aod_tb06, ARRAY_SIZE(aod_tb06));
	cb(dsi, handle, aod_tb07, ARRAY_SIZE(aod_tb07));
	cb(dsi, handle, aod_tb08, ARRAY_SIZE(aod_tb08));
	cb(dsi, handle, aod_tb09, ARRAY_SIZE(aod_tb09));
	cb(dsi, handle, aod_tb0a, ARRAY_SIZE(aod_tb0a));
	cb(dsi, handle, aod_tb0b, ARRAY_SIZE(aod_tb0b));
	cb(dsi, handle, aod_tb0c, ARRAY_SIZE(aod_tb0c));
	cb(dsi, handle, aod_tb0d, ARRAY_SIZE(aod_tb0d));
	cb(dsi, handle, aod_tb0e, ARRAY_SIZE(aod_tb0e));
	cb(dsi, handle, aod_tb0f, ARRAY_SIZE(aod_tb0f));
	cb(dsi, handle, aod_tb10, ARRAY_SIZE(aod_tb10));
	cb(dsi, handle, aod_tb11, ARRAY_SIZE(aod_tb11));
	cb(dsi, handle, aod_tb12, ARRAY_SIZE(aod_tb12));
	cb(dsi, handle, aod_tb13, ARRAY_SIZE(aod_tb13));
	cb(dsi, handle, aod_tb14, ARRAY_SIZE(aod_tb14));
	cb(dsi, handle, aod_tb15, ARRAY_SIZE(aod_tb15));
	cb(dsi, handle, aod_tb16, ARRAY_SIZE(aod_tb16));
	cb(dsi, handle, aod_tb17, ARRAY_SIZE(aod_tb17));
	cb(dsi, handle, aod_tb18, ARRAY_SIZE(aod_tb18));
	cb(dsi, handle, aod_tb19, ARRAY_SIZE(aod_tb19));
	cb(dsi, handle, aod_tb1a, ARRAY_SIZE(aod_tb1a));
	cb(dsi, handle, aod_tb1b, ARRAY_SIZE(aod_tb1b));
	cb(dsi, handle, aod_tb1c, ARRAY_SIZE(aod_tb1c));
	cb(dsi, handle, aod_tb1d, ARRAY_SIZE(aod_tb1d));
	cb(dsi, handle, aod_tb1e, ARRAY_SIZE(aod_tb1e));
	cb(dsi, handle, aod_tb1f, ARRAY_SIZE(aod_tb1f));
	cb(dsi, handle, aod_tb20, ARRAY_SIZE(aod_tb20));
	cb(dsi, handle, aod_tb21, ARRAY_SIZE(aod_tb21));
	cb(dsi, handle, aod_tb22, ARRAY_SIZE(aod_tb22));
	cb(dsi, handle, aod_tb23, ARRAY_SIZE(aod_tb23));
	cb(dsi, handle, aod_tb24, ARRAY_SIZE(aod_tb24));
	cb(dsi, handle, aod_tb25, ARRAY_SIZE(aod_tb25));
	cb(dsi, handle, aod_tb26, ARRAY_SIZE(aod_tb26));
	cb(dsi, handle, aod_tb27, ARRAY_SIZE(aod_tb27));
	cb(dsi, handle, aod_tb28, ARRAY_SIZE(aod_tb28));
	cb(dsi, handle, aod_tb29, ARRAY_SIZE(aod_tb29));
	cb(dsi, handle, aod_tb2a, ARRAY_SIZE(aod_tb2a));
	cb(dsi, handle, aod_tb2b, ARRAY_SIZE(aod_tb2b));
	cb(dsi, handle, aod_tb2c, ARRAY_SIZE(aod_tb2c));
	cb(dsi, handle, aod_tb2d, ARRAY_SIZE(aod_tb2d));
	cb(dsi, handle, aod_tb2e, ARRAY_SIZE(aod_tb2e));
	cb(dsi, handle, aod_tb2f, ARRAY_SIZE(aod_tb2f));

	mdelay(17);
	cb(dsi, handle, display_on, ARRAY_SIZE(display_on));
*/
	pr_info("[LCM]panel_doze_disable-\n");
	return 0;
}

static int panel_doze_area(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	return 0;
}

static struct mtk_panel_params ext_params_120hz = {
	.change_fps_by_vfp_send_cmd = 1,
	.data_rate = 1140,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9F,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x05,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0x0E,
		.count = 1,
		.para_list[0] = 0x80,
	},
	.lcm_esd_check_table[3] = {
		.cmd = 0xEE,
		.count = 2,
		.para_list[0] = 0x00,
	},
	//.lp_perline_en = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 40,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 989,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 631,
		.slice_bpg_offset = 651,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.vdo_mix_mode_en = true,
	.dyn_fps = {
		.data_rate = 1140,
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 3, {0xF0, 0x5A, 0x5A} },
		.dfps_cmd_table[1] = {0, 2, {0x60, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0xF7, 0x0B} },
		.dfps_cmd_table[3] = {0, 3, {0xF0, 0xA5, 0xA5} },
	},
//	.dyn = {
//		.switch_en = 1,
//		.data_rate = 1108 + 10,
//	},
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(panel_samsung_s6e8fc3x02_rc_top_pattern),
	.corner_pattern_lt_addr = (void *)panel_samsung_s6e8fc3x02_rc_top_pattern,
};

static struct mtk_panel_params ext_params_60hz = {
	.change_fps_by_vfp_send_cmd = 1,
	.data_rate = 1140,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9F,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x05,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0x0E,
		.count = 1,
		.para_list[0] = 0x80,
	},
	.lcm_esd_check_table[3] = {
		.cmd = 0xEE,
		.count = 2,
		.para_list[0] = 0x00,
	},
	//.lp_perline_en = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 40,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 989,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 631,
		.slice_bpg_offset = 651,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.vdo_mix_mode_en = true,
	.dyn_fps = {
		.data_rate = 1140,
		.switch_en = 1,
		.vact_timing_fps = 60,
		.dfps_cmd_table[0] = {0, 3, {0xF0, 0x5A, 0x5A} },
		.dfps_cmd_table[1] = {0, 2, {0x60, 0x21} },
		.dfps_cmd_table[2] = {0, 2, {0xF7, 0x0B} },
		.dfps_cmd_table[3] = {0, 3, {0xF0, 0xA5, 0xA5} },
	},
//	.dyn = {
//		.switch_en = 1,
//		.data_rate = 1108 + 10,
//	},
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(panel_samsung_s6e8fc3x02_rc_top_pattern),
	.corner_pattern_lt_addr = (void *)panel_samsung_s6e8fc3x02_rc_top_pattern,
};
/*
static struct mtk_panel_params ext_params_30hz = {
	.change_fps_by_vfp_send_cmd = 0,
	.data_rate = 1140,
	//.lp_perline_en = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 40,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 989,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 631,
		.slice_bpg_offset = 651,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.dyn_fps = {
		.data_rate = 1140,
		.switch_en = 1,
		.vact_timing_fps = 30,
		.dfps_cmd_table[0] = {0, 3, {0xF0, 0x5A, 0x5A} },
		.dfps_cmd_table[1] = {0, 2, {0x53, 0x20} },
		.dfps_cmd_table[2] = {0, 3, {0x51, 0x03, 0xFF} },
		.dfps_cmd_table[3] = {0, 2, {0x60, 0x01} },
		.dfps_cmd_table[4] = {0, 4, {0xB0, 0x01, 0x17, 0xB2} },
		.dfps_cmd_table[5] = {0, 2, {0xB2, 0x01} },
		.dfps_cmd_table[6] = {0, 4, {0xB0, 0x00, 0x03, 0xF4} },
		.dfps_cmd_table[7] = {0, 2, {0xF4, 0x6C} },
		.dfps_cmd_table[8] = {0, 4, {0xB0, 0x00, 0x1B, 0xF4} },
		.dfps_cmd_table[9] = {0, 2, {0xF4, 0x83} },
		.dfps_cmd_table[10] = {0, 4, {0xB0, 0x00, 0x1D, 0xF4} },
		.dfps_cmd_table[11] = {0, 2, {0xF4, 0x12} },
		.dfps_cmd_table[12] = {0, 4, {0xB0, 0x00, 0x11, 0xB2} },
		.dfps_cmd_table[13] = {0, 2, {0xB2, 0x1F} },
		.dfps_cmd_table[14] = {0, 2, {0xB7, 0x01} },
		.dfps_cmd_table[15] = {0, 4, {0xB0, 0x00, 0x86, 0xB1} },
		.dfps_cmd_table[16] = {0, 2, {0xB1, 0x01} },
		.dfps_cmd_table[17] = {0, 4, {0xB0, 0x00, 0x88, 0xB1} },
		.dfps_cmd_table[18] = {0, 2, {0xB1, 0x26} },
		.dfps_cmd_table[19] = {0, 4, {0xB0, 0x00, 0x18, 0xB6} },
		.dfps_cmd_table[20] = {0, 2, {0xB6, 0x81} },
		.dfps_cmd_table[21] = {0, 4, {0xB0, 0x00, 0x2D, 0xB6} },
		.dfps_cmd_table[22] = {0, 2, {0xB6, 0x01} },
		.dfps_cmd_table[23] = {0, 4, {0xB0, 0x00, 0xBC, 0xB5} },
		.dfps_cmd_table[24] = {0, 2, {0xB5, 0x25} },
		.dfps_cmd_table[25] = {0, 2, {0xC3, 0x33} },
		.dfps_cmd_table[26] = {0, 4, {0xB0, 0x00, 0x91, 0xC3} },
		.dfps_cmd_table[27] = {0, 4, {0xC3, 0x0B, 0x00, 0x02} },
		.dfps_cmd_table[28] = {0, 4, {0xB0, 0x00, 0xA3, 0xC3} },
		.dfps_cmd_table[29] = {0, 9, {0xC3, 0x6D, 0x24, 0x00, 0x00, 0x04, 0x3C, 0x44, 0x3C} },
		.dfps_cmd_table[30] = {0, 4, {0xB0, 0x00, 0xB6, 0xC3} },
		.dfps_cmd_table[31] = {0, 3, {0xC3, 0x06, 0x79} },
		.dfps_cmd_table[32] = {0, 4, {0xB0, 0x00, 0xEE, 0xC3} },
		.dfps_cmd_table[33] = {0, 4, {0xC3, 0x44, 0x44, 0x44} },
		.dfps_cmd_table[34] = {0, 4, {0xB0, 0x00, 0xF3, 0xC3} },
		.dfps_cmd_table[35] = {0, 3, {0xC3, 0x09, 0x09} },
		.dfps_cmd_table[36] = {0, 4, {0xB0, 0x00, 0x32, 0xF6} },
		.dfps_cmd_table[37] = {0, 3, {0xF6, 0x81, 0x41} },
		.dfps_cmd_table[38] = {0, 4, {0xB0, 0x00, 0x2F, 0xF6} },
		.dfps_cmd_table[39] = {0, 3, {0xF6, 0x86, 0x81} },
		.dfps_cmd_table[40] = {0, 4, {0xB0, 0x00, 0x39, 0xF6} },
		.dfps_cmd_table[41] = {0, 2, {0xF6, 0x41} },
		.dfps_cmd_table[42] = {0, 4, {0xB0, 0x01, 0x61, 0xB2} },
		.dfps_cmd_table[43] = {0, 4, {0xB2, 0x01, 0x08, 0xC0} },
		.dfps_cmd_table[44] = {0, 4, {0xB0, 0x00, 0x03, 0xCD} },
		.dfps_cmd_table[45] = {0, 2, {0xCD, 0x34} },
		.dfps_cmd_table[46] = {0, 2, {0xF7, 0x0B} },
		.dfps_cmd_table[47] = {0, 3, {0xF0, 0xA5, 0xA5} },
	},
//	.dyn = {
//		.switch_en = 1,
//		.data_rate = 1108 + 10,
//	},
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(panel_samsung_s6e8fc3x02_rc_top_pattern),
	.corner_pattern_lt_addr = (void *)panel_samsung_s6e8fc3x02_rc_top_pattern,
};

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
static int panel_set_aod_light_mode(void *dsi,
	dcs_write_gce cb, void *handle, unsigned int mode)
{
	return 0;
}

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
	} else if (drm_mode_vrefresh(m) == 60) {
		*ext_param = &ext_params_60hz;
	}else
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
	} else if (drm_mode_vrefresh(m) == 60){
		ext->params = &ext_params_60hz;
	}else
		ret = 1;

	return ret;
}
/*
static void mode_switch_to_30(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	pr_err("[LCM]%s 30fps\n", __func__);
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);
		pr_err("[LCM]%s 120fps\n", __func__);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x01);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);
		pr_err("[LCM]%s 60fps\n", __func__);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
		lcm_dcs_write_seq_static(ctx, 0x60, 0x21);
		lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	}
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);
	pr_err("[LCM]%s start ++\n", __func__);
	if (cur_mode == dst_mode || m == NULL){
		pr_err("[LCM]%s mode have been set!\n", __func__);
		return ret;
	}

	if (drm_mode_vrefresh(m) == 60) {
		mode_switch_to_60(panel, stage);
	} else if (drm_mode_vrefresh(m) == 120) {
		mode_switch_to_120(panel, stage);
	} else if (drm_mode_vrefresh(m) == 30) {
		mode_switch_to_30(panel, stage);
	} else
		ret = 1;

	if(!ret){
		drm_mode_vrefresh(m);
		pr_err("[LCM]%s start vrefresh!\n", __func__);
	}
	pr_err("[LCM]%s end--\n", __func__);
	return ret;
}
*/
static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.set_backlight_pack = samsung_set_backlight_pack,

	.ata_check = panel_ata_check,
	.hbm_set_cmdq = panel_hbm_set_cmdq,
	.hbm_set_pack = panel_set_hbm_pack,
	.hbm_get_state = panel_hbm_get_state,
	.hbm_get_wait_state = panel_hbm_get_wait_state,
	.hbm_set_wait_state = panel_hbm_set_wait_state,

	/* add for ramless AOD */
	.doze_get_mode_flags = NULL,//panel_doze_get_mode_flags,
	.doze_enable = panel_doze_enable,
	.doze_enable_start = panel_doze_enable_start,
	.doze_area = panel_doze_area,
	.doze_disable = panel_doze_disable,
	.doze_post_disp_on = NULL,//panel_doze_post_disp_on,
	.set_aod_light_mode = panel_set_aod_light_mode,

	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	//.mode_switch = mode_switch,

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
	.clock = 377349,
	.hdisplay = HACT,
	.hsync_start = HACT + HFP,
	.hsync_end = HACT + HFP + HSA,
	.htotal = HACT + HFP + HSA + HBP,
	.vdisplay = VACT,
	.vsync_start = VACT + VFP,
	.vsync_end = VACT + VFP + VSA,
	.vtotal = VACT + VFP + VSA + VBP,
};

static const struct drm_display_mode switch_mode_60hz = {
	.clock = 377349,
	.hdisplay = HACT,
	.hsync_start = HACT + HFP,
	.hsync_end = HACT + HFP + HSA,
	.htotal = HACT + HFP + HSA + HBP,
	.vdisplay = VACT,
	.vsync_start = VACT + VFP_60,
	.vsync_end = VACT + VFP_60 + VSA,
	.vtotal = VACT + VFP_60 + VSA + VBP,
};

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;

	mode = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_60hz.hdisplay, switch_mode_60hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode_1 = drm_mode_duplicate(connector->dev, &switch_mode_120hz);
	if (!mode_1) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_120hz.hdisplay, switch_mode_120hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_120hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);

	connector->display_info.width_mm = 83;
	connector->display_info.height_mm = 148;

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

static ssize_t hbm_mode_show(struct kobject* kodjs,struct kobj_attribute *attr,char *buf)
{
	int count = 0;
	count = sprintf(buf, "hbm state: %d\n",g_ctx->hbm_stat);
	return count;
}

static ssize_t hbm_mode_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count)
{
	int ret;
	unsigned int state;
	unsigned char tx[10] = {0};
	static unsigned int display_bl_now = 125;
	struct mtk_ddic_dsi_msg *cmd_msg = g_cmd_msg;

	ret = kstrtouint(buf, 10, &state);
	if (ret < 0) {
		goto err;
	}
	pr_info("[%s]set state:%d\n", __func__, state);
	pr_info("[%s]hbm_stat:%d\n", __func__, g_ctx->hbm_stat);

	if (state == 1) {
		if (g_ctx->hbm_stat == true) {
			pr_info("[%s]Has been in HBM mode\n", __func__);
			goto err;
		}
		display_bl_now = g_level;
		cmd_msg->channel = 0;
		cmd_msg->flags = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->type[0] = 0x15;
		tx[0] = 0x53;
		tx[1] = 0xE0;
		cmd_msg->tx_len[0] = 2;
		mtk_ddic_dsi_send_cmd(cmd_msg, 0);
		cmd_msg->type[0] = 0x39;
		tx[0] = 0x51;
		tx[1] = 0x05;
		tx[2] = 0xF4;
		cmd_msg->tx_len[0] = 3;
		mtk_ddic_dsi_send_cmd(cmd_msg, 0);
		cmd_msg->type[0] = 0x15;
		tx[0] = 0xF7;
		tx[1] = 0x0B;
		cmd_msg->tx_len[0] = 2;
		mtk_ddic_dsi_send_cmd(cmd_msg, 0);
		mdelay(17);
		g_ctx->hbm_stat = true;
	} else if (state == 0) {
		if (g_ctx->hbm_stat == false) {
			pr_info("[%s]Has been in Normal mode\n", __func__);
			goto err;
		}
		cmd_msg->channel = 0;
		cmd_msg->flags = 0;
		cmd_msg->tx_cmd_num = 1;
		cmd_msg->tx_buf[0] = tx;
		cmd_msg->type[0] = 0x39;
		tx[0] = 0xF0;
		tx[1] = 0x5A;
		tx[2] = 0x5A;
		cmd_msg->tx_len[0] = 3;
		mtk_ddic_dsi_send_cmd(cmd_msg, 0);
		cmd_msg->type[0] = 0x15;
		tx[0] = 0x53;
		tx[1] = 0x20;
		cmd_msg->tx_len[0] = 2;
		mtk_ddic_dsi_send_cmd(cmd_msg, 0);
		cmd_msg->type[0] = 0x39;
		tx[0] = 0x51;
		tx[1] = (display_bl_now>>8)&0xf;
		tx[2] = display_bl_now&0xff;
		cmd_msg->tx_len[0] = 3;
		mtk_ddic_dsi_send_cmd(cmd_msg, 0);
		cmd_msg->type[0] = 0x15;
		tx[0] = 0xF7;
		tx[1] = 0x0B;
		cmd_msg->tx_len[0] = 2;
		mtk_ddic_dsi_send_cmd(cmd_msg, 0);
		cmd_msg->type[0] = 0x39;
		tx[0] = 0xF0;
		tx[1] = 0xA5;
		tx[2] = 0xA5;
		cmd_msg->tx_len[0] = 3;
		mtk_ddic_dsi_send_cmd(cmd_msg, 0);
		g_ctx->hbm_stat = false;
	}

err:
	return count;
}
static struct kobj_attribute hbm_mode_attr = __ATTR(hbm_mode, 0664, hbm_mode_show, hbm_mode_store);


static ssize_t ui_status_show(struct kobject* kodjs,struct kobj_attribute *attr,char *buf)
{
	return sprintf(buf, "ui status: %d\n",mtk_panel_get_ui_status());
}

static ssize_t ui_status_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count)
{
	int ui_status = 0;
	if (0 > kstrtouint(buf, 10, &ui_status)) {
		goto err;
	}
	printk("[%s] ui_status:%d\n", __func__, ui_status);
	mtk_panel_proc_ui_status(ui_status);
err:
	return count;
}

static struct kobj_attribute ui_status_attr = __ATTR(ui_status, 0664, ui_status_show, ui_status_store);
int sys_node_init(void)
{
	int ret = 0;

	kobj = kobject_create_and_add("panel_feature", NULL);
	if (kobj == NULL) {
		return -ENOMEM;
	}

	ret = sysfs_create_file(kobj, &hbm_mode_attr.attr);
	if (ret < 0) {
		printk("[%s] sysfs_create_group failed\n",__func__);
		return -1;
	}

	ret = sysfs_create_file(kobj, &ui_status_attr.attr);
	if (ret < 0) {
		printk("[%s] sysfs_create_group ui_statud failed\n",__func__);
		return -1;
	}

	printk("[%s] is OK!!!\n", __func__);
	return 0;
}

static ssize_t displayid_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
	int count = 0;
	unsigned int display_id = 0;

	lcm_panel_get_data();
	display_id = lcm_id[0] << 16 | lcm_id[1] << 8 | lcm_id[2];
	printk("[%s] display_id:0x%06x!!!\n",__func__, display_id);
	count = sprintf(buf, "%06x\n", display_id);

	return count;
}
static DEVICE_ATTR(displayid, 0664, displayid_show, NULL);

static ssize_t brightnessid_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
	int count = 0;

	printk("[%s] display_bl:%d!!!\n",__func__, g_level);
	count = sprintf(buf, "%d\n", g_level);

	return count;
}

static ssize_t brightnessid_store(struct device *dev,
           struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned int state;
	unsigned char val1, val2;
	unsigned char tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg = g_cmd_msg;

	ret = kstrtouint(buf, 10, &state);
	if (ret < 0) {
		goto err;
	}
	printk("[%s] brightness level:%d\n", __func__, state);
	val1 = (state>>8)&0xf;
	val2 = (state)&0xff;

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->type[0] = 0x39;
	tx[0] = 0xF0;
	tx[1] = 0x5A;
	tx[2] = 0x5A;
	cmd_msg->tx_len[0] = 3;
	mtk_ddic_dsi_send_cmd(cmd_msg, 0);

	cmd_msg->type[0] = 0x15;
	tx[0] = 0x53;
	tx[1] = 0x20;
	cmd_msg->tx_len[0] = 2;
	mtk_ddic_dsi_send_cmd(cmd_msg, 0);


	cmd_msg->tx_buf[0] = tx;
	cmd_msg->type[0] = 0x39;
	tx[0] = 0x51;
	tx[1] = val1;
	tx[2] = val2;
	cmd_msg->tx_len[0] = 3;
	mtk_ddic_dsi_send_cmd(cmd_msg, 0);

	cmd_msg->type[0] = 0x15;
	tx[0] = 0xF7;
	tx[1] = 0x0B;
	cmd_msg->tx_len[0] = 2;
	mtk_ddic_dsi_send_cmd(cmd_msg, 0);

	cmd_msg->type[0] = 0x39;
	tx[0] = 0xF0;
	tx[1] = 0xA5;
	tx[2] = 0xA5;
	cmd_msg->tx_len[0] = 3;
	mtk_ddic_dsi_send_cmd(cmd_msg, 0);

	g_level = state & 0xfff;
err:
	return size;
}
static DEVICE_ATTR(brightnessid, 0664, brightnessid_show, brightnessid_store);

static struct attribute *displayid_attributes[] = {
	&dev_attr_displayid.attr,
	&dev_attr_brightnessid.attr,
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
		printk("[%s] sysfs_create_group failed\n",__func__);
	}
	printk("%s is OK!!!\n",__func__);

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
				pr_info("No panel connected,skip probe lcm\n");
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
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);

	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

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
	pr_info("%s-\n", __func__);

	ctx->hbm_en = false;
	ctx->hbm_stat = false;
	sys_node_init();
	displayid_node_init();
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
	{ .compatible = "samsung,s6e8fc3x02", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "samsung,s6e8fc3x02",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Linus Wallei <linus.walleij@linaro.org>");
MODULE_DESCRIPTION("MIPI-DSI s68fc01 Panel Driver");
MODULE_LICENSE("GPL v2");
