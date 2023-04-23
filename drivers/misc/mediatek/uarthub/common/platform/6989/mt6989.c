// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_core.h"
#include "uarthub_drv_export.h"
#include "common_def_id.h"

#include "inc/mt6989.h"
#include "inc/mt6989_debug.h"

#include <linux/clk.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/regmap.h>
#include <linux/string.h>
#include <linux/types.h>

void __iomem *gpio_base_remap_addr_mt6989;
void __iomem *pericfg_ao_remap_addr_mt6989;
void __iomem *topckgen_base_remap_addr_mt6989;
void __iomem *dev0_base_remap_addr_mt6989;
void __iomem *dev1_base_remap_addr_mt6989;
void __iomem *dev2_base_remap_addr_mt6989;
void __iomem *cmm_base_remap_addr_mt6989;
void __iomem *intfhub_base_remap_addr_mt6989;
void __iomem *apuart1_base_remap_addr_mt6989;
void __iomem *apuart2_base_remap_addr_mt6989;
void __iomem *apuart3_base_remap_addr_mt6989;
void __iomem *apdma_uart_tx_int_remap_addr_mt6989;
void __iomem *spm_remap_addr_mt6989;
void __iomem *apmixedsys_remap_addr_mt6989;
void __iomem *iocfg_rm_remap_addr_mt6989;
void __iomem *sys_sram_remap_addr_mt6989;

#if UARTHUB_SUPPORT_DX4_FPGA
unsigned long INTFHUB_BASE_DX4_FPGA;
#else
unsigned long INTFHUB_BASE_MT6989;
#endif

#if UARTHUB_SUPPORT_UNIVPLL_CTRL
struct clk *clk_apmixedsys_univpll_mt6989;
struct mutex g_lock_univpll_clk_mt6989;
static int g_univpll_clk_ref_count_mt6989;
#endif

static int uarthub_is_ready_state_mt6989(void);
static int uarthub_config_host_baud_rate_mt6989(int dev_index, int rate_index);
static int uarthub_config_cmm_baud_rate_mt6989(int rate_index);
static int uarthub_irq_mask_ctrl_mt6989(int mask);
static int uarthub_irq_clear_ctrl_mt6989(int irq_type);
static int uarthub_is_uarthub_clk_enable_mt6989(void);
static int uarthub_reset_to_ap_enable_only_mt6989(int ap_only);
static int uarthub_reset_flow_control_mt6989(void);
static int uarthub_is_assert_state_mt6989(void);
static int uarthub_assert_state_ctrl_mt6989(int assert_ctrl);
static int uarthub_get_host_status_mt6989(int dev_index);
static int uarthub_get_host_wakeup_status_mt6989(void);
static int uarthub_get_host_set_fw_own_status_mt6989(void);
static int uarthub_is_host_trx_idle_mt6989(int dev_index, enum uarthub_trx_type trx);
static int uarthub_get_host_byte_cnt_mt6989(int dev_index, enum uarthub_trx_type trx);
static int uarthub_get_cmm_byte_cnt_mt6989(enum uarthub_trx_type trx);
static int uarthub_config_crc_ctrl_mt6989(int enable);
static int uarthub_config_host_fifoe_ctrl_mt6989(int dev_index, int enable);
static int uarthub_get_rx_error_crc_info_mt6989(
	int dev_index, int *p_crc_error_data, int *p_crc_result);
static int uarthub_get_trx_timeout_info_mt6989(
	int dev_index, enum uarthub_trx_type trx,
	int *p_timeout_counter, int *p_pkt_counter);
static int uarthub_get_irq_err_type_mt6989(void);
static int uarthub_init_remap_reg_mt6989(void);
static int uarthub_deinit_unmap_reg_mt6989(void);
static int uarthub_get_spm_sys_timer_mt6989(uint32_t *hi, uint32_t *lo);

#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
static int uarthub_init_default_config_mt6989(void);
static int uarthub_init_trx_timeout_mt6989(void);
#endif

#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA) || (UARTHUB_SUPPORT_DVT)
static int uarthub_sspm_irq_clear_ctrl_mt6989(int irq_type);
static int uarthub_sspm_irq_get_sta_mt6989(void);
static int uarthub_sspm_irq_mask_ctrl_mt6989(int irq_type, int is_mask);
#endif

struct uarthub_ops_struct mt6989_plat_data = {
	.core_ops = &mt6989_plat_core_data,
	.debug_ops = &mt6989_plat_debug_data,
	.ut_test_ops = &mt6989_plat_ut_test_data,
};

struct uarthub_core_ops_struct mt6989_plat_core_data = {
	.uarthub_plat_is_apb_bus_clk_enable = uarthub_is_apb_bus_clk_enable_mt6989,
	.uarthub_plat_get_hwccf_univpll_on_info = uarthub_get_hwccf_univpll_on_info_mt6989,
	.uarthub_plat_is_ready_state = uarthub_is_ready_state_mt6989,
	.uarthub_plat_uarthub_init = uarthub_uarthub_init_mt6989,
	.uarthub_plat_config_host_baud_rate = uarthub_config_host_baud_rate_mt6989,
	.uarthub_plat_config_cmm_baud_rate = uarthub_config_cmm_baud_rate_mt6989,
	.uarthub_plat_irq_mask_ctrl = uarthub_irq_mask_ctrl_mt6989,
	.uarthub_plat_irq_clear_ctrl = uarthub_irq_clear_ctrl_mt6989,
	.uarthub_plat_is_uarthub_clk_enable = uarthub_is_uarthub_clk_enable_mt6989,
	.uarthub_plat_set_host_loopback_ctrl = uarthub_set_host_loopback_ctrl_mt6989,
	.uarthub_plat_set_cmm_loopback_ctrl = uarthub_set_cmm_loopback_ctrl_mt6989,
	.uarthub_plat_reset_to_ap_enable_only = uarthub_reset_to_ap_enable_only_mt6989,
	.uarthub_plat_reset_flow_control = uarthub_reset_flow_control_mt6989,
	.uarthub_plat_is_assert_state = uarthub_is_assert_state_mt6989,
	.uarthub_plat_assert_state_ctrl = uarthub_assert_state_ctrl_mt6989,
	.uarthub_plat_is_bypass_mode = uarthub_is_bypass_mode_mt6989,
	.uarthub_plat_get_host_status = uarthub_get_host_status_mt6989,
	.uarthub_plat_get_host_wakeup_status = uarthub_get_host_wakeup_status_mt6989,
	.uarthub_plat_get_host_set_fw_own_status = uarthub_get_host_set_fw_own_status_mt6989,
	.uarthub_plat_is_host_trx_idle = uarthub_is_host_trx_idle_mt6989,
	.uarthub_plat_set_host_trx_request = uarthub_set_host_trx_request_mt6989,
	.uarthub_plat_clear_host_trx_request = uarthub_clear_host_trx_request_mt6989,
	.uarthub_plat_get_host_byte_cnt = uarthub_get_host_byte_cnt_mt6989,
	.uarthub_plat_get_cmm_byte_cnt = uarthub_get_cmm_byte_cnt_mt6989,
	.uarthub_plat_config_crc_ctrl = uarthub_config_crc_ctrl_mt6989,
	.uarthub_plat_config_bypass_ctrl = uarthub_config_bypass_ctrl_mt6989,
	.uarthub_plat_config_host_fifoe_ctrl = uarthub_config_host_fifoe_ctrl_mt6989,
	.uarthub_plat_get_rx_error_crc_info = uarthub_get_rx_error_crc_info_mt6989,
	.uarthub_plat_get_trx_timeout_info = uarthub_get_trx_timeout_info_mt6989,
	.uarthub_plat_get_irq_err_type = uarthub_get_irq_err_type_mt6989,
	.uarthub_plat_init_remap_reg = uarthub_init_remap_reg_mt6989,
	.uarthub_plat_deinit_unmap_reg = uarthub_deinit_unmap_reg_mt6989,
	.uarthub_plat_get_spm_sys_timer = uarthub_get_spm_sys_timer_mt6989,

#if !(UARTHUB_SUPPORT_SSPM_DRIVER)
	.uarthub_plat_init_default_config = uarthub_init_default_config_mt6989,
	.uarthub_plat_init_trx_timeout = uarthub_init_trx_timeout_mt6989,
#endif

#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA) || (UARTHUB_SUPPORT_DVT)
	.uarthub_plat_sspm_irq_clear_ctrl = uarthub_sspm_irq_clear_ctrl_mt6989,
	.uarthub_plat_sspm_irq_get_sta = uarthub_sspm_irq_get_sta_mt6989,
	.uarthub_plat_sspm_irq_mask_ctrl = uarthub_sspm_irq_mask_ctrl_mt6989,
#endif
};

