// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chuan-wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>

#include <dt-bindings/power/mt6878-power.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
#include <devapc_public.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#include <mt-plat/dvfsrc-exp.h>
#endif

#include "clkchk.h"
#include "clkchk-mt6878.h"
#include "clk-fmeter.h"
#include "clk-mt6878-fmeter.h"

#define BUG_ON_CHK_ENABLE		0
#define CHECK_VCORE_FREQ		0
#define CG_CHK_PWRON_ENABLE		0

#define HWV_INT_PLL_TRIGGER		0x0004
#define HWV_INT_CG_TRIGGER		0x10001

#define HWV_DOMAIN_KEY			0x055C
#define HWV_IRQ_STATUS			0x0500
#define HWV_CG_SET(xpu, id)		((0x0200 * (xpu)) + (id * 0x8))
#define HWV_CG_STA(id)			(0x1800 + (id * 0x4))
#define HWV_CG_EN(id)			(0x1900 + (id * 0x4))
#define HWV_CG_XPU_DONE(xpu)		(0x1B00 + (xpu * 0x8))
#define HWV_CG_DONE(id)			(0x1C00 + (id * 0x4))
#define HWV_TIMELINE_PTR		(0x1AA0)
#define HWV_TIMELINE_HIS(idx)		(0x1AA4 + (idx / 4))
#define HWV_CG_ADDR_HIS(idx)		(0x18C8 + (idx * 4))
#define HWV_CG_ADDR_14_HIS(idx)		(0x19C8 + ((idx - 14) * 4))
#define HWV_CG_DATA_HIS(idx)		(0x1AC8 + (idx * 4))
#define HWV_CG_DATA_14_HIS(idx)		(0x1BC8 + ((idx - 14) * 4))
#define HWV_IRQ_XPU_HIS_PTR		(0x1B50)
#define HWV_IRQ_ADDR_HIS(idx)		(0x1B54 + (idx * 4))
#define HWV_IRQ_DATA_HIS(idx)		(0x1B8C + (idx * 4))

#define EVT_LEN				40
#define CLK_ID_SHIFT			0
#define CLK_STA_SHIFT			12

static DEFINE_SPINLOCK(clk_trace_lock);
static unsigned int clk_event[EVT_LEN];
static unsigned int evt_cnt, suspend_cnt;

/* xpu*/
enum {
	APMCU = 0,
	MD,
	SSPM,
	MMUP,
	SCP,
	XPU_NUM,
};

static u32 xpu_id[XPU_NUM] = {
	[APMCU] = 0,
	[MD] = 2,
	[SSPM] = 4,
	[MMUP] = 7,
	[SCP] = 9,
};

/* trace all subsys cgs */
enum {
	CLK_AFE_DL0_DAC_TML_CG = 0,
	CLK_AFE_DL0_DAC_HIRES_CG = 1,
	CLK_AFE_DL0_DAC_CG = 2,
	CLK_AFE_DL0_PREDIS_CG = 3,
	CLK_AFE_DL0_NLE_CG = 4,
	CLK_AFE_PCM1_CG = 5,
	CLK_AFE_PCM0_CG = 6,
	CLK_AFE_CM1_CG = 7,
	CLK_AFE_CM0_CG = 8,
	CLK_AFE_STF_CG = 9,
	CLK_AFE_HW_GAIN23_CG = 10,
	CLK_AFE_HW_GAIN01_CG = 11,
	CLK_AFE_FM_I2S_CG = 12,
	CLK_AFE_MTKAIFV4_CG = 13,
	CLK_AFE_AUDIO_HOPPING_CG = 14,
	CLK_AFE_AUDIO_F26M_CG = 15,
	CLK_AFE_APLL1_CG = 16,
	CLK_AFE_APLL2_CG = 17,
	CLK_AFE_H208M_CG = 18,
	CLK_AFE_APLL_TUNER2_CG = 19,
	CLK_AFE_APLL_TUNER1_CG = 20,
	CLK_AFE_UL1_ADC_HIRES_TML_CG = 21,
	CLK_AFE_UL1_ADC_HIRES_CG = 22,
	CLK_AFE_UL1_TML_CG = 23,
	CLK_AFE_UL1_ADC_CG = 24,
	CLK_AFE_UL0_ADC_HIRES_TML_CG = 25,
	CLK_AFE_UL0_ADC_HIRES_CG = 26,
	CLK_AFE_UL0_TML_CG = 27,
	CLK_AFE_UL0_ADC_CG = 28,
	CLK_AFE_ETDM_IN4_CG = 29,
	CLK_AFE_ETDM_IN2_CG = 30,
	CLK_AFE_ETDM_IN1_CG = 31,
	CLK_AFE_ETDM_OUT4_CG = 32,
	CLK_AFE_ETDM_OUT2_CG = 33,
	CLK_AFE_ETDM_OUT1_CG = 34,
	CLK_AFE_GENERAL2_ASRC_CG = 35,
	CLK_AFE_GENERAL1_ASRC_CG = 36,
	CLK_AFE_GENERAL0_ASRC_CG = 37,
	CLK_AFE_CONNSYS_I2S_ASRC_CG = 38,
	CLK_CAM_MAIN_LARB13_CG = 39,
	CLK_CAM_MAIN_LARB14_CG = 40,
	CLK_CAM_MAIN_LARB29_CG = 41,
	CLK_CAM_MAIN_CAM_CG = 42,
	CLK_CAM_MAIN_CAM_SUBA_CG = 43,
	CLK_CAM_MAIN_CAM_SUBB_CG = 44,
	CLK_CAM_MAIN_CAM_MRAW_CG = 45,
	CLK_CAM_MAIN_CAMTG_CG = 46,
	CLK_CAM_MAIN_SENINF_CG = 47,
	CLK_CAM_MAIN_CAMSV_TOP_CG = 48,
	CLK_CAM_MAIN_CAM2MM0_GALS_CG = 49,
	CLK_CAM_MAIN_CAM2MM1_GALS_CG = 50,
	CLK_CAM_MAIN_CCUSYS_CG = 51,
	CLK_CAM_MAIN_CAM_ASG_CG = 52,
	CLK_CAM_MAIN_CAMSV_A_CON_1_CG = 53,
	CLK_CAM_MAIN_CAMSV_B_CON_1_CG = 54,
	CLK_CAM_MAIN_CAMSV_C_CON_1_CG = 55,
	CLK_CAM_MAIN_CAMSV_D_CON_1_CG = 56,
	CLK_CAM_MAIN_CAMSV_E_CON_1_CG = 57,
	CLK_CAM_MAIN_CAMSV_CON_1_CG = 58,
	CLK_CAM_MR_LARBX_CG = 59,
	CLK_CAM_MR_GALS_CG = 60,
	CLK_CAM_MR_CAMTG_CG = 61,
	CLK_CAM_MR_MRAW0_CG = 62,
	CLK_CAM_MR_MRAW1_CG = 63,
	CLK_CAM_MR_MRAW2_CG = 64,
	CLK_CAM_MR_MRAW3_CG = 65,
	CLK_CAM_MR_PDA0_CG = 66,
	CLK_CAM_MR_PDA1_CG = 67,
	CLK_CAM_RA_LARBX_CG = 68,
	CLK_CAM_RA_CAM_CG = 69,
	CLK_CAM_RA_CAMTG_CG = 70,
	CLK_CAM_RA_RAW2MM_GALS_CG = 71,
	CLK_CAM_RA_YUV2RAW2MM_GALS_CG = 72,
	CLK_CAM_RB_LARBX_CG = 73,
	CLK_CAM_RB_CAM_CG = 74,
	CLK_CAM_RB_CAMTG_CG = 75,
	CLK_CAM_RB_RAW2MM_GALS_CG = 76,
	CLK_CAM_RB_YUV2RAW2MM_GALS_CG = 77,
	CLK_CAM_YA_LARBX_CG = 78,
	CLK_CAM_YA_CAM_CG = 79,
	CLK_CAM_YA_CAMTG_CG = 80,
	CLK_CAM_YB_LARBX_CG = 81,
	CLK_CAM_YB_CAM_CG = 82,
	CLK_CAM_YB_CAMTG_CG = 83,
	CLK_CAM_VCORE_C2MM0_DCM_DIS_CG = 84,
	CLK_CAM_VCORE_MM0_DCM_DIS_CG = 85,
	CLK_DIP_NR1_DIP1_LARB_CG = 86,
	CLK_DIP_NR1_DIP1_DIP_NR1_CG = 87,
	CLK_DIP_NR2_DIP1_LARB15_CG = 88,
	CLK_DIP_NR2_DIP1_DIP_NR_CG = 89,
	CLK_DIP_TOP_DIP1_DIP_TOP_CG = 90,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS0_CG = 91,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS1_CG = 92,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS2_CG = 93,
	CLK_DIP_TOP_DIP1_DIP_TOP_GALS3_CG = 94,
	CLK_DIP_TOP_DIP1_LARB10_CG = 95,
	CLK_DIP_TOP_DIP1_LARB15_CG = 96,
	CLK_DIP_TOP_DIP1_LARB38_CG = 97,
	CLK_DIP_TOP_DIP1_LARB39_CG = 98,
	CLK_IMG_LARB9_CG = 99,
	CLK_IMG_TRAW0_CG = 100,
	CLK_IMG_TRAW1_CG = 101,
	CLK_IMG_DIP0_CG = 102,
	CLK_IMG_WPE0_CG = 103,
	CLK_IMG_IPE_CG = 104,
	CLK_IMG_WPE1_CG = 105,
	CLK_IMG_WPE2_CG = 106,
	CLK_IMG_SUB_COMMON0_CG = 107,
	CLK_IMG_SUB_COMMON1_CG = 108,
	CLK_IMG_SUB_COMMON3_CG = 109,
	CLK_IMG_SUB_COMMON4_CG = 110,
	CLK_IMG_GALS_RX_DIP0_CG = 111,
	CLK_IMG_GALS_RX_DIP1_CG = 112,
	CLK_IMG_GALS_RX_TRAW0_CG = 113,
	CLK_IMG_GALS_RX_WPE0_CG = 114,
	CLK_IMG_GALS_RX_WPE1_CG = 115,
	CLK_IMG_GALS_RX_IPE0_CG = 116,
	CLK_IMG_GALS_TX_IPE0_CG = 117,
	CLK_IMG_GALS_CG = 118,
	CLK_IMG_FDVT_CG = 119,
	CLK_IMG_ME_CG = 120,
	CLK_IMG_MMG_CG = 121,
	CLK_IMG_LARB12_CG = 122,
	CLK_IMG_VCORE_GALS_DISP_CG = 123,
	CLK_IMG_VCORE_MAIN_CG = 124,
	CLK_IMG_VCORE_SUB0_CG = 125,
	CLK_IMG_VCORE_SUB1_CG = 126,
	CLK_TRAW_DIP1_LARB28_CG = 127,
	CLK_TRAW_DIP1_LARB40_CG = 128,
	CLK_TRAW_DIP1_TRAW_CG = 129,
	CLK_TRAW_DIP1_GALS_CG = 130,
	CLK_WPE1_DIP1_LARB11_CG = 131,
	CLK_WPE1_DIP1_WPE_CG = 132,
	CLK_WPE1_DIP1_GALS0_CG = 133,
	CLK_WPE2_DIP1_LARB11_CG = 134,
	CLK_WPE2_DIP1_WPE_CG = 135,
	CLK_WPE2_DIP1_GALS0_CG = 136,
	CLK_MM_DISP_OVL0_2L_CG = 137,
	CLK_MM_DISP_OVL1_2L_CG = 138,
	CLK_MM_DISP_OVL2_2L_CG = 139,
	CLK_MM_DISP_OVL3_2L_CG = 140,
	CLK_MM_DISP_UFBC_WDMA0_CG = 141,
	CLK_MM_DISP_RSZ1_CG = 142,
	CLK_MM_DISP_RSZ0_CG = 143,
	CLK_MM_DISP_TDSHP0_CG = 144,
	CLK_MM_DISP_C3D0_CG = 145,
	CLK_MM_DISP_COLOR0_CG = 146,
	CLK_MM_DISP_CCORR0_CG = 147,
	CLK_MM_DISP_CCORR1_CG = 148,
	CLK_MM_DISP_AAL0_CG = 149,
	CLK_MM_DISP_GAMMA0_CG = 150,
	CLK_MM_DISP_POSTMASK0_CG = 151,
	CLK_MM_DISP_DITHER0_CG = 152,
	CLK_MM_DISP_TDSHP1_CG = 153,
	CLK_MM_DISP_C3D1_CG = 154,
	CLK_MM_DISP_CCORR2_CG = 155,
	CLK_MM_DISP_CCORR3_CG = 156,
	CLK_MM_DISP_GAMMA1_CG = 157,
	CLK_MM_DISP_DITHER1_CG = 158,
	CLK_MM_DISP_SPLITTER0_CG = 159,
	CLK_MM_DISP_DSC_WRAP0_CG = 160,
	CLK_MM_DISP_DSI0_CG = 161,
	CLK_MM_DISP_DSI1_CG = 162,
	CLK_MM_DISP_WDMA1_CG = 163,
	CLK_MM_DISP_APB_BUS_CG = 164,
	CLK_MM_DISP_FAKE_ENG0_CG = 165,
	CLK_MM_DISP_FAKE_ENG1_CG = 166,
	CLK_MM_DISP_MUTEX0_CG = 167,
	CLK_MM_SMI_COMMON_CG = 168,
	CLK_MM_DSI0_CG = 169,
	CLK_MM_DSI1_CG = 170,
	CLK_MM_26M_CG = 171,
	CLK_MMINFRA_GCE_D_CG = 172,
	CLK_MMINFRA_GCE_M_CG = 173,
	CLK_MMINFRA_GCE_26M_CG = 174,
	CLK_IM_C_S_I3C5_W1S_CG = 175,
	CLK_IM_C_S_SEC_EN_W1S_CG = 176,
	CLK_IMP_ES_S_I3C10_W1S_CG = 177,
	CLK_IMP_ES_S_I3C11_W1S_CG = 178,
	CLK_IMP_ES_S_I3C12_W1S_CG = 179,
	CLK_IMP_ES_S_SEC_EN_W1S_CG = 180,
	CLK_IMP_E_S_I3C0_W1S_CG = 181,
	CLK_IMP_E_S_I3C1_W1S_CG = 182,
	CLK_IMP_E_S_I3C2_W1S_CG = 183,
	CLK_IMP_E_S_I3C4_W1S_CG = 184,
	CLK_IMP_E_S_I3C9_W1S_CG = 185,
	CLK_IMP_E_S_SEC_EN_W1S_CG = 186,
	CLK_IMP_W_S_I3C3_W1S_CG = 187,
	CLK_IMP_W_S_I3C6_W1S_CG = 188,
	CLK_IMP_W_S_I3C7_W1S_CG = 189,
	CLK_IMP_W_S_I3C8_W1S_CG = 190,
	CLK_IMP_W_S_SEC_EN_W1S_CG = 191,
	CLK_PERAOP_UART0_CG = 192,
	CLK_PERAOP_UART1_CG = 193,
	CLK_PERAOP_UART2_CG = 194,
	CLK_PERAOP_PWM_H_CG = 195,
	CLK_PERAOP_PWM_B_CG = 196,
	CLK_PERAOP_PWM_FB1_CG = 197,
	CLK_PERAOP_PWM_FB2_CG = 198,
	CLK_PERAOP_PWM_FB3_CG = 199,
	CLK_PERAOP_PWM_FB4_CG = 200,
	CLK_PERAOP_SPI0_B_CG = 201,
	CLK_PERAOP_SPI1_B_CG = 202,
	CLK_PERAOP_SPI2_B_CG = 203,
	CLK_PERAOP_SPI3_B_CG = 204,
	CLK_PERAOP_SPI4_B_CG = 205,
	CLK_PERAOP_SPI5_B_CG = 206,
	CLK_PERAOP_SPI6_B_CG = 207,
	CLK_PERAOP_SPI7_B_CG = 208,
	CLK_PERAOP_DMA_B_CG = 209,
	CLK_PERAOP_SSUSB0_FRMCNT_CG = 210,
	CLK_PERAOP_MSDC0_CG = 211,
	CLK_PERAOP_MSDC0_H_CG = 212,
	CLK_PERAOP_MSDC0_FAES_CG = 213,
	CLK_PERAOP_MSDC0_MST_F_CG = 214,
	CLK_PERAOP_MSDC0_SLV_H_CG = 215,
	CLK_PERAOP_MSDC1_CG = 216,
	CLK_PERAOP_MSDC1_H_CG = 217,
	CLK_PERAOP_MSDC1_MST_F_CG = 218,
	CLK_PERAOP_MSDC1_SLV_H_CG = 219,
	CLK_PERAOP_AUDIO0_CG = 220,
	CLK_PERAOP_AUDIO1_CG = 221,
	CLK_PERAOP_AUDIO2_CG = 222,
	CLK_UFSAO_UNIPRO_TX_SYM_CG = 223,
	CLK_UFSAO_UNIPRO_RX_SYM0_CG = 224,
	CLK_UFSAO_UNIPRO_RX_SYM1_CG = 225,
	CLK_UFSAO_UNIPRO_SYS_CG = 226,
	CLK_UFSAO_UNIPRO_SAP_CFG_CG = 227,
	CLK_UFSAO_PHY_TOP_AHB_S_BUS_CG = 228,
	CLK_UFSPDN_UFSHCI_CG = 229,
	CLK_UFSPDN_UFSHCI_AES_CG = 230,
	CLK_UFSPDN_UFSHCI_AHB_CG = 231,
	CLK_UFSPDN_UFSHCI_AXI_CG = 232,
	CLK_IFRAO_CQ_DMA_FPC_CG = 233,
	CLK_IFRAO_CCIF1_AP_CG = 234,
	CLK_IFRAO_CCIF1_MD_CG = 235,
	CLK_IFRAO_CCIF_AP_CG = 236,
	CLK_IFRAO_CCIF_MD_CG = 237,
	CLK_IFRAO_CLDMA_BCLK_CG = 238,
	CLK_IFRAO_CQ_DMA_CG = 239,
	CLK_IFRAO_CCIF5_MD_CG = 240,
	CLK_IFRAO_CCIF2_AP_CG = 241,
	CLK_IFRAO_CCIF2_MD_CG = 242,
	CLK_IFRAO_DPMAIF_MAIN_CG = 243,
	CLK_IFRAO_CCIF4_MD_CG = 244,
	CLK_IFRAO_RG_MMW_DPMAIF26M_CG = 245,
	CLK_MDP_MUTEX0_CG = 246,
	CLK_MDP_APB_BUS_CG = 247,
	CLK_MDP_SMI0_CG = 248,
	CLK_MDP_RDMA0_CG = 249,
	CLK_MDP_HDR0_CG = 250,
	CLK_MDP_AAL0_CG = 251,
	CLK_MDP_RSZ0_CG = 252,
	CLK_MDP_TDSHP0_CG = 253,
	CLK_MDP_WROT0_CG = 254,
	CLK_MDP_RDMA1_CG = 255,
	CLK_MDP_RSZ1_CG = 256,
	CLK_MDP_WROT1_CG = 257,
	CLK_SCP_SET_SPI0_CG = 258,
	CLK_SCP_SET_SPI1_CG = 259,
	CLK_SCP_SET_SPI2_CG = 260,
	CLK_SCP_SET_SPI3_CG = 261,
	CLK_VDE2_VDEC_CKEN_CG = 262,
	CLK_VDE2_VDEC_ACTIVE_CG = 263,
	CLK_VDE2_LARB_CKEN_CG = 264,
	CLK_VEN1_CKE0_LARB_CG = 265,
	CLK_VEN1_CKE1_VENC_CG = 266,
	CLK_VEN1_CKE2_JPGENC_CG = 267,
	CLK_VEN1_CKE5_GALS_CG = 268,
	TRACE_CLK_NUM = 269,
};

