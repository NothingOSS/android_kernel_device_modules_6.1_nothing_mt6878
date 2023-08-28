// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
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

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include "panel-ili7838e-dv2-spr-cmd.h"
//static unsigned int spr_in_rc_buf_thresh[14] = { 896, 1792, 2688, 3584, 4480, 5376,
//							    6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064 };
//static unsigned int spr_in_range_min_qp[15]	= { 0, 2, 3, 4, 6, 7, 7, 7, 7, 7, 9, 9, 9, 11, 14 };
//static unsigned int spr_in_range_max_qp[15]	= { 2, 5, 7, 8, 9, 10, 11, 12, 12, 13, 13, 13, 13, 14, 15 };
//static int spr_in_range_bpg_ofs[15] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12 };

static unsigned int rc_buf_thresh[14] = { 896, 1792, 2688, 3584, 4480, 5376,
							    6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064 };
static unsigned int range_min_qp[15]	= { 0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 11, 17 };
static unsigned int range_max_qp[15]	= { 8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 15, 16, 17, 17, 19 };
static int range_bpg_ofs[15] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12 };


static bool panel_spr_enable;


#define lcm_err(fmt, ...) \
	pr_info("lcm_err: %s(%d): " fmt,  __func__, __LINE__, ##__VA_ARGS__)

#define lcm_info(fmt, ...) \
	pr_info("lcm_info: %s(%d): " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF
#define MDSS_MAX_PANEL_LEN	256

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vddr_gpio;
	struct gpio_desc *ason_0p8_gpio;
	struct gpio_desc *ason_1p8_gpio;

	struct regulator *oled_vddi;
	struct regulator *oled_vci;

	bool prepared;
	bool enabled;

	int error;
};

#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
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
		pr_info("error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

inline void push_table(struct lcm *ctx, struct LCD_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	char delay_ms;

	for (i = 0; i < count; i++) {
		if (!table[i].count) {
			delay_ms = table[i].para_list[0];
			lcm_info("delay %dms(max 255ms once)\n", delay_ms);
			usleep_range(delay_ms * 1000, delay_ms * 1000 + 100);
		} else {
			lcm_dcs_write(ctx, &table[i].para_list,
					table[i].count);
		}
	}
}

inline void push_table_cmdq(struct mtk_dsi *dsi, dcs_write_gce cb, void *handle, struct LCD_setting_table *table,
		unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++)
		cb(dsi, handle, &table[i].para_list, table[i].count);

}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		pr_info("error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static unsigned int lcm_set_regulator(struct regulator *reg, int en)
{
	unsigned int ret = 0, volt = 0;

	if (en) {
		if (!IS_ERR_OR_NULL(reg)) {
			ret = regulator_enable(reg);
			if (regulator_is_enabled(reg))
				volt = regulator_get_voltage(reg);
			pr_info("enable: the reg vol = %d\n", volt);
		}
	} else {
		if (!IS_ERR_OR_NULL(reg)) {
			ret = regulator_disable(reg);
			pr_info("disable: the reg vol = %d\n", volt);
		}
	}
	return ret;
}

static int lcm_panel_power_regulator_init(struct device *dev, struct lcm *ctx)
{
	int ret = 0;

	ctx->oled_vddi = devm_regulator_get_optional(dev, "vddi");
	if (IS_ERR(ctx->oled_vddi)) {
		pr_info("cannot get oled_vddi %ld\n",
			PTR_ERR(ctx->oled_vddi));
	}
	regulator_set_voltage(ctx->oled_vddi,
				1800000, 1800000);

	ctx->oled_vci = devm_regulator_get_optional(dev, "vci");
	if (IS_ERR(ctx->oled_vci)) {
		pr_info("cannot get oled_vci %ld\n",
			PTR_ERR(ctx->oled_vci));
	}
	ret = regulator_set_voltage(ctx->oled_vci,
				3000000, 3000000);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_info("%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->vddr_gpio = devm_gpiod_get_index(ctx->dev, "vddr", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddr_gpio)) {
		pr_info("%s: cannot get vddr_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddr_gpio));
		return PTR_ERR(ctx->vddr_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->vddr_gpio);

	ctx->ason_0p8_gpio = devm_gpiod_get_index(ctx->dev, "ason-0p8", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->ason_0p8_gpio)) {
		pr_info("%s: cannot get ason_0p8_gpio %ld\n",
			__func__, PTR_ERR(ctx->ason_0p8_gpio));
		return PTR_ERR(ctx->ason_0p8_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->ason_0p8_gpio);

	ctx->ason_1p8_gpio = devm_gpiod_get_index(ctx->dev, "ason-1p8", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->ason_1p8_gpio)) {
		pr_info("%s: cannot get ason_1p8_gpio %ld\n",
			__func__, PTR_ERR(ctx->ason_1p8_gpio));
		return PTR_ERR(ctx->ason_1p8_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->ason_1p8_gpio);

	return ret;
}


static int lcm_panel_power_enable(struct lcm *ctx)
{
	int ret = 0;

	pr_info("%s+\n", __func__);
	lcm_set_regulator(ctx->oled_vddi, 1);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->vddr_gpio, 1);
	usleep_range(2000, 2100);
	lcm_set_regulator(ctx->oled_vci, 1);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->ason_0p8_gpio, 1);
	usleep_range(5000, 5100);
	gpiod_set_value(ctx->ason_1p8_gpio, 1);
	usleep_range(10000, 10100);

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(12000, 12100);

	return ret;
}

static int lcm_panel_power_disable(struct lcm *ctx)
{
	int ret = 0;

	pr_info("%s+\n", __func__);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(12000, 12100);
	lcm_set_regulator(ctx->oled_vci, 0);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->vddr_gpio, 0);
	usleep_range(2000, 2100);
	lcm_set_regulator(ctx->oled_vddi, 0);
	usleep_range(5000, 5100);
	gpiod_set_value(ctx->ason_0p8_gpio, 0);
	usleep_range(10000, 10100);
	gpiod_set_value(ctx->ason_1p8_gpio, 0);

	return ret;
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s+\n", __func__);
	push_table(ctx, init_setting_fhd_120hz_part1,
			sizeof(init_setting_fhd_120hz_part1) / sizeof(struct LCD_setting_table), 1);
	if(panel_spr_enable)
		push_table(ctx, init_setting_fhd_120hz_part2_rgb_in,
				sizeof(init_setting_fhd_120hz_part2_rgb_in) / sizeof(struct LCD_setting_table), 1);
	else
		push_table(ctx, init_setting_fhd_120hz_part2_spr_in,
				sizeof(init_setting_fhd_120hz_part2_spr_in) / sizeof(struct LCD_setting_table), 1);
	push_table(ctx, init_setting_fhd_120hz_part3,
			sizeof(init_setting_fhd_120hz_part3) / sizeof(struct LCD_setting_table), 1);
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

	if (!ctx->prepared)
		return 0;

	push_table(ctx, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCD_setting_table), 1);

	ctx->error = 0;
	ctx->prepared = false;
	lcm_panel_power_disable(ctx);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;


	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	lcm_panel_power_enable(ctx);
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

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

static const struct drm_display_mode default_mode = {
	.clock = 437960,
	.hdisplay = FHD_FRAME_WIDTH,
	.hsync_start = FHD_FRAME_WIDTH + HFP,
	.hsync_end = FHD_FRAME_WIDTH + HFP + HSA,
	.htotal = FHD_FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FHD_FRAME_HEIGHT,
	.vsync_start = FHD_FRAME_HEIGHT + VFP,
	.vsync_end = FHD_FRAME_HEIGHT + VFP + VSA,
	.vtotal = FHD_FRAME_HEIGHT + VFP + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_info("%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static void mipi_dcs_write_backlight_command(void *dsi, dcs_write_gce cb,
		void *handle, unsigned int level)
{
	cmd_bl_level[0].para_list[1] = (unsigned char)((level>>8) & 0xF);
	cmd_bl_level[0].para_list[2] = (unsigned char)(level & 0xFF);
	pr_info("dsi set backlight level %d\n", level);
	push_table_cmdq(dsi, cb, handle, cmd_bl_level, sizeof(cmd_bl_level) / sizeof(struct LCD_setting_table), 1);
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	if (!cb)
		return -1;

	mipi_dcs_write_backlight_command(dsi, cb, handle, level);

	return 0;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = MIPI_DATA_RATE_120HZ / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  FHD_DSC_ENABLE,
		.ver                   =  FHD_DSC_VER,
		.slice_mode            =  FHD_DSC_SLICE_MODE,
		.rgb_swap              =  FHD_DSC_RGB_SWAP,
		.dsc_cfg               =  FHD_DSC_DSC_CFG,
		.rct_on                =  FHD_DSC_RCT_ON,
		.bit_per_channel       =  FHD_DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  FHD_DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  FHD_DSC_BP_ENABLE,
		.bit_per_pixel         =  FHD_DSC_BIT_PER_PIXEL,
		.pic_height            =  FHD_FRAME_HEIGHT,
		.pic_width             =  FHD_FRAME_WIDTH,
		.slice_height          =  FHD_DSC_SLICE_HEIGHT,
		.slice_width           =  FHD_DSC_SLICE_WIDTH,
		.chunk_size            =  FHD_DSC_CHUNK_SIZE,
		.xmit_delay            =  FHD_DSC_XMIT_DELAY,
		.dec_delay             =  FHD_DSC_DEC_DELAY,
		.scale_value           =  FHD_DSC_SCALE_VALUE,
		.increment_interval    =  FHD_DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  FHD_DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  FHD_DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  FHD_DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  FHD_DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  FHD_DSC_INITIAL_OFFSET,
		.final_offset          =  FHD_DSC_FINAL_OFFSET,
		.flatness_minqp        =  FHD_DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  FHD_DSC_FLATNESS_MAXQP,
		.rc_model_size         =  FHD_DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  FHD_DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  FHD_DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  FHD_DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  FHD_DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  FHD_DSC_RC_TGT_OFFSET_LO,
		.ext_pps_cfg = {
				.enable = 1,
				.rc_buf_thresh = rc_buf_thresh,
				.range_min_qp = range_min_qp,
				.range_max_qp = range_max_qp,
				.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.spr_params = {
		.enable = 1,
		.relay = 0,
		.postalign_en = 0,
		.bypass_dither = 1,
		.custom_header = 11,//0x2C for default set
		.spr_format_type = MTK_PANEL_RGBG_BGRG_TYPE,
		//.rg_xy_swap = 1,
		.spr_ip_params = panel_boe_ili7838_spr_ip_cfg,
		.spr_switch_type = SPR_SWITCH_TYPE2,
	},

	.spr_output_mode = MTK_PANEL_PACKED_SPR_8_BITS,

	.dsc_params_spr_in = {
		.enable				   =  1,
		.ver				   =  17,
		.slice_mode			   =  1,
		.rgb_swap			   =  0,
		.dsc_cfg			   =  40,
		.rct_on				   =  1,
		.bit_per_channel	   =  10,
		.dsc_line_buf_depth    =  11,
		.bp_enable			   =  1,
		.bit_per_pixel		   =  128,
		.pic_height			   =  FHD_FRAME_HEIGHT,
		.pic_width			   =  FHD_FRAME_WIDTH,
		.slice_height		   =  20,
		.slice_width		   =  630,
		.chunk_size			   =  630,
		.xmit_delay			   =  512,
		.dec_delay			   =  571,
		.scale_value		   =  32,
		.increment_interval    =  526,
		.decrement_interval    =  8,
		.line_bpg_offset	   =  12,
		.nfl_bpg_offset		   =  1294,
		.slice_bpg_offset	   =  1116,
		.initial_offset		   =  6144,
		.final_offset		   =  4336,
		.flatness_minqp		   =  7,
		.flatness_maxqp		   =  16,
		.rc_model_size		   =  8192,
		.rc_edge_factor		   =  6,
		.rc_quant_incr_limit0  =  15,
		.rc_quant_incr_limit1  =  15,
		.rc_tgt_offset_hi	   =  3,
		.rc_tgt_offset_lo	   =  3,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},

	.dyn_fps = {
			.switch_en = 0, .vact_timing_fps = 120,
	},
	.data_rate = MIPI_DATA_RATE_120HZ,
};


static int lcm_set_spr_cmdq(void *dsi, struct drm_panel *panel, dcs_grp_write_gce cb,
	void *handle, unsigned int en)
{

static struct mtk_panel_para_table panel_spr_on_tb[] = {

		{0x04,{0xFF, 0x08, 0x38, 0x07}},

		{0x02,{0x0e, 0x03}},

		{0x04,{0xFF, 0x08, 0x38, 0x4F}},

		{0x02,{0x80, 0x01}},                 //CMD QUEUE ENABLE

		{0x02,{0x81, 0x02}},

		{0x04,{0xFF, 0x08, 0x38, 0x06}},

		{0x02,{0xCF, 0xC3}},

		{0x04,{0xFF, 0x08, 0x38, 0x4F}},

		{0x02,{0x88, 0x78}},               //CMD QUEUE START

		{0x04,{0xFF, 0x08, 0x38, 0x00}},

		{0x02,{0x22, 0x00}},                   //ALL PIXEL OFF

		{0x04,{0xFF, 0x08, 0x38, 0x01}},

		{0x02,{0x96, 0x02}},             //GOUTR_07_MUX[5:0], STV_B NSTV keep L

		{0x02,{0x9F, 0x04}},             //GOUTR_16_MUX[5:0], STV_F GSTV keep H

		{0x02,{0xB3, 0x04}},             //GOUTL_16_MUX[5:0], STV_F GSTV keep H

		{0x04,{0xFF, 0x08, 0x38, 0x4F}},

		{0x02,{0x8A, 0x78}},

		{0x04,{0xFF, 0x08, 0x38, 0x01}},

		{0x02,{0x96, 0x07}},             //GOUTR_07_MUX[5:0], STV_B NSTV

		{0x02,{0x9F, 0x1B}},             //GOUTR_16_MUX[5:0], STV_F GSTV

		{0x02,{0xB3, 0x1B}},             //GOUTL_16_MUX[5:0], STV_F GSTV

		{0x04,{0xFF, 0x08, 0x38, 0x00}},

		{0x02,{0x13, 0x00}},                 //NORMAL mode ON

		{0x04,{0xFF, 0x08, 0x38, 0x14}},

		{0x02,{0x82, 0x00}},             //SPR IN AP OFF

		{0x04,{0xFF, 0x08, 0x38, 0x07}},

		{0x03,{0x8B, 0x11, 0xA0}},        //pps chose reg80

		{0x04,{0xFF, 0x08, 0x38, 0x4F}},

		{0x02,{0x8B, 0x78}},                //CMD QUEUE  SET DONE

		{0x04,{0xFF, 0x08, 0x38, 0x00}},

		//{0x01,{0x2C}},

	};

	static struct mtk_panel_para_table panel_spr_off_tb[] = {

		{0x04,{0xFF, 0x08, 0x38, 0x07}},

		{0x02,{0x0e, 0x03}},

		{0x04,{0xFF, 0x08, 0x38, 0x4F}},

		{0x02,{0x80, 0x01}},                 //CMD QUEUE ENABLE

		{0x02,{0x81, 0x02}},

		{0x04,{0xFF, 0x08, 0x38, 0x06}},

		{0x02,{0xCF, 0xC3}},

		{0x04,{0xFF, 0x08, 0x38, 0x4F}},

		{0x02,{0x88, 0x78}},               //CMD QUEUE START

		{0x04,{0xFF, 0x08, 0x38, 0x00}},

		{0x02,{0x22, 0x00}},                   //ALL PIXEL OFF

		{0x04,{0xFF, 0x08, 0x38, 0x01}},

		{0x02,{0x96, 0x02}},             //GOUTR_07_MUX[5:0], STV_B NSTV keep L

		{0x02,{0x9F, 0x04}},             //GOUTR_16_MUX[5:0], STV_F GSTV keep H

		{0x02,{0xB3, 0x04}},             //GOUTL_16_MUX[5:0], STV_F GSTV keep H

		{0x04,{0xFF, 0x08, 0x38, 0x4F}},

		{0x02,{0x8A, 0x78}},

		{0x04,{0xFF, 0x08, 0x38, 0x01}},

		{0x02,{0x96, 0x07}},             //GOUTR_07_MUX[5:0], STV_B NSTV

		{0x02,{0x9F, 0x1B}},             //GOUTR_16_MUX[5:0], STV_F GSTV

		{0x02,{0xB3, 0x1B}},             //GOUTL_16_MUX[5:0], STV_F GSTV

		{0x04,{0xFF, 0x08, 0x38, 0x00}},

		{0x02,{0x13, 0x00}},                 //NORMAL mode ON

		{0x04,{0xFF, 0x08, 0x38, 0x14}},

		{0x02,{0x82, 0x01}},             //SPR IN AP ON

		{0x04,{0xFF, 0x08, 0x38, 0x07}},

		{0x03,{0x8B, 0x11, 0xE0}},        //pps chose reg81

		{0x04,{0xFF, 0x08, 0x38, 0x4F}},

		{0x02,{0x8B, 0x78}},                //CMD QUEUE  SET DONE

		{0x04,{0xFF, 0x08, 0x38, 0x00}},

		//{0x01,{0x2C}},

	};
		//ddic spr off
		if (en == 0xfefe) {
			panel_spr_enable = false;
			return -1;
		}
		//ddic spr on
		if (en == 0xeeee) {
			panel_spr_enable = true;
			return -1;
		}
		if (!cb)
			return -1;
		if (!handle)
			return -1;
		if (en)
			cb(dsi, handle, panel_spr_on_tb, ARRAY_SIZE(panel_spr_on_tb));
		else
			cb(dsi, handle, panel_spr_off_tb, ARRAY_SIZE(panel_spr_off_tb));
		return 0;
}


static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.set_spr_cmdq = lcm_set_spr_cmdq,
};
#endif

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 64;
	connector->display_info.height_mm = 129;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	pr_info("%s+\n", __func__);
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

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
				 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	lcm_panel_power_regulator_init(dev, ctx);
	lcm_set_regulator(ctx->oled_vddi, 1);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->vddr_gpio, 1);
	usleep_range(2000, 2100);
	lcm_set_regulator(ctx->oled_vci, 1);
	usleep_range(2000, 2100);
	gpiod_set_value(ctx->ason_0p8_gpio, 1);
	usleep_range(5000, 5100);
	gpiod_set_value(ctx->ason_1p8_gpio, 1);
	usleep_range(10000, 10100);

#ifndef CONFIG_MTK_DISP_NO_LK
	ctx->prepared = true;
	ctx->enabled = true;
#endif

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("%s-\n", __func__);

	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "mt6989,ili7838e-dv2-spr,cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-ili7838e-dv2-spr-cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Castro Dong <castro.dong@mediatek.com>");
MODULE_DESCRIPTION("MT6989 ILI7838E-DV2-ALPHA CMD Panel Driver");
MODULE_LICENSE("GPL");