int uarthub_is_apb_bus_clk_enable_mt6989(void)
{
	int state = 0;

	if (!pericfg_ao_remap_addr_mt6989) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	/* PERI_CG_1[25] = PERI_UARTHUB_pclk_CG */
	/* PERI_CG_1[24] = PERI_UARTHUB_hclk_CG */
	state = (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6989 + PERI_CG_1,
		PERI_CG_1_UARTHUB_CLK_CG_MASK) >> PERI_CG_1_UARTHUB_CLK_CG_SHIFT);

	if (state != 0x0) {
		pr_notice("[%s] UARTHUB PCLK/HCLK CG gating(0x%x,exp:0x0)\n", __func__, state);
		return 0;
	}

	state = CON1_GET_dev2_pkt_type_start(CON1_ADDR);

	if (state != 0x81) {
		pr_notice("[%s] UARTHUB pkt type start error(0x%x,exp:0x81)\n", __func__, state);
		return 0;
	}

	state = CON1_GET_dev2_pkt_type_end(CON1_ADDR);

	if (state != 0x85) {
		pr_notice("[%s] UARTHUB pkt type start error(0x%x,exp:0x85)\n", __func__, state);
		return 0;
	}

	return 1;
}

int uarthub_get_hwccf_univpll_on_info_mt6989(void)
{
#if UARTHUB_SUPPORT_UNIVPLL_CTRL
	if (!apmixedsys_remap_addr_mt6989) {
		pr_notice("[%s] apmixedsys_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(apmixedsys_remap_addr_mt6989 + UNIVPLL_CON0,
		UNIVPLL_CON0_UNIVPLL_EN_MASK) >> UNIVPLL_CON0_UNIVPLL_EN_SHIFT);
#else
	return 1;
#endif
}

int uarthub_is_ready_state_mt6989(void)
{
	return DEV0_STA_GET_dev0_intfhub_ready(DEV0_STA_ADDR);
}

int uarthub_config_baud_rate_m6989(void __iomem *dev_base, int rate_index)
{
	if (!dev_base) {
		pr_notice("[%s] dev_base is not been init\n", __func__);
		return -1;
	}

	if (rate_index == 115200) {
#if UARTHUB_SUPPORT_FPGA
		UARTHUB_REG_WRITE(FEATURE_SEL_ADDR(dev_base), 0x1);   /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(HIGHSPEED_ADDR(dev_base), 0x3);     /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(SAMPLE_COUNT_ADDR(dev_base), 0x55); /* 0x28 = 0x55 */
		UARTHUB_REG_WRITE(SAMPLE_POINT_ADDR(dev_base), 0x2B); /* 0x2c = 0x2B */
		UARTHUB_REG_WRITE(DLL_ADDR(dev_base), 0x1);           /* 0x90 = 0x1  */
		UARTHUB_REG_WRITE(FRACDIV_L_ADDR(dev_base), 0xEF);    /* 0x54 = 0xEF */
		UARTHUB_REG_WRITE(FRACDIV_M_ADDR(dev_base), 0x1);     /* 0x58 = 0x1  */
#else
		UARTHUB_REG_WRITE(FEATURE_SEL_ADDR(dev_base), 0x1);   /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(HIGHSPEED_ADDR(dev_base), 0x3);     /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(SAMPLE_COUNT_ADDR(dev_base), 0xe0); /* 0x28 = 0xe0 */
		UARTHUB_REG_WRITE(SAMPLE_POINT_ADDR(dev_base), 0x70); /* 0x2c = 0x70 */
		UARTHUB_REG_WRITE(DLL_ADDR(dev_base), 0x4);           /* 0x90 = 0x4  */
		UARTHUB_REG_WRITE(FRACDIV_L_ADDR(dev_base), 0xf6);    /* 0x54 = 0xf6 */
		UARTHUB_REG_WRITE(FRACDIV_M_ADDR(dev_base), 0x1);     /* 0x58 = 0x1  */
#endif
	} else if (rate_index == 3000000) {
		UARTHUB_REG_WRITE(FEATURE_SEL_ADDR(dev_base), 0x1);   /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(HIGHSPEED_ADDR(dev_base), 0x3);     /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(SAMPLE_COUNT_ADDR(dev_base), 0x21); /* 0x28 = 0x21 */
		UARTHUB_REG_WRITE(SAMPLE_POINT_ADDR(dev_base), 0x11); /* 0x2c = 0x11 */
		UARTHUB_REG_WRITE(DLL_ADDR(dev_base), 0x1);           /* 0x90 = 0x1  */
		UARTHUB_REG_WRITE(FRACDIV_L_ADDR(dev_base), 0xdb);    /* 0x54 = 0xdb */
		UARTHUB_REG_WRITE(FRACDIV_M_ADDR(dev_base), 0x1);     /* 0x58 = 0x1  */
	} else if (rate_index == 4000000) {
		UARTHUB_REG_WRITE(FEATURE_SEL_ADDR(dev_base), 0x1);   /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(HIGHSPEED_ADDR(dev_base), 0x3);     /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(SAMPLE_COUNT_ADDR(dev_base), 0x19); /* 0x28 = 0x19 */
		UARTHUB_REG_WRITE(SAMPLE_POINT_ADDR(dev_base), 0xd);  /* 0x2c = 0xd  */
		UARTHUB_REG_WRITE(DLL_ADDR(dev_base), 0x1);           /* 0x90 = 0x1  */
		UARTHUB_REG_WRITE(FRACDIV_L_ADDR(dev_base), 0x0);     /* 0x54 = 0x0  */
		UARTHUB_REG_WRITE(FRACDIV_M_ADDR(dev_base), 0x0);     /* 0x58 = 0x0  */
	} else if (rate_index == 12000000) {
		UARTHUB_REG_WRITE(FEATURE_SEL_ADDR(dev_base), 0x1);   /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(HIGHSPEED_ADDR(dev_base), 0x3);     /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(SAMPLE_COUNT_ADDR(dev_base), 0x7);  /* 0x28 = 0x7  */
		UARTHUB_REG_WRITE(SAMPLE_POINT_ADDR(dev_base), 0x4);  /* 0x2c = 0x4  */
		UARTHUB_REG_WRITE(DLL_ADDR(dev_base), 0x1);           /* 0x90 = 0x1  */
		UARTHUB_REG_WRITE(FRACDIV_L_ADDR(dev_base), 0xdb);    /* 0x54 = 0xdb */
		UARTHUB_REG_WRITE(FRACDIV_M_ADDR(dev_base), 0x1);     /* 0x58 = 0x1  */
	} else if (rate_index == 16000000) {
		/* TODO: support 16M baud rate */
	} else if (rate_index == 24000000) {
		/* TODO: support 24M baud rate */
	} else {
		pr_notice("[%s] not support rate_index(%d)\n", __func__, rate_index);
		return -1;
	}

	return 0;
}