const char *trace_subsys_cgs[] = {
	[CLK_AFE_DL0_DAC_TML_CG] = "afe_dl0_dac_tml",
	[CLK_AFE_DL0_DAC_HIRES_CG] = "afe_dl0_dac_hires",
	[CLK_AFE_DL0_DAC_CG] = "afe_dl0_dac",
	[CLK_AFE_DL0_PREDIS_CG] = "afe_dl0_predis",
	[CLK_AFE_DL0_NLE_CG] = "afe_dl0_nle",
	[CLK_AFE_PCM1_CG] = "afe_pcm1",
	[CLK_AFE_PCM0_CG] = "afe_pcm0",
	[CLK_AFE_CM1_CG] = "afe_cm1",
	[CLK_AFE_CM0_CG] = "afe_cm0",
	[CLK_AFE_STF_CG] = "afe_stf",
	[CLK_AFE_HW_GAIN23_CG] = "afe_hw_gain23",
	[CLK_AFE_HW_GAIN01_CG] = "afe_hw_gain01",
	[CLK_AFE_FM_I2S_CG] = "afe_fm_i2s",
	[CLK_AFE_MTKAIFV4_CG] = "afe_mtkaifv4",
	[CLK_AFE_AUDIO_HOPPING_CG] = "afe_audio_hopping_ck",
	[CLK_AFE_AUDIO_F26M_CG] = "afe_audio_f26m_ck",
	[CLK_AFE_APLL1_CG] = "afe_apll1_ck",
	[CLK_AFE_APLL2_CG] = "afe_apll2_ck",
	[CLK_AFE_H208M_CG] = "afe_h208m_ck",
	[CLK_AFE_APLL_TUNER2_CG] = "afe_apll_tuner2",
	[CLK_AFE_APLL_TUNER1_CG] = "afe_apll_tuner1",
	[CLK_AFE_UL1_ADC_HIRES_TML_CG] = "afe_ul1_aht",
	[CLK_AFE_UL1_ADC_HIRES_CG] = "afe_ul1_adc_hires",
	[CLK_AFE_UL1_TML_CG] = "afe_ul1_tml",
	[CLK_AFE_UL1_ADC_CG] = "afe_ul1_adc",
	[CLK_AFE_UL0_ADC_HIRES_TML_CG] = "afe_ul0_aht",
	[CLK_AFE_UL0_ADC_HIRES_CG] = "afe_ul0_adc_hires",
	[CLK_AFE_UL0_TML_CG] = "afe_ul0_tml",
	[CLK_AFE_UL0_ADC_CG] = "afe_ul0_adc",
	[CLK_AFE_ETDM_IN4_CG] = "afe_etdm_in4",
	[CLK_AFE_ETDM_IN2_CG] = "afe_etdm_in2",
	[CLK_AFE_ETDM_IN1_CG] = "afe_etdm_in1",
	[CLK_AFE_ETDM_OUT4_CG] = "afe_etdm_out4",
	[CLK_AFE_ETDM_OUT2_CG] = "afe_etdm_out2",
	[CLK_AFE_ETDM_OUT1_CG] = "afe_etdm_out1",
	[CLK_AFE_GENERAL2_ASRC_CG] = "afe_general2_asrc",
	[CLK_AFE_GENERAL1_ASRC_CG] = "afe_general1_asrc",
	[CLK_AFE_GENERAL0_ASRC_CG] = "afe_general0_asrc",
	[CLK_AFE_CONNSYS_I2S_ASRC_CG] = "afe_connsys_i2s_asrc",
	[CLK_CAM_MAIN_LARB13_CG] = "cam_m_larb13",
	[CLK_CAM_MAIN_LARB14_CG] = "cam_m_larb14",
	[CLK_CAM_MAIN_LARB29_CG] = "cam_m_larb29",
	[CLK_CAM_MAIN_CAM_CG] = "cam_m_cam",
	[CLK_CAM_MAIN_CAM_SUBA_CG] = "cam_m_cam_suba",
	[CLK_CAM_MAIN_CAM_SUBB_CG] = "cam_m_cam_subb",
	[CLK_CAM_MAIN_CAM_MRAW_CG] = "cam_m_cam_mraw",
	[CLK_CAM_MAIN_CAMTG_CG] = "cam_m_camtg",
	[CLK_CAM_MAIN_SENINF_CG] = "cam_m_seninf",
	[CLK_CAM_MAIN_CAMSV_TOP_CG] = "cam_m_camsv",
	[CLK_CAM_MAIN_CAM2MM0_GALS_CG] = "cam_m_cam2mm0_GCON_0",
	[CLK_CAM_MAIN_CAM2MM1_GALS_CG] = "cam_m_cam2mm1_GCON_0",
	[CLK_CAM_MAIN_CCUSYS_CG] = "cam_m_ccusys",
	[CLK_CAM_MAIN_CAM_ASG_CG] = "cam_m_cam_asg",
	[CLK_CAM_MAIN_CAMSV_A_CON_1_CG] = "cam_m_camsv_a_con_1",
	[CLK_CAM_MAIN_CAMSV_B_CON_1_CG] = "cam_m_camsv_b_con_1",
	[CLK_CAM_MAIN_CAMSV_C_CON_1_CG] = "cam_m_camsv_c_con_1",
	[CLK_CAM_MAIN_CAMSV_D_CON_1_CG] = "cam_m_camsv_d_con_1",
	[CLK_CAM_MAIN_CAMSV_E_CON_1_CG] = "cam_m_camsv_e_con_1",
	[CLK_CAM_MAIN_CAMSV_CON_1_CG] = "cam_m_camsv_con_1",
	[CLK_CAM_MR_LARBX_CG] = "cam_mr_larbx",
	[CLK_CAM_MR_GALS_CG] = "cam_mr_gals",
	[CLK_CAM_MR_CAMTG_CG] = "cam_mr_camtg",
	[CLK_CAM_MR_MRAW0_CG] = "cam_mr_mraw0",
	[CLK_CAM_MR_MRAW1_CG] = "cam_mr_mraw1",
	[CLK_CAM_MR_MRAW2_CG] = "cam_mr_mraw2",
	[CLK_CAM_MR_MRAW3_CG] = "cam_mr_mraw3",
	[CLK_CAM_MR_PDA0_CG] = "cam_mr_pda0",
	[CLK_CAM_MR_PDA1_CG] = "cam_mr_pda1",
	[CLK_CAM_RA_LARBX_CG] = "cam_ra_larbx",
	[CLK_CAM_RA_CAM_CG] = "cam_ra_cam",
	[CLK_CAM_RA_CAMTG_CG] = "cam_ra_camtg",
	[CLK_CAM_RA_RAW2MM_GALS_CG] = "cam_ra_raw2mm_gals",
	[CLK_CAM_RA_YUV2RAW2MM_GALS_CG] = "cam_ra_yuv2raw2mm",
	[CLK_CAM_RB_LARBX_CG] = "cam_rb_larbx",
	[CLK_CAM_RB_CAM_CG] = "cam_rb_cam",
	[CLK_CAM_RB_CAMTG_CG] = "cam_rb_camtg",
	[CLK_CAM_RB_RAW2MM_GALS_CG] = "cam_rb_raw2mm_gals",
	[CLK_CAM_RB_YUV2RAW2MM_GALS_CG] = "cam_rb_yuv2raw2mm",
	[CLK_CAM_YA_LARBX_CG] = "cam_ya_larbx",
	[CLK_CAM_YA_CAM_CG] = "cam_ya_cam",
	[CLK_CAM_YA_CAMTG_CG] = "cam_ya_camtg",
	[CLK_CAM_YB_LARBX_CG] = "cam_yb_larbx",
	[CLK_CAM_YB_CAM_CG] = "cam_yb_cam",
	[CLK_CAM_YB_CAMTG_CG] = "cam_yb_camtg",
	[CLK_CAM_VCORE_C2MM0_DCM_DIS_CG] = "cam_vcore_c2mm0_dis",
	[CLK_CAM_VCORE_MM0_DCM_DIS_CG] = "cam_vcore_mm0_dis",
	[CLK_DIP_NR1_DIP1_LARB_CG] = "dip_nr1_dip1_larb",
	[CLK_DIP_NR1_DIP1_DIP_NR1_CG] = "dip_nr1_dip1_dip_nr1",
	[CLK_DIP_NR2_DIP1_LARB15_CG] = "dip_nr2_dip1_larb15",
	[CLK_DIP_NR2_DIP1_DIP_NR_CG] = "dip_nr2_dip1_dip_nr",
	[CLK_DIP_TOP_DIP1_DIP_TOP_CG] = "dip_dip1_dip_top",
	[CLK_DIP_TOP_DIP1_DIP_TOP_GALS0_CG] = "dip_dip1_dip_gals0",
	[CLK_DIP_TOP_DIP1_DIP_TOP_GALS1_CG] = "dip_dip1_dip_gals1",
	[CLK_DIP_TOP_DIP1_DIP_TOP_GALS2_CG] = "dip_dip1_dip_gals2",
	[CLK_DIP_TOP_DIP1_DIP_TOP_GALS3_CG] = "dip_dip1_dip_gals3",
	[CLK_DIP_TOP_DIP1_LARB10_CG] = "dip_dip1_larb10",
	[CLK_DIP_TOP_DIP1_LARB15_CG] = "dip_dip1_larb15",
	[CLK_DIP_TOP_DIP1_LARB38_CG] = "dip_dip1_larb38",
	[CLK_DIP_TOP_DIP1_LARB39_CG] = "dip_dip1_larb39",
	[CLK_IMG_LARB9_CG] = "img_larb9",
	[CLK_IMG_TRAW0_CG] = "img_traw0",
	[CLK_IMG_TRAW1_CG] = "img_traw1",
	[CLK_IMG_DIP0_CG] = "img_dip0",
	[CLK_IMG_WPE0_CG] = "img_wpe0",
	[CLK_IMG_IPE_CG] = "img_ipe",
	[CLK_IMG_WPE1_CG] = "img_wpe1",
	[CLK_IMG_WPE2_CG] = "img_wpe2",
	[CLK_IMG_SUB_COMMON0_CG] = "img_sub_common0",
	[CLK_IMG_SUB_COMMON1_CG] = "img_sub_common1",
	[CLK_IMG_SUB_COMMON3_CG] = "img_sub_common3",
	[CLK_IMG_SUB_COMMON4_CG] = "img_sub_common4",
	[CLK_IMG_GALS_RX_DIP0_CG] = "img_gals_rx_dip0",
	[CLK_IMG_GALS_RX_DIP1_CG] = "img_gals_rx_dip1",
	[CLK_IMG_GALS_RX_TRAW0_CG] = "img_gals_rx_traw0",
	[CLK_IMG_GALS_RX_WPE0_CG] = "img_gals_rx_wpe0",
	[CLK_IMG_GALS_RX_WPE1_CG] = "img_gals_rx_wpe1",
	[CLK_IMG_GALS_RX_IPE0_CG] = "img_gals_rx_ipe0",
	[CLK_IMG_GALS_TX_IPE0_CG] = "img_gals_tx_ipe0",
	[CLK_IMG_GALS_CG] = "img_gals",
	[CLK_IMG_FDVT_CG] = "img_fdvt",
	[CLK_IMG_ME_CG] = "img_me",
	[CLK_IMG_MMG_CG] = "img_mmg",
	[CLK_IMG_LARB12_CG] = "img_larb12",
	[CLK_IMG_VCORE_GALS_DISP_CG] = "img_vcore_gals_disp",
	[CLK_IMG_VCORE_MAIN_CG] = "img_vcore_main",
	[CLK_IMG_VCORE_SUB0_CG] = "img_vcore_sub0",
	[CLK_IMG_VCORE_SUB1_CG] = "img_vcore_sub1",
	[CLK_TRAW_DIP1_LARB28_CG] = "traw_dip1_larb28",
	[CLK_TRAW_DIP1_LARB40_CG] = "traw_dip1_larb40",
	[CLK_TRAW_DIP1_TRAW_CG] = "traw_dip1_traw",
	[CLK_TRAW_DIP1_GALS_CG] = "traw_dip1_gals",
	[CLK_WPE1_DIP1_LARB11_CG] = "wpe1_dip1_larb11",
	[CLK_WPE1_DIP1_WPE_CG] = "wpe1_dip1_wpe",
	[CLK_WPE1_DIP1_GALS0_CG] = "wpe1_dip1_gals0",
	[CLK_WPE2_DIP1_LARB11_CG] = "wpe2_dip1_larb11",
	[CLK_WPE2_DIP1_WPE_CG] = "wpe2_dip1_wpe",
	[CLK_WPE2_DIP1_GALS0_CG] = "wpe2_dip1_gals0",
	[CLK_MM_DISP_OVL0_2L_CG] = "mm_disp_ovl0_2l",
	[CLK_MM_DISP_OVL1_2L_CG] = "mm_disp_ovl1_2l",
	[CLK_MM_DISP_OVL2_2L_CG] = "mm_disp_ovl2_2l",
	[CLK_MM_DISP_OVL3_2L_CG] = "mm_disp_ovl3_2l",
	[CLK_MM_DISP_UFBC_WDMA0_CG] = "mm_disp_ufbc_wdma0",
	[CLK_MM_DISP_RSZ1_CG] = "mm_disp_rsz1",
	[CLK_MM_DISP_RSZ0_CG] = "mm_disp_rsz0",
	[CLK_MM_DISP_TDSHP0_CG] = "mm_disp_tdshp0",
	[CLK_MM_DISP_C3D0_CG] = "mm_disp_c3d0",
	[CLK_MM_DISP_COLOR0_CG] = "mm_disp_color0",
	[CLK_MM_DISP_CCORR0_CG] = "mm_disp_ccorr0",
	[CLK_MM_DISP_CCORR1_CG] = "mm_disp_ccorr1",
	[CLK_MM_DISP_AAL0_CG] = "mm_disp_aal0",
	[CLK_MM_DISP_GAMMA0_CG] = "mm_disp_gamma0",
	[CLK_MM_DISP_POSTMASK0_CG] = "mm_disp_postmask0",
	[CLK_MM_DISP_DITHER0_CG] = "mm_disp_dither0",
	[CLK_MM_DISP_TDSHP1_CG] = "mm_disp_tdshp1",
	[CLK_MM_DISP_C3D1_CG] = "mm_disp_c3d1",
	[CLK_MM_DISP_CCORR2_CG] = "mm_disp_ccorr2",
	[CLK_MM_DISP_CCORR3_CG] = "mm_disp_ccorr3",
	[CLK_MM_DISP_GAMMA1_CG] = "mm_disp_gamma1",
	[CLK_MM_DISP_DITHER1_CG] = "mm_disp_dither1",
	[CLK_MM_DISP_SPLITTER0_CG] = "mm_disp_splitter0",
	[CLK_MM_DISP_DSC_WRAP0_CG] = "mm_disp_dsc_wrap0",
	[CLK_MM_DISP_DSI0_CG] = "mm_CLK0",
	[CLK_MM_DISP_DSI1_CG] = "mm_CLK1",
	[CLK_MM_DISP_WDMA1_CG] = "mm_disp_wdma1",
	[CLK_MM_DISP_APB_BUS_CG] = "mm_disp_apb_bus",
	[CLK_MM_DISP_FAKE_ENG0_CG] = "mm_disp_fake_eng0",
	[CLK_MM_DISP_FAKE_ENG1_CG] = "mm_disp_fake_eng1",
	[CLK_MM_DISP_MUTEX0_CG] = "mm_disp_mutex0",
	[CLK_MM_SMI_COMMON_CG] = "mm_smi_common",
	[CLK_MM_DSI0_CG] = "mm_dsi0_ck",
	[CLK_MM_DSI1_CG] = "mm_dsi1_ck",
	[CLK_MM_26M_CG] = "mm_26m_ck",
	[CLK_MMINFRA_GCE_D_CG] = "mminfra_gce_d",
	[CLK_MMINFRA_GCE_M_CG] = "mminfra_gce_m",
	[CLK_MMINFRA_GCE_26M_CG] = "mminfra_gce_26m",
	[CLK_IM_C_S_I3C5_W1S_CG] = "im_c_s_i3c5_w1s",
	[CLK_IM_C_S_SEC_EN_W1S_CG] = "im_c_s_sec_w1s",
	[CLK_IMP_ES_S_I3C10_W1S_CG] = "imp_es_s_i3c10_w1s",
	[CLK_IMP_ES_S_I3C11_W1S_CG] = "imp_es_s_i3c11_w1s",
	[CLK_IMP_ES_S_I3C12_W1S_CG] = "imp_es_s_i3c12_w1s",
	[CLK_IMP_ES_S_SEC_EN_W1S_CG] = "imp_es_s_sec_w1s",
	[CLK_IMP_E_S_I3C0_W1S_CG] = "imp_e_s_i3c0_w1s",
	[CLK_IMP_E_S_I3C1_W1S_CG] = "imp_e_s_i3c1_w1s",
	[CLK_IMP_E_S_I3C2_W1S_CG] = "imp_e_s_i3c2_w1s",
	[CLK_IMP_E_S_I3C4_W1S_CG] = "imp_e_s_i3c4_w1s",
	[CLK_IMP_E_S_I3C9_W1S_CG] = "imp_e_s_i3c9_w1s",
	[CLK_IMP_E_S_SEC_EN_W1S_CG] = "imp_e_s_sec_w1s",
	[CLK_IMP_W_S_I3C3_W1S_CG] = "imp_w_s_i3c3_w1s",
	[CLK_IMP_W_S_I3C6_W1S_CG] = "imp_w_s_i3c6_w1s",
	[CLK_IMP_W_S_I3C7_W1S_CG] = "imp_w_s_i3c7_w1s",
	[CLK_IMP_W_S_I3C8_W1S_CG] = "imp_w_s_i3c8_w1s",
	[CLK_IMP_W_S_SEC_EN_W1S_CG] = "imp_w_s_sec_w1s",
	[CLK_PERAOP_UART0_CG] = "peraop_uart0",
	[CLK_PERAOP_UART1_CG] = "peraop_uart1",
	[CLK_PERAOP_UART2_CG] = "peraop_uart2",
	[CLK_PERAOP_PWM_H_CG] = "peraop_pwm_h",
	[CLK_PERAOP_PWM_B_CG] = "peraop_pwm_b",
	[CLK_PERAOP_PWM_FB1_CG] = "peraop_pwm_fb1",
	[CLK_PERAOP_PWM_FB2_CG] = "peraop_pwm_fb2",
	[CLK_PERAOP_PWM_FB3_CG] = "peraop_pwm_fb3",
	[CLK_PERAOP_PWM_FB4_CG] = "peraop_pwm_fb4",
	[CLK_PERAOP_SPI0_B_CG] = "peraop_spi0_b",
	[CLK_PERAOP_SPI1_B_CG] = "peraop_spi1_b",
	[CLK_PERAOP_SPI2_B_CG] = "peraop_spi2_b",
	[CLK_PERAOP_SPI3_B_CG] = "peraop_spi3_b",
	[CLK_PERAOP_SPI4_B_CG] = "peraop_spi4_b",
	[CLK_PERAOP_SPI5_B_CG] = "peraop_spi5_b",
	[CLK_PERAOP_SPI6_B_CG] = "peraop_spi6_b",
	[CLK_PERAOP_SPI7_B_CG] = "peraop_spi7_b",
	[CLK_PERAOP_DMA_B_CG] = "peraop_dma_b",
	[CLK_PERAOP_SSUSB0_FRMCNT_CG] = "peraop_ssusb0_frmcnt",
	[CLK_PERAOP_MSDC0_CG] = "peraop_msdc0",
	[CLK_PERAOP_MSDC0_H_CG] = "peraop_msdc0_h",
	[CLK_PERAOP_MSDC0_FAES_CG] = "peraop_msdc0_faes",
	[CLK_PERAOP_MSDC0_MST_F_CG] = "peraop_msdc0_mst_f",
	[CLK_PERAOP_MSDC0_SLV_H_CG] = "peraop_msdc0_slv_h",
	[CLK_PERAOP_MSDC1_CG] = "peraop_msdc1",
	[CLK_PERAOP_MSDC1_H_CG] = "peraop_msdc1_h",
	[CLK_PERAOP_MSDC1_MST_F_CG] = "peraop_msdc1_mst_f",
	[CLK_PERAOP_MSDC1_SLV_H_CG] = "peraop_msdc1_slv_h",
	[CLK_PERAOP_AUDIO0_CG] = "peraop_audio0",
	[CLK_PERAOP_AUDIO1_CG] = "peraop_audio1",
	[CLK_PERAOP_AUDIO2_CG] = "peraop_audio2",
	[CLK_UFSAO_UNIPRO_TX_SYM_CG] = "ufsao_unipro_tx_sym",
	[CLK_UFSAO_UNIPRO_RX_SYM0_CG] = "ufsao_unipro_rx_sym0",
	[CLK_UFSAO_UNIPRO_RX_SYM1_CG] = "ufsao_unipro_rx_sym1",
	[CLK_UFSAO_UNIPRO_SYS_CG] = "ufsao_unipro_sys",
	[CLK_UFSAO_UNIPRO_SAP_CFG_CG] = "ufsao_unipro_sap_cfg",
	[CLK_UFSAO_PHY_TOP_AHB_S_BUS_CG] = "ufsao_phy_ahb_s_bus",
	[CLK_UFSPDN_UFSHCI_CG] = "ufspdn_UFSHCI",
	[CLK_UFSPDN_UFSHCI_AES_CG] = "ufspdn_ufshci_aes",
	[CLK_UFSPDN_UFSHCI_AHB_CG] = "ufspdn_UFSHCI_ahb",
	[CLK_UFSPDN_UFSHCI_AXI_CG] = "ufspdn_UFSHCI_axi",
	[CLK_IFRAO_CQ_DMA_FPC_CG] = "ifrao_dma",
	[CLK_IFRAO_CCIF1_AP_CG] = "ifrao_ccif1_ap",
	[CLK_IFRAO_CCIF1_MD_CG] = "ifrao_ccif1_md",
	[CLK_IFRAO_CCIF_AP_CG] = "ifrao_ccif_ap",
	[CLK_IFRAO_CCIF_MD_CG] = "ifrao_ccif_md",
	[CLK_IFRAO_CLDMA_BCLK_CG] = "ifrao_cldmabclk",
	[CLK_IFRAO_CQ_DMA_CG] = "ifrao_cq_dma",
	[CLK_IFRAO_CCIF5_MD_CG] = "ifrao_ccif5_md",
	[CLK_IFRAO_CCIF2_AP_CG] = "ifrao_ccif2_ap",
	[CLK_IFRAO_CCIF2_MD_CG] = "ifrao_ccif2_md",
	[CLK_IFRAO_DPMAIF_MAIN_CG] = "ifrao_dpmaif_main",
	[CLK_IFRAO_CCIF4_MD_CG] = "ifrao_ccif4_md",
	[CLK_IFRAO_RG_MMW_DPMAIF26M_CG] = "ifrao_dpmaif_26m",
	[CLK_MDP_MUTEX0_CG] = "mdp_mutex0",
	[CLK_MDP_APB_BUS_CG] = "mdp_apb_bus",
	[CLK_MDP_SMI0_CG] = "mdp_smi0",
	[CLK_MDP_RDMA0_CG] = "mdp_rdma0",
	[CLK_MDP_HDR0_CG] = "mdp_hdr0",
	[CLK_MDP_AAL0_CG] = "mdp_aal0",
	[CLK_MDP_RSZ0_CG] = "mdp_rsz0",
	[CLK_MDP_TDSHP0_CG] = "mdp_tdshp0",
	[CLK_MDP_WROT0_CG] = "mdp_wrot0",
	[CLK_MDP_RDMA1_CG] = "mdp_rdma1",
	[CLK_MDP_RSZ1_CG] = "mdp_rsz1",
	[CLK_MDP_WROT1_CG] = "mdp_wrot1",
	[CLK_SCP_SET_SPI0_CG] = "scp_set_spi0",
	[CLK_SCP_SET_SPI1_CG] = "scp_set_spi1",
	[CLK_SCP_SET_SPI2_CG] = "scp_set_spi2",
	[CLK_SCP_SET_SPI3_CG] = "scp_set_spi3",
	[CLK_VDE2_VDEC_CKEN_CG] = "vde2_vdec_cken",
	[CLK_VDE2_VDEC_ACTIVE_CG] = "vde2_vdec_active",
	[CLK_VDE2_LARB_CKEN_CG] = "vde2_larb_cken",
	[CLK_VEN1_CKE0_LARB_CG] = "ven1_larb",
	[CLK_VEN1_CKE1_VENC_CG] = "ven1_venc",
	[CLK_VEN1_CKE2_JPGENC_CG] = "ven1_jpgenc",
	[CLK_VEN1_CKE5_GALS_CG] = "ven1_gals",
	[TRACE_CLK_NUM] = "NULL",
};

