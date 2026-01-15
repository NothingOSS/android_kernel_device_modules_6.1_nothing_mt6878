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
//#include "../mediatek/mediatek_v2/mtk_corner_pattern/panel-rm69220-boe.h"
extern int mtk_ddic_dsi_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg, bool blocking);
static atomic_t current_backlight;

#define HSA 4
#define HBP 36
#define HFP 22
#define HFP_30hz 1492

#define VSA 4
#define VBP 52
#define VFP 72
#define VFP_90 888
#define VFP_60 2556
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
	usleep_range(1 * 1000, 2 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(32 * 1000, 33 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx,  0xFE, 0x76);	//DBV 0 duty=0
	lcm_dcs_write_seq_static(ctx,  0x9A, 0x10);
	lcm_dcs_write_seq_static(ctx,  0x9B, 0x00);
	lcm_dcs_write_seq_static(ctx,  0xFE, 0x77);
	lcm_dcs_write_seq_static(ctx,  0x9A, 0x10);
	lcm_dcs_write_seq_static(ctx,  0x9B, 0x00);
	lcm_dcs_write_seq_static(ctx,  0xFE, 0x78);
	lcm_dcs_write_seq_static(ctx,  0x9A, 0x10);
	lcm_dcs_write_seq_static(ctx,  0x9B, 0x00);
	lcm_dcs_write_seq_static(ctx,  0xFE, 0x79);
	lcm_dcs_write_seq_static(ctx,  0x9A, 0xF0);
	lcm_dcs_write_seq_static(ctx,  0x9B, 0x00);
	lcm_dcs_write_seq_static(ctx,  0xFE, 0x74);
	lcm_dcs_write_seq_static(ctx,  0x1A, 0xE0);
	lcm_dcs_write_seq_static(ctx,  0x1B, 0x00);

	lcm_dcs_write_seq_static(ctx,  0xFE, 0xD0);
	lcm_dcs_write_seq_static(ctx,  0x9B, 0x9B);

	lcm_dcs_write_seq_static(ctx,  0xFE, 0xA0);
	lcm_dcs_write_seq_static(ctx,  0x66, 0x06);//hsync compensstion off

	lcm_dcs_write_seq_static(ctx,  0xFE, 0x49);
	lcm_dcs_write_seq_static(ctx,  0x87, 0x00);//mipi ultra lp setting
	lcm_dcs_write_seq_static(ctx,  0x3F, 0x00);//video mode

	lcm_dcs_write_seq_static(ctx,  0xFE, 0xA0);
	lcm_dcs_write_seq_static(ctx,  0x04, 0x07);
	lcm_dcs_write_seq_static(ctx,  0x4B, 0x40);
	lcm_dcs_write_seq_static(ctx,  0x4D, 0x40);

	lcm_dcs_write_seq_static(ctx,  0xFE, 0x40);
	lcm_dcs_write_seq_static(ctx,  0xC0, 0x36);//hsync compensstion off
	lcm_dcs_write_seq_static(ctx,  0xBF, 0x87);

	lcm_dcs_write_seq_static(ctx,  0xFE, 0xA1);
	lcm_dcs_write_seq_static(ctx,  0x58, 0x67);//mipi ultra lp setting
	lcm_dcs_write_seq_static(ctx,  0x75, 0xA7);//video mode
	lcm_dcs_write_seq_static(ctx,  0xFE, 0xD0);
	lcm_dcs_write_seq_static(ctx,  0xAD, 0x58);//LSB
	lcm_dcs_write_seq_static(ctx,  0xAE, 0x09);//MSB
	lcm_dcs_write_seq_static(ctx,  0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx,  0x2F, 0x00);//00 Nor_120Hz

	//DSC
	lcm_dcs_write_seq_static(ctx, 0xFE, 0xD4);

	lcm_dcs_write_seq_static(ctx, 0x61, 0x08);
	lcm_dcs_write_seq_static(ctx, 0xA2, 0x04);    // the read start line of VESA DSC

	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFA, 0x01);    //0x01 VESA ON

	lcm_dcs_write_seq_static(ctx, 0xFE, 0xD2);
	lcm_dcs_write_seq_static(ctx, 0x97, 0x08);    // use ENG PPS

	lcm_dcs_write_seq_static(ctx, 0x36, 0x11);
	lcm_dcs_write_seq_static(ctx, 0x39, 0x89);
	lcm_dcs_write_seq_static(ctx, 0x3A, 0x30);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x3D, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x3F, 0x58);
	lcm_dcs_write_seq_static(ctx, 0x40, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x41, 0x38);
	lcm_dcs_write_seq_static(ctx, 0x42, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x43, 0x0d);
	lcm_dcs_write_seq_static(ctx, 0x44, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x45, 0x1c);
	lcm_dcs_write_seq_static(ctx, 0x46, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x47, 0x1c);
	lcm_dcs_write_seq_static(ctx, 0x48, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x49, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x4A, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x4B, 0x0e);
	lcm_dcs_write_seq_static(ctx, 0x4D, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x4E, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0x39);
	lcm_dcs_write_seq_static(ctx, 0x50, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x07);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x0c);
	lcm_dcs_write_seq_static(ctx, 0x54, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x56, 0x07);
	lcm_dcs_write_seq_static(ctx, 0x58, 0xd3);
	lcm_dcs_write_seq_static(ctx, 0x59, 0x18);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5B, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x5C, 0xf0);
	lcm_dcs_write_seq_static(ctx, 0x5D, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x5E, 0x0c);
	lcm_dcs_write_seq_static(ctx, 0x5F, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x61, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x62, 0x0b);
	lcm_dcs_write_seq_static(ctx, 0x63, 0x0b);
	lcm_dcs_write_seq_static(ctx, 0x64, 0x33);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x0e);
	lcm_dcs_write_seq_static(ctx, 0x66, 0x1c);
	lcm_dcs_write_seq_static(ctx, 0x67, 0x2a);
	lcm_dcs_write_seq_static(ctx, 0x68, 0x38);
	lcm_dcs_write_seq_static(ctx, 0x69, 0x46);
	lcm_dcs_write_seq_static(ctx, 0x6A, 0x54);
	lcm_dcs_write_seq_static(ctx, 0x6B, 0x62);
	lcm_dcs_write_seq_static(ctx, 0x6C, 0x69);
	lcm_dcs_write_seq_static(ctx, 0x6D, 0x70);
	lcm_dcs_write_seq_static(ctx, 0x6E, 0x77);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x79);
	lcm_dcs_write_seq_static(ctx, 0x70, 0x7b);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x7d);
	lcm_dcs_write_seq_static(ctx, 0x72, 0x7e);
	lcm_dcs_write_seq_static(ctx, 0x73, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x74, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x75, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x76, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x78, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x7A, 0xbe);
	lcm_dcs_write_seq_static(ctx, 0x7B, 0x19);
	lcm_dcs_write_seq_static(ctx, 0x7C, 0xfc);
	lcm_dcs_write_seq_static(ctx, 0x7D, 0x19);
	lcm_dcs_write_seq_static(ctx, 0x7E, 0xfa);
	lcm_dcs_write_seq_static(ctx, 0x7F, 0x19);
	lcm_dcs_write_seq_static(ctx, 0x80, 0xf8);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x1a);
	lcm_dcs_write_seq_static(ctx, 0x82, 0x38);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x1a);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x78);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x1a);
	lcm_dcs_write_seq_static(ctx, 0x86, 0xb6);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x2a);
	lcm_dcs_write_seq_static(ctx, 0x88, 0xf6);
	lcm_dcs_write_seq_static(ctx, 0x89, 0x2b);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0x34);
	lcm_dcs_write_seq_static(ctx, 0x8B, 0x2b);
	lcm_dcs_write_seq_static(ctx, 0x8C, 0x74);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x3b);
	lcm_dcs_write_seq_static(ctx, 0x8E, 0x74);
	lcm_dcs_write_seq_static(ctx, 0x8F, 0x6b);
	lcm_dcs_write_seq_static(ctx, 0x90, 0xf4);
	lcm_dcs_write_seq_static(ctx, 0x91, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x92, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x93, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x94, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x95, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x96, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xFE,0xA0);
	lcm_dcs_write_seq_static(ctx, 0x06,0x76);
	lcm_dcs_write_seq_static(ctx, 0x7C,0x15);
	lcm_dcs_write_seq_static(ctx, 0xFE,0xD0);
	lcm_dcs_write_seq_static(ctx, 0x11,0x75);
	lcm_dcs_write_seq_static(ctx, 0x92,0x03);
	lcm_dcs_write_seq_static(ctx, 0xFE,0x42);
	lcm_dcs_write_seq_static(ctx, 0x17,0x00);
	lcm_dcs_write_seq_static(ctx, 0xFE,0xD4);
	lcm_dcs_write_seq_static(ctx, 0x40,0x03);
	lcm_dcs_write_seq_static(ctx, 0xFE,0xFD);
	lcm_dcs_write_seq_static(ctx, 0x80,0x04);
	lcm_dcs_write_seq_static(ctx, 0x83,0x00);
	lcm_dcs_write_seq_static(ctx, 0xFE,0xA1);
	lcm_dcs_write_seq_static(ctx, 0x74,0x70);
	lcm_dcs_write_seq_static(ctx, 0xC3,0x83);
	lcm_dcs_write_seq_static(ctx, 0xC4,0xFF);
	lcm_dcs_write_seq_static(ctx, 0xC5,0x7F);

	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFA, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xC1, 0x03);    //By pass RAM
	lcm_dcs_write_seq_static(ctx, 0xC2, 0x03);   //Video mode
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x03);

	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11);
	usleep_range(80 * 1000, 81 * 1000);
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

	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x28);
	usleep_range(16 * 1000, 17 * 1000);
	lcm_dcs_write_seq_static(ctx, 0x10);
	//deep standby
	lcm_dcs_write_seq_static(ctx, 0xFE, 0xFD);
	lcm_dcs_write_seq_static(ctx, 0x84, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x5A);
	usleep_range(100 * 1000, 101 * 1000);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0x01);

	ctx->error = 0;
	ctx->prepared = false;
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(2 * 1000, 4 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->vci_en_gpio = devm_gpiod_get(ctx->dev, "vci-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vci_en_gpio, 0);
	usleep_range(2 * 1000, 4 * 1000);
	devm_gpiod_put(ctx->dev, ctx->vci_en_gpio);

	ctx->dvdd_en_gpio = devm_gpiod_get(ctx->dev, "dvdd-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->dvdd_en_gpio, 0);
	usleep_range(1 * 1000, 2 * 1000);
	devm_gpiod_put(ctx->dev, ctx->dvdd_en_gpio);

	ctx->vddi_en_gpio = devm_gpiod_get(ctx->dev, "vddi-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddi_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddi_en_gpio);

	ctx->hbm_en = false;
	ctx->local_hbm_en = false;

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
	usleep_range(2 * 1000, 4 * 1000);
	devm_gpiod_put(ctx->dev, ctx->vddi_en_gpio);

	ctx->dvdd_en_gpio = devm_gpiod_get(ctx->dev, "dvdd-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->dvdd_en_gpio, 1);
	usleep_range(2 * 1000, 3 * 1000);
	devm_gpiod_put(ctx->dev, ctx->dvdd_en_gpio);

	ctx->vci_en_gpio = devm_gpiod_get(ctx->dev, "vci-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vci_en_gpio, 1);
	usleep_range(2 * 1000, 4 * 1000);
	devm_gpiod_put(ctx->dev, ctx->vci_en_gpio);

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

	DDPPR_ERR("ATA read data %x %x %x\n", data[0], data[1], data[2]);
/*
	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;
*/
	DDPPR_ERR("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 1;
}

static struct LCM_setting_table lhbm_on_cmd_tb[] = {
	{REGFLAG_CMD, 2, {0xFE, 0x00}},
	{REGFLAG_CMD, 2, {0x2F, 0x07}},
	{REGFLAG_CMD, 2, {0x83, 0x01}},
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lhbm_off_cmd_tb[] = {
	{REGFLAG_CMD, 2, {0xFE, 0x00}},
	{REGFLAG_CMD, 2, {0x83, 0x00}},
	{REGFLAG_CMD, 2, {0x2F, 0x00}},
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
	// aod 60nit
	{REGFLAG_CMD, 3, {0x51, 0x0F, 0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_aod_middle_mode[] = {
	// aod 30nit
	{REGFLAG_CMD, 3, {0x51, 0x0B, 0x2E} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_aod_low_mode[] = {
	// aod 5nit
	{REGFLAG_CMD, 3, {0x51, 0x00, 0x03} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

extern int last_bl_level ;
static int lcm_setbacklight_cmdq(void *dsi,
		dcs_write_gce cb, void *handle, unsigned int level)
{
	int i;
	char page0[] = {0xFE, 0x00};

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

	cb(dsi, handle, page0, ARRAY_SIZE(page0));
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

static struct LCM_setting_table exit_aod_black[] = {
	{REGFLAG_CMD, 2, {0xFE, 0x00} },
	{REGFLAG_CMD, 3, {0x51, 0x00, 0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int panel_exit_aod_insert_black(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	int i;
	pr_info("[LCM] %s\n", __func__);
	for (i = 0; i < sizeof(exit_aod_black)/sizeof(struct LCM_setting_table); i++)
		cb(dsi, handle, exit_aod_black[i].para_list, exit_aod_black[i].count);

	return 0;
}

static struct mtk_panel_params ext_params_120hz = {
	.change_fps_by_vfp_send_cmd = 1,
	.data_rate = 885,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.platform_esdbl_rec = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0xFA,
		.count = 1,
		.para_list[0] = 0x01,
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
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFE, 0x00} },
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x00} },
		.exit_doze_black = true,
		.doze_dis_cmd_table[0] = {0, 2, {0xFE, 0x00} },
		.doze_dis_cmd_table[1] = {0, 1, {0x38} },
		.bl_update_cmd_table[0] = {0, 3, {0x51, 0x03, 0x09} },
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.change_fps_by_vfp_send_cmd = 1,
	.data_rate = 885,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.platform_esdbl_rec = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0xFA,
		.count = 1,
		.para_list[0] = 0x01,
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
		.switch_en = 1,
		.vact_timing_fps = 90,
		.dfps_cmd_table[0] = {0, 2, {0xFE, 0x00} },
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x05} },
		.exit_doze_black = true,
		.doze_dis_cmd_table[0] = {0, 2, {0xFE, 0x00} },
		.doze_dis_cmd_table[1] = {0, 1, {0x38} },
		.bl_update_cmd_table[0] = {0, 3, {0x51, 0x03, 0x09} },
	},
};

static struct mtk_panel_params ext_params_60hz = {
	.change_fps_by_vfp_send_cmd = 1,
	.data_rate = 885,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.platform_esdbl_rec = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0xFA,
		.count = 1,
		.para_list[0] = 0x01,
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
		.switch_en = 1,
		.vact_timing_fps = 60,
		.dfps_cmd_table[0] = {0, 2, {0xFE, 0x00} },
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x06} },
		.exit_doze_black = true,
		.doze_dis_cmd_table[0] = {0, 2, {0xFE, 0x00} },
		.doze_dis_cmd_table[1] = {0, 1, {0x38} },
		.bl_update_cmd_table[0] = {0, 3, {0x51, 0x03, 0x09} },
	},
};

static struct mtk_panel_params ext_params_30hz = {
	.change_fps_by_vfp_send_cmd = 1,
	.data_rate = 885,
	.esd_check_enable = 0,
	.cust_esd_check = 1,
	.platform_esdbl_rec = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0xDC,
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
		.switch_en = 1,
		.vact_timing_fps = 30,
		.doze_en_cmd_table[0] = {0, 2, {0xFE, 0x00} },
		.doze_en_cmd_table[1] = {0, 1, {0x39} },
		.aod_high_cmd_table[0] = {0, 3, {0x51, 0x0F, 0xFF} },         //AOD 60nit
		.aod_middle_cmd_table[0] = {0, 3, {0x51, 0x0B, 0x2E} },  //AOD 30nit
		.aod_low_cmd_table[0] = {0, 3, {0x51, 0x00, 0x03} },          //AOD 5nit
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
	} else
		ret = 1;

//	if (ext->params->dyn_fps.vact_timing_fps != 30) {
//		ext->params->dyn_fps.doze_dis_cmd_table[3].para_list[1] = (last_level >> 8) & 0x0F;
//		ext->params->dyn_fps.doze_dis_cmd_table[3].para_list[2] = last_level & 0xFF;
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
	.exit_aod_insert_black = panel_exit_aod_insert_black,
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
	.clock = 345340,
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
	.clock = 342874,
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
	.clock = 342874,
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
	.clock = 197467,
	.hdisplay = HACT,
	.hsync_start = HACT + HFP_30hz,
	.hsync_end = HACT + HFP_30hz + HSA,
	.htotal = HACT + HFP_30hz + HSA + HBP,
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
			tx[0] = 0xFE;
			tx[1] = 0x00;
			cmd_msg->tx_len[0] = 2;
			mtk_ddic_dsi_send_cmd(cmd_msg, 0);
			cmd_msg->type[0] = 0x15;
			tx[0] = 0x83;
			tx[1] = 0x00;
			cmd_msg->tx_len[0] = 2;
			mtk_ddic_dsi_send_cmd(cmd_msg, 0);
			cmd_msg->type[0] = 0x15;
			tx[0] = 0x2F;
			tx[1] = 0x00;
			cmd_msg->tx_len[0] = 2;
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
	{2, {0xFE, 0x00} },
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
	set_backlight_cmd[1].para_list[1] = (state >> 8) & 0x0F;
	set_backlight_cmd[1].para_list[2] = (state) & 0xFF;

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

	ret = mtk_panel_ext_create(dev, &ext_params_120hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

	ret = sysfs_create_group(&dev->kobj, &aod_area_sysfs_attr_group);
	if (ret)
		return ret;

	ctx->hbm_en = false;
	ctx->local_hbm_en = false;
	lcm_sys_node_init();
	displayid_node_init();
	atomic_set(&current_backlight, 4095);

	snprintf(panel_name_find, sizeof(panel_name_find), "rm69220_boe");

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
	{ .compatible = "rm69220,boe", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "rm69220,boe",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Linus Wallei <linus.walleij@linaro.org>");
MODULE_DESCRIPTION("MIPI-DSI rm69220 Panel Driver");
MODULE_LICENSE("GPL v2");