int uarthub_config_host_baud_rate_mt6989(int dev_index, int rate_index)
{
	void __iomem *uarthub_dev_base = NULL;
	int iRtn = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		uarthub_dev_base = dev0_base_remap_addr_mt6989;
	else if (dev_index == 1)
		uarthub_dev_base = dev1_base_remap_addr_mt6989;
	else if (dev_index == 2)
		uarthub_dev_base = dev2_base_remap_addr_mt6989;

	iRtn = uarthub_config_baud_rate_m6989(uarthub_dev_base, rate_index);
	if (iRtn != 0) {
		pr_notice("[%s] config baud rate fail, dev_index=[%d], rate_index=[%d]\n",
			__func__, dev_index, rate_index);
		return -1;
	}

	return 0;
}

int uarthub_config_cmm_baud_rate_mt6989(int rate_index)
{
	int iRtn = 0;

	iRtn = uarthub_config_baud_rate_m6989(cmm_base_remap_addr_mt6989, rate_index);
	if (iRtn != 0) {
		pr_notice("[%s] config baud rate fail, rate_index=[%d]\n",
			__func__, rate_index);
		return -1;
	}

	return 0;
}

int uarthub_irq_mask_ctrl_mt6989(int mask)
{
	if (mask == 0)
		UARTHUB_REG_WRITE(DEV0_IRQ_MASK_ADDR, 0x0);
	else
		UARTHUB_REG_WRITE(DEV0_IRQ_MASK_ADDR, 0xFFFFFFFF);

	return 0;
}

int uarthub_irq_clear_ctrl_mt6989(int irq_type)
{
	if (irq_type < 0)
		UARTHUB_REG_WRITE(DEV0_IRQ_CLR_ADDR, 0xFFFFFFFF);
	else
		UARTHUB_SET_BIT(DEV0_IRQ_CLR_ADDR, irq_type);

	return 0;
}

#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA) || (UARTHUB_SUPPORT_DVT)
int uarthub_sspm_irq_clear_ctrl_mt6989(int irq_type)
{
	if (irq_type <= 0)
		UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	else
		UARTHUB_SET_BIT(IRQ_CLR_ADDR, irq_type);

	return 0;
}
#endif

#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA) || (UARTHUB_SUPPORT_DVT)
int uarthub_sspm_irq_get_sta_mt6989(void)
{
	int mask = 0;

	mask = IRQ_STA_VAL_intfhub_restore_req(1) |
		IRQ_STA_VAL_intfhub_ckoff_req(1) |
		IRQ_STA_VAL_intfhub_ckon_req(1);

	return UARTHUB_REG_READ_BIT(IRQ_STA_ADDR, mask);
}
#endif

#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA) || (UARTHUB_SUPPORT_DVT)
int uarthub_sspm_irq_mask_ctrl_mt6989(int irq_type, int is_mask)
{
	int mask = 0;

	mask = IRQ_STA_VAL_intfhub_restore_req(1) |
		IRQ_STA_VAL_intfhub_ckoff_req(1) |
		IRQ_STA_VAL_intfhub_ckon_req(1);

	if (irq_type <= 0) {
		UARTHUB_REG_WRITE_MASK(IRQ_MASK_ADDR,
			((is_mask == 1) ? mask : 0x0), mask);
	} else {
		UARTHUB_REG_WRITE_MASK(IRQ_MASK_ADDR,
			((is_mask == 1) ? irq_type : 0x0), (irq_type & mask));
	}

	return 0;
}
#endif

int uarthub_is_uarthub_clk_enable_mt6989(void)
{
	int state = 0;

	if (!pericfg_ao_remap_addr_mt6989) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	if (!apmixedsys_remap_addr_mt6989) {
		pr_notice("[%s] apmixedsys_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	if (!spm_remap_addr_mt6989) {
		pr_notice("[%s] spm_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	/* PERI_CG_1[25] = PERI_UARTHUB_pclk_CG */
	/* PERI_CG_1[24] = PERI_UARTHUB_hclk_CG */
	/* PERI_CG_1[23] = PERI_UARTHUB_CG */
	state = (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6989 + PERI_CG_1,
		PERI_CG_1_UARTHUB_CG_MASK) >> PERI_CG_1_UARTHUB_CG_SHIFT);

	if (state != 0x0) {
		pr_notice("[%s] UARTHUB CG gating(0x%x,exp:0x0)\n", __func__, state);
		return 0;
	}

	/* UNIVPLL_CON0[0] RG_UNIVPLL_EN */
	state = (UARTHUB_REG_READ_BIT(apmixedsys_remap_addr_mt6989 + UNIVPLL_CON0,
		UNIVPLL_CON0_UNIVPLL_EN_MASK) >> UNIVPLL_CON0_UNIVPLL_EN_SHIFT);

	if (state != 0x1) {
		pr_notice("[%s] UNIVPLL CLK is OFF(0x%x,exp:0x1)\n", __func__, state);
		return 0;
	}

	/* SPM_REQ_STA_9[21] = UART_HUB_VRF18_REQ */
	/* SPM_REQ_STA_9[20] = UART_HUB_VCORE_REQ */
	/* SPM_REQ_STA_9[19] = UART_HUB_SRCCLKENA */
	/* SPM_REQ_STA_9[18] = UART_HUB_PMIC_REQ */
	/* SPM_REQ_STA_9[17] = UART_HUB_INFRA_REQ */
	state = UARTHUB_REG_READ_BIT(spm_remap_addr_mt6989 + SPM_REQ_STA_9,
		SPM_REQ_STA_9_UARTHUB_REQ_MASK) >> SPM_REQ_STA_9_UARTHUB_REQ_SHIFT;

	if (state != 0x1D) {
		pr_notice("[%s] UARTHUB SPM REQ(0x%x,exp:0x1D)\n", __func__, state);
		return 0;
	}

	state = UARTHUB_REG_READ_BIT(spm_remap_addr_mt6989 + MD32PCM_SCU_CTRL1,
		MD32PCM_SCU_CTRL1_MASK) >> MD32PCM_SCU_CTRL1_SHIFT;

	if (state != 0x17) {
		pr_notice("[%s] UARTHUB SPM RES(0x%x,exp:0x17)\n", __func__, state);
		return 0;
	}

	state = CON1_GET_dev2_pkt_type_start(CON1_ADDR);

	if (state != 0x81) {
		pr_notice("[%s] UARTHUB pkt type start error(0x%x,exp:0x81)\n", __func__, state);
		return 0;
	}

	state = CON1_GET_dev2_pkt_type_end(CON1_ADDR);

	if (state != 0x85) {
		pr_notice("[%s] UARTHUB pkt type start error(0x%x,exp:0x85)\n", __func__, state);
		return 0;
	}

	state = DEV0_STA_GET_dev0_sw_rx_sta(DEV0_STA_ADDR) +
		DEV0_STA_GET_dev0_sw_tx_sta(DEV0_STA_ADDR);

	if (state == 0) {
		pr_notice("[%s] AP host clear the sw tx/rx req\n", __func__);
		return 0;
	}

	return 1;
}

int uarthub_set_host_loopback_ctrl_mt6989(int dev_index, int tx_to_rx, int enable)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (tx_to_rx == 0)
			LOOPBACK_SET_dev0_rx2tx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
		else
			LOOPBACK_SET_dev0_tx2rx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
	} else if (dev_index == 1) {
		if (tx_to_rx == 0)
			LOOPBACK_SET_dev1_rx2tx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
		else
			LOOPBACK_SET_dev1_tx2rx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
	} else if (dev_index == 2) {
		if (tx_to_rx == 0)
			LOOPBACK_SET_dev2_rx2tx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
		else
			LOOPBACK_SET_dev2_tx2rx_loopback(
				LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
	}

	return 0;
}

int uarthub_set_cmm_loopback_ctrl_mt6989(int tx_to_rx, int enable)
{
	if (tx_to_rx == 0)
		LOOPBACK_SET_dev_bt_rx2tx_loopback(LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));
	else
		LOOPBACK_SET_dev_cmm_tx2rx_loopback(LOOPBACK_ADDR, ((enable == 0) ? 0x0 : 0x1));

	return 0;
}

