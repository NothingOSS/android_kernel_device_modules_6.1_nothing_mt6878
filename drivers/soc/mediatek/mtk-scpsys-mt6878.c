// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Chuan-wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "scpsys.h"
#include "mtk-scpsys.h"

#include <dt-bindings/power/mt6878-power.h>

#define SCPSYS_BRINGUP			(0)
#if SCPSYS_BRINGUP
#define default_cap			(MTK_SCPD_BYPASS_OFF)
#else
#define default_cap			(0)
#endif

#define MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_1_MD	(BIT(9))
#define MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_0_MD	(BIT(11))
#define MT6878_NEMICFG_AO_MEM_PROT_EN_GLITCH_MD	(BIT(6) | BIT(7))
#define MT6878_TOP_AXI_PROT_EN_MCU_STA_0_CONN	(BIT(1))
#define MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_1_CONN	(BIT(12))
#define MT6878_TOP_AXI_PROT_EN_MCU_STA_0_CONN_2ND	(BIT(0))
#define MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_0_CONN	(BIT(8))
#define MT6878_TOP_AXI_PROT_EN_PERISYS_STA_0_AUDIO	(BIT(6))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN	(BIT(2))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_2ND	(BIT(3))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_3RD	(BIT(4))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_4RD	(BIT(5))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_DIP1	(BIT(6))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_DIP1_2ND	(BIT(7))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_VCORE	(BIT(8))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_1_ISP_VCORE	(BIT(7) | BIT(8))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_VDE0	(BIT(20))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_1_VDE0	(BIT(13))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_VEN0	(BIT(12))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_1_VEN0	(BIT(12))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN	(BIT(29))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN_2ND	(BIT(30))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBA	(BIT(25))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBA_2ND	(BIT(26))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBB	(BIT(27))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBB_2ND	(BIT(28))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_VCORE	(BIT(31))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_VCORE	(BIT(9) | BIT(10))
#define MT6878_TOP_AXI_PROT_EN_DRAMC_STA_0_CAM_CCU	(BIT(8) | BIT(10))
#define MT6878_TOP_AXI_PROT_EN_DRAMC_STA_0_CAM_CCU_2ND	(BIT(9) | BIT(11) |  \
			BIT(12))
#define MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_0_CAM_CCU	(BIT(13))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_DISP	(BIT(0) | BIT(1) |  \
			BIT(18))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA	(BIT(1) | BIT(2) |  \
			BIT(3) | BIT(6))
#define MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_1_MM_INFRA	(BIT(11))
#define MT6878_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA_2ND	(BIT(0) | BIT(5) |  \
			BIT(7) | BIT(8) |  \
			BIT(9) | BIT(10) |  \
			BIT(11) | BIT(12) |  \
			BIT(13) | BIT(14) |  \
			BIT(15))
#define MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_0_MM_INFRA	(BIT(16))
#define MT6878_TOP_AXI_PROT_EN_EMISYS_STA_0_MM_INFRA	(BIT(20) | BIT(21))
#define MT6878_VLP_AXI_PROT_EN_MM_PROC	(BIT(8))
#define MT6878_VLP_AXI_PROT_EN_MM_PROC_2ND	(BIT(9) | BIT(10))
#define MT6878_AXI_PROT_EN_SSRSYS	(BIT(0))
#define MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_0_SSRSYS	(BIT(29))
#define MT6878_TOP_AXI_PROT_EN_PERISYS_STA_0_SSUSB	(BIT(7))
#define MT6878_TOP_AXI_PROT_EN_MD_STA_0_MFG0	(BIT(4))
#define MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_0_MFG0	(BIT(9))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE = 1,
	NEMICFG_AO_MEM_REG_TYPE = 2,
	VLP_TYPE = 3,
	SSR_TOP_TYPE = 4,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "ifrao",
	[NEMICFG_AO_MEM_REG_TYPE] = "nemicfg-ao-mem-reg-bus",
	[VLP_TYPE] = "vlpcfg-reg",
	[SSR_TOP_TYPE] = "ssr-top",
};

/*
 * MT6878 power domain support
 */