struct clkchk_fm {
	const char *fm_name;
	unsigned int fm_id;
	unsigned int fm_type;
};

/* check which fmeter clk you want to get freq */
enum {
	CHK_FM_NUM = 0,
};

/* fill in the fmeter clk you want to get freq */
struct  clkchk_fm chk_fm_list[] = {
	{},
};

static void trace_clk_event(const char *name, unsigned int clk_sta)
{
	unsigned long flags = 0;
	int i;

	spin_lock_irqsave(&clk_trace_lock, flags);

	if (!name)
		goto OUT;

	for (i = 0; i < TRACE_CLK_NUM; i++) {
		if (!strcmp(trace_subsys_cgs[i], name))
			break;
	}

	if (i == TRACE_CLK_NUM)
		goto OUT;

	clk_event[evt_cnt] = (i << CLK_ID_SHIFT) | (clk_sta << CLK_STA_SHIFT);
	evt_cnt++;
	if (evt_cnt >= EVT_LEN)
		evt_cnt = 0;

OUT:
	spin_unlock_irqrestore(&clk_trace_lock, flags);
}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
void dump_clk_event(void)
{
	unsigned long flags = 0;
	int i;

	spin_lock_irqsave(&clk_trace_lock, flags);

	pr_notice("first idx: %d\n", evt_cnt);
	for (i = 0; i < EVT_LEN; i += 5)
		pr_notice("clk_evt[%d] = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
				i,
				clk_event[i],
				clk_event[i + 1],
				clk_event[i + 2],
				clk_event[i + 3],
				clk_event[i + 4]);

	spin_unlock_irqrestore(&clk_trace_lock, flags);
}
EXPORT_SYMBOL_GPL(dump_clk_event);
#endif