int uarthub_reset_to_ap_enable_only_mt6989(int ap_only)
{
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	int trx_mask = 0, trx_state = 0;
	int dev1_fifoe = -1, dev2_fifoe = -1;

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		if (dev0_sta == 0x300 ||  dev0_sta == 0x0) {
			pr_notice("[%s] all host sta is[0x%x]\n", __func__, dev0_sta);
			return -1;
		}
	}

	dev1_fifoe = FCR_RD_GET_FIFOE(dev1_base_remap_addr_mt6989);
	dev2_fifoe = FCR_RD_GET_FIFOE(dev2_base_remap_addr_mt6989);

	trx_mask = (DEV0_STA_SET_VAL_dev0_sw_rx_set(1) | DEV0_STA_SET_VAL_dev0_sw_tx_set(1));
	trx_state = UARTHUB_REG_READ_BIT(DEV0_STA_ADDR, trx_mask);

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] dev1/2_fifoe=[%d/%d], trx_state=[0x%x], trx_state=[0x%x]\n",
		__func__, dev1_fifoe, dev2_fifoe, trx_mask, trx_state);
#endif

	/* set trx request */
	UARTHUB_REG_WRITE(DEV0_STA_SET_ADDR, trx_mask);

	/* disable and clear uarthub FIFO for UART0/1/2/CMM */
	UARTHUB_REG_WRITE(FCR_ADDR(cmm_base_remap_addr_mt6989), 0x80);
	UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6989), 0x80);
#if UARTHUB_ENABLE_MD_CHANNEL
	UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6989), 0x80);
#endif
	UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6989), 0x80);

	/* sw_rst4 */
	CON4_SET_sw4_rst(CON4_ADDR, 1);

	/* enable uarthub FIFO for UART0/1/2/CMM */
	UARTHUB_REG_WRITE(FCR_ADDR(cmm_base_remap_addr_mt6989), 0x81);
	UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6989), 0x81);

#if UARTHUB_ENABLE_MD_CHANNEL
	if (dev1_fifoe == 1 && ap_only == 0)
		UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6989), 0x81);
#endif

	if (dev2_fifoe == 1 && ap_only == 0)
		UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6989), 0x81);

	/* restore trx request state */
	UARTHUB_REG_WRITE(DEV0_STA_SET_ADDR, trx_state);

	return 0;
}

int uarthub_reset_flow_control_mt6989(void)
{
	void __iomem *uarthub_dev_base = NULL;
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int val = 0;
	int retry = 0;
	int i = 0;

	debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(dev0_base_remap_addr_mt6989));
	debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(dev1_base_remap_addr_mt6989));
	debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(dev2_base_remap_addr_mt6989));
	debug8.cmm = UARTHUB_REG_READ(DEBUG_8(cmm_base_remap_addr_mt6989));

	if (((debug8.dev0 & 0x8) >> 3) == 0 && ((debug8.dev1 & 0x8) >> 3) == 0 &&
			((debug8.dev2 & 0x8) >> 3) == 0 && ((debug8.cmm & 0x8) >> 3) == 0)
		return 0;

	debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(dev0_base_remap_addr_mt6989));
	debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(dev1_base_remap_addr_mt6989));
	debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(dev2_base_remap_addr_mt6989));
	debug1.cmm = UARTHUB_REG_READ(DEBUG_1(cmm_base_remap_addr_mt6989));

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][BEGIN] xcstate(wsend_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		__func__,
		((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0),
		((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		", swtxdis(det_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
		((debug8.dev0 & 0x8) >> 3),
		((debug8.dev1 & 0x8) >> 3),
		((debug8.dev2 & 0x8) >> 3),
		((debug8.cmm & 0x8) >> 3));

	pr_info("%s\n", dmp_info_buf);

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++) {
		if (i != UARTHUB_MAX_NUM_DEV_HOST) {
			if (i == 0)
				uarthub_dev_base = dev0_base_remap_addr_mt6989;
			else if (i == 1)
				uarthub_dev_base = dev1_base_remap_addr_mt6989;
			else if (i == 2)
				uarthub_dev_base = dev2_base_remap_addr_mt6989;
		} else
			uarthub_dev_base = cmm_base_remap_addr_mt6989;

		retry = 20;
		while (retry-- > 0) {
			val = UARTHUB_REG_READ(DEBUG_1(uarthub_dev_base));
			if ((val & 0x1f) == 0x0)
				break;
			usleep_range(3, 4);
		}

		UARTHUB_REG_WRITE(MCR_ADDR(uarthub_dev_base), 0x10);
		UARTHUB_REG_WRITE(DMA_EN_ADDR(uarthub_dev_base), 0x0);
		UARTHUB_REG_WRITE(IIR_ADDR(uarthub_dev_base), 0x80);
		UARTHUB_REG_WRITE(SLEEP_REQ_ADDR(uarthub_dev_base), 0x1);
		UARTHUB_REG_WRITE(SLEEP_EN_ADDR(uarthub_dev_base), 0x1);

		retry = 20;
		while (retry-- > 0) {
			val = UARTHUB_REG_READ(DEBUG_1(uarthub_dev_base));
			if ((val & 0x1f) == 0x0)
				break;
			usleep_range(3, 4);
		}

		debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(dev0_base_remap_addr_mt6989));
		debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(dev1_base_remap_addr_mt6989));
		debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(dev2_base_remap_addr_mt6989));
		debug8.cmm = UARTHUB_REG_READ(DEBUG_8(cmm_base_remap_addr_mt6989));

		debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(dev0_base_remap_addr_mt6989));
		debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(dev1_base_remap_addr_mt6989));
		debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(dev2_base_remap_addr_mt6989));
		debug1.cmm = UARTHUB_REG_READ(DEBUG_1(cmm_base_remap_addr_mt6989));

		len = 0;
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][SLEEP_REQ][%d] xcstate(wsend_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
			__func__, i,
			((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0));

		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			", swtxdis(det_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
			((debug8.dev0 & 0x8) >> 3),
			((debug8.dev1 & 0x8) >> 3),
			((debug8.dev2 & 0x8) >> 3),
			((debug8.cmm & 0x8) >> 3));

		pr_info("%s\n", dmp_info_buf);

		UARTHUB_REG_WRITE(SLEEP_REQ_ADDR(uarthub_dev_base), 0x0);
		UARTHUB_REG_WRITE(SLEEP_EN_ADDR(uarthub_dev_base), 0x0);

		retry = 20;
		while (retry-- > 0) {
			val = UARTHUB_REG_READ(DEBUG_1(uarthub_dev_base));
			if ((val & 0x1f) == 0x0)
				break;
			usleep_range(3, 4);
		}

		UARTHUB_REG_WRITE(IIR_ADDR(uarthub_dev_base), 0x81);
		UARTHUB_REG_WRITE(DMA_EN_ADDR(uarthub_dev_base), 0x3);
		UARTHUB_REG_WRITE(MCR_ADDR(uarthub_dev_base), 0x0);

		debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(dev0_base_remap_addr_mt6989));
		debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(dev1_base_remap_addr_mt6989));
		debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(dev2_base_remap_addr_mt6989));
		debug8.cmm = UARTHUB_REG_READ(DEBUG_8(cmm_base_remap_addr_mt6989));

		debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(dev0_base_remap_addr_mt6989));
		debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(dev1_base_remap_addr_mt6989));
		debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(dev2_base_remap_addr_mt6989));
		debug1.cmm = UARTHUB_REG_READ(DEBUG_1(cmm_base_remap_addr_mt6989));

		len = 0;
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][SLEEP_REQ_DIS][%d] xcstate(wsend_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
			__func__, i,
			((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0),
			((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0));

		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			", swtxdis(det_xoff)=[d0:%d, d1:%d, d2:%d, cmm:%d]",
			((debug8.dev0 & 0x8) >> 3),
			((debug8.dev1 & 0x8) >> 3),
			((debug8.dev2 & 0x8) >> 3),
			((debug8.cmm & 0x8) >> 3));

		pr_info("%s\n", dmp_info_buf);
	}

	return 0;
}