static const struct scp_domain_data scp_domain_mt6878_spm_data[] = {
	[MT6878_POWER_DOMAIN_MD] = {
		.name = "md",
		.ctl_offs = 0xE00,
		.extb_iso_offs = 0xF24,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_1_MD),
			BUS_PROT(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_0_MD),
			BUS_PROT(NEMICFG_AO_MEM_REG_TYPE, 0x84, 0x88, 0x80, 0x8C,
				MT6878_NEMICFG_AO_MEM_PROT_EN_GLITCH_MD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_MD_OPS |
			MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_REMOVE_MD_RSTB,
	},
	[MT6878_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6878_TOP_AXI_PROT_EN_MCU_STA_0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_1_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6878_TOP_AXI_PROT_EN_MCU_STA_0_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_0_CONN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6878_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.ctl_offs = 0xE18,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audio"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				MT6878_TOP_AXI_PROT_EN_PERISYS_STA_0_AUDIO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp-main",
		.ctl_offs = 0xE28,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp", "ipe"},
		.subsys_clk_prefix = "isp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_3RD),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_MAIN_4RD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_ISP_DIP1] = {
		.name = "isp-dip1",
		.ctl_offs = 0xE2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "dip1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_DIP1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_DIP1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp-vcore",
		.ctl_offs = 0xE34,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_1_ISP_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.ctl_offs = 0xE38,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde"},
		.subsys_clk_prefix = "vde0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_VDE0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_1_VDE0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.ctl_offs = 0xE40,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_1_VEN0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam-main",
		.ctl_offs = 0xE48,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam_main",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam-suba",
		.ctl_offs = 0xE50,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_suba",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBA_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam-subb",
		.ctl_offs = 0xE54,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_subb",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBB),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_SUBB_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam-vcore",
		.ctl_offs = 0xE5C,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_CAM_CCU] = {
		.name = "cam-ccu",
		.ctl_offs = 0xE60,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ccu", "ccu_ahb"},
		.subsys_clk_prefix = "cam_ccu",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				MT6878_TOP_AXI_PROT_EN_DRAMC_STA_0_CAM_CCU),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				MT6878_TOP_AXI_PROT_EN_DRAMC_STA_0_CAM_CCU_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_0_CAM_CCU),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_CAM_CCU_AO] = {
		.name = "cam-ccu-ao",
		.ctl_offs = 0xE64,
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.ctl_offs = 0xE70,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "disp0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6878_TOP_AXI_PROT_EN_MMSYS_STA_0_DISP),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_MM_INFRA] = {
		.name = "mm-infra",
		.hwv_comp = "hw-voter-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 0,
		.basic_clk_name = {"mm_infra"},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6878_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm-proc-dormant",
		.ctl_offs = 0xE7C,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"mmup"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6878_VLP_AXI_PROT_EN_MM_PROC),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6878_VLP_AXI_PROT_EN_MM_PROC_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_CSI_RX] = {
		.name = "csi-rx",
		.ctl_offs = 0xE9C,
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_SSR] = {
		.name = "ssrsys",
		.hwv_comp = "hw-voter-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 1,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6878_POWER_DOMAIN_SSUSB] = {
		.name = "ssusb",
		.ctl_offs = 0xEA8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				MT6878_TOP_AXI_PROT_EN_PERISYS_STA_0_SSUSB),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_MFG0_SHUTDOWN] = {
		.name = "mfg0-shutdown",
		.ctl_offs = 0xEB4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6878_TOP_AXI_PROT_EN_MD_STA_0_MFG0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6878_TOP_AXI_PROT_EN_INFRASYS_STA_0_MFG0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON | default_cap,
	},
	[MT6878_POWER_DOMAIN_APU] = {
		.name = "apu",
		.caps = MTK_SCPD_APU_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_mt6878_spm[] = {
	{MT6878_POWER_DOMAIN_ISP_VCORE, MT6878_POWER_DOMAIN_ISP_MAIN},
	{MT6878_POWER_DOMAIN_ISP_MAIN, MT6878_POWER_DOMAIN_ISP_DIP1},
	{MT6878_POWER_DOMAIN_MM_INFRA, MT6878_POWER_DOMAIN_ISP_VCORE},
	{MT6878_POWER_DOMAIN_MM_INFRA, MT6878_POWER_DOMAIN_VDE0},
	{MT6878_POWER_DOMAIN_MM_INFRA, MT6878_POWER_DOMAIN_VEN0},
	{MT6878_POWER_DOMAIN_CAM_VCORE, MT6878_POWER_DOMAIN_CAM_MAIN},
	{MT6878_POWER_DOMAIN_CAM_MAIN, MT6878_POWER_DOMAIN_CAM_SUBA},
	{MT6878_POWER_DOMAIN_CAM_MAIN, MT6878_POWER_DOMAIN_CAM_SUBB},
	{MT6878_POWER_DOMAIN_MM_INFRA, MT6878_POWER_DOMAIN_CAM_VCORE},
	{MT6878_POWER_DOMAIN_CAM_VCORE, MT6878_POWER_DOMAIN_CAM_CCU},
	{MT6878_POWER_DOMAIN_CAM_CCU, MT6878_POWER_DOMAIN_CAM_CCU_AO},
	{MT6878_POWER_DOMAIN_MM_INFRA, MT6878_POWER_DOMAIN_DISP},
	{MT6878_POWER_DOMAIN_MM_INFRA, MT6878_POWER_DOMAIN_MM_PROC_DORMANT},
};

static const struct scp_soc_data mt6878_spm_data = {
	.domains = scp_domain_mt6878_spm_data,
	.num_domains = MT6878_SPM_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6878_spm,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6878_spm),
	.regs = {
		.pwr_sta_offs = 0xF50,
		.pwr_sta2nd_offs = 0xF54,
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6878-scpsys",
		.data = &mt6878_spm_data,
	}, {
		/* sentinel */
	}
};

static int mt6878_scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);

	scp = init_scp(pdev, soc->domains, soc->num_domains, &soc->regs, bus_list, BUS_TYPE_NUM);
	if (IS_ERR(scp))
		return PTR_ERR(scp);

	ret = mtk_register_power_domains(pdev, scp, soc->num_domains);
	if (ret)
		return ret;

	pd_data = &scp->pd_data;

	for (i = 0, sd = soc->subdomains; i < soc->num_subdomains; i++, sd++) {
		ret = pm_genpd_add_subdomain(pd_data->domains[sd->origin],
					     pd_data->domains[sd->subdomain]);
		if (ret && IS_ENABLED(CONFIG_PM)) {
			dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
			return ret;
		}
	}

	return 0;
}

static struct platform_driver mt6878_scpsys_drv = {
	.probe = mt6878_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6878",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6878_scpsys_drv);
MODULE_LICENSE("GPL");