/*
 * clkchk dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg, _pn) { .phys = _phys, .id = _id_name,	\
		.name = #_id_name, .pg = _pg, .pn = _pn}

static struct regbase rb[] = {
	[top] = REGBASE_V(0x10000000, top, PD_NULL, CLK_NULL),
	[ifrao] = REGBASE_V(0x10001000, ifrao, PD_NULL, CLK_NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, PD_NULL, CLK_NULL),
	[ifr] = REGBASE_V(0x1020e000, ifr, PD_NULL, CLK_NULL),
	[emi_reg] = REGBASE_V(0x10219000, emi_reg, PD_NULL, CLK_NULL),
	[nemicfg_ao_mem_reg_bus] = REGBASE_V(0x10270000, nemicfg_ao_mem_reg_bus, PD_NULL, CLK_NULL),
	[ssr_top] = REGBASE_V(0x10400000, ssr_top, MT6878_CHK_PD_SSRSYS, CLK_NULL),
	[perao] = REGBASE_V(0x11036000, perao, PD_NULL, CLK_NULL),
	[afe] = REGBASE_V(0x11050000, afe, MT6878_CHK_PD_AUDIO, CLK_NULL),
	[im_c_s] = REGBASE_V(0x11281000, im_c_s, PD_NULL, CLK_NULL),
	[ufsao] = REGBASE_V(0x112b8000, ufsao, PD_NULL, CLK_NULL),
	[ufspdn] = REGBASE_V(0x112bb000, ufspdn, PD_NULL, CLK_NULL),
	[imp_e_s] = REGBASE_V(0x11C25000, imp_e_s, PD_NULL, CLK_NULL),
	[imp_es_s] = REGBASE_V(0x11D73000, imp_es_s, PD_NULL, CLK_NULL),
	[imp_w_s] = REGBASE_V(0x11E04000, imp_w_s, PD_NULL, CLK_NULL),
	[mfg_ao] = REGBASE_V(0x13fa0000, mfg_ao, PD_NULL, CLK_NULL),
	[mfgsc_ao] = REGBASE_V(0x13fa0c00, mfgsc_ao, PD_NULL, CLK_NULL),
	[mm] = REGBASE_V(0x14000000, mm, MT6878_CHK_PD_DIS0, CLK_NULL),
	[img] = REGBASE_V(0x15000000, img, MT6878_CHK_PD_ISP_MAIN, CLK_NULL),
	[dip_top_dip1] = REGBASE_V(0x15110000, dip_top_dip1, MT6878_CHK_PD_ISP_DIP1, CLK_NULL),
	[dip_nr1_dip1] = REGBASE_V(0x15130000, dip_nr1_dip1, MT6878_CHK_PD_ISP_DIP1, CLK_NULL),
	[dip_nr2_dip1] = REGBASE_V(0x15170000, dip_nr2_dip1, MT6878_CHK_PD_ISP_DIP1, CLK_NULL),
	[wpe1_dip1] = REGBASE_V(0x15220000, wpe1_dip1, MT6878_CHK_PD_ISP_DIP1, CLK_NULL),
	[wpe2_dip1] = REGBASE_V(0x15520000, wpe2_dip1, MT6878_CHK_PD_ISP_DIP1, CLK_NULL),
	[traw_dip1] = REGBASE_V(0x15710000, traw_dip1, MT6878_CHK_PD_ISP_DIP1, CLK_NULL),
	[img_v] = REGBASE_V(0x15780000, img_v, MT6878_CHK_PD_ISP_MAIN, CLK_NULL),
	[vde2] = REGBASE_V(0x1602f000, vde2, MT6878_CHK_PD_VDE0, CLK_NULL),
	[ven1] = REGBASE_V(0x17000000, ven1, MT6878_CHK_PD_VEN0, CLK_NULL),
	[spm] = REGBASE_V(0x1C001000, spm, PD_NULL, CLK_NULL),
	[vlpcfg_reg] = REGBASE_V(0x1C00C000, vlpcfg_reg, PD_NULL, CLK_NULL),
	[vlp] = REGBASE_V(0x1C012000, vlp, PD_NULL, CLK_NULL),
	[scp] = REGBASE_V(0x1CB21000, scp, PD_NULL, CLK_NULL),
	[hfrp] = REGBASE_V(0x1EC35000, hfrp, PD_NULL, CLK_NULL),
	[cam_m] = REGBASE_V(0x1a000000, cam_m, MT6878_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_ra] = REGBASE_V(0x1a04f000, cam_ra, MT6878_CHK_PD_CAM_SUBA, CLK_NULL),
	[cam_ya] = REGBASE_V(0x1a06f000, cam_ya, MT6878_CHK_PD_CAM_SUBA, CLK_NULL),
	[cam_rb] = REGBASE_V(0x1a08f000, cam_rb, MT6878_CHK_PD_CAM_SUBB, CLK_NULL),
	[cam_yb] = REGBASE_V(0x1a0af000, cam_yb, MT6878_CHK_PD_CAM_SUBB, CLK_NULL),
	[cam_mr] = REGBASE_V(0x1a170000, cam_mr, MT6878_CHK_PD_CAM_MAIN, CLK_NULL),
	[ccu] = REGBASE_V(0x1b200000, ccu, MT6878_CHK_PD_CAM_CCU_AO, CLK_NULL),
	[cam_vcore] = REGBASE_V(0x1b204000, cam_vcore, MT6878_CHK_PD_CAM_VCORE, CLK_NULL),
	[dvfsrc_top] = REGBASE_V(0x1c00f000, dvfsrc_top, PD_NULL, CLK_NULL),
	[mminfra_config] = REGBASE_V(0x1e800000, mminfra_config, MT6878_CHK_PD_MM_INFRA, CLK_NULL),
	[mdp] = REGBASE_V(0x1f000000, mdp, MT6878_CHK_PD_DIS0, CLK_NULL),
	[hwv] = REGBASE_V(0x10320000, hwv, PD_NULL, CLK_NULL),
	[hwv_ext] = REGBASE_V(0x10321000, hwv_ext, PD_NULL, CLK_NULL),
	[hwv_wrt] = REGBASE_V(0x10321000, hwv_wrt, PD_NULL, CLK_NULL),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .id = _base, .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	/* TOPCKGEN register */
	REGNAME(top, 0x0010, CLK_CFG_0),
	REGNAME(top, 0x0020, CLK_CFG_1),
	REGNAME(top, 0x0030, CLK_CFG_2),
	REGNAME(top, 0x0040, CLK_CFG_3),
	REGNAME(top, 0x0050, CLK_CFG_4),
	REGNAME(top, 0x0060, CLK_CFG_5),
	REGNAME(top, 0x0070, CLK_CFG_6),
	REGNAME(top, 0x0080, CLK_CFG_7),
	REGNAME(top, 0x0090, CLK_CFG_8),
	REGNAME(top, 0x00A0, CLK_CFG_9),
	REGNAME(top, 0x00B0, CLK_CFG_10),
	REGNAME(top, 0x00C0, CLK_CFG_11),
	REGNAME(top, 0x00D0, CLK_CFG_12),
	REGNAME(top, 0x00E0, CLK_CFG_13),
	REGNAME(top, 0x00F0, CLK_CFG_14),
	REGNAME(top, 0x0100, CLK_CFG_15),
	REGNAME(top, 0x0110, CLK_CFG_16),
	REGNAME(top, 0x0190, CLK_CFG_18),
	REGNAME(top, 0x0120, CLK_CFG_20),
	REGNAME(top, 0x0320, CLK_AUDDIV_0),
	REGNAME(top, 0x033C, CLK_AUDDIV_5),
	REGNAME(top, 0x320, CLK_AUDDIV_0),
	/* INFRA_AO_REG register */
	REGNAME(ifrao, 0x90, MODULE_CG_0),
	REGNAME(ifrao, 0x94, MODULE_CG_1),
	REGNAME(ifrao, 0xAC, MODULE_CG_2),
	REGNAME(ifrao, 0xC8, MODULE_CG_3),
	REGNAME(ifrao, 0xE8, MODULE_CG_4),
	REGNAME(ifrao, 0x0C50, INFRASYS_PROTECT_EN_STA_1),
	REGNAME(ifrao, 0x0C5C, INFRASYS_PROTECT_RDY_STA_1),
	REGNAME(ifrao, 0x0C60, EMISYS_PROTECT_EN_STA_0),
	REGNAME(ifrao, 0x0C6C, EMISYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0C90, MCU_CONNSYS_PROTECT_EN_STA_0),
	REGNAME(ifrao, 0x0C9C, MCU_CONNSYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0C40, INFRASYS_PROTECT_EN_STA_0),
	REGNAME(ifrao, 0x0C4C, INFRASYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0CA0, MD_MFGSYS_PROTECT_EN_STA_0),
	REGNAME(ifrao, 0x0CAC, MD_MFGSYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0C20, MMSYS_PROTECT_EN_STA_1),
	REGNAME(ifrao, 0x0C2C, MMSYS_PROTECT_RDY_STA_1),
	REGNAME(ifrao, 0x0C80, PERISYS_PROTECT_EN_STA_0),
	REGNAME(ifrao, 0x0C8C, PERISYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0C10, MMSYS_PROTECT_EN_STA_0),
	REGNAME(ifrao, 0x0C1C, MMSYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0CC0, DRAMC_CCUSYS_PROTECT_EN_STA_0),
	REGNAME(ifrao, 0x0CCC, DRAMC_CCUSYS_PROTECT_RDY_STA_0),
	/* APMIXEDSYS register */
	REGNAME(apmixed, 0x204, ARMPLL_LL_CON0),
	REGNAME(apmixed, 0x208, ARMPLL_LL_CON1),
	REGNAME(apmixed, 0x20c, ARMPLL_LL_CON2),
	REGNAME(apmixed, 0x210, ARMPLL_LL_CON3),
	REGNAME(apmixed, 0x214, ARMPLL_BL_CON0),
	REGNAME(apmixed, 0x218, ARMPLL_BL_CON1),
	REGNAME(apmixed, 0x21c, ARMPLL_BL_CON2),
	REGNAME(apmixed, 0x220, ARMPLL_BL_CON3),
	REGNAME(apmixed, 0x224, CCIPLL_CON0),
	REGNAME(apmixed, 0x228, CCIPLL_CON1),
	REGNAME(apmixed, 0x22c, CCIPLL_CON2),
	REGNAME(apmixed, 0x230, CCIPLL_CON3),
	REGNAME(apmixed, 0x304, MAINPLL_CON0),
	REGNAME(apmixed, 0x308, MAINPLL_CON1),
	REGNAME(apmixed, 0x30c, MAINPLL_CON2),
	REGNAME(apmixed, 0x310, MAINPLL_CON3),
	REGNAME(apmixed, 0x314, UNIVPLL_CON0),
	REGNAME(apmixed, 0x318, UNIVPLL_CON1),
	REGNAME(apmixed, 0x31c, UNIVPLL_CON2),
	REGNAME(apmixed, 0x320, UNIVPLL_CON3),
	REGNAME(apmixed, 0x35c, MSDCPLL_CON0),
	REGNAME(apmixed, 0x360, MSDCPLL_CON1),
	REGNAME(apmixed, 0x364, MSDCPLL_CON2),
	REGNAME(apmixed, 0x368, MSDCPLL_CON3),
	REGNAME(apmixed, 0x324, MMPLL_CON0),
	REGNAME(apmixed, 0x328, MMPLL_CON1),
	REGNAME(apmixed, 0x32c, MMPLL_CON2),
	REGNAME(apmixed, 0x330, MMPLL_CON3),
	REGNAME(apmixed, 0x36c, UFSPLL_CON0),
	REGNAME(apmixed, 0x370, UFSPLL_CON1),
	REGNAME(apmixed, 0x374, UFSPLL_CON2),
	REGNAME(apmixed, 0x378, UFSPLL_CON3),
	REGNAME(apmixed, 0x334, APLL1_CON0),
	REGNAME(apmixed, 0x338, APLL1_CON1),
	REGNAME(apmixed, 0x33c, APLL1_CON2),
	REGNAME(apmixed, 0x340, APLL1_CON3),
	REGNAME(apmixed, 0x344, APLL1_CON4),
	REGNAME(apmixed, 0x0040, APLL1_TUNER_CON0),
	REGNAME(apmixed, 0x000C, AP_PLL_CON3),
	REGNAME(apmixed, 0x348, APLL2_CON0),
	REGNAME(apmixed, 0x34c, APLL2_CON1),
	REGNAME(apmixed, 0x350, APLL2_CON2),
	REGNAME(apmixed, 0x354, APLL2_CON3),
	REGNAME(apmixed, 0x358, APLL2_CON4),
	REGNAME(apmixed, 0x0044, APLL2_TUNER_CON0),
	REGNAME(apmixed, 0x000C, AP_PLL_CON3),
	/* INFRA_INFRACFG_REG register */
	REGNAME(ifr, 0xB00, BUS_MON_CKEN),
	/* EMI_REG register */
	REGNAME(emi_reg, 0x858, EMI_THRO_CTRL1),
	/* NEMICFG_AO_MEM_REG_BUS register */
	REGNAME(nemicfg_ao_mem_reg_bus, 0x40, GLITCH_PROTECT_EN),
	REGNAME(nemicfg_ao_mem_reg_bus, 0x8c, GLITCH_PROTECT_RDY),
	/* SSR_TOP register */
	REGNAME(ssr_top, 0x0, SSR_TOP_CLK_CFG),
	REGNAME(ssr_top, 0x4, SSR_TOP_CLK_CFG_1),
	REGNAME(ssr_top, 0x8, SSR_TOP_CLK_CFG_2),
	REGNAME(ssr_top, 0x44, SSR_AO_CTRL0),
	REGNAME(ssr_top, 0x50, SSR_AO_STATUS0),
	/* PERICFG_AO register */
	REGNAME(perao, 0x10, PERI_CG_0),
	REGNAME(perao, 0x14, PERI_CG_1),
	REGNAME(perao, 0x18, PERI_CG_2),
	/* AFE register */
	REGNAME(afe, 0x0, AUDIO_TOP_0),
	REGNAME(afe, 0x4, AUDIO_TOP_1),
	REGNAME(afe, 0x8, AUDIO_TOP_2),
	REGNAME(afe, 0xC, AUDIO_TOP_3),
	REGNAME(afe, 0x10, AUDIO_TOP_4),
	/* IMP_IIC_WRAP_CEN_S register */
	REGNAME(im_c_s, 0xE00, AP_CLOCK_CG),
	/* UFSCFG_AO register */
	REGNAME(ufsao, 0x4, UFS_AO_CG_0),
	/* UFSCFG_PDN register */
	REGNAME(ufspdn, 0x4, UFS_PDN_CG_0),
	/* IMP_IIC_WRAP_E_S register */
	REGNAME(imp_e_s, 0xE00, AP_CLOCK_CG),
	/* IMP_IIC_WRAP_ES_S register */
	REGNAME(imp_es_s, 0xE00, AP_CLOCK_CG),
	/* IMP_IIC_WRAP_W_S register */
	REGNAME(imp_w_s, 0xE00, AP_CLOCK_CG),
	/* MFGPLL_PLL_CTRL register */
	REGNAME(mfg_ao, 0x8, MFGPLL_CON0),
	REGNAME(mfg_ao, 0xc, MFGPLL_CON1),
	REGNAME(mfg_ao, 0x10, MFGPLL_CON2),
	REGNAME(mfg_ao, 0x14, MFGPLL_CON3),
	/* MFGSCPLL_PLL_CTRL register */
	REGNAME(mfgsc_ao, 0x8, MFGSCPLL_CON0),
	REGNAME(mfgsc_ao, 0xc, MFGSCPLL_CON1),
	REGNAME(mfgsc_ao, 0x10, MFGSCPLL_CON2),
	REGNAME(mfgsc_ao, 0x14, MFGSCPLL_CON3),
	/* DISPSYS_CONFIG register */
	REGNAME(mm, 0x100, MMSYS_CG_0),
	REGNAME(mm, 0x110, MMSYS_CG_1),
	/* IMGSYS_MAIN register */
	REGNAME(img, 0x50, IMG_IPE_CG),
	REGNAME(img, 0x0, IMG_MAIN_CG),
	/* DIP_TOP_DIP1 register */
	REGNAME(dip_top_dip1, 0x0, MACRO_CG),
	/* DIP_NR1_DIP1 register */
	REGNAME(dip_nr1_dip1, 0x0, MACRO_CG),
	/* DIP_NR2_DIP1 register */
	REGNAME(dip_nr2_dip1, 0x0, MACRO_CG),
	/* WPE1_DIP1 register */
	REGNAME(wpe1_dip1, 0x0, MACRO_CG),
	/* WPE2_DIP1 register */
	REGNAME(wpe2_dip1, 0x0, MACRO_CG),
	/* TRAW_DIP1 register */
	REGNAME(traw_dip1, 0x0, MACRO_CG),
	/* IMG_VCORE_D1A register */
	REGNAME(img_v, 0x0, IMG_VCORE_CG_0),
	/* VDEC_GCON_BASE register */
	REGNAME(vde2, 0x8, LARB_CKEN_CON),
	REGNAME(vde2, 0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven1, 0x0, VENCSYS_CG),
	/* SPM register */
	REGNAME(spm, 0xE00, MD1_PWR_CON),
	REGNAME(spm, 0xF40, PWR_STATUS),
	REGNAME(spm, 0xF44, PWR_STATUS_2ND),
	REGNAME(spm, 0xF24, MD_BUCK_ISO_CON),
	REGNAME(spm, 0xE04, CONN_PWR_CON),
	REGNAME(spm, 0xE10, UFS0_PWR_CON),
	REGNAME(spm, 0xE14, UFS0_PHY_PWR_CON),
	REGNAME(spm, 0xE18, AUDIO_PWR_CON),
	REGNAME(spm, 0xE28, ISP_MAIN_PWR_CON),
	REGNAME(spm, 0xE2C, ISP_DIP1_PWR_CON),
	REGNAME(spm, 0xE34, ISP_VCORE_PWR_CON),
	REGNAME(spm, 0xE38, VDE0_PWR_CON),
	REGNAME(spm, 0xE40, VEN0_PWR_CON),
	REGNAME(spm, 0xE48, CAM_MAIN_PWR_CON),
	REGNAME(spm, 0xE50, CAM_SUBA_PWR_CON),
	REGNAME(spm, 0xE54, CAM_SUBB_PWR_CON),
	REGNAME(spm, 0xE5C, CAM_VCORE_PWR_CON),
	REGNAME(spm, 0xE60, CAM_CCU_PWR_CON),
	REGNAME(spm, 0xE64, CAM_CCU_AO_PWR_CON),
	REGNAME(spm, 0xE70, DIS0_PWR_CON),
	REGNAME(spm, 0xE78, MM_INFRA_PWR_CON),
	REGNAME(spm, 0xF28, SOC_BUCK_ISO_CON),
	REGNAME(spm, 0xE7C, MM_PROC_PWR_CON),
	REGNAME(spm, 0xE9C, CSI_RX_PWR_CON),
	REGNAME(spm, 0xEA0, SSRSYS_PWR_CON),
	REGNAME(spm, 0xEA8, SSUSB_PWR_CON),
	REGNAME(spm, 0xEB4, MFG0_PWR_CON),
	REGNAME(spm, 0xF50, XPU_PWR_STATUS),
	REGNAME(spm, 0xF54, XPU_PWR_STATUS_2ND),
	/* VLPCFG_REG register */
	REGNAME(vlpcfg_reg, 0x0210, VLP_TOPAXI_PROTECTEN),
	REGNAME(vlpcfg_reg, 0x0220, VLP_TOPAXI_PROTECTEN_STA1),
	/* VLP_CKSYS register */
	REGNAME(vlp, 0x0008, VLP_CLK_CFG_0),
	REGNAME(vlp, 0x0014, VLP_CLK_CFG_1),
	REGNAME(vlp, 0x0020, VLP_CLK_CFG_2),
	REGNAME(vlp, 0x002C, VLP_CLK_CFG_3),
	REGNAME(vlp, 0x0038, VLP_CLK_CFG_4),
	/* SCP register */
	REGNAME(scp, 0x154, AP_SPI_CG),
	/* HFRP register */
	REGNAME(hfrp, 0x0, Vdisp_DVFSRC_BASIC_CONTROL),
	REGNAME(hfrp, 0x0, Vmm_DVFSRC_BASIC_CONTROL),
	/* CAMSYS_MAIN register */
	REGNAME(cam_m, 0x0, CAM_MAIN_CG_0),
	REGNAME(cam_m, 0x4C, CAM_MAIN_CG_1),
	/* CAMSYS_RAWA register */
	REGNAME(cam_ra, 0x0, CAMSYS_CG),
	/* CAMSYS_YUVA register */
	REGNAME(cam_ya, 0x0, CAMSYS_CG),
	/* CAMSYS_RAWB register */
	REGNAME(cam_rb, 0x0, CAMSYS_CG),
	/* CAMSYS_YUVB register */
	REGNAME(cam_yb, 0x0, CAMSYS_CG),
	/* CAMSYS_MRAW register */
	REGNAME(cam_mr, 0x0, CAMSYS_CG),
	/* CCU_MAIN register */
	REGNAME(ccu, 0x0, CCUSYS_CG),
	/* CAM_VCORE register */
	REGNAME(cam_vcore, 0x2C, CAM_VCORE_SUBCOMM_DCM_DIS),
	/* DVFSRC_TOP register */
	REGNAME(dvfsrc_top, 0x0, DVFSRC_BASIC_CONTROL),
	/* MMINFRA_CONFIG register */
	REGNAME(mminfra_config, 0x100, MMINFRA_CG_0),
	REGNAME(mminfra_config, 0x110, MMINFRA_CG_1),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp, 0x100, MDPSYS_CG_0),
	/* HWV register */
	REGNAME(hwv, 0x190, HW_CCF_APMCU_PLL_SET),
	REGNAME(hwv, 0x198, HW_CCF_APMCU_MTCMOS_SET),
	REGNAME(hwv, 0x390, HW_CCF_TEE_PLL_SET),
	REGNAME(hwv, 0x398, HW_CCF_TEE_MTCMOS_SET),
	REGNAME(hwv, 0x590, HW_CCF_MMUP_PLL_SET),
	REGNAME(hwv, 0x598, HW_CCF_MMUP_MTCMOS_SET),
	REGNAME(hwv, 0x790, HW_CCF_SCP_PLL_SET),
	REGNAME(hwv, 0x798, HW_CCF_SCP_MTCMOS_SET),
	REGNAME(hwv, 0x990, HW_CCF_SSPM_PLL_SET),
	REGNAME(hwv, 0xb98, HW_CCF_APU_MTCMOS_SET),
	REGNAME(hwv, 0xb90, HW_CCF_APU_PLL_SET),
	REGNAME(hwv, 0x998, HW_CCF_SSPM_MTCMOS_SET),
	/* HWV register */
	REGNAME(hwv_ext, 0x400, HW_CCF_PLL_ENABLE),
	REGNAME(hwv_ext, 0x404, HW_CCF_PLL_STATUS),
	REGNAME(hwv_ext, 0x40c, HW_CCF_PLL_DONE),
	REGNAME(hwv_ext, 0x410, HW_CCF_MTCMOS_ENABLE),
	REGNAME(hwv_ext, 0x414, HW_CCF_MTCMOS_STATUS),
	REGNAME(hwv_ext, 0x41c, HW_CCF_MTCMOS_DONE),
	REGNAME(hwv_ext, 0x450, HW_CCF_PLL_STATUS_CLR),
	REGNAME(hwv_ext, 0x454, HW_CCF_MTCMOS_STATUS_CLR),
	REGNAME(hwv_ext, 0x464, HW_CCF_PLL_SET_STATUS),
	REGNAME(hwv_ext, 0x468, HW_CCF_PLL_CLR_STATUS),
	REGNAME(hwv_ext, 0x46c, HW_CCF_MTCMOS_SET_STATUS),
	REGNAME(hwv_ext, 0x470, HW_CCF_MTCMOS_CLR_STATUS),
	REGNAME(hwv_ext, 0x4a8, HW_CCF_MTCMOS_FLOW_FLAG_SET),
	REGNAME(hwv_ext, 0x4ac, HW_CCF_MTCMOS_FLOW_FLAG_CLR),
	REGNAME(hwv_ext, 0x500, HW_CCF_INT_STATUS),
	REGNAME(hwv_wrt, 0x55c, HWV_DOMAIN_KEY),
	REGNAME(hwv_ext, 0xf04, HWV_ADDR_HISTORY_0),
	REGNAME(hwv_ext, 0xf08, HWV_ADDR_HISTORY_1),
	REGNAME(hwv_ext, 0xf0c, HWV_ADDR_HISTORY_2),
	REGNAME(hwv_ext, 0xf10, HWV_ADDR_HISTORY_3),
	REGNAME(hwv_ext, 0xf14, HWV_ADDR_HISTORY_4),
	REGNAME(hwv_ext, 0xf18, HWV_ADDR_HISTORY_5),
	REGNAME(hwv_ext, 0xf1c, HWV_ADDR_HISTORY_6),
	REGNAME(hwv_ext, 0xf20, HWV_ADDR_HISTORY_7),
	REGNAME(hwv_ext, 0xf24, HWV_ADDR_HISTORY_8),
	REGNAME(hwv_ext, 0xf28, HWV_ADDR_HISTORY_9),
	REGNAME(hwv_ext, 0xf2c, HWV_ADDR_HISTORY_10),
	REGNAME(hwv_ext, 0xf30, HWV_ADDR_HISTORY_11),
	REGNAME(hwv_ext, 0xf34, HWV_ADDR_HISTORY_12),
	REGNAME(hwv_ext, 0xf38, HWV_ADDR_HISTORY_13),
	REGNAME(hwv_ext, 0xf3c, HWV_ADDR_HISTORY_14),
	REGNAME(hwv_ext, 0xf40, HWV_ADDR_HISTORY_15),
	REGNAME(hwv_ext, 0xf44, HWV_DATA_HISTORY_0),
	REGNAME(hwv_ext, 0xf48, HWV_DATA_HISTORY_1),
	REGNAME(hwv_ext, 0xf4c, HWV_DATA_HISTORY_2),
	REGNAME(hwv_ext, 0xf50, HWV_DATA_HISTORY_3),
	REGNAME(hwv_ext, 0xf54, HWV_DATA_HISTORY_4),
	REGNAME(hwv_ext, 0xf58, HWV_DATA_HISTORY_5),
	REGNAME(hwv_ext, 0xf5c, HWV_DATA_HISTORY_6),
	REGNAME(hwv_ext, 0xf60, HWV_DATA_HISTORY_7),
	REGNAME(hwv_ext, 0xf64, HWV_DATA_HISTORY_8),
	REGNAME(hwv_ext, 0xf68, HWV_DATA_HISTORY_9),
	REGNAME(hwv_ext, 0xf6c, HWV_DATA_HISTORY_10),
	REGNAME(hwv_ext, 0xf70, HWV_DATA_HISTORY_11),
	REGNAME(hwv_ext, 0xf74, HWV_DATA_HISTORY_12),
	REGNAME(hwv_ext, 0xf78, HWV_DATA_HISTORY_13),
	REGNAME(hwv_ext, 0xf7c, HWV_DATA_HISTORY_14),
	REGNAME(hwv_ext, 0xf80, HWV_DATA_HISTORY_15),
	REGNAME(hwv_ext, 0xf84, HWV_IDX_POINTER),
	{},
};