int uarthub_is_assert_state_mt6989(void)
{
	int state = 0;

	state = (UARTHUB_REG_READ_BIT(DBG_CTRL_ADDR, (0x1 << 0)) >> 0);
	return state;
}

int uarthub_assert_state_ctrl_mt6989(int assert_ctrl)
{
	if (assert_ctrl == uarthub_is_assert_state_mt6989()) {
		if (assert_ctrl == 1)
			pr_info("[%s] assert state has been set\n", __func__);
		else
			pr_info("[%s] assert state has been cleared\n", __func__);
		return 0;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] assert_ctrl=[%d]\n", __func__, assert_ctrl);
#endif

	if (assert_ctrl == 1) {
		UARTHUB_SET_BIT(DBG_CTRL_ADDR, (0x1 << 0));
	} else {
		UARTHUB_CLR_BIT(DBG_CTRL_ADDR, (0x1 << 0));
		uarthub_irq_clear_ctrl_mt6989(-1);
		uarthub_irq_mask_ctrl_mt6989(0);
	}

	return 0;
}

int uarthub_is_bypass_mode_mt6989(void)
{
	int state = 0;

	state = CON2_GET_intfhub_bypass(CON2_ADDR);
	return state;
}

int uarthub_usb_rx_pin_ctrl_mt6989(void __iomem *dev_base, int enable)
{
	if (!dev_base) {
		pr_notice("[%s] dev_base is not been init\n", __func__);
		return -1;
	}

	UARTHUB_REG_WRITE(
		USB_RX_SEL_ADDR(dev_base), ((enable == 1) ? 0x1 : 0x0));
	return 0;
}

int uarthub_uarthub_init_mt6989(struct platform_device *pdev)
{
#if UARTHUB_CONFIG_TRX_GPIO
	if (!gpio_base_remap_addr_mt6989) {
		pr_notice("[%s] gpio_base_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}
#endif

#if UARTHUB_SUPPORT_UNIVPLL_CTRL
	if (pdev) {
		clk_apmixedsys_univpll_mt6989 = devm_clk_get(&pdev->dev, "univpll");
		if (IS_ERR(clk_apmixedsys_univpll_mt6989)) {
			pr_notice("[%s] cannot get clk_apmixedsys_univpll_mt6989 clock.\n",
				__func__);
			return PTR_ERR(clk_apmixedsys_univpll_mt6989);
		}
		pr_info("[%s] clk_apmixedsys_univpll_mt6989=[%p]\n",
			__func__, clk_apmixedsys_univpll_mt6989);

		mutex_init(&g_lock_univpll_clk_mt6989);
	}
#endif

#if UARTHUB_CONFIG_TRX_GPIO
	UARTHUB_REG_WRITE_MASK(gpio_base_remap_addr_mt6989 + GPIO_HUB_MODE_TX,
		GPIO_HUB_MODE_TX_VALUE, GPIO_HUB_MODE_TX_MASK);
	UARTHUB_REG_WRITE_MASK(gpio_base_remap_addr_mt6989 + GPIO_HUB_MODE_RX,
		GPIO_HUB_MODE_RX_VALUE, GPIO_HUB_MODE_RX_MASK);
#endif

	UARTHUB_SET_BIT(DBG_CTRL_ADDR, (0x1 << 0));
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
	CON2_SET_intfhub_bypass(CON2_ADDR, 0);
	CON2_SET_crc_en(CON2_ADDR, 1);
	uarthub_init_trx_timeout_mt6989();
#endif

	pr_info("[%s] assert=[0x%x], bypass=[0x%x], crc=[0x%x]\n",
		__func__, uarthub_is_assert_state_mt6989(),
		CON2_GET_intfhub_bypass(CON2_ADDR),
		CON2_GET_crc_en(CON2_ADDR));

#if UARTHUB_SUPPORT_FPGA
	uarthub_init_default_config_mt6989();
#if UARTHUB_SUPPORT_DX4_FPGA
	/* config dx4 wakeup/sleep flow */
	FEEDBACK_DATA1_SET_feedback_data1(FEEDBACK_DATA1_ADDR, 0x02FDFF01);
	CON3_SET_feedback_header_mode(CON3_ADDR, 0x0);
	STA0_SET_feedback_pkt_type(STA0_ADDR, 0x1);
	/* config semaphore timeout to max 5x ms */
	DEV0_SEMAPHORE_CON_SET_sema_own_timeout_set(DEV0_SEMAPHORE_CON_ADDR, 0xFF);
	/* mask sspm irq, feedback_host_awake_tx_done */
	IRQ_MASK_SET_feedback_host_awake_tx_done_mask(IRQ_MASK_ADDR, 1);
#endif
#endif

	return 0;
}

#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
int uarthub_init_default_config_mt6989(void)
{
	void __iomem *uarthub_dev_base = NULL;
	int baud_rate = 0;
	int i = 0;

	if (!dev0_base_remap_addr_mt6989 || !dev1_base_remap_addr_mt6989 ||
			!dev2_base_remap_addr_mt6989 || !cmm_base_remap_addr_mt6989) {
		pr_notice("[%s] dev0/1/2/cmm_base_remap_addr_mt6989 is not all init\n",
			__func__);
		return -1;
	}

	uarthub_usb_rx_pin_ctrl_mt6989(dev0_base_remap_addr_mt6989, 1);
	uarthub_usb_rx_pin_ctrl_mt6989(dev1_base_remap_addr_mt6989, 1);
	uarthub_usb_rx_pin_ctrl_mt6989(dev2_base_remap_addr_mt6989, 1);
	uarthub_usb_rx_pin_ctrl_mt6989(cmm_base_remap_addr_mt6989, 1);

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++) {
		if (i != UARTHUB_MAX_NUM_DEV_HOST) {
			if (i == 0) {
				uarthub_dev_base = dev0_base_remap_addr_mt6989;
				baud_rate = UARTHUB_DEV_0_BAUD_RATE;
			} else if (i == 1) {
				uarthub_dev_base = dev1_base_remap_addr_mt6989;
				baud_rate = UARTHUB_DEV_1_BAUD_RATE;
			} else if (i == 2) {
				uarthub_dev_base = dev2_base_remap_addr_mt6989;
				baud_rate = UARTHUB_DEV_2_BAUD_RATE;
			}
		} else {
			uarthub_dev_base = cmm_base_remap_addr_mt6989;
			baud_rate = UARTHUB_CMM_BAUD_RATE;
		}

		if (baud_rate >= 0)
			uarthub_config_baud_rate_m6989(uarthub_dev_base, baud_rate);

		/* 0x0c = 0x3,  byte length: 8 bit*/
		UARTHUB_REG_WRITE(LCR_ADDR(uarthub_dev_base), 0x3);
		/* 0x98 = 0xa,  xon1/xoff1 flow control enable */
		UARTHUB_REG_WRITE(EFR_ADDR(uarthub_dev_base), 0xa);
		/* 0xa8 = 0x13, xoff1 keyword */
		UARTHUB_REG_WRITE(XOFF1_ADDR(uarthub_dev_base), 0x13);
		/* 0xa0 = 0x11, xon1 keyword */
		UARTHUB_REG_WRITE(XON1_ADDR(uarthub_dev_base), 0x11);
		/* 0xac = 0x13, xoff2 keyword */
		UARTHUB_REG_WRITE(XOFF2_ADDR(uarthub_dev_base), 0x13);
		/* 0xa4 = 0x11, xon2 keyword */
		UARTHUB_REG_WRITE(XON2_ADDR(uarthub_dev_base), 0x11);
		/* 0x44 = 0x1,  esc char enable */
		UARTHUB_REG_WRITE(ESCAPE_EN_ADDR(uarthub_dev_base), 0x1);
		/* 0x40 = 0xdb, esc char */
		UARTHUB_REG_WRITE(ESCAPE_DAT_ADDR(uarthub_dev_base), 0xdb);
	}

	uarthub_usb_rx_pin_ctrl_mt6989(dev0_base_remap_addr_mt6989, 0);
	uarthub_usb_rx_pin_ctrl_mt6989(dev1_base_remap_addr_mt6989, 0);
	uarthub_usb_rx_pin_ctrl_mt6989(dev2_base_remap_addr_mt6989, 0);
	uarthub_usb_rx_pin_ctrl_mt6989(cmm_base_remap_addr_mt6989, 0);

	mdelay(2);

	/* 0x4c = 0x3,  rx/tx channel dma enable */
	UARTHUB_REG_WRITE(DMA_EN_ADDR(dev0_base_remap_addr_mt6989), 0x3);
	UARTHUB_REG_WRITE(DMA_EN_ADDR(dev1_base_remap_addr_mt6989), 0x3);
	UARTHUB_REG_WRITE(DMA_EN_ADDR(dev2_base_remap_addr_mt6989), 0x3);
	UARTHUB_REG_WRITE(DMA_EN_ADDR(cmm_base_remap_addr_mt6989), 0x3);

	/* 0x08 = 0x87, fifo control register */
	UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6989), 0x87);
	UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6989), 0x87);
	UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6989), 0x87);
	UARTHUB_REG_WRITE(FCR_ADDR(cmm_base_remap_addr_mt6989), 0x87);

	return 0;
}
#endif

