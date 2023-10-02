// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chuan-wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6878-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs afe0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs afe1_cg_regs = {
	.set_ofs = 0x10,
	.clr_ofs = 0x10,
	.sta_ofs = 0x10,
};

static const struct mtk_gate_regs afe2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

static const struct mtk_gate_regs afe3_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

static const struct mtk_gate_regs afe4_cg_regs = {
	.set_ofs = 0xC,
	.clr_ofs = 0xC,
	.sta_ofs = 0xC,
};

#define GATE_AFE0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AFE4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &afe4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate afe_clks[] = {
	/* AFE0 */
	GATE_AFE0(CLK_AFE_DL0_DAC_TML, "afe_dl0_dac_tml",
			"top_f26m_ck"/* parent */, 7),
	GATE_AFE0(CLK_AFE_DL0_DAC_HIRES, "afe_dl0_dac_hires",
			"top_audio_h_ck"/* parent */, 8),
	GATE_AFE0(CLK_AFE_DL0_DAC, "afe_dl0_dac",
			"top_f26m_ck"/* parent */, 9),
	GATE_AFE0(CLK_AFE_DL0_PREDIS, "afe_dl0_predis",
			"top_f26m_ck"/* parent */, 10),
	GATE_AFE0(CLK_AFE_DL0_NLE, "afe_dl0_nle",
			"top_f26m_ck"/* parent */, 11),
	GATE_AFE0(CLK_AFE_PCM1, "afe_pcm1",
			"top_f26m_ck"/* parent */, 13),
	GATE_AFE0(CLK_AFE_PCM0, "afe_pcm0",
			"top_f26m_ck"/* parent */, 14),
	GATE_AFE0(CLK_AFE_CM1, "afe_cm1",
			"top_f26m_ck"/* parent */, 17),
	GATE_AFE0(CLK_AFE_CM0, "afe_cm0",
			"top_f26m_ck"/* parent */, 18),
	GATE_AFE0(CLK_AFE_STF, "afe_stf",
			"top_f26m_ck"/* parent */, 19),
	GATE_AFE0(CLK_AFE_HW_GAIN23, "afe_hw_gain23",
			"top_f26m_ck"/* parent */, 20),
	GATE_AFE0(CLK_AFE_HW_GAIN01, "afe_hw_gain01",
			"top_f26m_ck"/* parent */, 21),
	GATE_AFE0(CLK_AFE_FM_I2S, "afe_fm_i2s",
			"top_f26m_ck"/* parent */, 24),
	GATE_AFE0(CLK_AFE_MTKAIFV4, "afe_mtkaifv4",
			"top_f26m_ck"/* parent */, 25),
	/* AFE1 */
	GATE_AFE1(CLK_AFE_AUDIO_HOPPING, "afe_audio_hopping_ck",
			"top_f26m_ck"/* parent */, 0),
	GATE_AFE1(CLK_AFE_AUDIO_F26M, "afe_audio_f26m_ck",
			"top_f26m_ck"/* parent */, 1),
	GATE_AFE1(CLK_AFE_APLL1, "afe_apll1_ck",
			"top_aud_1_ck"/* parent */, 2),
	GATE_AFE1(CLK_AFE_APLL2, "afe_apll2_ck",
			"top_aud_2_ck"/* parent */, 3),
	GATE_AFE1(CLK_AFE_H208M, "afe_h208m_ck",
			"top_audio_h_ck"/* parent */, 4),
	GATE_AFE1(CLK_AFE_APLL_TUNER2, "afe_apll_tuner2",
			"top_aud_engen2_ck"/* parent */, 12),
	GATE_AFE1(CLK_AFE_APLL_TUNER1, "afe_apll_tuner1",
			"top_aud_engen1_ck"/* parent */, 13),
	/* AFE2 */
	GATE_AFE2(CLK_AFE_UL1_ADC_HIRES_TML, "afe_ul1_aht",
			"top_audio_h_ck"/* parent */, 16),
	GATE_AFE2(CLK_AFE_UL1_ADC_HIRES, "afe_ul1_adc_hires",
			"top_audio_h_ck"/* parent */, 17),
	GATE_AFE2(CLK_AFE_UL1_TML, "afe_ul1_tml",
			"top_f26m_ck"/* parent */, 18),
	GATE_AFE2(CLK_AFE_UL1_ADC, "afe_ul1_adc",
			"top_f26m_ck"/* parent */, 19),
	GATE_AFE2(CLK_AFE_UL0_ADC_HIRES_TML, "afe_ul0_aht",
			"top_audio_h_ck"/* parent */, 20),
	GATE_AFE2(CLK_AFE_UL0_ADC_HIRES, "afe_ul0_adc_hires",
			"top_audio_h_ck"/* parent */, 21),
	GATE_AFE2(CLK_AFE_UL0_TML, "afe_ul0_tml",
			"top_f26m_ck"/* parent */, 22),
	GATE_AFE2(CLK_AFE_UL0_ADC, "afe_ul0_adc",
			"top_f26m_ck"/* parent */, 23),
	/* AFE3 */
	GATE_AFE3(CLK_AFE_ETDM_IN4, "afe_etdm_in4",
			"top_aud_engen1_ck"/* parent */, 9),
	GATE_AFE3(CLK_AFE_ETDM_IN2, "afe_etdm_in2",
			"top_aud_engen1_ck"/* parent */, 11),
	GATE_AFE3(CLK_AFE_ETDM_IN1, "afe_etdm_in1",
			"top_aud_engen1_ck"/* parent */, 12),
	GATE_AFE3(CLK_AFE_ETDM_OUT4, "afe_etdm_out4",
			"top_aud_engen1_ck"/* parent */, 17),
	GATE_AFE3(CLK_AFE_ETDM_OUT2, "afe_etdm_out2",
			"top_aud_engen1_ck"/* parent */, 19),
	GATE_AFE3(CLK_AFE_ETDM_OUT1, "afe_etdm_out1",
			"top_aud_engen1_ck"/* parent */, 20),
	/* AFE4 */
	GATE_AFE4(CLK_AFE_GENERAL2_ASRC, "afe_general2_asrc",
			"top_audio_h_ck"/* parent */, 22),
	GATE_AFE4(CLK_AFE_GENERAL1_ASRC, "afe_general1_asrc",
			"top_audio_h_ck"/* parent */, 23),
	GATE_AFE4(CLK_AFE_GENERAL0_ASRC, "afe_general0_asrc",
			"top_audio_h_ck"/* parent */, 24),
	GATE_AFE4(CLK_AFE_CONNSYS_I2S_ASRC, "afe_connsys_i2s_asrc",
			"top_audio_h_ck"/* parent */, 25),
};

static const struct mtk_clk_desc afe_mcd = {
	.clks = afe_clks,
	.num_clks = CLK_AFE_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6878_adsp[] = {
	{
		.compatible = "mediatek,mt6878-audiosys",
		.data = &afe_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6878_adsp_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6878_adsp_drv = {
	.probe = clk_mt6878_adsp_grp_probe,
	.driver = {
		.name = "clk-mt6878-adsp",
		.of_match_table = of_match_clk_mt6878_adsp,
	},
};

module_platform_driver(clk_mt6878_adsp_drv);
MODULE_LICENSE("GPL");