static const struct regname *get_all_mt6878_regnames(void)
{
	return rn;
}

static void init_regbase(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rb) - 1; i++) {
		if (!rb[i].phys)
			continue;

		rb[i].virt = ioremap(rb[i].phys, 0x1000);
	}
}

u32 get_mt6878_reg_value(u32 id, u32 ofs)
{
	if (id >= chk_sys_num)
		return 0;

	return clk_readl(rb[id].virt + ofs);
}
EXPORT_SYMBOL_GPL(get_mt6878_reg_value);

/*
 * clkchk pwr_data
 */

struct pwr_data {
	const char *pvdname;
	enum chk_sys_id id;
	u32 base;
	u32 ofs;
};

/*
 * clkchk pwr_data
 */
static struct pwr_data pvd_pwr_data[] = {
	{"audiosys", afe, spm, 0x0E18},
	{"camsys_main", cam_m, spm, 0x0E48},
	{"camsys_mraw", cam_mr, spm, 0x0E48},
	{"camsys_rawa", cam_ra, spm, 0x0E50},
	{"camsys_rawb", cam_rb, spm, 0x0E54},
	{"camsys_yuva", cam_ya, spm, 0x0E50},
	{"camsys_yuvb", cam_yb, spm, 0x0E54},
	{"cam_vcore", cam_vcore, spm, 0x0E5C},
	{"ccu", ccu, spm, 0x0E64},
	{"dip_nr1_dip1", dip_nr1_dip1, spm, 0x0E2C},
	{"dip_nr2_dip1", dip_nr2_dip1, spm, 0x0E2C},
	{"dip_top_dip1", dip_top_dip1, spm, 0x0E2C},
	{"mmsys0", mm, spm, 0x0E70},
	{"imgsys_main", img, spm, 0x0E28},
	{"img_vcore_d1a", img_v, spm, 0x0E28},
	{"mdpsys", mdp, spm, 0x0E70},
	{"mminfra_config", mminfra_config, spm, 0x0E78},
	{"ssr_top", ssr_top, spm, 0x0EA0},
	{"traw_dip1", traw_dip1, spm, 0x0E2C},
	{"vdecsys", vde2, spm, 0x0E38},
	{"vencsys", ven1, spm, 0x0E40},
	{"wpe1_dip1", wpe1_dip1, spm, 0x0E2C},
	{"wpe2_dip1", wpe2_dip1, spm, 0x0E2C},
};