#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
int uarthub_init_trx_timeout_mt6989(void)
{
	TIMEOUT_SET_dev0_tx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev1_tx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev2_tx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev0_rx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev1_rx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev2_rx_timeout_init_fsm_en(TIMEOUT_ADDR, 0x1);
	TIMEOUT_SET_dev_timeout_time(TIMEOUT_ADDR, 0xF);
	return 0;
}
#endif

int uarthub_init_remap_reg_mt6989(void)
{
	gpio_base_remap_addr_mt6989 = ioremap(GPIO_BASE_ADDR, 0x500);
	pericfg_ao_remap_addr_mt6989 = ioremap(PERICFG_AO_BASE_ADDR, 0x100);
	topckgen_base_remap_addr_mt6989 = ioremap(TOPCKGEN_BASE_ADDR, 0x100);
	dev0_base_remap_addr_mt6989 = ioremap(UARTHUB_DEV_0_BASE_ADDR, 0x100);
	dev1_base_remap_addr_mt6989 = ioremap(UARTHUB_DEV_1_BASE_ADDR, 0x100);
	dev2_base_remap_addr_mt6989 = ioremap(UARTHUB_DEV_2_BASE_ADDR, 0x100);
	cmm_base_remap_addr_mt6989 = ioremap(UARTHUB_CMM_BASE_ADDR, 0x100);
	intfhub_base_remap_addr_mt6989 = ioremap(UARTHUB_INTFHUB_BASE_ADDR, 0x100);
	apuart1_base_remap_addr_mt6989 = ioremap(UART1_BASE_ADDR, 0x100);
	apuart2_base_remap_addr_mt6989 = ioremap(UART2_BASE_ADDR, 0x100);
	apuart3_base_remap_addr_mt6989 = ioremap(UART3_BASE_ADDR, 0x100);
	apdma_uart_tx_int_remap_addr_mt6989 = ioremap(AP_DMA_UART_TX_INT_FLAG_ADDR, 0x100);
	spm_remap_addr_mt6989 = ioremap(SPM_BASE_ADDR, 0x1000);
	apmixedsys_remap_addr_mt6989 = ioremap(APMIXEDSYS_BASE_ADDR, 0x500);
	iocfg_rm_remap_addr_mt6989 = ioremap(IOCFG_RM_BASE_ADDR, 100);
	sys_sram_remap_addr_mt6989 = ioremap(SYS_SRAM_BASE_ADDR, 0x200);

#if UARTHUB_SUPPORT_DX4_FPGA
	INTFHUB_BASE_DX4_FPGA = (unsigned long) intfhub_base_remap_addr_mt6989;
	uarthub_core_open();
#else
	INTFHUB_BASE_MT6989 = (unsigned long) intfhub_base_remap_addr_mt6989;
#endif

	return 0;
}

int uarthub_deinit_unmap_reg_mt6989(void)
{
#if UARTHUB_SUPPORT_FPGA
	uarthub_core_close();
#endif

	if (gpio_base_remap_addr_mt6989)
		iounmap(gpio_base_remap_addr_mt6989);

	if (pericfg_ao_remap_addr_mt6989)
		iounmap(pericfg_ao_remap_addr_mt6989);

	if (topckgen_base_remap_addr_mt6989)
		iounmap(topckgen_base_remap_addr_mt6989);

	if (apuart1_base_remap_addr_mt6989)
		iounmap(apuart1_base_remap_addr_mt6989);

	if (apuart2_base_remap_addr_mt6989)
		iounmap(apuart2_base_remap_addr_mt6989);

	if (apuart3_base_remap_addr_mt6989)
		iounmap(apuart3_base_remap_addr_mt6989);

	if (apdma_uart_tx_int_remap_addr_mt6989)
		iounmap(apdma_uart_tx_int_remap_addr_mt6989);

	if (spm_remap_addr_mt6989)
		iounmap(spm_remap_addr_mt6989);

	if (apmixedsys_remap_addr_mt6989)
		iounmap(apmixedsys_remap_addr_mt6989);

	if (iocfg_rm_remap_addr_mt6989)
		iounmap(iocfg_rm_remap_addr_mt6989);

	if (sys_sram_remap_addr_mt6989)
		iounmap(sys_sram_remap_addr_mt6989);

	return 0;
}

int uarthub_get_spm_sys_timer_mt6989(uint32_t *hi, uint32_t *lo)
{
	if (hi == NULL || lo == NULL) {
		pr_notice("[%s] invalid argument\n", __func__);
		return -1;
	}

	if (!spm_remap_addr_mt6989) {
		pr_notice("[%s] spm_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	*hi = UARTHUB_REG_READ(spm_remap_addr_mt6989 + SPM_SYS_TIMER_H);
	*lo = UARTHUB_REG_READ(spm_remap_addr_mt6989 + SPM_SYS_TIMER_L);

	return 1;
}

int uarthub_get_host_status_mt6989(int dev_index)
{
	int state = 0;

	if (dev_index == 0)
		state = UARTHUB_REG_READ(DEV0_STA_ADDR);
	else if (dev_index == 1)
		state = UARTHUB_REG_READ(DEV1_STA_ADDR);
	else if (dev_index == 2)
		state = UARTHUB_REG_READ(DEV2_STA_ADDR);

	return state;
}

int uarthub_get_host_wakeup_status_mt6989(void)
{
	int state = 0;
	int state0 = 0, state1 = 0, state2 = 0;

	state0 = ((UARTHUB_REG_READ_BIT(DEV0_STA_ADDR, 0x3) == 0x2) ? 1 : 0);
	state1 = ((UARTHUB_REG_READ_BIT(DEV1_STA_ADDR, 0x3) == 0x2) ? 1 : 0);
	state2 = ((UARTHUB_REG_READ_BIT(DEV2_STA_ADDR, 0x3) == 0x2) ? 1 : 0);

	state = (state0 | (state1 << 1) | (state2 << 2));
	return state;
}

int uarthub_get_host_set_fw_own_status_mt6989(void)
{
	int state = 0;
	int state0 = 0, state1 = 0, state2 = 0;

	state0 = ((UARTHUB_REG_READ_BIT(DEV0_STA_ADDR, 0x3) == 0x1) ? 1 : 0);
	state1 = ((UARTHUB_REG_READ_BIT(DEV1_STA_ADDR, 0x3) == 0x1) ? 1 : 0);
	state2 = ((UARTHUB_REG_READ_BIT(DEV2_STA_ADDR, 0x3) == 0x1) ? 1 : 0);

	state = (state0 | (state1 << 1) | (state2 << 2));
	return state;
}

int uarthub_is_host_trx_idle_mt6989(int dev_index, enum uarthub_trx_type trx)
{
	int state = -1;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (trx == RX) {
			state = DEV0_STA_GET_dev0_sw_rx_sta(DEV0_STA_ADDR);
		} else if (trx == TX) {
			state = DEV0_STA_GET_dev0_sw_tx_sta(DEV0_STA_ADDR);
		} else {
			state = (DEV0_STA_GET_dev0_sw_tx_sta(DEV0_STA_ADDR) << 1) |
				DEV0_STA_GET_dev0_sw_rx_sta(DEV0_STA_ADDR);
		}
	} else if (dev_index == 1) {
		if (trx == RX) {
			state = DEV1_STA_GET_dev1_sw_rx_sta(DEV1_STA_ADDR);
		} else if (trx == TX) {
			state = DEV1_STA_GET_dev1_sw_tx_sta(DEV1_STA_ADDR);
		} else {
			state = (DEV1_STA_GET_dev1_sw_tx_sta(DEV1_STA_ADDR) << 1) |
				DEV1_STA_GET_dev1_sw_rx_sta(DEV1_STA_ADDR);
		}
	} else if (dev_index == 2) {
		if (trx == RX) {
			state = DEV2_STA_GET_dev2_sw_rx_sta(DEV2_STA_ADDR);
		} else if (trx == TX) {
			state = DEV2_STA_GET_dev2_sw_tx_sta(DEV2_STA_ADDR);
		} else {
			state = (DEV2_STA_GET_dev2_sw_tx_sta(DEV2_STA_ADDR) << 1) |
				DEV2_STA_GET_dev2_sw_rx_sta(DEV2_STA_ADDR);
		}
	}

	return state;
}

int uarthub_set_host_trx_request_mt6989(int dev_index, enum uarthub_trx_type trx)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (trx == RX) {
			DEV0_STA_SET_SET_dev0_sw_rx_set(DEV0_STA_SET_ADDR, 1);
		} else if (trx == TX) {
			DEV0_STA_SET_SET_dev0_sw_tx_set(DEV0_STA_SET_ADDR, 1);
		} else {
			UARTHUB_REG_WRITE(DEV0_STA_SET_ADDR,
				(DEV0_STA_SET_VAL_dev0_sw_rx_set(1) |
				DEV0_STA_SET_VAL_dev0_sw_tx_set(1)));
		}
	} else if (dev_index == 1) {
		if (trx == RX) {
			DEV1_STA_SET_SET_dev1_sw_rx_set(DEV1_STA_SET_ADDR, 1);
		} else if (trx == TX) {
			DEV1_STA_SET_SET_dev1_sw_tx_set(DEV1_STA_SET_ADDR, 1);
		} else {
			UARTHUB_REG_WRITE(DEV1_STA_SET_ADDR,
				(DEV1_STA_SET_VAL_dev1_sw_rx_set(1) |
				DEV1_STA_SET_VAL_dev1_sw_tx_set(1)));
		}
	} else if (dev_index == 2) {
		if (trx == RX) {
			DEV2_STA_SET_SET_dev2_sw_rx_set(DEV2_STA_SET_ADDR, 1);
		} else if (trx == TX) {
			DEV2_STA_SET_SET_dev2_sw_tx_set(DEV2_STA_SET_ADDR, 1);
		} else {
			UARTHUB_REG_WRITE(DEV2_STA_SET_ADDR,
				(DEV2_STA_SET_VAL_dev2_sw_rx_set(1) |
				DEV2_STA_SET_VAL_dev2_sw_tx_set(1)));
		}
	}

	return 0;
}