static int get_pvd_pwr_data_idx(const char *pvdname)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_data); i++) {
		if (pvd_pwr_data[i].pvdname == NULL)
			continue;
		if (!strcmp(pvdname, pvd_pwr_data[i].pvdname))
			return i;
	}

	return -1;
}

/*
 * clkchk pwr_status
 */
static u32 get_pwr_status(s32 idx)
{
	if (idx < 0 || idx >= ARRAY_SIZE(pvd_pwr_data))
		return 0;

	if (pvd_pwr_data[idx].id >= chk_sys_num)
		return 0;

	return  clk_readl(rb[pvd_pwr_data[idx].base].virt + pvd_pwr_data[idx].ofs);
}

static bool is_cg_chk_pwr_on(void)
{
#if CG_CHK_PWRON_ENABLE
	return true;
#endif
	return false;
}

#if CHECK_VCORE_FREQ
/*
 * clkchk vf table
 */

struct mtk_vf {
	const char *name;
	int freq_table[6];
};

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3, _freq4, _freq5) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3, _freq4, _freq5},	\
	}

/*
 * Opp0 : 0p725v
 * Opp1 : 0p70v
 * Opp2 : 0p65v
 * Opp3 : 0p60v
 * Opp4 : 0p575v
 * Opp5 : 0p55v
 */