int uarthub_clear_host_trx_request_mt6989(int dev_index, enum uarthub_trx_type trx)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (trx == RX) {
			UARTHUB_REG_WRITE(DEV0_STA_CLR_ADDR,
				(DEV0_STA_CLR_VAL_dev0_sw_rx_clr(1) |
				DEV0_STA_CLR_VAL_dev0_hw_rx_clr(1) |
				DEV0_STA_CLR_VAL_dev0_rx_data_req_clr(1)));
		} else if (trx == TX) {
			UARTHUB_REG_WRITE(DEV0_STA_CLR_ADDR,
				DEV0_STA_CLR_VAL_dev0_sw_tx_clr(1));
		} else {
			UARTHUB_REG_WRITE(DEV0_STA_CLR_ADDR,
				(DEV0_STA_CLR_VAL_dev0_sw_rx_clr(1) |
				DEV0_STA_CLR_VAL_dev0_sw_tx_clr(1) |
				DEV0_STA_CLR_VAL_dev0_hw_rx_clr(1) |
				DEV0_STA_CLR_VAL_dev0_rx_data_req_clr(1)));
		}
	} else if (dev_index == 1) {
		if (trx == RX) {
			UARTHUB_REG_WRITE(DEV1_STA_CLR_ADDR,
				(DEV1_STA_CLR_VAL_dev1_sw_rx_clr(1) |
				DEV1_STA_CLR_VAL_dev1_hw_rx_clr(1) |
				DEV1_STA_CLR_VAL_dev1_rx_data_req_clr(1)));
		} else if (trx == TX) {
			UARTHUB_REG_WRITE(DEV1_STA_CLR_ADDR,
				DEV1_STA_CLR_VAL_dev1_sw_tx_clr(1));
		} else {
			UARTHUB_REG_WRITE(DEV1_STA_CLR_ADDR,
				(DEV1_STA_CLR_VAL_dev1_sw_rx_clr(1) |
				DEV1_STA_CLR_VAL_dev1_sw_tx_clr(1) |
				DEV1_STA_CLR_VAL_dev1_hw_rx_clr(1) |
				DEV1_STA_CLR_VAL_dev1_rx_data_req_clr(1)));
		}
	} else if (dev_index == 2) {
		if (trx == RX) {
			UARTHUB_REG_WRITE(DEV2_STA_CLR_ADDR,
				(DEV2_STA_CLR_VAL_dev2_sw_rx_clr(1) |
				DEV2_STA_CLR_VAL_dev2_hw_rx_clr(1) |
				DEV2_STA_CLR_VAL_dev2_rx_data_req_clr(1)));
		} else if (trx == TX) {
			UARTHUB_REG_WRITE(DEV2_STA_CLR_ADDR,
				DEV2_STA_CLR_VAL_dev2_sw_tx_clr(1));
		} else {
			UARTHUB_REG_WRITE(DEV2_STA_CLR_ADDR,
				(DEV2_STA_CLR_VAL_dev2_sw_rx_clr(1) |
				DEV2_STA_CLR_VAL_dev2_sw_tx_clr(1) |
				DEV2_STA_CLR_VAL_dev2_hw_rx_clr(1) |
				DEV2_STA_CLR_VAL_dev2_rx_data_req_clr(1)));
		}
	}

	return 0;
}

int uarthub_get_host_byte_cnt_mt6989(int dev_index, enum uarthub_trx_type trx)
{
	int byte_cnt = -1;
	void __iomem *uarthub_dev_base = NULL;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (trx == TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (dev_index == 0)
		uarthub_dev_base = dev0_base_remap_addr_mt6989;
	else if (dev_index == 1)
		uarthub_dev_base = dev1_base_remap_addr_mt6989;
	else if (dev_index == 2)
		uarthub_dev_base = dev2_base_remap_addr_mt6989;

	if (uarthub_dev_base != NULL) {
		if (trx == RX)
			byte_cnt = UARTHUB_DEBUG_GET_OP_RX_REQ_VAL(uarthub_dev_base);
		else if (trx == TX)
			byte_cnt = UARTHUB_DEBUG_GET_IP_TX_DMA_VAL(uarthub_dev_base);
	}

	return byte_cnt;
}

int uarthub_get_cmm_byte_cnt_mt6989(enum uarthub_trx_type trx)
{
	int byte_cnt = -1;

	if (trx == TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return -1;
	}

	if (trx == RX)
		byte_cnt = UARTHUB_DEBUG_GET_OP_RX_REQ_VAL(cmm_base_remap_addr_mt6989);
	else if (trx == TX)
		byte_cnt = UARTHUB_DEBUG_GET_IP_TX_DMA_VAL(cmm_base_remap_addr_mt6989);

	return byte_cnt;
}

int uarthub_config_crc_ctrl_mt6989(int enable)
{
	CON2_SET_crc_en(CON2_ADDR, enable);
	return 0;
}

int uarthub_config_bypass_ctrl_mt6989(int enable)
{
	CON2_SET_intfhub_bypass(CON2_ADDR, enable);
	return 0;
}

int uarthub_config_host_fifoe_ctrl_mt6989(int dev_index, int enable)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6989), (0x80 | enable));
#if UARTHUB_ENABLE_MD_CHANNEL
	else if (dev_index == 1)
		UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6989), (0x80 | enable));
#endif
	else if (dev_index == 2)
		UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6989), (0x80 | enable));

	return 0;
}

int uarthub_get_rx_error_crc_info_mt6989(int dev_index, int *p_crc_error_data, int *p_crc_result)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (p_crc_error_data)
			*p_crc_error_data = DEV0_RX_ERR_CRC_STA_GET_dev0_rx_err_crc_data(
				DEV0_RX_ERR_CRC_STA_ADDR);
		if (p_crc_result)
			*p_crc_result = DEV0_RX_ERR_CRC_STA_GET_dev0_rx_err_crc_result(
				DEV0_RX_ERR_CRC_STA_ADDR);
	} else if (dev_index == 1) {
		if (p_crc_error_data)
			*p_crc_error_data = DEV1_RX_ERR_CRC_STA_GET_dev1_rx_err_crc_data(
				DEV1_RX_ERR_CRC_STA_ADDR);
		if (p_crc_result)
			*p_crc_result = DEV1_RX_ERR_CRC_STA_GET_dev1_rx_err_crc_result(
				DEV1_RX_ERR_CRC_STA_ADDR);
	} else if (dev_index == 2) {
		if (p_crc_error_data)
			*p_crc_error_data = DEV2_RX_ERR_CRC_STA_GET_dev2_rx_err_crc_data(
				DEV2_RX_ERR_CRC_STA_ADDR);
		if (p_crc_result)
			*p_crc_result = DEV2_RX_ERR_CRC_STA_GET_dev2_rx_err_crc_result(
				DEV2_RX_ERR_CRC_STA_ADDR);
	}

	return 0;
}

int uarthub_get_trx_timeout_info_mt6989(
	int dev_index, enum uarthub_trx_type trx,
	int *p_timeout_counter, int *p_pkt_counter)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (trx == TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (trx == RX) {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV0_PKT_CNT_GET_dev0_rx_timeout_cnt(DEV0_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV0_PKT_CNT_GET_dev0_rx_pkt_cnt(DEV0_PKT_CNT_ADDR);
		} else {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV0_PKT_CNT_GET_dev0_tx_timeout_cnt(DEV0_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV0_PKT_CNT_GET_dev0_tx_pkt_cnt(DEV0_PKT_CNT_ADDR);
		}
	} else if (dev_index == 1) {
		if (trx == RX) {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV1_PKT_CNT_GET_dev1_rx_timeout_cnt(DEV1_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV1_PKT_CNT_GET_dev1_rx_pkt_cnt(DEV1_PKT_CNT_ADDR);
		} else {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV1_PKT_CNT_GET_dev1_tx_timeout_cnt(DEV1_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV1_PKT_CNT_GET_dev1_tx_pkt_cnt(DEV1_PKT_CNT_ADDR);
		}
	} else if (dev_index == 2) {
		if (trx == RX) {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV2_PKT_CNT_GET_dev2_rx_timeout_cnt(DEV2_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV2_PKT_CNT_GET_dev2_rx_pkt_cnt(DEV2_PKT_CNT_ADDR);
		} else {
			if (p_timeout_counter)
				*p_timeout_counter =
					DEV2_PKT_CNT_GET_dev2_tx_timeout_cnt(DEV2_PKT_CNT_ADDR);
			if (p_pkt_counter)
				*p_pkt_counter =
					DEV2_PKT_CNT_GET_dev2_tx_pkt_cnt(DEV2_PKT_CNT_ADDR);
		}
	}

	return 0;
}

int uarthub_get_irq_err_type_mt6989(void)
{
	return UARTHUB_REG_READ_BIT(DEV0_IRQ_STA_ADDR, 0x7FFFFFFF);
}