static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3, Opp4, Opp5 */
	MTK_VF_TABLE("top_axi_sel", 156000, 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("top_axip_sel", 156000, 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("top_axi_ufs_sel", 78000, 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("top_bus_aximem_sel", 364000, 364000, 273000, 273000, 218400, 218400),
	MTK_VF_TABLE("top_disp0_sel", 624000, 458333, 364000, 273000, 273000),
	MTK_VF_TABLE("top_mminfra_sel", 624000, 624000, 458333, 364000, 273000, 273000),
	MTK_VF_TABLE("top_mmup_sel", 728000, 728000, 728000, 728000, 728000, 728000),
	MTK_VF_TABLE("top_camtg_sel", 52000, 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("top_camtg2_sel", 52000, 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("top_camtg3_sel", 52000, 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("top_camtg4_sel", 52000, 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("top_camtg5_sel", 52000, 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("top_camtg6_sel", 52000, 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("top_uart_sel", 52000, 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("top_spi0_sel", 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi1_sel", 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi2_sel", 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi3_sel", 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi4_sel", 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi5_sel", 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi6_sel", 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_spi7_sel", 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_msdc_0p_sel", 384000, 384000, 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("top_msdc5hclk_sel", 273000, 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("top_msdc50_0_sel", 384000, 384000, 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("top_aes_msdcfde_sel", 384000, 384000, 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("top_msdc_1p_sel", 384000, 384000, 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("top_msdc30_1_sel", 192000, 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("top_msdc30_1_h_sel", 136500, 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("top_aud_intbus_sel", 136500, 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("top_atb_sel", 273000, 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("top_usb_sel", 124800, 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("top_usb_xhci_sel", 124800, 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("top_i2c_sel", 136500, 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("top_seninf_sel", 499200, 416000, 343750, 312000, 312000),
	MTK_VF_TABLE("top_seninf1_sel", 499200, 416000, 343750, 312000, 312000),
	MTK_VF_TABLE("top_seninf2_sel", 499200, 416000, 343750, 312000, 312000),
	MTK_VF_TABLE("top_seninf3_sel", 499200, 416000, 343750, 312000, 312000),
	MTK_VF_TABLE("top_aud_engen1_sel", 45158, 45158, 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("top_aud_engen2_sel", 49152, 49152, 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("top_aes_ufsfde_sel", 546000, 546000, 546000, 546000, 546000, 416000),
	MTK_VF_TABLE("top_ufs_sel", 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_ufs_mbist_sel", 297000, 297000, 297000, 297000, 297000, 297000),
	MTK_VF_TABLE("top_aud_1_sel", 180634, 180634, 180634, 180634, 180634, 180634),
	MTK_VF_TABLE("top_aud_2_sel", 196608, 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("top_dpmaif_main_sel", 436800, 436800, 416000, 364000, 273000, 273000),
	MTK_VF_TABLE("top_venc_sel", 624000, 458333, 343750, 249600, 249600),
	MTK_VF_TABLE("top_vdec_sel", 546000, 416000, 312000, 218400, 218400),
	MTK_VF_TABLE("top_pwm_sel", 78000, 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("top_audio_h_sel", 196608, 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("top_mcupm_sel", 218400, 218400, 218400, 218400, 218400, 218400),
	MTK_VF_TABLE("top_mem_sub_sel", 546000, 546000, 436800, 273000, 218400, 218400),
	MTK_VF_TABLE("top_mem_subp_sel", 546000, 546000, 436800, 273000, 218400, 218400),
	MTK_VF_TABLE("top_mem_sub_ufs_sel", 546000, 546000, 436800, 273000, 218400, 218400),
	MTK_VF_TABLE("top_emi_n_sel", 728000, 728000, 499200, 260000, 260000, 260000),
	MTK_VF_TABLE("top_dsi_occ_sel", 312000, 312000, 312000, 249600, 208000, 208000),
	MTK_VF_TABLE("top_ap2conn_host_sel", 78000, 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("top_ccusys_sel", 832000, 832000, 832000, 832000, 832000, 832000),
	MTK_VF_TABLE("top_ccutm_sel", 208000, 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("top_msdc_1p_rx_sel", 192000, 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("top_dsp_sel", 249600, 249600, 249600, 249600, 249600, 249600),
	MTK_VF_TABLE("top_md_emi_sel", 546000, 546000, 546000, 546000, 546000, 546000),
	{},
};
#endif

static const char *get_vf_name(int id)
{
#if CHECK_VCORE_FREQ
	if (id < 0) {
		pr_err("[%s]Negative index detected\n", __func__);
		return NULL;
	}

	return vf_table[id].name;

#else
	return NULL;
#endif
}

static int get_vf_opp(int id, int opp)
{
#if CHECK_VCORE_FREQ
	if (id < 0 || opp < 0) {
		pr_err("[%s]Negative index detected\n", __func__);
		return 0;
	}

	return vf_table[id].freq_table[opp];
#else
	return 0;
#endif
}

static u32 get_vf_num(void)
{
#if CHECK_VCORE_FREQ
	return ARRAY_SIZE(vf_table) - 1;
#else
	return 0;
#endif
}

static int get_vcore_opp(void)
{
#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER) && CHECK_VCORE_FREQ
	return mtk_dvfsrc_query_opp_info(MTK_DVFSRC_SW_REQ_VCORE_OPP);
#else
	return VCORE_NULL;
#endif
}

static unsigned int reg_dump_addr[ARRAY_SIZE(rn) - 1];
static unsigned int reg_dump_val[ARRAY_SIZE(rn) - 1];
static bool reg_dump_valid[ARRAY_SIZE(rn) - 1];

void set_subsys_reg_dump_mt6878(enum chk_sys_id id[])
{
	const struct regname *rns = &rn[0];
	int i, j, k;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		int pwr_idx = PD_NULL;

		if (!is_valid_reg(ADDR(rns)))
			continue;

		for (j = 0; id[j] != chk_sys_num; j++) {
			/* filter out the subsys that we don't want */
			if (rns->id == id[j])
				break;
		}

		if (id[j] == chk_sys_num)
			continue;

		for (k = 0; k < ARRAY_SIZE(pvd_pwr_data); k++) {
			if (pvd_pwr_data[k].id == id[j]) {
				pwr_idx = k;
				break;
			}
		}

		if (pwr_idx != PD_NULL)
			if (!pwr_hw_is_on(PWR_CON_STA, pwr_idx))
				continue;

		reg_dump_addr[i] = PHYSADDR(rns);
		reg_dump_val[i] = clk_readl(ADDR(rns));
		/* record each register dump index validation */
		reg_dump_valid[i] = true;
	}
}
EXPORT_SYMBOL_GPL(set_subsys_reg_dump_mt6878);

void get_subsys_reg_dump_mt6878(void)
{
	const struct regname *rns = &rn[0];
	int i;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (reg_dump_valid[i])
			pr_info("%-18s: [0x%08x] = 0x%08x\n",
					rns->name, reg_dump_addr[i], reg_dump_val[i]);
	}
}
EXPORT_SYMBOL_GPL(get_subsys_reg_dump_mt6878);

void print_subsys_reg_mt6878(enum chk_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int pwr_idx = PD_NULL;
	int i;

	if (id >= chk_sys_num) {
		pr_info("wrong id:%d\n", id);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_data); i++) {
		if (pvd_pwr_data[i].id == id) {
			pwr_idx = i;
			break;
		}
	}

	rb_dump = &rb[id];

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (!is_valid_reg(ADDR(rns)))
			return;

		/* filter out the subsys that we don't want */
		if (rns->base != rb_dump)
			continue;

		if (pwr_idx != PD_NULL) {
			if (!pwr_hw_is_on(PWR_CON_STA, pwr_idx))
				return;
		}

		pr_info("%-18s: [0x%08x] = 0x%08x\n",
			rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}
EXPORT_SYMBOL_GPL(print_subsys_reg_mt6878);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
static enum chk_sys_id devapc_dump_id[] = {
	spm,
	top,
	apmixed,
	mfg_ao,
	mfgsc_ao,
	vlp,
	hwv,
	hwv_ext,
	chk_sys_num,
};

static void devapc_dump(void)
{
	const struct fmeter_clk *fclks;

	fclks = mt_get_fmeter_clks();
	set_subsys_reg_dump_mt6878(devapc_dump_id);
	get_subsys_reg_dump_mt6878();

	dump_clk_event();
	pdchk_dump_trace_evt();

	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}

	mdelay(5000);
}

static void serror_dump(void)
{
	const struct fmeter_clk *fclks;

	fclks = mt_get_fmeter_clks();

	set_subsys_reg_dump_mt6878(devapc_dump_id);
	get_subsys_reg_dump_mt6878();

	dump_clk_event();
	pdchk_dump_trace_evt();

	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}

	mdelay(5000);
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump,
};

static struct devapc_vio_callbacks serror_handle = {
	.id = DEVAPC_SUBSYS_CLKM,
	.debug_dump = serror_dump,
};

#endif

static const char * const off_pll_names[] = {
	"univpll",
	"msdcpll",
	"mmpll",
	"ufspll",
	"mfg-ao-mfgpll",
	"mfgsc-ao-mfgscpll",
	NULL
};

static const char * const notice_pll_names[] = {
	"apll1",
	"apll2",
	NULL
};

static const char * const bypass_pll_name[] = {
	"univpll",
	NULL
};

static const char * const *get_off_pll_names(void)
{
	return off_pll_names;
}

static const char * const *get_notice_pll_names(void)
{
	return notice_pll_names;
}

static const char * const *get_bypass_pll_name(void)
{
	return bypass_pll_name;
}

static bool is_pll_chk_bug_on(void)
{
#if (BUG_ON_CHK_ENABLE) || (IS_ENABLED(CONFIG_MTK_CLKMGR_DEBUG))
	return true;
#endif
	return false;
}

static bool is_suspend_retry_stop(bool reset_cnt)
{
	if (reset_cnt == true) {
		suspend_cnt = 0;
		return true;
	}

	suspend_cnt++;
	pr_notice("%s: suspend cnt: %d\n", __func__, suspend_cnt);

	if (suspend_cnt < 2)
		return false;

	return true;
}

static enum chk_sys_id history_dump_id[] = {
	top,
	apmixed,
	hwv,
	hwv_ext,
	chk_sys_num,
};

static void dump_hwv_history(struct regmap *regmap, u32 id)
{
	u32 set[XPU_NUM] = {0}, sta = 0, en = 0, done = 0;
	int i;

	set_subsys_reg_dump_mt6878(history_dump_id);

	if (regmap != NULL) {
		for (i = 0; i < XPU_NUM; i++)
			regmap_read(regmap, HWV_CG_SET(xpu_id[i], id), &set[i]);

		regmap_read(regmap, HWV_CG_STA(id), &sta);
		regmap_read(regmap, HWV_CG_EN(id), &en);
		regmap_read(regmap, HWV_CG_DONE(id), &done);


		for (i = 0; i < XPU_NUM; i++)
			pr_notice("set: (%x)%x", HWV_CG_SET(xpu_id[i], id), set[i]);
		pr_notice("[%d] (%x)%x, (%x)%x, (%x)%x\n",
				id,
				HWV_CG_STA(id), sta,
				HWV_CG_EN(id), en,
				HWV_CG_DONE(id), done);
	}

	get_subsys_reg_dump_mt6878();
}

static enum chk_sys_id bus_dump_id[] = {
	top,
	apmixed,
	chk_sys_num,
};

static void get_bus_reg(void)
{
	set_subsys_reg_dump_mt6878(bus_dump_id);
}

static void dump_bus_reg(struct regmap *regmap, u32 ofs)
{
	const struct fmeter_clk *fclks;

	fclks = mt_get_fmeter_clks();
	set_subsys_reg_dump_mt6878(bus_dump_id);
	get_subsys_reg_dump_mt6878();
	for (; fclks != NULL && fclks->type != FT_NULL; fclks++) {
		if (fclks->type != VLPCK && fclks->type != SUBSYS)
			pr_notice("[%s] %d khz\n", fclks->name,
				mt_get_fmeter_freq(fclks->id, fclks->type));
	}
	/* sspm need some time to run isr */
	mdelay(1000);

	BUG_ON(1);
}

static enum chk_sys_id pll_dump_id[] = {
	apmixed,
	top,
	hwv,
	hwv_ext,
	chk_sys_num,
};

static void dump_pll_reg(bool bug_on)
{
	set_subsys_reg_dump_mt6878(pll_dump_id);
	get_subsys_reg_dump_mt6878();

	if (bug_on) {
		mdelay(300);
		BUG_ON(1);
	}
}

static void check_hwv_irq_sta(void)
{
	u32 irq_sta;

	irq_sta = get_mt6878_reg_value(hwv_ext, HWV_IRQ_STATUS);

	if ((irq_sta & HWV_INT_CG_TRIGGER) == HWV_INT_CG_TRIGGER) {
		dump_hwv_history(NULL, 0);
		dump_bus_reg(NULL, 0);
	}
	if ((irq_sta & HWV_INT_PLL_TRIGGER) == HWV_INT_PLL_TRIGGER)
		dump_pll_reg(true);
}

/*
 * init functions
 */

static struct clkchk_ops clkchk_mt6878_ops = {
	.get_all_regnames = get_all_mt6878_regnames,
	.get_pvd_pwr_data_idx = get_pvd_pwr_data_idx,
	.get_pwr_status = get_pwr_status,
	.is_cg_chk_pwr_on = is_cg_chk_pwr_on,
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.get_bypass_pll_name = get_bypass_pll_name,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	.get_vf_name = get_vf_name,
	.get_vf_opp = get_vf_opp,
	.get_vf_num = get_vf_num,
	.get_vcore_opp = get_vcore_opp,
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
	.devapc_dump = devapc_dump,
#endif
	.dump_hwv_history = dump_hwv_history,
	.get_bus_reg = get_bus_reg,
	.dump_bus_reg = dump_bus_reg,
	.dump_pll_reg = dump_pll_reg,
	.trace_clk_event = trace_clk_event,
	.check_hwv_irq_sta = check_hwv_irq_sta,
	.is_suspend_retry_stop = is_suspend_retry_stop,
};

static int clk_chk_mt6878_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;

	init_regbase();

	set_clkchk_notify();

	set_clkchk_ops(&clkchk_mt6878_ops);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
	register_devapc_vio_callback(&serror_handle);
#endif

#if CHECK_VCORE_FREQ
	mtk_clk_check_muxes();
#endif

	clkchk_hwv_irq_init(pdev);

	return 0;
}

static const struct of_device_id of_match_clkchk_mt6878[] = {
	{
		.compatible = "mediatek,mt6878-clkchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_chk_mt6878_drv = {
	.probe = clk_chk_mt6878_probe,
	.driver = {
		.name = "clk-chk-mt6878",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
		.of_match_table = of_match_clkchk_mt6878,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6878_init(void)
{
	return platform_driver_register(&clk_chk_mt6878_drv);
}

static void __exit clkchk_mt6878_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6878_drv);
}

subsys_initcall(clkchk_mt6878_init);
module_exit(clkchk_mt6878_exit);
MODULE_LICENSE("GPL");
