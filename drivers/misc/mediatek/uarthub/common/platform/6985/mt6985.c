// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_core.h"
#include "uarthub_drv_export.h"
#include "common_def_id.h"

#include "inc/mt6985.h"
#include "inc/INTFHUB_c_header.h"
#include "inc/UARTHUB_UART0_c_header.h"

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

#define UARTHUB_SUPPORT_SSPM_DRIVER 1
#define UARTHUB_CONFIG_TRX_GPIO 0
#define UARTHUB_ENABLE_MD_CHANNEL 1
#define UARTHUB_ENABLE_DUMP_APUART_DBG_INFO 0

void __iomem *gpio_base_remap_addr_mt6985;
void __iomem *pericfg_ao_remap_addr_mt6985;
void __iomem *topckgen_base_remap_addr_mt6985;
void __iomem *uarthub_base_remap_addr_mt6985;
void __iomem *dev0_base_remap_addr_mt6985;
void __iomem *dev1_base_remap_addr_mt6985;
void __iomem *dev2_base_remap_addr_mt6985;
void __iomem *cmm_base_remap_addr_mt6985;
void __iomem *intfhub_base_remap_addr_mt6985;
void __iomem *apuart3_base_remap_addr_mt6985;
void __iomem *apdma_uart_tx_int_remap_addr_mt6985;
void __iomem *spm_remap_addr_mt6985;
void __iomem *apmixedsys_remap_addr_mt6985;
void __iomem *iocfg_rm_remap_addr_mt6985;
void __iomem *sys_sram_remap_addr_mt6985;

unsigned long INTFHUB_BASE_MT6985;

struct clk *clk_apmixedsys_univpll_mt6985;
struct mutex g_lock_univpll_clk_mt6985;
static int g_univpll_clk_ref_count_mt6985;
static int g_enable_apuart_debug_info_mt6985;

static int uarthub_is_ready_state_mt6985(void);
static int uarthub_uarthub_init_mt6985(struct platform_device *pdev);
static int uarthub_config_host_baud_rate_mt6985(int dev_index, int rate_index);
static int uarthub_config_cmm_baud_rate_mt6985(int rate_index);
static int uarthub_irq_mask_ctrl_mt6985(int mask);
static int uarthub_irq_clear_ctrl_mt6985(int irq_type);
static int uarthub_is_apb_bus_clk_enable_mt6985(void);
static int uarthub_is_uarthub_clk_enable_mt6985(void);
static int uarthub_set_host_loopback_ctrl_mt6985(int dev_index, int tx_to_rx, int enable);
static int uarthub_reset_to_ap_enable_only_mt6985(int ap_only);
static int uarthub_reset_flow_control_mt6985(void);
static int uarthub_is_assert_state_mt6985(void);
static int uarthub_assert_state_ctrl_mt6985(int assert_ctrl);
static int uarthub_is_bypass_mode_mt6985(void);
static int uarthub_dump_uartip_debug_info_mt6985(
	const char *tag, struct mutex *uartip_lock);
static int uarthub_dump_intfhub_debug_info_mt6985(const char *tag);
static int uarthub_dump_debug_tx_rx_count_mt6985(const char *tag, int trigger_point);
static int uarthub_dump_debug_clk_info_mt6985(const char *tag);
static int uarthub_dump_debug_byte_cnt_info_mt6985(const char *tag);
static int uarthub_dump_debug_apdma_uart_info_mt6985(const char *tag);
static int uarthub_dump_sspm_log_mt6985(const char *tag);
static int uarthub_get_host_status_mt6985(int dev_index);
static int uarthub_get_host_wakeup_status_mt6985(void);
static int uarthub_get_host_set_fw_own_status_mt6985(void);
static int uarthub_is_host_trx_idle_mt6985(int dev_index, enum uarthub_trx_type trx);
static int uarthub_set_host_trx_request_mt6985(int dev_index, enum uarthub_trx_type trx);
static int uarthub_clear_host_trx_request_mt6985(int dev_index, enum uarthub_trx_type trx);
static int uarthub_get_host_byte_cnt_mt6985(int dev_index, enum uarthub_trx_type trx);
static int uarthub_get_cmm_byte_cnt_mt6985(enum uarthub_trx_type trx);
static int uarthub_config_crc_ctrl_mt6985(int enable);
static int uarthub_config_bypass_ctrl_mt6985(int enable);
static int uarthub_config_host_fifoe_ctrl_mt6985(int dev_index, int enable);
static int uarthub_get_rx_error_crc_info_mt6985(
	int dev_index, int *p_crc_error_data, int *p_crc_result);
static int uarthub_get_trx_timeout_info_mt6985(
	int dev_index, enum uarthub_trx_type trx,
	int *p_timeout_counter, int *p_pkt_counter);
static int uarthub_get_irq_err_type_mt6985(void);
static int uarthub_init_remap_reg_mt6985(void);
static int uarthub_deinit_unmap_reg_mt6985(void);
static int uarthub_get_hwccf_univpll_on_info_mt6985(void);
static int uarthub_get_spm_sys_timer_mt6985(uint32_t *hi, uint32_t *lo);
static int uarthub_dump_apuart_debug_ctrl_mt6985(int enable);
static int uarthub_get_apuart_debug_ctrl_sta_mt6985(void);
static int uarthub_get_intfhub_base_addr_mt6985(void);
static int uarthub_get_uartip_base_addr_mt6985(int dev_index);
static int uarthub_trigger_dvt_testing_mt6985(int type);

#if !(UARTHUB_SUPPORT_SSPM_DRIVER)
static int uarthub_init_default_config_mt6985(void);
static int uarthub_sspm_irq_clear_ctrl_mt6985(int irq_type);
static int uarthub_init_trx_timeout_mt6985(void);
#endif

/* internal function */
static int uarthub_config_baud_rate_m6985(void __iomem *dev_base, int rate_index);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER)
static int uarthub_usb_rx_pin_ctrl_mt6985(void __iomem *dev_base, int enable);
#endif
static int uarthub_get_uart_mux_info_mt6985(void);
static int uarthub_get_uarthub_cg_info_mt6985(void);
static int uarthub_get_peri_uart_pad_mode_mt6985(void);
static int uarthub_get_peri_clk_info_mt6985(void);
static int uarthub_get_spm_res_info_mt6985(unsigned int *pspm_res1, unsigned int *pspm_res2);
static int uarthub_get_gpio_trx_info_mt6985(struct uarthub_gpio_trx_info *info);
static int uarthub_clk_univpll_ctrl_mt6985(int clk_on);

struct uarthub_core_ops_struct mt6985_plat_core_data = {
	.uarthub_plat_is_apb_bus_clk_enable = uarthub_is_apb_bus_clk_enable_mt6985,
	.uarthub_plat_get_hwccf_univpll_on_info = uarthub_get_hwccf_univpll_on_info_mt6985,
	.uarthub_plat_is_ready_state = uarthub_is_ready_state_mt6985,
	.uarthub_plat_uarthub_init = uarthub_uarthub_init_mt6985,
	.uarthub_plat_config_host_baud_rate = uarthub_config_host_baud_rate_mt6985,
	.uarthub_plat_config_cmm_baud_rate = uarthub_config_cmm_baud_rate_mt6985,
	.uarthub_plat_irq_mask_ctrl = uarthub_irq_mask_ctrl_mt6985,
	.uarthub_plat_irq_clear_ctrl = uarthub_irq_clear_ctrl_mt6985,
	.uarthub_plat_is_uarthub_clk_enable = uarthub_is_uarthub_clk_enable_mt6985,
	.uarthub_plat_set_host_loopback_ctrl = uarthub_set_host_loopback_ctrl_mt6985,
	.uarthub_plat_reset_to_ap_enable_only = uarthub_reset_to_ap_enable_only_mt6985,
	.uarthub_plat_reset_flow_control = uarthub_reset_flow_control_mt6985,
	.uarthub_plat_is_assert_state = uarthub_is_assert_state_mt6985,
	.uarthub_plat_assert_state_ctrl = uarthub_assert_state_ctrl_mt6985,
	.uarthub_plat_is_bypass_mode = uarthub_is_bypass_mode_mt6985,
	.uarthub_plat_get_host_status = uarthub_get_host_status_mt6985,
	.uarthub_plat_get_host_wakeup_status = uarthub_get_host_wakeup_status_mt6985,
	.uarthub_plat_get_host_set_fw_own_status = uarthub_get_host_set_fw_own_status_mt6985,
	.uarthub_plat_is_host_trx_idle = uarthub_is_host_trx_idle_mt6985,
	.uarthub_plat_set_host_trx_request = uarthub_set_host_trx_request_mt6985,
	.uarthub_plat_clear_host_trx_request = uarthub_clear_host_trx_request_mt6985,
	.uarthub_plat_get_host_byte_cnt = uarthub_get_host_byte_cnt_mt6985,
	.uarthub_plat_get_cmm_byte_cnt = uarthub_get_cmm_byte_cnt_mt6985,
	.uarthub_plat_config_crc_ctrl = uarthub_config_crc_ctrl_mt6985,
	.uarthub_plat_config_bypass_ctrl = uarthub_config_bypass_ctrl_mt6985,
	.uarthub_plat_config_host_fifoe_ctrl = uarthub_config_host_fifoe_ctrl_mt6985,
	.uarthub_plat_get_rx_error_crc_info = uarthub_get_rx_error_crc_info_mt6985,
	.uarthub_plat_get_trx_timeout_info = uarthub_get_trx_timeout_info_mt6985,
	.uarthub_plat_get_irq_err_type = uarthub_get_irq_err_type_mt6985,
	.uarthub_plat_init_remap_reg = uarthub_init_remap_reg_mt6985,
	.uarthub_plat_deinit_unmap_reg = uarthub_deinit_unmap_reg_mt6985,
	.uarthub_plat_get_spm_sys_timer = uarthub_get_spm_sys_timer_mt6985,
#if !(UARTHUB_SUPPORT_SSPM_DRIVER)
	.uarthub_plat_sspm_irq_clear_ctrl = uarthub_sspm_irq_clear_ctrl_mt6985,
#endif
};

struct uarthub_debug_ops_struct mt6985_plat_debug_data = {
	.uarthub_plat_dump_apuart_debug_ctrl = uarthub_dump_apuart_debug_ctrl_mt6985,
	.uarthub_plat_get_apuart_debug_ctrl_sta = uarthub_get_apuart_debug_ctrl_sta_mt6985,
	.uarthub_plat_get_intfhub_base_addr = uarthub_get_intfhub_base_addr_mt6985,
	.uarthub_plat_get_uartip_base_addr = uarthub_get_uartip_base_addr_mt6985,
	.uarthub_plat_dump_uartip_debug_info = uarthub_dump_uartip_debug_info_mt6985,
	.uarthub_plat_dump_intfhub_debug_info = uarthub_dump_intfhub_debug_info_mt6985,
	.uarthub_plat_dump_debug_tx_rx_count = uarthub_dump_debug_tx_rx_count_mt6985,
	.uarthub_plat_dump_debug_clk_info = uarthub_dump_debug_clk_info_mt6985,
	.uarthub_plat_dump_debug_byte_cnt_info = uarthub_dump_debug_byte_cnt_info_mt6985,
	.uarthub_plat_dump_debug_apdma_uart_info = uarthub_dump_debug_apdma_uart_info_mt6985,
	.uarthub_plat_dump_sspm_log = uarthub_dump_sspm_log_mt6985,
	.uarthub_plat_trigger_dvt_testing = uarthub_trigger_dvt_testing_mt6985,
};

struct uarthub_ops_struct mt6985_plat_data = {
	.core_ops = &mt6985_plat_core_data,
	.debug_ops = &mt6985_plat_debug_data,
};

int uarthub_is_ready_state_mt6985(void)
{
	return DEV0_STA_GET_dev0_intfhub_ready(DEV0_STA_ADDR);
}

int uarthub_config_baud_rate_m6985(void __iomem *dev_base, int rate_index)
{
	if (!dev_base) {
		pr_notice("[%s] dev_base is not been init\n", __func__);
		return -1;
	}

	if (rate_index == 115200) {
		UARTHUB_REG_WRITE(FEATURE_SEL_ADDR(dev_base), 0x1);   /* 0x9c = 0x1  */
		UARTHUB_REG_WRITE(HIGHSPEED_ADDR(dev_base), 0x3);     /* 0x24 = 0x3  */
		UARTHUB_REG_WRITE(SAMPLE_COUNT_ADDR(dev_base), 0xe0); /* 0x28 = 0xe0 */
		UARTHUB_REG_WRITE(SAMPLE_POINT_ADDR(dev_base), 0x70); /* 0x2c = 0x70 */
		UARTHUB_REG_WRITE(DLL_ADDR(dev_base), 0x4);           /* 0x90 = 0x4  */
		UARTHUB_REG_WRITE(FRACDIV_L_ADDR(dev_base), 0xf6);    /* 0x54 = 0xf6 */
		UARTHUB_REG_WRITE(FRACDIV_M_ADDR(dev_base), 0x1);     /* 0x58 = 0x1  */
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

int uarthub_config_host_baud_rate_mt6985(int dev_index, int rate_index)
{
	void __iomem *uarthub_dev_base = NULL;
	int iRtn = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		uarthub_dev_base = dev0_base_remap_addr_mt6985;
	else if (dev_index == 1)
		uarthub_dev_base = dev1_base_remap_addr_mt6985;
	else if (dev_index == 2)
		uarthub_dev_base = dev2_base_remap_addr_mt6985;

	iRtn = uarthub_config_baud_rate_m6985(uarthub_dev_base, rate_index);
	if (iRtn != 0) {
		pr_notice("[%s] config baud rate fail, dev_index=[%d], rate_index=[%d]\n",
			__func__, dev_index, rate_index);
		return -1;
	}

	return 0;
}

int uarthub_config_cmm_baud_rate_mt6985(int rate_index)
{
	int iRtn = 0;

	iRtn = uarthub_config_baud_rate_m6985(cmm_base_remap_addr_mt6985, rate_index);
	if (iRtn != 0) {
		pr_notice("[%s] config baud rate fail, rate_index=[%d]\n",
			__func__, rate_index);
		return -1;
	}

	return 0;
}

int uarthub_irq_mask_ctrl_mt6985(int mask)
{
	if (mask == 0)
		UARTHUB_REG_WRITE(DEV0_IRQ_MASK_ADDR, 0x0);
	else
		UARTHUB_REG_WRITE(DEV0_IRQ_MASK_ADDR, 0xFFFFFFFF);

	return 0;
}

int uarthub_irq_clear_ctrl_mt6985(int irq_type)
{
	UARTHUB_SET_BIT(DEV0_IRQ_CLR_ADDR, irq_type);
	return 0;
}

#if !(UARTHUB_SUPPORT_SSPM_DRIVER)
int uarthub_sspm_irq_clear_ctrl_mt6985(int irq_type)
{
	UARTHUB_SET_BIT(IRQ_CLR_ADDR, irq_type);
	return 0;
}
#endif

int uarthub_is_apb_bus_clk_enable_mt6985(void)
{
	int state = 0;

	if (!pericfg_ao_remap_addr_mt6985) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	/* PERI_CG_1[25] = PERI_UARTHUB_pclk_CG */
	/* PERI_CG_1[24] = PERI_UARTHUB_hclk_CG */
	state = (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6985 + PERI_CG_1,
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

int uarthub_is_uarthub_clk_enable_mt6985(void)
{
	int state = 0;

	if (!pericfg_ao_remap_addr_mt6985) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	if (!apmixedsys_remap_addr_mt6985) {
		pr_notice("[%s] apmixedsys_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	if (!spm_remap_addr_mt6985) {
		pr_notice("[%s] spm_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	/* PERI_CG_1[25] = PERI_UARTHUB_pclk_CG */
	/* PERI_CG_1[24] = PERI_UARTHUB_hclk_CG */
	/* PERI_CG_1[23] = PERI_UARTHUB_CG */
	state = (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6985 + PERI_CG_1,
		PERI_CG_1_UARTHUB_CG_MASK) >> PERI_CG_1_UARTHUB_CG_SHIFT);

	if (state != 0x0) {
		pr_notice("[%s] UARTHUB CG gating(0x%x,exp:0x0)\n", __func__, state);
		return 0;
	}

	/* UNIVPLL_CON0[0] RG_UNIVPLL_EN */
	state = (UARTHUB_REG_READ_BIT(apmixedsys_remap_addr_mt6985 + UNIVPLL_CON0,
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
	state = UARTHUB_REG_READ_BIT(spm_remap_addr_mt6985 + SPM_REQ_STA_9,
		SPM_REQ_STA_9_UARTHUB_REQ_MASK) >> SPM_REQ_STA_9_UARTHUB_REQ_SHIFT;

	if (state != 0x1D) {
		pr_notice("[%s] UARTHUB SPM REQ(0x%x,exp:0x1D)\n", __func__, state);
		return 0;
	}

	state = UARTHUB_REG_READ_BIT(spm_remap_addr_mt6985 + MD32PCM_SCU_CTRL1,
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

int uarthub_set_host_loopback_ctrl_mt6985(int dev_index, int tx_to_rx, int enable)
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

int uarthub_reset_to_ap_enable_only_mt6985(int ap_only)
{
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	int trx_mask = 0, trx_state = 0;
	int dev1_fifoe = -1, dev2_fifoe = -1;
	int i = 0;

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		if (dev0_sta == 0x300 ||  dev0_sta == 0x0) {
			pr_notice("[%s] all host sta is[0x%x]\n", __func__, dev0_sta);
			return -1;
		}
	}

	dev1_fifoe = FCR_RD_GET_FIFOE(FCR_RD_ADDR(dev1_base_remap_addr_mt6985));
	dev2_fifoe = FCR_RD_GET_FIFOE(FCR_RD_ADDR(dev2_base_remap_addr_mt6985));

	trx_mask = (REG_FLD_MASK(DEV0_STA_SET_FLD_dev0_sw_rx_set) |
		REG_FLD_MASK(DEV0_STA_SET_FLD_dev0_sw_tx_set));
	trx_state = UARTHUB_REG_READ_BIT(DEV0_STA_ADDR, trx_mask);

#if UARTHUB_DEBUG_LOG
	pr_info("[%s] dev1/2_fifoe=[%d/%d], trx_state=[0x%x], trx_state=[0x%x]\n",
		__func__, dev1_fifoe, dev2_fifoe, trx_mask, trx_state);
#endif

	/* set trx request */
	UARTHUB_REG_WRITE(DEV0_STA_SET_ADDR, trx_mask);

	/* disable and clear uarthub FIFO for UART0/1/2/CMM */
	UARTHUB_REG_WRITE(FCR_ADDR(cmm_base_remap_addr_mt6985), 0x80);
	UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6985), 0x80);
#if UARTHUB_ENABLE_MD_CHANNEL
	UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6985), 0x80);
#endif
	UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6985), 0x80);

	/* sw_rst3 4 times */
	for (i = 0; i < 4; i++) {
		CON4_SET_sw3_rst(CON4_ADDR, 1);
		usleep_range(5, 6);
	}

	/* enable uarthub FIFO for UART0/1/2/CMM */
	UARTHUB_REG_WRITE(FCR_ADDR(cmm_base_remap_addr_mt6985), 0x81);
	UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6985), 0x81);

#if UARTHUB_ENABLE_MD_CHANNEL
	if (dev1_fifoe == 1 && ap_only == 0)
		UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6985), 0x81);
#endif

	if (dev2_fifoe == 1 && ap_only == 0)
		UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6985), 0x81);

	/* restore trx request state */
	UARTHUB_REG_WRITE(DEV0_STA_SET_ADDR, trx_state);

	return 0;
}

int uarthub_reset_flow_control_mt6985(void)
{
	void __iomem *uarthub_dev_base = NULL;
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int val = 0;
	int retry = 0;
	int i = 0;

	debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(dev0_base_remap_addr_mt6985));
	debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(dev1_base_remap_addr_mt6985));
	debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(dev2_base_remap_addr_mt6985));
	debug8.cmm = UARTHUB_REG_READ(DEBUG_8(cmm_base_remap_addr_mt6985));

	if (((debug8.dev0 & 0x8) >> 3) == 0 && ((debug8.dev1 & 0x8) >> 3) == 0 &&
			((debug8.dev2 & 0x8) >> 3) == 0 && ((debug8.cmm & 0x8) >> 3) == 0)
		return 0;

	debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(dev0_base_remap_addr_mt6985));
	debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(dev1_base_remap_addr_mt6985));
	debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(dev2_base_remap_addr_mt6985));
	debug1.cmm = UARTHUB_REG_READ(DEBUG_1(cmm_base_remap_addr_mt6985));

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
				uarthub_dev_base = dev0_base_remap_addr_mt6985;
			else if (i == 1)
				uarthub_dev_base = dev1_base_remap_addr_mt6985;
			else if (i == 2)
				uarthub_dev_base = dev2_base_remap_addr_mt6985;
		} else
			uarthub_dev_base = cmm_base_remap_addr_mt6985;

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

		debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(dev0_base_remap_addr_mt6985));
		debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(dev1_base_remap_addr_mt6985));
		debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(dev2_base_remap_addr_mt6985));
		debug8.cmm = UARTHUB_REG_READ(DEBUG_8(cmm_base_remap_addr_mt6985));

		debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(dev0_base_remap_addr_mt6985));
		debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(dev1_base_remap_addr_mt6985));
		debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(dev2_base_remap_addr_mt6985));
		debug1.cmm = UARTHUB_REG_READ(DEBUG_1(cmm_base_remap_addr_mt6985));

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

		debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(dev0_base_remap_addr_mt6985));
		debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(dev1_base_remap_addr_mt6985));
		debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(dev2_base_remap_addr_mt6985));
		debug8.cmm = UARTHUB_REG_READ(DEBUG_8(cmm_base_remap_addr_mt6985));

		debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(dev0_base_remap_addr_mt6985));
		debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(dev1_base_remap_addr_mt6985));
		debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(dev2_base_remap_addr_mt6985));
		debug1.cmm = UARTHUB_REG_READ(DEBUG_1(cmm_base_remap_addr_mt6985));

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

int uarthub_is_assert_state_mt6985(void)
{
	int state = 0;

	state = (UARTHUB_REG_READ_BIT(DBG_ADDR, (0x1 << 0)) >> 0);
	return state;
}

int uarthub_assert_state_ctrl_mt6985(int assert_ctrl)
{
	if (assert_ctrl == uarthub_is_assert_state_mt6985()) {
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
		UARTHUB_SET_BIT(DBG_ADDR, (0x1 << 0));
	} else {
		UARTHUB_CLR_BIT(DBG_ADDR, (0x1 << 0));
		uarthub_irq_clear_ctrl_mt6985(BIT_0xFFFF_FFFF);
		uarthub_irq_mask_ctrl_mt6985(0);
	}

	return 0;
}

int uarthub_is_bypass_mode_mt6985(void)
{
	int state = 0;

	state = CON2_GET_intfhub_bypass(CON2_ADDR);
	return state;
}

#if !(UARTHUB_SUPPORT_SSPM_DRIVER)
int uarthub_usb_rx_pin_ctrl_mt6985(void __iomem *dev_base, int enable)
{
	if (!dev_base) {
		pr_notice("[%s] dev_base is not been init\n", __func__);
		return -1;
	}

	UARTHUB_REG_WRITE(
		USB_RX_SEL_ADDR(dev_base), ((enable == 1) ? 0x1 : 0x0));
	return 0;
}
#endif

int uarthub_uarthub_init_mt6985(struct platform_device *pdev)
{
#if UARTHUB_CONFIG_TRX_GPIO
	if (!gpio_base_remap_addr_mt6985) {
		pr_notice("[%s] gpio_base_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}
#endif

	clk_apmixedsys_univpll_mt6985 = devm_clk_get(&pdev->dev, "univpll");
	if (IS_ERR(clk_apmixedsys_univpll_mt6985)) {
		pr_notice("[%s] cannot get clk_apmixedsys_univpll_mt6985 clock.\n", __func__);
		return PTR_ERR(clk_apmixedsys_univpll_mt6985);
	}
	pr_info("[%s] clk_apmixedsys_univpll_mt6985=[%p]\n",
		__func__, clk_apmixedsys_univpll_mt6985);

	mutex_init(&g_lock_univpll_clk_mt6985);

#if UARTHUB_CONFIG_TRX_GPIO
	UARTHUB_REG_WRITE_MASK(gpio_base_remap_addr_mt6985 + GPIO_HUB_MODE_TX,
		GPIO_HUB_MODE_TX_VALUE, GPIO_HUB_MODE_TX_MASK);
	UARTHUB_REG_WRITE_MASK(gpio_base_remap_addr_mt6985 + GPIO_HUB_MODE_RX,
		GPIO_HUB_MODE_RX_VALUE, GPIO_HUB_MODE_RX_MASK);
#endif

	UARTHUB_SET_BIT(DBG_ADDR, (0x1 << 0));
#if !(UARTHUB_SUPPORT_SSPM_DRIVER)
	uarthub_clk_univpll_ctrl_mt6985(1);
	uarthub_init_default_config_mt6985();
	CON2_SET_intfhub_bypass(CON2_ADDR, 0);
	CON2_SET_crc_en(CON2_ADDR, 1);
	uarthub_init_trx_timeout_mt6985();

	/* Switch UART3 to external TOPCK 104M */
	if (pericfg_ao_remap_addr_mt6985)
		UARTHUB_REG_WRITE_MASK(pericfg_ao_remap_addr_mt6985 + PERI_CLOCK_CON,
			PERI_UART_FBCLK_CKSEL_UART_CK, PERI_UART_FBCLK_CKSEL_MASK);

	if (topckgen_base_remap_addr_mt6985) {
		/* Switch UART_MUX to 104M */
		UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6985 + CLK_CFG_6_CLR,
			CLK_CFG_6_UART_SEL_MASK);
		UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6985 + CLK_CFG_6_SET,
			CLK_CFG_6_UART_SEL_104M);
		UARTHUB_REG_WRITE(topckgen_base_remap_addr_mt6985 + CLK_CFG_UPDATE,
			CLK_CFG_UPDATE_UART_CK_UPDATE_MASK);
	}
#endif

	pr_info("[%s] assert=[0x%x], bypass=[0x%x], crc=[0x%x]\n",
		__func__, uarthub_is_assert_state_mt6985(),
		CON2_GET_intfhub_bypass(CON2_ADDR),
		CON2_GET_crc_en(CON2_ADDR));

	return 0;
}

#if !(UARTHUB_SUPPORT_SSPM_DRIVER)
int uarthub_init_default_config_mt6985(void)
{
	void __iomem *uarthub_dev_base = NULL;
	int baud_rate = 0;
	int i = 0;

	if (!dev0_base_remap_addr_mt6985 || !dev1_base_remap_addr_mt6985 ||
			!dev2_base_remap_addr_mt6985 || !cmm_base_remap_addr_mt6985) {
		pr_notice("[%s] dev0/1/2/cmm_base_remap_addr_mt6985 is not all init\n",
			__func__);
		return -1;
	}

	uarthub_usb_rx_pin_ctrl_mt6985(dev0_base_remap_addr_mt6985, 1);
	uarthub_usb_rx_pin_ctrl_mt6985(dev1_base_remap_addr_mt6985, 1);
	uarthub_usb_rx_pin_ctrl_mt6985(dev2_base_remap_addr_mt6985, 1);
	uarthub_usb_rx_pin_ctrl_mt6985(cmm_base_remap_addr_mt6985, 1);

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++) {
		if (i != UARTHUB_MAX_NUM_DEV_HOST) {
			if (i == 0) {
				uarthub_dev_base = dev0_base_remap_addr_mt6985;
				baud_rate = UARTHUB_DEV_0_BAUD_RATE;
			} else if (i == 1) {
				uarthub_dev_base = dev1_base_remap_addr_mt6985;
				baud_rate = UARTHUB_DEV_1_BAUD_RATE;
			} else if (i == 2) {
				uarthub_dev_base = dev2_base_remap_addr_mt6985;
				baud_rate = UARTHUB_DEV_2_BAUD_RATE;
			}
		} else {
			uarthub_dev_base = cmm_base_remap_addr_mt6985;
			baud_rate = UARTHUB_CMM_BAUD_RATE;
		}

		if (baud_rate >= 0)
			uarthub_config_baud_rate_m6985(uarthub_dev_base, baud_rate);

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

	uarthub_usb_rx_pin_ctrl_mt6985(dev0_base_remap_addr_mt6985, 0);
	uarthub_usb_rx_pin_ctrl_mt6985(dev1_base_remap_addr_mt6985, 0);
	uarthub_usb_rx_pin_ctrl_mt6985(dev2_base_remap_addr_mt6985, 0);
	uarthub_usb_rx_pin_ctrl_mt6985(cmm_base_remap_addr_mt6985, 0);

	/* 0x4c = 0x3,  rx/tx channel dma enable */
	UARTHUB_REG_WRITE(DMA_EN_ADDR(dev0_base_remap_addr_mt6985), 0x3);
	UARTHUB_REG_WRITE(DMA_EN_ADDR(dev1_base_remap_addr_mt6985), 0x3);
	UARTHUB_REG_WRITE(DMA_EN_ADDR(dev2_base_remap_addr_mt6985), 0x3);
	UARTHUB_REG_WRITE(DMA_EN_ADDR(cmm_base_remap_addr_mt6985), 0x3);

	/* 0x08 = 0x87, fifo control register */
	UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6985), 0x87);
	UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6985), 0x87);
	UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6985), 0x87);
	UARTHUB_REG_WRITE(FCR_ADDR(cmm_base_remap_addr_mt6985), 0x87);

	return 0;
}
#endif

#if !(UARTHUB_SUPPORT_SSPM_DRIVER)
int uarthub_init_trx_timeout_mt6985(void)
{
	CON3_SET_dev_timeout_time(CON3_ADDR, 0xF);
	return 0;
}
#endif

int uarthub_init_remap_reg_mt6985(void)
{
	gpio_base_remap_addr_mt6985 = ioremap(GPIO_BASE_ADDR, 0x500);
	pericfg_ao_remap_addr_mt6985 = ioremap(PERICFG_AO_BASE_ADDR, 0x100);
	topckgen_base_remap_addr_mt6985 = ioremap(TOPCKGEN_BASE_ADDR, 0x100);
	uarthub_base_remap_addr_mt6985 = ioremap(UARTHUB_BASE_ADDR, 0x500);
	dev0_base_remap_addr_mt6985 = UARTHUB_DEV_0_BASE_ADDR(uarthub_base_remap_addr_mt6985);
	dev1_base_remap_addr_mt6985 = UARTHUB_DEV_1_BASE_ADDR(uarthub_base_remap_addr_mt6985);
	dev2_base_remap_addr_mt6985 = UARTHUB_DEV_2_BASE_ADDR(uarthub_base_remap_addr_mt6985);
	cmm_base_remap_addr_mt6985 = UARTHUB_CMM_BASE_ADDR(uarthub_base_remap_addr_mt6985);
	intfhub_base_remap_addr_mt6985 = UARTHUB_INTFHUB_BASE_ADDR(uarthub_base_remap_addr_mt6985);
	apuart3_base_remap_addr_mt6985 = ioremap(UART3_BASE_ADDR, 0x100);
	apdma_uart_tx_int_remap_addr_mt6985 = ioremap(AP_DMA_UART_TX_INT_FLAG_ADDR, 0x100);
	spm_remap_addr_mt6985 = ioremap(SPM_BASE_ADDR, 0x1000);
	apmixedsys_remap_addr_mt6985 = ioremap(APMIXEDSYS_BASE_ADDR, 0x500);
	iocfg_rm_remap_addr_mt6985 = ioremap(IOCFG_RM_BASE_ADDR, 100);
	sys_sram_remap_addr_mt6985 = ioremap(SYS_SRAM_BASE_ADDR, 0x200);

	INTFHUB_BASE_MT6985 = (unsigned long) intfhub_base_remap_addr_mt6985;

	return 0;
}

int uarthub_deinit_unmap_reg_mt6985(void)
{
	if (gpio_base_remap_addr_mt6985)
		iounmap(gpio_base_remap_addr_mt6985);

	if (pericfg_ao_remap_addr_mt6985)
		iounmap(pericfg_ao_remap_addr_mt6985);

	if (topckgen_base_remap_addr_mt6985)
		iounmap(topckgen_base_remap_addr_mt6985);

	if (uarthub_base_remap_addr_mt6985)
		iounmap(uarthub_base_remap_addr_mt6985);

	if (apuart3_base_remap_addr_mt6985)
		iounmap(apuart3_base_remap_addr_mt6985);

	if (apdma_uart_tx_int_remap_addr_mt6985)
		iounmap(apdma_uart_tx_int_remap_addr_mt6985);

	if (spm_remap_addr_mt6985)
		iounmap(spm_remap_addr_mt6985);

	if (apmixedsys_remap_addr_mt6985)
		iounmap(apmixedsys_remap_addr_mt6985);

	if (iocfg_rm_remap_addr_mt6985)
		iounmap(iocfg_rm_remap_addr_mt6985);

	if (sys_sram_remap_addr_mt6985)
		iounmap(sys_sram_remap_addr_mt6985);

	return 0;
}

int uarthub_get_gpio_trx_info_mt6985(struct uarthub_gpio_trx_info *info)
{
	if (!info) {
		pr_notice("[%s] info is NULL\n", __func__);
		return -1;
	}

	if (!gpio_base_remap_addr_mt6985) {
		pr_notice("[%s] gpio_base_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	if (!iocfg_rm_remap_addr_mt6985) {
		pr_notice("[%s] iocfg_rm_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	info->tx_mode.addr = GPIO_BASE_ADDR + GPIO_HUB_MODE_TX;
	info->tx_mode.mask = GPIO_HUB_MODE_TX_MASK;
	info->tx_mode.value = GPIO_HUB_MODE_TX_VALUE;
	info->tx_mode.gpio_value = UARTHUB_REG_READ(
		gpio_base_remap_addr_mt6985 + GPIO_HUB_MODE_TX);

	info->rx_mode.addr = GPIO_BASE_ADDR + GPIO_HUB_MODE_RX;
	info->rx_mode.mask = GPIO_HUB_MODE_RX_MASK;
	info->rx_mode.value = GPIO_HUB_MODE_RX_VALUE;
	info->rx_mode.gpio_value = UARTHUB_REG_READ(
		gpio_base_remap_addr_mt6985 + GPIO_HUB_MODE_RX);

	info->tx_dir.addr = GPIO_BASE_ADDR + GPIO_HUB_DIR_TX;
	info->tx_dir.mask = GPIO_HUB_DIR_TX_MASK;
	info->tx_dir.gpio_value = (UARTHUB_REG_READ_BIT(
		gpio_base_remap_addr_mt6985 + GPIO_HUB_DIR_TX,
		GPIO_HUB_DIR_TX_MASK) >> GPIO_HUB_DIR_TX_SHIFT);

	info->rx_dir.addr = GPIO_BASE_ADDR + GPIO_HUB_DIR_RX;
	info->rx_dir.mask = GPIO_HUB_DIR_RX_MASK;
	info->rx_dir.gpio_value = (UARTHUB_REG_READ_BIT(
		gpio_base_remap_addr_mt6985 + GPIO_HUB_DIR_RX,
		GPIO_HUB_DIR_RX_MASK) >> GPIO_HUB_DIR_RX_SHIFT);

	info->tx_ies.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_IES_TX;
	info->tx_ies.mask = GPIO_HUB_IES_TX_MASK;
	info->tx_ies.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_IES_TX,
		GPIO_HUB_IES_TX_MASK) >> GPIO_HUB_IES_TX_SHIFT);

	info->rx_ies.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_IES_RX;
	info->rx_ies.mask = GPIO_HUB_IES_RX_MASK;
	info->rx_ies.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_IES_RX,
		GPIO_HUB_IES_RX_MASK) >> GPIO_HUB_IES_RX_SHIFT);

	info->tx_pu.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_PU_TX;
	info->tx_pu.mask = GPIO_HUB_PU_TX_MASK;
	info->tx_pu.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_PU_TX,
		GPIO_HUB_PU_TX_MASK) >> GPIO_HUB_PU_TX_SHIFT);

	info->rx_pu.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_PU_RX;
	info->rx_pu.mask = GPIO_HUB_PU_RX_MASK;
	info->rx_pu.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_PU_RX,
		GPIO_HUB_PU_RX_MASK) >> GPIO_HUB_PU_RX_SHIFT);

	info->tx_pd.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_PD_TX;
	info->tx_pd.mask = GPIO_HUB_PD_TX_MASK;
	info->tx_pd.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_PD_TX,
		GPIO_HUB_PD_TX_MASK) >> GPIO_HUB_PD_TX_SHIFT);

	info->rx_pd.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_PD_RX;
	info->rx_pd.mask = GPIO_HUB_PD_RX_MASK;
	info->rx_pd.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_PD_RX,
		GPIO_HUB_PD_RX_MASK) >> GPIO_HUB_PD_RX_SHIFT);

	info->tx_drv.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_DRV_TX;
	info->tx_drv.mask = GPIO_HUB_DRV_TX_MASK;
	info->tx_drv.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_DRV_TX,
		GPIO_HUB_DRV_TX_MASK) >> GPIO_HUB_DRV_TX_SHIFT);

	info->rx_drv.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_DRV_RX;
	info->rx_drv.mask = GPIO_HUB_DRV_RX_MASK;
	info->rx_drv.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_DRV_RX,
		GPIO_HUB_DRV_RX_MASK) >> GPIO_HUB_DRV_RX_SHIFT);

	info->tx_smt.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_SMT_TX;
	info->tx_smt.mask = GPIO_HUB_SMT_TX_MASK;
	info->tx_smt.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_SMT_TX,
		GPIO_HUB_SMT_TX_MASK) >> GPIO_HUB_SMT_TX_SHIFT);

	info->rx_smt.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_SMT_RX;
	info->rx_smt.mask = GPIO_HUB_SMT_RX_MASK;
	info->rx_smt.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_SMT_RX,
		GPIO_HUB_SMT_RX_MASK) >> GPIO_HUB_SMT_RX_SHIFT);

	info->tx_tdsel.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_TDSEL_TX;
	info->tx_tdsel.mask = GPIO_HUB_TDSEL_TX_MASK;
	info->tx_tdsel.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_TDSEL_TX,
		GPIO_HUB_TDSEL_TX_MASK) >> GPIO_HUB_TDSEL_TX_SHIFT);

	info->rx_tdsel.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_TDSEL_RX;
	info->rx_tdsel.mask = GPIO_HUB_TDSEL_RX_MASK;
	info->rx_tdsel.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_TDSEL_RX,
		GPIO_HUB_TDSEL_RX_MASK) >> GPIO_HUB_TDSEL_RX_SHIFT);

	info->tx_rdsel.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_RDSEL_TX;
	info->tx_rdsel.mask = GPIO_HUB_RDSEL_TX_MASK;
	info->tx_rdsel.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_RDSEL_TX,
		GPIO_HUB_RDSEL_TX_MASK) >> GPIO_HUB_RDSEL_TX_SHIFT);

	info->rx_rdsel.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_RDSEL_RX;
	info->rx_rdsel.mask = GPIO_HUB_RDSEL_RX_MASK;
	info->rx_rdsel.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_RDSEL_RX,
		GPIO_HUB_RDSEL_RX_MASK) >> GPIO_HUB_RDSEL_RX_SHIFT);

	info->tx_sec_en.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_SEC_EN_TX;
	info->tx_sec_en.mask = GPIO_HUB_SEC_EN_TX_MASK;
	info->tx_sec_en.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_SEC_EN_TX,
		GPIO_HUB_SEC_EN_TX_MASK) >> GPIO_HUB_SEC_EN_TX_SHIFT);

	info->rx_sec_en.addr = IOCFG_RM_BASE_ADDR + GPIO_HUB_SEC_EN_RX;
	info->rx_sec_en.mask = GPIO_HUB_SEC_EN_RX_MASK;
	info->rx_sec_en.gpio_value = (UARTHUB_REG_READ_BIT(
		iocfg_rm_remap_addr_mt6985 + GPIO_HUB_SEC_EN_RX,
		GPIO_HUB_SEC_EN_RX_MASK) >> GPIO_HUB_SEC_EN_RX_SHIFT);

	info->rx_din.addr = GPIO_BASE_ADDR + GPIO_HUB_DIN_RX;
	info->rx_din.mask = GPIO_HUB_DIN_RX_MASK;
	info->rx_din.gpio_value = (UARTHUB_REG_READ_BIT(
		gpio_base_remap_addr_mt6985 + GPIO_HUB_DIN_RX,
		GPIO_HUB_DIN_RX_MASK) >> GPIO_HUB_DIN_RX_SHIFT);

	return 0;
}

int uarthub_get_uarthub_cg_info_mt6985(void)
{
	if (!pericfg_ao_remap_addr_mt6985) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6985 + PERI_CG_1,
		PERI_CG_1_UARTHUB_CG_MASK) >> PERI_CG_1_UARTHUB_CG_SHIFT);
}

int uarthub_get_peri_clk_info_mt6985(void)
{
	if (!pericfg_ao_remap_addr_mt6985) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	return UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6985 + PERI_CLOCK_CON,
		PERI_UART_FBCLK_CKSEL_MASK);
}

int uarthub_get_hwccf_univpll_on_info_mt6985(void)
{
	if (!apmixedsys_remap_addr_mt6985) {
		pr_notice("[%s] apmixedsys_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(apmixedsys_remap_addr_mt6985 + UNIVPLL_CON0,
		UNIVPLL_CON0_UNIVPLL_EN_MASK) >> UNIVPLL_CON0_UNIVPLL_EN_SHIFT);
}

int uarthub_get_uart_mux_info_mt6985(void)
{
	if (!topckgen_base_remap_addr_mt6985) {
		pr_notice("[%s] topckgen_base_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(topckgen_base_remap_addr_mt6985 + CLK_CFG_6,
		CLK_CFG_6_UART_SEL_MASK) >> CLK_CFG_6_UART_SEL_SHIFT);
}

int uarthub_get_spm_sys_timer_mt6985(uint32_t *hi, uint32_t *lo)
{
	if (hi == NULL || lo == NULL) {
		pr_notice("[%s] invalid argument\n", __func__);
		return -1;
	}

	if (!spm_remap_addr_mt6985) {
		pr_notice("[%s] spm_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	*hi = UARTHUB_REG_READ(spm_remap_addr_mt6985 + SPM_SYS_TIMER_H);
	*lo = UARTHUB_REG_READ(spm_remap_addr_mt6985 + SPM_SYS_TIMER_L);

	return 1;
}

int uarthub_get_peri_uart_pad_mode_mt6985(void)
{
	if (!pericfg_ao_remap_addr_mt6985) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	/* 1: UART_PAD mode */
	/* 0: UARTHUB mode */
	return (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6985 + PERI_UART_WAKEUP,
		PERI_UART_WAKEUP_MASK) >> PERI_UART_WAKEUP_SHIFT);
}

int uarthub_dump_uartip_debug_info_mt6985(
	const char *tag, struct mutex *uartip_lock)
{
	const char *def_tag = "HUB_DBG_UIP";
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug2 = {0};
	struct uarthub_uartip_debug_info debug3 = {0};
	struct uarthub_uartip_debug_info debug4 = {0};
	struct uarthub_uartip_debug_info debug5 = {0};
	struct uarthub_uartip_debug_info debug6 = {0};
	struct uarthub_uartip_debug_info debug7 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	struct uarthub_uartip_debug_info feature_sel = {0};
	struct uarthub_uartip_debug_info highspeed = {0};
	struct uarthub_uartip_debug_info dll = {0};
	struct uarthub_uartip_debug_info sample_cnt = {0};
	struct uarthub_uartip_debug_info sample_pt = {0};
	struct uarthub_uartip_debug_info fracdiv_l = {0};
	struct uarthub_uartip_debug_info fracdiv_m = {0};
	struct uarthub_uartip_debug_info dma_en = {0};
	struct uarthub_uartip_debug_info iir_fcr = {0};
	struct uarthub_uartip_debug_info lcr = {0};
	struct uarthub_uartip_debug_info efr = {0};
	struct uarthub_uartip_debug_info xon1 = {0};
	struct uarthub_uartip_debug_info xoff1 = {0};
	struct uarthub_uartip_debug_info xon2 = {0};
	struct uarthub_uartip_debug_info xoff2 = {0};
	struct uarthub_uartip_debug_info esc_en = {0};
	struct uarthub_uartip_debug_info esc_dat = {0};
	struct uarthub_uartip_debug_info fcr_rd = {0};
	struct uarthub_uartip_debug_info mcr = {0};
	struct uarthub_uartip_debug_info lsr = {0};
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0, cmm_sta = 0, ap_sta = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;

	if (!uartip_lock)
		pr_notice("[%s] uartip_lock is NULL\n", __func__);

	if (uartip_lock) {
		if (mutex_lock_killable(uartip_lock)) {
			pr_notice("[%s] mutex_lock_killable(uartip_lock) fail\n", __func__);
			return UARTHUB_ERR_MUTEX_LOCK_FAIL;
		}
	}

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		if (dev0_sta == 0x300 ||  dev0_sta == 0x0) {
			pr_notice("[%s] all host sta is[0x%x]\n", __func__, dev0_sta);
			if (uartip_lock)
				mutex_unlock(uartip_lock);
			return -1;
		}
	}

	uarthub_clk_univpll_ctrl_mt6985(1);
	if (uarthub_get_hwccf_univpll_on_info_mt6985() == 0) {
		pr_notice("[%s] uarthub_get_hwccf_univpll_on_info_mt6985=[0]\n", __func__);
		uarthub_clk_univpll_ctrl_mt6985(0);
		if (uartip_lock)
			mutex_unlock(uartip_lock);
		return -1;
	}

	debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(dev0_base_remap_addr_mt6985));
	debug2.dev0 = UARTHUB_REG_READ(DEBUG_2(dev0_base_remap_addr_mt6985));
	debug3.dev0 = UARTHUB_REG_READ(DEBUG_3(dev0_base_remap_addr_mt6985));
	debug4.dev0 = UARTHUB_REG_READ(DEBUG_4(dev0_base_remap_addr_mt6985));
	debug5.dev0 = UARTHUB_REG_READ(DEBUG_5(dev0_base_remap_addr_mt6985));
	debug6.dev0 = UARTHUB_REG_READ(DEBUG_6(dev0_base_remap_addr_mt6985));
	debug7.dev0 = UARTHUB_REG_READ(DEBUG_7(dev0_base_remap_addr_mt6985));
	debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(dev0_base_remap_addr_mt6985));
	feature_sel.dev0 = UARTHUB_REG_READ(FEATURE_SEL_ADDR(dev0_base_remap_addr_mt6985));
	highspeed.dev0 = UARTHUB_REG_READ(HIGHSPEED_ADDR(dev0_base_remap_addr_mt6985));
	dll.dev0 = UARTHUB_REG_READ(DLL_ADDR(dev0_base_remap_addr_mt6985));
	sample_cnt.dev0 = UARTHUB_REG_READ(SAMPLE_COUNT_ADDR(dev0_base_remap_addr_mt6985));
	sample_pt.dev0 = UARTHUB_REG_READ(SAMPLE_POINT_ADDR(dev0_base_remap_addr_mt6985));
	fracdiv_l.dev0 = UARTHUB_REG_READ(FRACDIV_L_ADDR(dev0_base_remap_addr_mt6985));
	fracdiv_m.dev0 = UARTHUB_REG_READ(FRACDIV_M_ADDR(dev0_base_remap_addr_mt6985));
	dma_en.dev0 = UARTHUB_REG_READ(DMA_EN_ADDR(dev0_base_remap_addr_mt6985));
	iir_fcr.dev0 = UARTHUB_REG_READ(IIR_ADDR(dev0_base_remap_addr_mt6985));
	lcr.dev0 = UARTHUB_REG_READ(LCR_ADDR(dev0_base_remap_addr_mt6985));
	efr.dev0 = UARTHUB_REG_READ(EFR_ADDR(dev0_base_remap_addr_mt6985));
	xon1.dev0 = UARTHUB_REG_READ(XON1_ADDR(dev0_base_remap_addr_mt6985));
	xoff1.dev0 = UARTHUB_REG_READ(XOFF1_ADDR(dev0_base_remap_addr_mt6985));
	xon2.dev0 = UARTHUB_REG_READ(XON2_ADDR(dev0_base_remap_addr_mt6985));
	xoff2.dev0 = UARTHUB_REG_READ(XOFF2_ADDR(dev0_base_remap_addr_mt6985));
	esc_en.dev0 = UARTHUB_REG_READ(ESCAPE_EN_ADDR(dev0_base_remap_addr_mt6985));
	esc_dat.dev0 = UARTHUB_REG_READ(ESCAPE_DAT_ADDR(dev0_base_remap_addr_mt6985));
	fcr_rd.dev0 = UARTHUB_REG_READ(FCR_RD_ADDR(dev0_base_remap_addr_mt6985));
	mcr.dev0 = UARTHUB_REG_READ(MCR_ADDR(dev0_base_remap_addr_mt6985));
	lsr.dev0 = UARTHUB_REG_READ(LSR_ADDR(dev0_base_remap_addr_mt6985));

	debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(dev1_base_remap_addr_mt6985));
	debug2.dev1 = UARTHUB_REG_READ(DEBUG_2(dev1_base_remap_addr_mt6985));
	debug3.dev1 = UARTHUB_REG_READ(DEBUG_3(dev1_base_remap_addr_mt6985));
	debug4.dev1 = UARTHUB_REG_READ(DEBUG_4(dev1_base_remap_addr_mt6985));
	debug5.dev1 = UARTHUB_REG_READ(DEBUG_5(dev1_base_remap_addr_mt6985));
	debug6.dev1 = UARTHUB_REG_READ(DEBUG_6(dev1_base_remap_addr_mt6985));
	debug7.dev1 = UARTHUB_REG_READ(DEBUG_7(dev1_base_remap_addr_mt6985));
	debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(dev1_base_remap_addr_mt6985));
	feature_sel.dev1 = UARTHUB_REG_READ(FEATURE_SEL_ADDR(dev1_base_remap_addr_mt6985));
	highspeed.dev1 = UARTHUB_REG_READ(HIGHSPEED_ADDR(dev1_base_remap_addr_mt6985));
	dll.dev1 = UARTHUB_REG_READ(DLL_ADDR(dev1_base_remap_addr_mt6985));
	sample_cnt.dev1 = UARTHUB_REG_READ(SAMPLE_COUNT_ADDR(dev1_base_remap_addr_mt6985));
	sample_pt.dev1 = UARTHUB_REG_READ(SAMPLE_POINT_ADDR(dev1_base_remap_addr_mt6985));
	fracdiv_l.dev1 = UARTHUB_REG_READ(FRACDIV_L_ADDR(dev1_base_remap_addr_mt6985));
	fracdiv_m.dev1 = UARTHUB_REG_READ(FRACDIV_M_ADDR(dev1_base_remap_addr_mt6985));
	dma_en.dev1 = UARTHUB_REG_READ(DMA_EN_ADDR(dev1_base_remap_addr_mt6985));
	iir_fcr.dev1 = UARTHUB_REG_READ(IIR_ADDR(dev1_base_remap_addr_mt6985));
	lcr.dev1 = UARTHUB_REG_READ(LCR_ADDR(dev1_base_remap_addr_mt6985));
	efr.dev1 = UARTHUB_REG_READ(EFR_ADDR(dev1_base_remap_addr_mt6985));
	xon1.dev1 = UARTHUB_REG_READ(XON1_ADDR(dev1_base_remap_addr_mt6985));
	xoff1.dev1 = UARTHUB_REG_READ(XOFF1_ADDR(dev1_base_remap_addr_mt6985));
	xon2.dev1 = UARTHUB_REG_READ(XON2_ADDR(dev1_base_remap_addr_mt6985));
	xoff2.dev1 = UARTHUB_REG_READ(XOFF2_ADDR(dev1_base_remap_addr_mt6985));
	esc_en.dev1 = UARTHUB_REG_READ(ESCAPE_EN_ADDR(dev1_base_remap_addr_mt6985));
	esc_dat.dev1 = UARTHUB_REG_READ(ESCAPE_DAT_ADDR(dev1_base_remap_addr_mt6985));
	fcr_rd.dev1 = UARTHUB_REG_READ(FCR_RD_ADDR(dev1_base_remap_addr_mt6985));
	mcr.dev1 = UARTHUB_REG_READ(MCR_ADDR(dev1_base_remap_addr_mt6985));
	lsr.dev1 = UARTHUB_REG_READ(LSR_ADDR(dev1_base_remap_addr_mt6985));

	debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(dev2_base_remap_addr_mt6985));
	debug2.dev2 = UARTHUB_REG_READ(DEBUG_2(dev2_base_remap_addr_mt6985));
	debug3.dev2 = UARTHUB_REG_READ(DEBUG_3(dev2_base_remap_addr_mt6985));
	debug4.dev2 = UARTHUB_REG_READ(DEBUG_4(dev2_base_remap_addr_mt6985));
	debug5.dev2 = UARTHUB_REG_READ(DEBUG_5(dev2_base_remap_addr_mt6985));
	debug6.dev2 = UARTHUB_REG_READ(DEBUG_6(dev2_base_remap_addr_mt6985));
	debug7.dev2 = UARTHUB_REG_READ(DEBUG_7(dev2_base_remap_addr_mt6985));
	debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(dev2_base_remap_addr_mt6985));
	feature_sel.dev2 = UARTHUB_REG_READ(FEATURE_SEL_ADDR(dev2_base_remap_addr_mt6985));
	highspeed.dev2 = UARTHUB_REG_READ(HIGHSPEED_ADDR(dev2_base_remap_addr_mt6985));
	dll.dev2 = UARTHUB_REG_READ(DLL_ADDR(dev2_base_remap_addr_mt6985));
	sample_cnt.dev2 = UARTHUB_REG_READ(SAMPLE_COUNT_ADDR(dev2_base_remap_addr_mt6985));
	sample_pt.dev2 = UARTHUB_REG_READ(SAMPLE_POINT_ADDR(dev2_base_remap_addr_mt6985));
	fracdiv_l.dev2 = UARTHUB_REG_READ(FRACDIV_L_ADDR(dev2_base_remap_addr_mt6985));
	fracdiv_m.dev2 = UARTHUB_REG_READ(FRACDIV_M_ADDR(dev2_base_remap_addr_mt6985));
	dma_en.dev2 = UARTHUB_REG_READ(DMA_EN_ADDR(dev2_base_remap_addr_mt6985));
	iir_fcr.dev2 = UARTHUB_REG_READ(IIR_ADDR(dev2_base_remap_addr_mt6985));
	lcr.dev2 = UARTHUB_REG_READ(LCR_ADDR(dev2_base_remap_addr_mt6985));
	efr.dev2 = UARTHUB_REG_READ(EFR_ADDR(dev2_base_remap_addr_mt6985));
	xon1.dev2 = UARTHUB_REG_READ(XON1_ADDR(dev2_base_remap_addr_mt6985));
	xoff1.dev2 = UARTHUB_REG_READ(XOFF1_ADDR(dev2_base_remap_addr_mt6985));
	xon2.dev2 = UARTHUB_REG_READ(XON2_ADDR(dev2_base_remap_addr_mt6985));
	xoff2.dev2 = UARTHUB_REG_READ(XOFF2_ADDR(dev2_base_remap_addr_mt6985));
	esc_en.dev2 = UARTHUB_REG_READ(ESCAPE_EN_ADDR(dev2_base_remap_addr_mt6985));
	esc_dat.dev2 = UARTHUB_REG_READ(ESCAPE_DAT_ADDR(dev2_base_remap_addr_mt6985));
	fcr_rd.dev2 = UARTHUB_REG_READ(FCR_RD_ADDR(dev2_base_remap_addr_mt6985));
	mcr.dev2 = UARTHUB_REG_READ(MCR_ADDR(dev2_base_remap_addr_mt6985));
	lsr.dev2 = UARTHUB_REG_READ(LSR_ADDR(dev2_base_remap_addr_mt6985));

	debug1.cmm = UARTHUB_REG_READ(DEBUG_1(cmm_base_remap_addr_mt6985));
	debug2.cmm = UARTHUB_REG_READ(DEBUG_2(cmm_base_remap_addr_mt6985));
	debug3.cmm = UARTHUB_REG_READ(DEBUG_3(cmm_base_remap_addr_mt6985));
	debug4.cmm = UARTHUB_REG_READ(DEBUG_4(cmm_base_remap_addr_mt6985));
	debug5.cmm = UARTHUB_REG_READ(DEBUG_5(cmm_base_remap_addr_mt6985));
	debug6.cmm = UARTHUB_REG_READ(DEBUG_6(cmm_base_remap_addr_mt6985));
	debug7.cmm = UARTHUB_REG_READ(DEBUG_7(cmm_base_remap_addr_mt6985));
	debug8.cmm = UARTHUB_REG_READ(DEBUG_8(cmm_base_remap_addr_mt6985));
	feature_sel.cmm = UARTHUB_REG_READ(FEATURE_SEL_ADDR(cmm_base_remap_addr_mt6985));
	highspeed.cmm = UARTHUB_REG_READ(HIGHSPEED_ADDR(cmm_base_remap_addr_mt6985));
	dll.cmm = UARTHUB_REG_READ(DLL_ADDR(cmm_base_remap_addr_mt6985));
	sample_cnt.cmm = UARTHUB_REG_READ(SAMPLE_COUNT_ADDR(cmm_base_remap_addr_mt6985));
	sample_pt.cmm = UARTHUB_REG_READ(SAMPLE_POINT_ADDR(cmm_base_remap_addr_mt6985));
	fracdiv_l.cmm = UARTHUB_REG_READ(FRACDIV_L_ADDR(cmm_base_remap_addr_mt6985));
	fracdiv_m.cmm = UARTHUB_REG_READ(FRACDIV_M_ADDR(cmm_base_remap_addr_mt6985));
	dma_en.cmm = UARTHUB_REG_READ(DMA_EN_ADDR(cmm_base_remap_addr_mt6985));
	iir_fcr.cmm = UARTHUB_REG_READ(IIR_ADDR(cmm_base_remap_addr_mt6985));
	lcr.cmm = UARTHUB_REG_READ(LCR_ADDR(cmm_base_remap_addr_mt6985));
	efr.cmm = UARTHUB_REG_READ(EFR_ADDR(cmm_base_remap_addr_mt6985));
	xon1.cmm = UARTHUB_REG_READ(XON1_ADDR(cmm_base_remap_addr_mt6985));
	xoff1.cmm = UARTHUB_REG_READ(XOFF1_ADDR(cmm_base_remap_addr_mt6985));
	xon2.cmm = UARTHUB_REG_READ(XON2_ADDR(cmm_base_remap_addr_mt6985));
	xoff2.cmm = UARTHUB_REG_READ(XOFF2_ADDR(cmm_base_remap_addr_mt6985));
	esc_en.cmm = UARTHUB_REG_READ(ESCAPE_EN_ADDR(cmm_base_remap_addr_mt6985));
	esc_dat.cmm = UARTHUB_REG_READ(ESCAPE_DAT_ADDR(cmm_base_remap_addr_mt6985));
	fcr_rd.cmm = UARTHUB_REG_READ(FCR_RD_ADDR(cmm_base_remap_addr_mt6985));
	mcr.cmm = UARTHUB_REG_READ(MCR_ADDR(cmm_base_remap_addr_mt6985));
	lsr.cmm = UARTHUB_REG_READ(LSR_ADDR(cmm_base_remap_addr_mt6985));

	uarthub_clk_univpll_ctrl_mt6985(0);
	if (uartip_lock)
		mutex_unlock(uartip_lock);

	if (apuart3_base_remap_addr_mt6985 != NULL && g_enable_apuart_debug_info_mt6985 == 1) {
		debug1.ap = UARTHUB_REG_READ(DEBUG_1(apuart3_base_remap_addr_mt6985));
		debug2.ap = UARTHUB_REG_READ(DEBUG_2(apuart3_base_remap_addr_mt6985));
		debug3.ap = UARTHUB_REG_READ(DEBUG_3(apuart3_base_remap_addr_mt6985));
		debug4.ap = UARTHUB_REG_READ(DEBUG_4(apuart3_base_remap_addr_mt6985));
		debug5.ap = UARTHUB_REG_READ(DEBUG_5(apuart3_base_remap_addr_mt6985));
		debug6.ap = UARTHUB_REG_READ(DEBUG_6(apuart3_base_remap_addr_mt6985));
		debug7.ap = UARTHUB_REG_READ(DEBUG_7(apuart3_base_remap_addr_mt6985));
		debug8.ap = UARTHUB_REG_READ(DEBUG_8(apuart3_base_remap_addr_mt6985));
		feature_sel.ap = UARTHUB_REG_READ(FEATURE_SEL_ADDR(apuart3_base_remap_addr_mt6985));
		highspeed.ap = UARTHUB_REG_READ(HIGHSPEED_ADDR(apuart3_base_remap_addr_mt6985));
		dll.ap = UARTHUB_REG_READ(DLL_ADDR(apuart3_base_remap_addr_mt6985));
		sample_cnt.ap = UARTHUB_REG_READ(SAMPLE_COUNT_ADDR(apuart3_base_remap_addr_mt6985));
		sample_pt.ap = UARTHUB_REG_READ(SAMPLE_POINT_ADDR(apuart3_base_remap_addr_mt6985));
		fracdiv_l.ap = UARTHUB_REG_READ(FRACDIV_L_ADDR(apuart3_base_remap_addr_mt6985));
		fracdiv_m.ap = UARTHUB_REG_READ(FRACDIV_M_ADDR(apuart3_base_remap_addr_mt6985));
		dma_en.ap = UARTHUB_REG_READ(DMA_EN_ADDR(apuart3_base_remap_addr_mt6985));
		iir_fcr.ap = UARTHUB_REG_READ(IIR_ADDR(apuart3_base_remap_addr_mt6985));
		lcr.ap = UARTHUB_REG_READ(LCR_ADDR(apuart3_base_remap_addr_mt6985));
		efr.ap = UARTHUB_REG_READ(EFR_ADDR(apuart3_base_remap_addr_mt6985));
		xon1.ap = UARTHUB_REG_READ(XON1_ADDR(apuart3_base_remap_addr_mt6985));
		xoff1.ap = UARTHUB_REG_READ(XOFF1_ADDR(apuart3_base_remap_addr_mt6985));
		xon2.ap = UARTHUB_REG_READ(XON2_ADDR(apuart3_base_remap_addr_mt6985));
		xoff2.ap = UARTHUB_REG_READ(XOFF2_ADDR(apuart3_base_remap_addr_mt6985));
		esc_en.ap = UARTHUB_REG_READ(ESCAPE_EN_ADDR(apuart3_base_remap_addr_mt6985));
		esc_dat.ap = UARTHUB_REG_READ(ESCAPE_DAT_ADDR(apuart3_base_remap_addr_mt6985));
		fcr_rd.ap = UARTHUB_REG_READ(FCR_RD_ADDR(apuart3_base_remap_addr_mt6985));
		mcr.ap = UARTHUB_REG_READ(MCR_ADDR(apuart3_base_remap_addr_mt6985));
		lsr.ap = UARTHUB_REG_READ(LSR_ADDR(apuart3_base_remap_addr_mt6985));
	} else {
		debug1.ap = debug1.dev0;
		debug2.ap = debug2.dev0;
		debug3.ap = debug3.dev0;
		debug4.ap = debug4.dev0;
		debug5.ap = debug5.dev0;
		debug6.ap = debug6.dev0;
		debug7.ap = debug7.dev0;
		debug8.ap = debug8.dev0;
		feature_sel.ap = feature_sel.dev0;
		highspeed.ap = highspeed.dev0;
		dll.ap = dll.dev0;
		sample_cnt.ap = sample_cnt.dev0;
		sample_pt.ap = sample_pt.dev0;
		fracdiv_l.ap = fracdiv_l.dev0;
		fracdiv_m.ap = fracdiv_m.dev0;
		dma_en.ap = dma_en.dev0;
		iir_fcr.ap = iir_fcr.dev0;
		lcr.ap = lcr.dev0;
		efr.ap = efr.dev0;
		xon1.ap = xon1.dev0;
		xoff1.ap = xoff1.dev0;
		xon2.ap = xon2.dev0;
		xoff2.ap = xoff2.dev0;
		esc_en.ap = esc_en.dev0;
		esc_dat.ap = esc_dat.dev0;
		fcr_rd.ap = fcr_rd.dev0;
		mcr.ap = mcr.dev0;
		lsr.ap = lsr.dev0;
	}

	len = 0;
	dev0_sta = (((debug5.dev0 & 0xF0) >> 4) + ((debug6.dev0 & 0x3) << 4));
	dev1_sta = (((debug5.dev1 & 0xF0) >> 4) + ((debug6.dev1 & 0x3) << 4));
	dev2_sta = (((debug5.dev2 & 0xF0) >> 4) + ((debug6.dev2 & 0x3) << 4));
	cmm_sta = (((debug5.cmm & 0xF0) >> 4) + ((debug6.cmm & 0x3) << 4));
	ap_sta = (((debug5.ap & 0xF0) >> 4) + ((debug6.ap & 0x3) << 4));
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] op_rx_req=[%d]",
			def_tag, ((tag == NULL) ? "null" : tag), dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] op_rx_req=[%d-%d-%d-%d-%d]",
				def_tag, ((tag == NULL) ? "null" : tag),
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] op_rx_req=[%d-%d-%d-%d]",
				def_tag, ((tag == NULL) ? "null" : tag),
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = (((debug2.dev0 & 0xF0) >> 4) + ((debug3.dev0 & 0x3) << 4));
	dev1_sta = (((debug2.dev1 & 0xF0) >> 4) + ((debug3.dev1 & 0x3) << 4));
	dev2_sta = (((debug2.dev2 & 0xF0) >> 4) + ((debug3.dev2 & 0x3) << 4));
	cmm_sta = (((debug2.cmm & 0xF0) >> 4) + ((debug3.cmm & 0x3) << 4));
	ap_sta = (((debug2.ap & 0xF0) >> 4) + ((debug3.ap & 0x3) << 4));
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ip_tx_dma=[%d]", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",ip_tx_dma=[%d-%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",ip_tx_dma=[%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = (debug7.dev0 & 0x3F);
	dev1_sta = (debug7.dev1 & 0x3F);
	dev2_sta = (debug7.dev2 & 0x3F);
	cmm_sta =  (debug7.cmm & 0x3F);
	ap_sta =  (debug7.ap & 0x3F);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",fifo_woffset=[R:%d", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",fifo_woffset=[R:%d-%d-%d-%d-%d",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",fifo_woffset=[R:%d-%d-%d-%d",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = (debug4.dev0 & 0x3F);
	dev1_sta = (debug4.dev1 & 0x3F);
	dev2_sta = (debug4.dev2 & 0x3F);
	cmm_sta =  (debug4.cmm & 0x3F);
	ap_sta =  (debug4.ap & 0x3F);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d]", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = (((debug4.dev0 & 0xC0) >> 6) + ((debug5.dev0 & 0xF) << 2));
	dev1_sta = (((debug4.dev1 & 0xC0) >> 6) + ((debug5.dev1 & 0xF) << 2));
	dev2_sta = (((debug4.dev2 & 0xC0) >> 6) + ((debug5.dev2 & 0xF) << 2));
	cmm_sta = (((debug4.cmm & 0xC0) >> 6) + ((debug5.cmm & 0xF) << 2));
	ap_sta = (((debug4.ap & 0xC0) >> 6) + ((debug5.ap & 0xF) << 2));
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",fifo_tx_roffset=[%d]", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",fifo_tx_roffset=[%d-%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",fifo_tx_roffset=[%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = ((debug6.dev0 & 0xFC) >> 2);
	dev1_sta = ((debug6.dev1 & 0xFC) >> 2);
	dev2_sta = ((debug6.dev2 & 0xFC) >> 2);
	cmm_sta = ((debug6.cmm & 0xFC) >> 2);
	ap_sta = ((debug6.ap & 0xFC) >> 2);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",offset_dma=[R:%d", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",offset_dma=[R:%d-%d-%d-%d-%d",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",offset_dma=[R:%d-%d-%d-%d",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = ((debug3.dev0 & 0xFC) >> 2);
	dev1_sta = ((debug3.dev1 & 0xFC) >> 2);
	dev2_sta = ((debug3.dev2 & 0xFC) >> 2);
	cmm_sta = ((debug3.cmm & 0xFC) >> 2);
	ap_sta = ((debug3.ap & 0xFC) >> 2);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d]", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = ((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0);
	dev1_sta = ((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0);
	dev2_sta = ((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0);
	cmm_sta = ((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0);
	ap_sta = ((((debug1.ap & 0xE0) >> 5) == 1) ? 1 : 0);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] xcstate(wsend_xoff)=[%d]",
			def_tag, ((tag == NULL) ? "null" : tag), dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] xcstate(wsend_xoff)=[%d-%d-%d-%d-%d]",
				def_tag, ((tag == NULL) ? "null" : tag),
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] xcstate(wsend_xoff)=[%d-%d-%d-%d]",
				def_tag, ((tag == NULL) ? "null" : tag),
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = ((debug8.dev0 & 0x8) >> 3);
	dev1_sta = ((debug8.dev1 & 0x8) >> 3);
	dev2_sta = ((debug8.dev2 & 0x8) >> 3);
	cmm_sta = ((debug8.cmm & 0x8) >> 3);
	ap_sta = ((debug8.ap & 0x8) >> 3);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",swtxdis(det_xoff)=[%d]", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",swtxdis(det_xoff)=[%d-%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",swtxdis(det_xoff)=[%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	if (feature_sel.dev0 == feature_sel.dev1 && feature_sel.dev1 == feature_sel.dev2 &&
			feature_sel.dev2 == feature_sel.cmm && feature_sel.cmm == feature_sel.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FEATURE_SEL(0x9c)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), feature_sel.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] FEATURE_SEL(0x9c)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				feature_sel.dev0, feature_sel.dev1, feature_sel.dev2,
				feature_sel.cmm, feature_sel.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] FEATURE_SEL(0x9c)=[0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				feature_sel.dev0, feature_sel.dev1,
				feature_sel.dev2, feature_sel.cmm);
		}
	}

	if (highspeed.dev0 == highspeed.dev1 && highspeed.dev1 == highspeed.dev2 &&
			highspeed.dev2 == highspeed.cmm && highspeed.cmm == highspeed.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HIGHSPEED(0x24)=[0x%x]", highspeed.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",HIGHSPEED(0x24)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				highspeed.dev0, highspeed.dev1, highspeed.dev2,
				highspeed.cmm, highspeed.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",HIGHSPEED(0x24)=[0x%x-0x%x-0x%x-0x%x]",
				highspeed.dev0, highspeed.dev1, highspeed.dev2, highspeed.cmm);
		}
	}

	if (dll.dev0 == dll.dev1 && dll.dev1 == dll.dev2 &&
			dll.dev2 == dll.cmm && dll.cmm == dll.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",DLL(0x90)=[0x%x]", dll.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",DLL(0x90)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				dll.dev0, dll.dev1, dll.dev2, dll.cmm, dll.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",DLL(0x90)=[0x%x-0x%x-0x%x-0x%x]",
				dll.dev0, dll.dev1, dll.dev2, dll.cmm);
		}
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	if (sample_cnt.dev0 == sample_cnt.dev1 && sample_cnt.dev1 == sample_cnt.dev2 &&
			sample_cnt.dev2 == sample_cnt.cmm && sample_cnt.cmm == sample_cnt.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] SAMPLE_CNT(0x28)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), sample_cnt.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] SAMPLE_CNT(0x28)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				sample_cnt.dev0, sample_cnt.dev1, sample_cnt.dev2,
				sample_cnt.cmm, sample_cnt.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] SAMPLE_CNT(0x28)=[0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				sample_cnt.dev0, sample_cnt.dev1, sample_cnt.dev2, sample_cnt.cmm);
		}
	}

	if (sample_pt.dev0 == sample_pt.dev1 && sample_pt.dev1 == sample_pt.dev2 &&
			sample_pt.dev2 == sample_pt.cmm && sample_pt.cmm == sample_pt.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SAMPLE_PT(0x2c)=[0x%x]", sample_pt.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",SAMPLE_PT(0x2c)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				sample_pt.dev0, sample_pt.dev1, sample_pt.dev2,
				sample_pt.cmm, sample_pt.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",SAMPLE_PT(0x2c)=[0x%x-0x%x-0x%x-0x%x]",
				sample_pt.dev0, sample_pt.dev1, sample_pt.dev2, sample_pt.cmm);
		}
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	if (fracdiv_l.dev0 == fracdiv_l.dev1 && fracdiv_l.dev1 == fracdiv_l.dev2 &&
			fracdiv_l.dev2 == fracdiv_l.cmm && fracdiv_l.cmm == fracdiv_l.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FRACDIV_L(0x54)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), fracdiv_l.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] FRACDIV_L(0x54)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				fracdiv_l.dev0, fracdiv_l.dev1, fracdiv_l.dev2,
				fracdiv_l.cmm, fracdiv_l.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] FRACDIV_L(0x54)=[0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				fracdiv_l.dev0, fracdiv_l.dev1, fracdiv_l.dev2, fracdiv_l.cmm);
		}
	}

	if (fracdiv_m.dev0 == fracdiv_m.dev1 && fracdiv_m.dev1 == fracdiv_m.dev2 &&
			fracdiv_m.dev2 == fracdiv_m.cmm && fracdiv_m.cmm == fracdiv_m.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",FRACDIV_M(0x58)=[0x%x]", fracdiv_m.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",FRACDIV_M(0x58)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				fracdiv_m.dev0, fracdiv_m.dev1, fracdiv_m.dev2,
				fracdiv_m.cmm, fracdiv_m.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",FRACDIV_M(0x58)=[0x%x-0x%x-0x%x-0x%x]",
				fracdiv_m.dev0, fracdiv_m.dev1, fracdiv_m.dev2, fracdiv_m.cmm);
		}
	}

	if (dma_en.dev0 == dma_en.dev1 && dma_en.dev1 == dma_en.dev2 &&
			dma_en.dev2 == dma_en.cmm && dma_en.cmm == dma_en.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",DMA_EN(0x4c)=[0x%x]", dma_en.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",DMA_EN(0x4c)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				dma_en.dev0, dma_en.dev1, dma_en.dev2,
				dma_en.cmm, dma_en.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",DMA_EN(0x4c)=[0x%x-0x%x-0x%x-0x%x]",
				dma_en.dev0, dma_en.dev1, dma_en.dev2, dma_en.cmm);
		}
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	if (iir_fcr.dev0 == iir_fcr.dev1 && iir_fcr.dev1 == iir_fcr.dev2 &&
			iir_fcr.dev2 == iir_fcr.cmm && iir_fcr.cmm == iir_fcr.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] IIR(0x8)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), iir_fcr.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] IIR(0x8)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				iir_fcr.dev0, iir_fcr.dev1, iir_fcr.dev2,
				iir_fcr.cmm, iir_fcr.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] IIR(0x8)=[0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				iir_fcr.dev0, iir_fcr.dev1, iir_fcr.dev2, iir_fcr.cmm);
		}
	}

	if (lcr.dev0 == lcr.dev1 && lcr.dev1 == lcr.dev2 &&
			lcr.dev2 == lcr.cmm && lcr.cmm == lcr.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",LCR(0xc)=[0x%x]", lcr.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",LCR(0xc)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				lcr.dev0, lcr.dev1, lcr.dev2,
				lcr.cmm, lcr.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",LCR(0xc)=[0x%x-0x%x-0x%x-0x%x]",
				lcr.dev0, lcr.dev1, lcr.dev2, lcr.cmm);
		}
	}

	if (efr.dev0 == efr.dev1 && efr.dev1 == efr.dev2 &&
			efr.dev2 == efr.cmm && efr.cmm == efr.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",EFR(0x98)=[0x%x]", efr.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",EFR(0x98)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				efr.dev0, efr.dev1, efr.dev2, efr.cmm, efr.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",EFR(0x98)=[0x%x-0x%x-0x%x-0x%x]",
				efr.dev0, efr.dev1, efr.dev2, efr.cmm);
		}
	}

	if (xon1.dev0 == xon1.dev1 && xon1.dev1 == xon1.dev2 &&
			xon1.dev2 == xon1.cmm && xon1.cmm == xon1.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XON1(0xa0)=[0x%x]", xon1.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",XON1(0xa0)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				xon1.dev0, xon1.dev1, xon1.dev2, xon1.cmm, xon1.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",XON1(0xa0)=[0x%x-0x%x-0x%x-0x%x]",
				xon1.dev0, xon1.dev1, xon1.dev2, xon1.cmm);
		}
	}

	if (xoff1.dev0 == xoff1.dev1 && xoff1.dev1 == xoff1.dev2 &&
			xoff1.dev2 == xoff1.cmm && xoff1.cmm == xoff1.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XOFF1(0xa8)=[0x%x]", xoff1.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",XOFF1(0xa8)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				xoff1.dev0, xoff1.dev1, xoff1.dev2, xoff1.cmm, xoff1.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",XOFF1(0xa8)=[0x%x-0x%x-0x%x-0x%x]",
				xoff1.dev0, xoff1.dev1, xoff1.dev2, xoff1.cmm);
		}
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	if (xon2.dev0 == xon2.dev1 && xon2.dev1 == xon2.dev2 &&
			xon2.dev2 == xon2.cmm && xon2.cmm == xon2.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] XON2(0xa4)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), xon2.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] XON2(0xa4)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				xon2.dev0, xon2.dev1, xon2.dev2, xon2.cmm, xon2.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] XON2(0xa4)=[0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				xon2.dev0, xon2.dev1, xon2.dev2, xon2.cmm);
		}
	}

	if (xoff2.dev0 == xoff2.dev1 && xoff2.dev1 == xoff2.dev2 &&
			xoff2.dev2 == xoff2.cmm && xoff2.cmm == xoff2.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",XOFF2(0xac)=[0x%x]", xoff2.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",XOFF2(0xac)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				xoff2.dev0, xoff2.dev1, xoff2.dev2, xoff2.cmm, xoff2.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",XOFF2(0xac)=[0x%x-0x%x-0x%x-0x%x]",
				xoff2.dev0, xoff2.dev1, xoff2.dev2, xoff2.cmm);
		}
	}

	if (esc_en.dev0 == esc_en.dev1 && esc_en.dev1 == esc_en.dev2 &&
			esc_en.dev2 == esc_en.cmm && esc_en.cmm == esc_en.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ESC_EN(0x44)=[0x%x]", esc_en.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",ESC_EN(0x44)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				esc_en.dev0, esc_en.dev1, esc_en.dev2,
				esc_en.cmm, esc_en.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",ESC_EN(0x44)=[0x%x-0x%x-0x%x-0x%x]",
				esc_en.dev0, esc_en.dev1, esc_en.dev2, esc_en.cmm);
		}
	}

	if (esc_dat.dev0 == esc_dat.dev1 && esc_dat.dev1 == esc_dat.dev2 &&
			esc_dat.dev2 == esc_dat.cmm && esc_dat.cmm == esc_dat.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",ESC_DAT(0x40)=[0x%x]", esc_dat.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",ESC_DAT(0x40)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				esc_dat.dev0, esc_dat.dev1, esc_dat.dev2,
				esc_dat.cmm, esc_dat.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",ESC_DAT(0x40)=[0x%x-0x%x-0x%x-0x%x]",
				esc_dat.dev0, esc_dat.dev1, esc_dat.dev2, esc_dat.cmm);
		}
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	if (fcr_rd.dev0 == fcr_rd.dev1 && fcr_rd.dev1 == fcr_rd.dev2 &&
			fcr_rd.dev2 == fcr_rd.cmm && fcr_rd.cmm == fcr_rd.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] FCR_RD(0x5c)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), fcr_rd.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] FCR_RD(0x5c)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				fcr_rd.dev0, fcr_rd.dev1, fcr_rd.dev2,
				fcr_rd.cmm, fcr_rd.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] FCR_RD(0x5c)=[0x%x-0x%x-0x%x-0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				fcr_rd.dev0, fcr_rd.dev1, fcr_rd.dev2, fcr_rd.cmm);
		}
	}

	if (mcr.dev0 == mcr.dev1 && mcr.dev1 == mcr.dev2 &&
			mcr.dev2 == mcr.cmm && mcr.cmm == mcr.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",MCR(0x10)=[0x%x]", mcr.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",MCR(0x10)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				mcr.dev0, mcr.dev1, mcr.dev2,
				mcr.cmm, mcr.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",MCR(0x10)=[0x%x-0x%x-0x%x-0x%x]",
				mcr.dev0, mcr.dev1, mcr.dev2, mcr.cmm);
		}
	}

	if (lsr.dev0 == lsr.dev1 && lsr.dev1 == lsr.dev2 &&
			lsr.dev2 == lsr.cmm && lsr.cmm == lsr.ap) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",LSR(0x14)=[0x%x]", lsr.dev0);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",LSR(0x14)=[0x%x-0x%x-0x%x-0x%x-0x%x]",
				lsr.dev0, lsr.dev1, lsr.dev2,
				lsr.cmm, lsr.ap);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",LSR(0x14)=[0x%x-0x%x-0x%x-0x%x]",
				lsr.dev0, lsr.dev1, lsr.dev2, lsr.cmm);
		}
	}

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_intfhub_debug_info_mt6985(const char *tag)
{
	int val = 0;
	unsigned int spm_res1 = 0, spm_res2 = 0;
	struct uarthub_gpio_trx_info gpio_base_addr;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	const char *def_tag = "HUB_DBG";
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;

	val = DBG_GET_intfhub_dbg_sel(DBG_ADDR);
	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] IDBG=[0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag), val);

	val = uarthub_is_apb_bus_clk_enable_mt6985();
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",APB=[%d]", val);

	if (val == 0) {
		pr_info("%s\n", dmp_info_buf);
		return 0;
	}

	val = uarthub_get_uarthub_cg_info_mt6985();
	if (val >= 0) {
		/* the expect value is 0x0 */
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HCG=[0x%x]", val);
	}

	val = uarthub_get_peri_clk_info_mt6985();
	if (val >= 0) {
		/* the expect value is 0x800 */
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UPCLK=[0x%x]", val);
	}

	val = uarthub_get_peri_uart_pad_mode_mt6985();
	if (val >= 0) {
		/* the expect value is 0x0 */
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UPAD=[0x%x(%s)]",
			val, ((val == 0) ? "HUB" : "UART_PAD"));
	}

	val = uarthub_get_spm_res_info_mt6985(&spm_res1, &spm_res2);
	if (val == 1) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[1]");
	} else if (val == 0) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[0(0x%x/0x%x)]", spm_res1, spm_res2);
	}

	val = uarthub_get_hwccf_univpll_on_info_mt6985();
	if (val >= 0) {
		/* the expect value is 0x1 */
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UVPLL=[%d]", val);
	}

	val = uarthub_get_uart_mux_info_mt6985();
	if (val >= 0) {
		/* the expect value is 0x2 */
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UMUX=[0x%x]", val);
	}

	val = DBG_GET_intfhub_dbg_sel(DBG_ADDR);
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);

	pr_info("%s\n", dmp_info_buf);

	val = uarthub_get_gpio_trx_info_mt6985(&gpio_base_addr);
	if (val == 0) {
		len = 0;
		if (gpio_base_addr.tx_mode.gpio_value ==
				gpio_base_addr.rx_mode.gpio_value) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] GPIO MODE=[T/R:0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				gpio_base_addr.tx_mode.gpio_value);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s] GPIO MODE=[T:0x%x,R:0x%x]",
				def_tag, ((tag == NULL) ? "null" : tag),
				gpio_base_addr.tx_mode.gpio_value,
				gpio_base_addr.rx_mode.gpio_value);
		}

		if (gpio_base_addr.tx_drv.gpio_value ==
				gpio_base_addr.rx_drv.gpio_value) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",DRV=[T/R:0x%x]", gpio_base_addr.tx_drv.gpio_value);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",DRV=[T:0x%x,R:0x%x]",
				gpio_base_addr.tx_drv.gpio_value,
				gpio_base_addr.rx_drv.gpio_value);
		}
	}

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ILPBACK(0xe4)=[0x%x]", UARTHUB_REG_READ(LOOPBACK_ADDR));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG(0xf4)=[0x%x]", UARTHUB_REG_READ(DBG_ADDR));

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] IDEVx_STA(0x0/0x40/0x80)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] IDEVx_STA(0x0/0x40/0x80)=[0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			dev0_sta, dev1_sta, dev2_sta);
	}

	dev0_sta = UARTHUB_REG_READ(DEV0_PKT_CNT_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_PKT_CNT_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_PKT_CNT_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEVx_PKT_CNT(0x1c/0x50/0x90)=[0x%x]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEVx_PKT_CNT(0x1c/0x50/0x90)=[0x%x-0x%x-0x%x]",
			dev0_sta, dev1_sta, dev2_sta);
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = UARTHUB_REG_READ(DEV0_CRC_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_CRC_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_CRC_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] IDEVx_CRC_STA(0x20/0x54/0x94)=[0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag), dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s] IDEVx_CRC_STA(0x20/0x54/0x94)=[0x%x-0x%x-0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			dev0_sta, dev1_sta, dev2_sta);
	}

	dev0_sta = UARTHUB_REG_READ(DEV0_RX_ERR_CRC_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_RX_ERR_CRC_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_RX_ERR_CRC_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEVx_RX_ERR_CRC_STA(0x10/0x14/0x18)=[0x%x]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEVx_RX_ERR_CRC_STA(0x10/0x14/0x18)=[0x%x-0x%x-0x%x]",
			dev0_sta, dev1_sta, dev2_sta);
	}

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] IDEV0_IRQ_STA/MASK(0x30/0x38)=[0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(DEV0_IRQ_STA_ADDR),
		UARTHUB_REG_READ(DEV0_IRQ_MASK_ADDR));

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IIRQ_STA/MASK(0xd0/0xd8)=[0x%x-0x%x]",
		UARTHUB_REG_READ(IRQ_STA_ADDR),
		UARTHUB_REG_READ(IRQ_MASK_ADDR));

	val = UARTHUB_REG_READ(STA0_ADDR);
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0(0xe0)=[0x%x]", val);

	val = UARTHUB_REG_READ(CON2_ADDR);
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ICON2(0xc8)=[0x%x]", val);

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_debug_tx_rx_count_mt6985(const char *tag, int trigger_point)
{
	static int cur_tx_pkt_cnt_d0;
	static int cur_tx_pkt_cnt_d1;
	static int cur_tx_pkt_cnt_d2;
	static int cur_rx_pkt_cnt_d0;
	static int cur_rx_pkt_cnt_d1;
	static int cur_rx_pkt_cnt_d2;
	static int d0_wait_for_send_xoff;
	static int d1_wait_for_send_xoff;
	static int d2_wait_for_send_xoff;
	static int cmm_wait_for_send_xoff;
	static int ap_wait_for_send_xoff;
	static int d0_detect_xoff;
	static int d1_detect_xoff;
	static int d2_detect_xoff;
	static int cmm_detect_xoff;
	static int ap_detect_xoff;
	static int d0_rx_bcnt;
	static int d1_rx_bcnt;
	static int d2_rx_bcnt;
	static int cmm_rx_bcnt;
	static int ap_rx_bcnt;
	static int d0_tx_bcnt;
	static int d1_tx_bcnt;
	static int d2_tx_bcnt;
	static int cmm_tx_bcnt;
	static int ap_tx_bcnt;
	static int pre_trigger_point = -1;
	struct uarthub_uartip_debug_info pkt_cnt = {0};
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug2 = {0};
	struct uarthub_uartip_debug_info debug3 = {0};
	struct uarthub_uartip_debug_info debug5 = {0};
	struct uarthub_uartip_debug_info debug6 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	const char *def_tag = "HUB_DBG";
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;

	if (trigger_point != DUMP0 && trigger_point != DUMP1) {
		pr_notice("[%s] trigger_point = %d is invalid\n", __func__, trigger_point);
		return -1;
	}

	if (trigger_point == DUMP1 && pre_trigger_point == 0) {
		len = 0;
		if (cur_rx_pkt_cnt_d0 == cur_rx_pkt_cnt_d1 &&
				cur_rx_pkt_cnt_d1 == cur_rx_pkt_cnt_d2) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s], dump0, pcnt=[R:%d",
				def_tag, ((tag == NULL) ? "null" : tag),
				cur_rx_pkt_cnt_d0);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s], dump0, pcnt=[R:%d-%d-%d",
				def_tag, ((tag == NULL) ? "null" : tag),
				cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2);
		}

		if (cur_rx_pkt_cnt_d0 == cur_rx_pkt_cnt_d1 &&
				cur_rx_pkt_cnt_d1 == cur_rx_pkt_cnt_d2) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d]", cur_tx_pkt_cnt_d0);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d]",
				cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		}

		if (d0_rx_bcnt == d1_rx_bcnt && d1_rx_bcnt == d2_rx_bcnt &&
				d2_rx_bcnt == cmm_rx_bcnt && cmm_rx_bcnt == ap_rx_bcnt) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",bcnt=[R:%d", d0_rx_bcnt);
		} else {
			if (g_enable_apuart_debug_info_mt6985 == 1) {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",bcnt=[R:%d-%d-%d-%d-%d",
					d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt,
					cmm_rx_bcnt, ap_rx_bcnt);
			} else {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",bcnt=[R:%d-%d-%d-%d",
					d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt, cmm_rx_bcnt);
			}
		}

		if (d0_tx_bcnt == d1_tx_bcnt && d1_tx_bcnt == d2_tx_bcnt &&
				d2_tx_bcnt == cmm_tx_bcnt && cmm_tx_bcnt == ap_tx_bcnt) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d]", d0_tx_bcnt);
		} else {
			if (g_enable_apuart_debug_info_mt6985 == 1) {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",T:%d-%d-%d-%d-%d]",
					d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt,
					cmm_tx_bcnt, ap_tx_bcnt);
			} else {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",T:%d-%d-%d-%d]",
					d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt, cmm_tx_bcnt);
			}
		}

		if (d0_wait_for_send_xoff == d1_wait_for_send_xoff &&
				d1_wait_for_send_xoff == d2_wait_for_send_xoff &&
				d2_wait_for_send_xoff == cmm_wait_for_send_xoff &&
				cmm_wait_for_send_xoff == ap_wait_for_send_xoff) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",wsend_xoff=[%d]", d0_wait_for_send_xoff);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",wsend_xoff=[%d-%d-%d-%d-%d]",
				d0_wait_for_send_xoff, d1_wait_for_send_xoff,
				d2_wait_for_send_xoff, cmm_wait_for_send_xoff,
				ap_wait_for_send_xoff);
		}

		if (d0_detect_xoff == d1_detect_xoff &&
				d1_detect_xoff == d2_detect_xoff &&
				d2_detect_xoff == cmm_detect_xoff &&
				cmm_detect_xoff == ap_detect_xoff) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d]", d0_detect_xoff);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d-%d-%d-%d-%d]",
				d0_detect_xoff, d1_detect_xoff, d2_detect_xoff,
				cmm_detect_xoff, ap_detect_xoff);
		}

		pr_info("%s\n", dmp_info_buf);
	}

	if (uarthub_is_apb_bus_clk_enable_mt6985() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	pkt_cnt.dev0 = UARTHUB_REG_READ(DEV0_PKT_CNT_ADDR);
	pkt_cnt.dev1 = UARTHUB_REG_READ(DEV1_PKT_CNT_ADDR);
	pkt_cnt.dev2 = UARTHUB_REG_READ(DEV2_PKT_CNT_ADDR);

	cur_tx_pkt_cnt_d0 = ((pkt_cnt.dev0 & 0xFF000000) >> 24);
	cur_tx_pkt_cnt_d1 = ((pkt_cnt.dev1 & 0xFF000000) >> 24);
	cur_tx_pkt_cnt_d2 = ((pkt_cnt.dev2 & 0xFF000000) >> 24);
	cur_rx_pkt_cnt_d0 = ((pkt_cnt.dev0 & 0xFF00) >> 8);
	cur_rx_pkt_cnt_d1 = ((pkt_cnt.dev1 & 0xFF00) >> 8);
	cur_rx_pkt_cnt_d2 = ((pkt_cnt.dev2 & 0xFF00) >> 8);

	uarthub_clk_univpll_ctrl_mt6985(1);
	if (uarthub_get_hwccf_univpll_on_info_mt6985() == 1) {
		debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(dev0_base_remap_addr_mt6985));
		debug2.dev0 = UARTHUB_REG_READ(DEBUG_2(dev0_base_remap_addr_mt6985));
		debug3.dev0 = UARTHUB_REG_READ(DEBUG_3(dev0_base_remap_addr_mt6985));
		debug5.dev0 = UARTHUB_REG_READ(DEBUG_5(dev0_base_remap_addr_mt6985));
		debug6.dev0 = UARTHUB_REG_READ(DEBUG_6(dev0_base_remap_addr_mt6985));
		debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(dev0_base_remap_addr_mt6985));

		debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(dev1_base_remap_addr_mt6985));
		debug2.dev1 = UARTHUB_REG_READ(DEBUG_2(dev1_base_remap_addr_mt6985));
		debug3.dev1 = UARTHUB_REG_READ(DEBUG_3(dev1_base_remap_addr_mt6985));
		debug5.dev1 = UARTHUB_REG_READ(DEBUG_5(dev1_base_remap_addr_mt6985));
		debug6.dev1 = UARTHUB_REG_READ(DEBUG_6(dev1_base_remap_addr_mt6985));
		debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(dev1_base_remap_addr_mt6985));

		debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(dev2_base_remap_addr_mt6985));
		debug2.dev2 = UARTHUB_REG_READ(DEBUG_2(dev2_base_remap_addr_mt6985));
		debug3.dev2 = UARTHUB_REG_READ(DEBUG_3(dev2_base_remap_addr_mt6985));
		debug5.dev2 = UARTHUB_REG_READ(DEBUG_5(dev2_base_remap_addr_mt6985));
		debug6.dev2 = UARTHUB_REG_READ(DEBUG_6(dev2_base_remap_addr_mt6985));
		debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(dev2_base_remap_addr_mt6985));

		debug1.cmm = UARTHUB_REG_READ(DEBUG_1(cmm_base_remap_addr_mt6985));
		debug2.cmm = UARTHUB_REG_READ(DEBUG_2(cmm_base_remap_addr_mt6985));
		debug3.cmm = UARTHUB_REG_READ(DEBUG_3(cmm_base_remap_addr_mt6985));
		debug5.cmm = UARTHUB_REG_READ(DEBUG_5(cmm_base_remap_addr_mt6985));
		debug6.cmm = UARTHUB_REG_READ(DEBUG_6(cmm_base_remap_addr_mt6985));
		debug8.cmm = UARTHUB_REG_READ(DEBUG_8(cmm_base_remap_addr_mt6985));

		if (apuart3_base_remap_addr_mt6985 != NULL &&
			g_enable_apuart_debug_info_mt6985 == 1) {
			debug1.ap = UARTHUB_REG_READ(DEBUG_1(apuart3_base_remap_addr_mt6985));
			debug2.ap = UARTHUB_REG_READ(DEBUG_2(apuart3_base_remap_addr_mt6985));
			debug3.ap = UARTHUB_REG_READ(DEBUG_3(apuart3_base_remap_addr_mt6985));
			debug5.ap = UARTHUB_REG_READ(DEBUG_5(apuart3_base_remap_addr_mt6985));
			debug6.ap = UARTHUB_REG_READ(DEBUG_6(apuart3_base_remap_addr_mt6985));
			debug8.ap = UARTHUB_REG_READ(DEBUG_8(apuart3_base_remap_addr_mt6985));
		} else {
			debug1.ap = debug1.dev0;
			debug2.ap = debug2.dev0;
			debug3.ap = debug3.dev0;
			debug5.ap = debug5.dev0;
			debug6.ap = debug6.dev0;
			debug8.ap = debug8.dev0;
		}
	} else
		pr_notice("[%s] uarthub_get_hwccf_univpll_on_info_mt6985=[0]\n", __func__);
	uarthub_clk_univpll_ctrl_mt6985(0);

	d0_wait_for_send_xoff = ((((debug1.dev0 & 0xE0) >> 5) == 1) ? 1 : 0);
	d1_wait_for_send_xoff = ((((debug1.dev1 & 0xE0) >> 5) == 1) ? 1 : 0);
	d2_wait_for_send_xoff = ((((debug1.dev2 & 0xE0) >> 5) == 1) ? 1 : 0);
	cmm_wait_for_send_xoff = ((((debug1.cmm & 0xE0) >> 5) == 1) ? 1 : 0);
	ap_wait_for_send_xoff = ((((debug1.ap & 0xE0) >> 5) == 1) ? 1 : 0);

	d0_detect_xoff = ((debug8.dev0 & 0x8) >> 3);
	d1_detect_xoff = ((debug8.dev1 & 0x8) >> 3);
	d2_detect_xoff = ((debug8.dev2 & 0x8) >> 3);
	cmm_detect_xoff = ((debug8.cmm & 0x8) >> 3);
	ap_detect_xoff = ((debug8.ap & 0x8) >> 3);

	d0_rx_bcnt = (((debug5.dev0 & 0xF0) >> 4) + ((debug6.dev0 & 0x3) << 4));
	d1_rx_bcnt = (((debug5.dev1 & 0xF0) >> 4) + ((debug6.dev1 & 0x3) << 4));
	d2_rx_bcnt = (((debug5.dev2 & 0xF0) >> 4) + ((debug6.dev2 & 0x3) << 4));
	cmm_rx_bcnt = (((debug5.cmm & 0xF0) >> 4) + ((debug6.cmm & 0x3) << 4));
	ap_rx_bcnt = (((debug5.ap & 0xF0) >> 4) + ((debug6.ap & 0x3) << 4));
	d0_tx_bcnt = (((debug2.dev0 & 0xF0) >> 4) + ((debug3.dev0 & 0x3) << 4));
	d1_tx_bcnt = (((debug2.dev1 & 0xF0) >> 4) + ((debug3.dev1 & 0x3) << 4));
	d2_tx_bcnt = (((debug2.dev2 & 0xF0) >> 4) + ((debug3.dev2 & 0x3) << 4));
	cmm_tx_bcnt = (((debug2.cmm & 0xF0) >> 4) + ((debug3.cmm & 0x3) << 4));
	ap_tx_bcnt = (((debug2.ap & 0xF0) >> 4) + ((debug3.ap & 0x3) << 4));

	if (trigger_point != DUMP0) {
		len = 0;
		if (cur_rx_pkt_cnt_d0 == cur_rx_pkt_cnt_d1 &&
				cur_rx_pkt_cnt_d1 == cur_rx_pkt_cnt_d2) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s], dump1, pcnt=[R:%d",
				def_tag, ((tag == NULL) ? "null" : tag),
				cur_rx_pkt_cnt_d0);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				"[%s][%s], dump1, pcnt=[R:%d-%d-%d",
				def_tag, ((tag == NULL) ? "null" : tag),
				cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2);
		}

		if (cur_rx_pkt_cnt_d0 == cur_rx_pkt_cnt_d1 &&
				cur_rx_pkt_cnt_d1 == cur_rx_pkt_cnt_d2) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d]", cur_tx_pkt_cnt_d0);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d]",
				cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		}

		if (d0_rx_bcnt == d1_rx_bcnt && d1_rx_bcnt == d2_rx_bcnt &&
				d2_rx_bcnt == cmm_rx_bcnt && cmm_rx_bcnt == ap_rx_bcnt) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",bcnt=[R:%d", d0_rx_bcnt);
		} else {
			if (g_enable_apuart_debug_info_mt6985 == 1) {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",bcnt=[R:%d-%d-%d-%d-%d",
					d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt,
					cmm_rx_bcnt, ap_rx_bcnt);
			} else {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",bcnt=[R:%d-%d-%d-%d",
					d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt, cmm_rx_bcnt);
			}
		}

		if (d0_tx_bcnt == d1_tx_bcnt && d1_tx_bcnt == d2_tx_bcnt &&
				d2_tx_bcnt == cmm_tx_bcnt && cmm_tx_bcnt == ap_tx_bcnt) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d]", d0_tx_bcnt);
		} else {
			if (g_enable_apuart_debug_info_mt6985 == 1) {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",T:%d-%d-%d-%d-%d]",
					d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt,
					cmm_tx_bcnt, ap_tx_bcnt);
			} else {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",T:%d-%d-%d-%d]",
					d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt, cmm_tx_bcnt);
			}
		}

		if (d0_wait_for_send_xoff == d1_wait_for_send_xoff &&
				d1_wait_for_send_xoff == d2_wait_for_send_xoff &&
				d2_wait_for_send_xoff == cmm_wait_for_send_xoff &&
				cmm_wait_for_send_xoff == ap_wait_for_send_xoff) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",wsend_xoff=[%d]", d0_wait_for_send_xoff);
		} else {
			if (g_enable_apuart_debug_info_mt6985 == 1) {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",wsend_xoff=[%d-%d-%d-%d-%d]",
					d0_wait_for_send_xoff, d1_wait_for_send_xoff,
					d2_wait_for_send_xoff, cmm_wait_for_send_xoff,
					ap_wait_for_send_xoff);
			} else {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",wsend_xoff=[%d-%d-%d-%d]",
					d0_wait_for_send_xoff, d1_wait_for_send_xoff,
					d2_wait_for_send_xoff, cmm_wait_for_send_xoff);
			}
		}

		if (d0_detect_xoff == d1_detect_xoff &&
				d1_detect_xoff == d2_detect_xoff &&
				d2_detect_xoff == cmm_detect_xoff &&
				cmm_detect_xoff == ap_detect_xoff) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d]", d0_detect_xoff);
		} else {
			if (g_enable_apuart_debug_info_mt6985 == 1) {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",det_xoff=[%d-%d-%d-%d-%d]",
					d0_detect_xoff, d1_detect_xoff,
					d2_detect_xoff, cmm_detect_xoff,
					ap_detect_xoff);
			} else {
				len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
					",det_xoff=[%d-%d-%d-%d]",
					d0_detect_xoff, d1_detect_xoff,
					d2_detect_xoff, cmm_detect_xoff);
			}
		}

		pr_info("%s\n", dmp_info_buf);
	}

	pre_trigger_point = trigger_point;
	return 0;
}

int uarthub_dump_debug_clk_info_mt6985(const char *tag)
{
	int val = 0;
	unsigned int spm_res1 = 0, spm_res2 = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	int len = 0;

	if (uarthub_is_apb_bus_clk_enable_mt6985() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	val = DBG_GET_intfhub_dbg_sel(DBG_ADDR);
	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] IDBG=[0x%x]", ((tag == NULL) ? "null" : tag), val);

	val = UARTHUB_REG_READ(STA0_ADDR);
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);

	val = uarthub_is_apb_bus_clk_enable_mt6985();
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",APB=[0x%x]", val);

	if (val == 0) {
		pr_info("%s\n", dmp_info_buf);
		return -1;
	}

	val = uarthub_get_uarthub_cg_info_mt6985();
	if (val >= 0) {
		/* the expect value is 0x0 */
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HCG=[0x%x]", val);
	}

	val = uarthub_get_peri_clk_info_mt6985();
	if (val >= 0) {
		/* the expect value is 0x800 */
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UPCLK=[0x%x]", val);
	}

	val = uarthub_get_spm_res_info_mt6985(&spm_res1, &spm_res2);
	if (val == 1) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[1]");
	} else if (val == 0) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[0(0x%x/0x%x)]", spm_res1, spm_res2);
	}

	val = uarthub_get_hwccf_univpll_on_info_mt6985();
	if (val >= 0) {
		/* the expect value is 0x1 */
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UVPLL=[%d]", val);
	}

	val = uarthub_get_uart_mux_info_mt6985();
	if (val >= 0) {
		/* the expect value is 0x2 */
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UMUX=[0x%x]", val);
	}

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x-0x%x-0x%x]", dev0_sta, dev1_sta, dev2_sta);
	}

	val = UARTHUB_REG_READ(STA0_ADDR);
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);

	val = DBG_GET_intfhub_dbg_sel(DBG_ADDR);
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_debug_byte_cnt_info_mt6985(const char *tag)
{
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug2 = {0};
	struct uarthub_uartip_debug_info debug3 = {0};
	struct uarthub_uartip_debug_info debug4 = {0};
	struct uarthub_uartip_debug_info debug5 = {0};
	struct uarthub_uartip_debug_info debug6 = {0};
	struct uarthub_uartip_debug_info debug7 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0, cmm_sta = 0, ap_sta = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int val = 0;

	uarthub_clk_univpll_ctrl_mt6985(1);
	if (uarthub_get_hwccf_univpll_on_info_mt6985() == 0) {
		pr_notice("[%s] uarthub_get_hwccf_univpll_on_info_mt6985=[0]\n", __func__);
		uarthub_clk_univpll_ctrl_mt6985(0);
		return -1;
	}

	val = DBG_GET_intfhub_dbg_sel(DBG_ADDR);
	len = 0;
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] IDBG=[0x%x]", ((tag == NULL) ? "null" : tag), val);

	val = UARTHUB_REG_READ(STA0_ADDR);
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);

	debug1.dev0 = UARTHUB_REG_READ(DEBUG_1(dev0_base_remap_addr_mt6985));
	debug2.dev0 = UARTHUB_REG_READ(DEBUG_2(dev0_base_remap_addr_mt6985));
	debug3.dev0 = UARTHUB_REG_READ(DEBUG_3(dev0_base_remap_addr_mt6985));
	debug4.dev0 = UARTHUB_REG_READ(DEBUG_4(dev0_base_remap_addr_mt6985));
	debug5.dev0 = UARTHUB_REG_READ(DEBUG_5(dev0_base_remap_addr_mt6985));
	debug6.dev0 = UARTHUB_REG_READ(DEBUG_6(dev0_base_remap_addr_mt6985));
	debug7.dev0 = UARTHUB_REG_READ(DEBUG_7(dev0_base_remap_addr_mt6985));
	debug8.dev0 = UARTHUB_REG_READ(DEBUG_8(dev0_base_remap_addr_mt6985));

	debug1.dev1 = UARTHUB_REG_READ(DEBUG_1(dev1_base_remap_addr_mt6985));
	debug2.dev1 = UARTHUB_REG_READ(DEBUG_2(dev1_base_remap_addr_mt6985));
	debug3.dev1 = UARTHUB_REG_READ(DEBUG_3(dev1_base_remap_addr_mt6985));
	debug4.dev1 = UARTHUB_REG_READ(DEBUG_4(dev1_base_remap_addr_mt6985));
	debug5.dev1 = UARTHUB_REG_READ(DEBUG_5(dev1_base_remap_addr_mt6985));
	debug6.dev1 = UARTHUB_REG_READ(DEBUG_6(dev1_base_remap_addr_mt6985));
	debug7.dev1 = UARTHUB_REG_READ(DEBUG_7(dev1_base_remap_addr_mt6985));
	debug8.dev1 = UARTHUB_REG_READ(DEBUG_8(dev1_base_remap_addr_mt6985));

	debug1.dev2 = UARTHUB_REG_READ(DEBUG_1(dev2_base_remap_addr_mt6985));
	debug2.dev2 = UARTHUB_REG_READ(DEBUG_2(dev2_base_remap_addr_mt6985));
	debug3.dev2 = UARTHUB_REG_READ(DEBUG_3(dev2_base_remap_addr_mt6985));
	debug4.dev2 = UARTHUB_REG_READ(DEBUG_4(dev2_base_remap_addr_mt6985));
	debug5.dev2 = UARTHUB_REG_READ(DEBUG_5(dev2_base_remap_addr_mt6985));
	debug6.dev2 = UARTHUB_REG_READ(DEBUG_6(dev2_base_remap_addr_mt6985));
	debug7.dev2 = UARTHUB_REG_READ(DEBUG_7(dev2_base_remap_addr_mt6985));
	debug8.dev2 = UARTHUB_REG_READ(DEBUG_8(dev2_base_remap_addr_mt6985));

	debug1.cmm = UARTHUB_REG_READ(DEBUG_1(cmm_base_remap_addr_mt6985));
	debug2.cmm = UARTHUB_REG_READ(DEBUG_2(cmm_base_remap_addr_mt6985));
	debug3.cmm = UARTHUB_REG_READ(DEBUG_3(cmm_base_remap_addr_mt6985));
	debug4.cmm = UARTHUB_REG_READ(DEBUG_4(cmm_base_remap_addr_mt6985));
	debug5.cmm = UARTHUB_REG_READ(DEBUG_5(cmm_base_remap_addr_mt6985));
	debug6.cmm = UARTHUB_REG_READ(DEBUG_6(cmm_base_remap_addr_mt6985));
	debug7.cmm = UARTHUB_REG_READ(DEBUG_7(cmm_base_remap_addr_mt6985));
	debug8.cmm = UARTHUB_REG_READ(DEBUG_8(cmm_base_remap_addr_mt6985));

	if (apuart3_base_remap_addr_mt6985 != NULL && g_enable_apuart_debug_info_mt6985 == 1) {
		debug1.ap = UARTHUB_REG_READ(DEBUG_1(apuart3_base_remap_addr_mt6985));
		debug2.ap = UARTHUB_REG_READ(DEBUG_2(apuart3_base_remap_addr_mt6985));
		debug3.ap = UARTHUB_REG_READ(DEBUG_3(apuart3_base_remap_addr_mt6985));
		debug4.ap = UARTHUB_REG_READ(DEBUG_4(apuart3_base_remap_addr_mt6985));
		debug5.ap = UARTHUB_REG_READ(DEBUG_5(apuart3_base_remap_addr_mt6985));
		debug6.ap = UARTHUB_REG_READ(DEBUG_6(apuart3_base_remap_addr_mt6985));
		debug7.ap = UARTHUB_REG_READ(DEBUG_7(apuart3_base_remap_addr_mt6985));
		debug8.ap = UARTHUB_REG_READ(DEBUG_8(apuart3_base_remap_addr_mt6985));
	} else {
		debug1.ap = debug1.dev0;
		debug2.ap = debug2.dev0;
		debug3.ap = debug3.dev0;
		debug4.ap = debug4.dev0;
		debug5.ap = debug5.dev0;
		debug6.ap = debug6.dev0;
		debug7.ap = debug7.dev0;
		debug8.ap = debug8.dev0;
	}

	uarthub_clk_univpll_ctrl_mt6985(0);

	dev0_sta = (((debug5.dev0 & 0xF0) >> 4) + ((debug6.dev0 & 0x3) << 4));
	dev1_sta = (((debug5.dev1 & 0xF0) >> 4) + ((debug6.dev1 & 0x3) << 4));
	dev2_sta = (((debug5.dev2 & 0xF0) >> 4) + ((debug6.dev2 & 0x3) << 4));
	cmm_sta = (((debug5.cmm & 0xF0) >> 4) + ((debug6.cmm & 0x3) << 4));
	ap_sta = (((debug5.ap & 0xF0) >> 4) + ((debug6.ap & 0x3) << 4));
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",bcnt=[R:%d", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",bcnt=[R:%d-%d-%d-%d-%d",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",bcnt=[R:%d-%d-%d-%d",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = (((debug2.dev0 & 0xF0) >> 4) + ((debug3.dev0 & 0x3) << 4));
	dev1_sta = (((debug2.dev1 & 0xF0) >> 4) + ((debug3.dev1 & 0x3) << 4));
	dev2_sta = (((debug2.dev2 & 0xF0) >> 4) + ((debug3.dev2 & 0x3) << 4));
	cmm_sta = (((debug2.cmm & 0xF0) >> 4) + ((debug3.cmm & 0x3) << 4));
	ap_sta = (((debug2.ap & 0xF0) >> 4) + ((debug3.ap & 0x3) << 4));
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d]", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = (debug7.dev0 & 0x3F);
	dev1_sta = (debug7.dev1 & 0x3F);
	dev2_sta = (debug7.dev2 & 0x3F);
	cmm_sta = (debug7.cmm & 0x3F);
	ap_sta = (debug7.ap & 0x3F);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",fifo_woffset=[R:%d", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",fifo_woffset=[R:%d-%d-%d-%d-%d",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",fifo_woffset=[R:%d-%d-%d-%d",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = (debug4.dev0 & 0x3F);
	dev1_sta = (debug4.dev1 & 0x3F);
	dev2_sta = (debug4.dev2 & 0x3F);
	cmm_sta = (debug4.cmm & 0x3F);
	ap_sta = (debug4.ap & 0x3F);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d]", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = (((debug4.dev0 & 0xC0) >> 6) + ((debug5.dev0 & 0xF) << 2));
	dev1_sta = (((debug4.dev1 & 0xC0) >> 6) + ((debug5.dev1 & 0xF) << 2));
	dev2_sta = (((debug4.dev2 & 0xC0) >> 6) + ((debug5.dev2 & 0xF) << 2));
	cmm_sta = (((debug4.cmm & 0xC0) >> 6) + ((debug5.cmm & 0xF) << 2));
	ap_sta = (((debug4.ap & 0xC0) >> 6) + ((debug5.ap & 0xF) << 2));
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",fifo_tx_roffset=[%d]", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",fifo_tx_roffset=[%d-%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",fifo_tx_roffset=[%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = ((debug6.dev0 & 0xFC) >> 2);
	dev1_sta = ((debug6.dev1 & 0xFC) >> 2);
	dev2_sta = ((debug6.dev2 & 0xFC) >> 2);
	cmm_sta = ((debug6.cmm & 0xFC) >> 2);
	ap_sta = ((debug6.ap & 0xFC) >> 2);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",offset_dma=[R:%d", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",offset_dma=[R:%d-%d-%d-%d-%d",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",offset_dma=[R:%d-%d-%d-%d",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = ((debug3.dev0 & 0xFC) >> 2);
	dev1_sta = ((debug3.dev1 & 0xFC) >> 2);
	dev2_sta = ((debug3.dev2 & 0xFC) >> 2);
	cmm_sta = ((debug3.cmm & 0xFC) >> 2);
	ap_sta = ((debug3.ap & 0xFC) >> 2);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d]", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",T:%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = ((debug1.dev0 & 0xE0) >> 5);
	dev1_sta = ((debug1.dev1 & 0xE0) >> 5);
	dev2_sta = ((debug1.dev2 & 0xE0) >> 5);
	cmm_sta = ((debug1.cmm & 0xE0) >> 5);
	ap_sta = ((debug1.ap & 0xE0) >> 5);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",wsend_xoff=[%d]", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",wsend_xoff=[%d-%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",wsend_xoff=[%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	dev0_sta = ((debug8.dev0 & 0x8) >> 3);
	dev1_sta = ((debug8.dev1 & 0x8) >> 3);
	dev2_sta = ((debug8.dev2 & 0x8) >> 3);
	cmm_sta = ((debug8.cmm & 0x8) >> 3);
	ap_sta = ((debug8.ap & 0x8) >> 3);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta &&
			dev2_sta == cmm_sta && cmm_sta == ap_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",det_xoff=[%d]", dev0_sta);
	} else {
		if (g_enable_apuart_debug_info_mt6985 == 1) {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d-%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta, ap_sta);
		} else {
			len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d-%d-%d-%d]",
				dev0_sta, dev1_sta, dev2_sta, cmm_sta);
		}
	}

	val = uarthub_get_hwccf_univpll_on_info_mt6985();
	if (val >= 0) {
		/* the expect value is 0x1 */
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UVPLL=[%d]", val);
	}

	val = uarthub_get_uart_mux_info_mt6985();
	if (val >= 0) {
		/* the expect value is 0x2 */
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UMUX=[0x%x]", val);
	}

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x]", dev0_sta);
	} else {
		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x-0x%x-0x%x]", dev0_sta, dev1_sta, dev2_sta);
	}

	val = UARTHUB_REG_READ(STA0_ADDR);
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);

	val = DBG_GET_intfhub_dbg_sel(DBG_ADDR);
	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_debug_apdma_uart_info_mt6985(const char *tag)
{
	const char *def_tag = "HUB_DBG_APMDA";

	pr_info("[%s][%s] 0=[0x%x],4=[0x%x],8=[0x%x],c=[0x%x],10=[0x%x],14=[0x%x],18=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x00),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x04),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x08),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x0c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x10),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x14),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x18));

	pr_info("[%s][%s] 1c=[0x%x],20=[0x%x],24=[0x%x],28=[0x%x],2c=[0x%x],30=[0x%x],34=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x1c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x20),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x24),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x28),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x2c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x30),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x34));

	pr_info("[%s][%s] 38=[0x%x],3c=[0x%x],40=[0x%x],44=[0x%x],48=[0x%x],4c=[0x%x],50=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x38),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x3c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x40),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x44),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x48),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x4c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x50));

	pr_info("[%s][%s] 54=[0x%x],58=[0x%x],5c=[0x%x],60=[0x%x],64=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x54),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x58),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x5c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x60),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6985 + 0x64));

	return 0;
}

#define UARTHUB_IRQ_OP_LOG_SIZE     5
#define UARTHUB_LOG_IRQ_PKT_SIZE    12
#define UARTHUB_LOG_IRQ_IDX_ADDR(addr) (addr)

#define UARTHUB_TSK_OP_LOG_SIZE     20
#define UARTHUB_LOG_TSK_PKT_SIZE    20
#define UARTHUB_LOG_TSK_IDX_ADDR(addr) \
		(addr + (UARTHUB_LOG_IRQ_PKT_SIZE * UARTHUB_IRQ_OP_LOG_SIZE) + 4)

#define UARTHUB_CK_CNT_ADDR(addr) \
	(UARTHUB_LOG_TSK_IDX_ADDR(addr) + (UARTHUB_TSK_OP_LOG_SIZE * UARTHUB_LOG_TSK_PKT_SIZE) + 4)


#define UARTHUB_LAST_CK_ON(addr) (UARTHUB_CK_CNT_ADDR(addr) + 4)
#define UARTHUB_LAST_CK_ON_CNT(addr) (UARTHUB_LAST_CK_ON(addr) + 8)

#define UARTHUB_TMP_BUF_SZ  512
char g_buf_m6985[UARTHUB_TMP_BUF_SZ];

int uarthub_dump_sspm_log_mt6985(const char *tag)
{
	void __iomem *log_addr = NULL;
	int i, n, used;
	uint32_t val, irq_idx = 0, tsk_idx = 0;
	uint32_t v1, v2, v3;
	uint64_t t;
	char *tmp;
	const char *def_tag = "HUB_DBG_SSPM";

	g_buf_m6985[0] = '\0';
	log_addr = UARTHUB_LOG_IRQ_IDX_ADDR(sys_sram_remap_addr_mt6985);
	irq_idx = UARTHUB_REG_READ(log_addr);
	log_addr += 4;

	tmp = g_buf_m6985;
	used = 0;
	for (i = 0; i < UARTHUB_IRQ_OP_LOG_SIZE; i++) {
		t = UARTHUB_REG_READ(log_addr);
		t = t << 32 | UARTHUB_REG_READ(log_addr + 4);
		n = snprintf(tmp + used, UARTHUB_TMP_BUF_SZ - used, "[%llu:%X] ",
							t,
							UARTHUB_REG_READ(log_addr + 8));
		if (n > 0)
			used += n;
		log_addr += UARTHUB_LOG_IRQ_PKT_SIZE;
	}
	pr_info("[%s][%s] [%x] %s",
		def_tag, ((tag == NULL) ? "null" : tag), irq_idx, g_buf_m6985);

	log_addr = UARTHUB_LOG_TSK_IDX_ADDR(sys_sram_remap_addr_mt6985);
	tsk_idx = UARTHUB_REG_READ(log_addr);
	log_addr += 4;
	g_buf_m6985[0] = '\0';
	tmp = g_buf_m6985;
	used = 0;
	for (i = 0; i < UARTHUB_TSK_OP_LOG_SIZE; i++) {
		t = UARTHUB_REG_READ(log_addr);
		t = t << 32 | UARTHUB_REG_READ(log_addr + 4);
		n = snprintf(tmp + used, UARTHUB_TMP_BUF_SZ - used, "[%llu:%x-%x-%x]",
							t,
							UARTHUB_REG_READ(log_addr + 8),
							UARTHUB_REG_READ(log_addr + 12),
							UARTHUB_REG_READ(log_addr + 16));
		if (n > 0) {
			used += n;
			if ((i % (UARTHUB_TSK_OP_LOG_SIZE/2))
					== ((UARTHUB_TSK_OP_LOG_SIZE/2) - 1)) {
				pr_info("[%s][%s] [%x] %s",
					def_tag, ((tag == NULL) ? "null" : tag),
					tsk_idx, g_buf_m6985);
				g_buf_m6985[0] = '\0';
				tmp = g_buf_m6985;
				used = 0;
			}
		}
		log_addr += UARTHUB_LOG_TSK_PKT_SIZE;
	}

	log_addr = UARTHUB_CK_CNT_ADDR(sys_sram_remap_addr_mt6985);
	val = UARTHUB_REG_READ(log_addr);

	log_addr = UARTHUB_LAST_CK_ON(sys_sram_remap_addr_mt6985);
	v1 = UARTHUB_REG_READ(log_addr);
	v2 = UARTHUB_REG_READ(log_addr + 4);

	log_addr = UARTHUB_LAST_CK_ON_CNT(sys_sram_remap_addr_mt6985);
	v3 = UARTHUB_REG_READ(log_addr);

	pr_info("[%s][%s] off/on cnt=[%d][%d] ckon=[%x][%x] cnt=[%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		(val & 0xFFFF), (val >> 16),
		v1, v2, v3);

	return 0;
}

int uarthub_clk_univpll_ctrl_mt6985(int clk_on)
{
	int ret = 0;
#if UARTHUB_DEBUG_LOG
	unsigned int before_pll_sta = 0;
#endif

#if !(UARTHUB_SUPPORT_SSPM_DRIVER)
	if (clk_on == 0)
		return 0;
#endif

	if (clk_apmixedsys_univpll_mt6985 == NULL || IS_ERR(clk_apmixedsys_univpll_mt6985)) {
		pr_notice("[%s] clk_apmixedsys_univpll_mt6985 is not init\n", __func__);
		return -1;
	}

	if (mutex_lock_killable(&g_lock_univpll_clk_mt6985)) {
		pr_notice("[%s] mutex_lock_killable(g_lock_univpll_clk_mt6985) fail\n", __func__);
		return UARTHUB_ERR_MUTEX_LOCK_FAIL;
	}

	if (clk_on == 1 && g_univpll_clk_ref_count_mt6985 >= 1) {
		mutex_unlock(&g_lock_univpll_clk_mt6985);
		return 0;
	} else if (clk_on == 0 && g_univpll_clk_ref_count_mt6985 <= 0) {
		mutex_unlock(&g_lock_univpll_clk_mt6985);
		return 0;
	}

#if UARTHUB_DEBUG_LOG
	before_pll_sta = uarthub_get_hwccf_univpll_on_info_mt6985();
#endif

	if (clk_on == 1) {
		ret = clk_prepare_enable(clk_apmixedsys_univpll_mt6985);
		if (ret) {
			pr_notice("[%s] UNIVPLL ON fail(%d)\n", __func__, ret);
			mutex_unlock(&g_lock_univpll_clk_mt6985);
			return ret;
		}
		g_univpll_clk_ref_count_mt6985++;

#if UARTHUB_DEBUG_LOG
		pr_info("[%s] UNIVPLL ON pass, ref_cnt=[%d], pll_sta=[%d --> %d] +\n",
			__func__, g_univpll_clk_ref_count_mt6985,
			before_pll_sta, uarthub_get_hwccf_univpll_on_info_mt6985());
#endif
	} else {
		g_univpll_clk_ref_count_mt6985--;
		clk_disable_unprepare(clk_apmixedsys_univpll_mt6985);

#if UARTHUB_DEBUG_LOG
		pr_info("[%s] UNIVPLL OFF pass, ref_cnt=[%d], pll_sta=[%d --> %d] -\n",
			__func__, g_univpll_clk_ref_count_mt6985,
			before_pll_sta, uarthub_get_hwccf_univpll_on_info_mt6985());
#endif
	}

	mutex_unlock(&g_lock_univpll_clk_mt6985);
	return 0;
}

int uarthub_get_host_status_mt6985(int dev_index)
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

int uarthub_get_host_wakeup_status_mt6985(void)
{
	int state = 0;
	int state0 = 0, state1 = 0, state2 = 0;

	state0 = ((UARTHUB_REG_READ_BIT(DEV0_STA_ADDR, 0x3) == 0x2) ? 1 : 0);
	state1 = ((UARTHUB_REG_READ_BIT(DEV1_STA_ADDR, 0x3) == 0x2) ? 1 : 0);
	state2 = ((UARTHUB_REG_READ_BIT(DEV2_STA_ADDR, 0x3) == 0x2) ? 1 : 0);

	state = (state0 | (state1 << 1) | (state2 << 2));
	return state;
}

int uarthub_get_host_set_fw_own_status_mt6985(void)
{
	int state = 0;
	int state0 = 0, state1 = 0, state2 = 0;

	state0 = ((UARTHUB_REG_READ_BIT(DEV0_STA_ADDR, 0x3) == 0x1) ? 1 : 0);
	state1 = ((UARTHUB_REG_READ_BIT(DEV1_STA_ADDR, 0x3) == 0x1) ? 1 : 0);
	state2 = ((UARTHUB_REG_READ_BIT(DEV2_STA_ADDR, 0x3) == 0x1) ? 1 : 0);

	state = (state0 | (state1 << 1) | (state2 << 2));
	return state;
}

int uarthub_is_host_trx_idle_mt6985(int dev_index, enum uarthub_trx_type trx)
{
	int state = -1;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
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

int uarthub_set_host_trx_request_mt6985(int dev_index, enum uarthub_trx_type trx)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (trx == RX) {
			DEV0_STA_SET_SET_dev0_sw_rx_set(DEV0_STA_SET_ADDR, 1);
		} else if (trx == TX) {
			DEV0_STA_SET_SET_dev0_sw_tx_set(DEV0_STA_SET_ADDR, 1);
		} else {
			UARTHUB_REG_WRITE(DEV0_STA_SET_ADDR,
				(REG_FLD_MASK(DEV0_STA_SET_FLD_dev0_sw_rx_set) |
				REG_FLD_MASK(DEV0_STA_SET_FLD_dev0_sw_tx_set)));
		}
	} else if (dev_index == 1) {
		if (trx == RX) {
			DEV1_STA_SET_SET_dev1_sw_rx_set(DEV1_STA_SET_ADDR, 1);
		} else if (trx == TX) {
			DEV1_STA_SET_SET_dev1_sw_tx_set(DEV1_STA_SET_ADDR, 1);
		} else {
			UARTHUB_REG_WRITE(DEV1_STA_SET_ADDR,
				(REG_FLD_MASK(DEV1_STA_SET_FLD_dev1_sw_rx_set) |
				REG_FLD_MASK(DEV1_STA_SET_FLD_dev1_sw_tx_set)));
		}
	} else if (dev_index == 2) {
		if (trx == RX) {
			DEV2_STA_SET_SET_dev2_sw_rx_set(DEV2_STA_SET_ADDR, 1);
		} else if (trx == TX) {
			DEV2_STA_SET_SET_dev2_sw_tx_set(DEV2_STA_SET_ADDR, 1);
		} else {
			UARTHUB_REG_WRITE(DEV2_STA_SET_ADDR,
				(REG_FLD_MASK(DEV2_STA_SET_FLD_dev2_sw_rx_set) |
				REG_FLD_MASK(DEV2_STA_SET_FLD_dev2_sw_tx_set)));
		}
	}

	return 0;
}

int uarthub_clear_host_trx_request_mt6985(int dev_index, enum uarthub_trx_type trx)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (trx == RX) {
			UARTHUB_REG_WRITE(DEV0_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_sw_rx_clr) |
				REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_hw_rx_clr)));
		} else if (trx == TX) {
			UARTHUB_REG_WRITE(DEV0_STA_CLR_ADDR,
				REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_sw_tx_clr));
		} else {
			UARTHUB_REG_WRITE(DEV0_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_sw_rx_clr) |
				REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_sw_tx_clr) |
				REG_FLD_MASK(DEV0_STA_CLR_FLD_dev0_hw_rx_clr)));
		}
	} else if (dev_index == 1) {
		if (trx == RX) {
			UARTHUB_REG_WRITE(DEV1_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_sw_rx_clr) |
				REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_hw_rx_clr)));
		} else if (trx == TX) {
			UARTHUB_REG_WRITE(DEV1_STA_CLR_ADDR,
				REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_sw_tx_clr));
		} else {
			UARTHUB_REG_WRITE(DEV1_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_sw_rx_clr) |
				REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_sw_tx_clr) |
				REG_FLD_MASK(DEV1_STA_CLR_FLD_dev1_hw_rx_clr)));
		}
	} else if (dev_index == 2) {
		if (trx == RX) {
			UARTHUB_REG_WRITE(DEV2_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_sw_rx_clr) |
				REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_hw_rx_clr)));
		} else if (trx == TX) {
			UARTHUB_REG_WRITE(DEV2_STA_CLR_ADDR,
				REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_sw_tx_clr));
		} else {
			UARTHUB_REG_WRITE(DEV2_STA_CLR_ADDR,
				(REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_sw_rx_clr) |
				REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_sw_tx_clr) |
				REG_FLD_MASK(DEV2_STA_CLR_FLD_dev2_hw_rx_clr)));
		}
	}

	return 0;
}

int uarthub_get_host_byte_cnt_mt6985(int dev_index, enum uarthub_trx_type trx)
{
	int byte_cnt = -1;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx == TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return -1;
	}

	if (dev_index == 0) {
		if (trx == RX) {
			byte_cnt = (((UARTHUB_REG_READ(DEBUG_5(
					dev0_base_remap_addr_mt6985)) & 0xF0) >> 4) +
				((UARTHUB_REG_READ(DEBUG_6(
					dev0_base_remap_addr_mt6985)) & 0x3) << 4));
		} else if (trx == TX) {
			byte_cnt = (((UARTHUB_REG_READ(DEBUG_2(
					dev0_base_remap_addr_mt6985)) & 0xF0) >> 4) +
				((UARTHUB_REG_READ(DEBUG_3(
					dev0_base_remap_addr_mt6985)) & 0x3) << 4));
		}
	} else if (dev_index == 1) {
		if (trx == RX) {
			byte_cnt = (((UARTHUB_REG_READ(DEBUG_5(
					dev1_base_remap_addr_mt6985)) & 0xF0) >> 4) +
				((UARTHUB_REG_READ(DEBUG_6(
					dev1_base_remap_addr_mt6985)) & 0x3) << 4));
		} else if (trx == TX) {
			byte_cnt = (((UARTHUB_REG_READ(DEBUG_2(
					dev1_base_remap_addr_mt6985)) & 0xF0) >> 4) +
				((UARTHUB_REG_READ(DEBUG_3(
					dev1_base_remap_addr_mt6985)) & 0x3) << 4));
		}
	} else if (dev_index == 2) {
		if (trx == RX) {
			byte_cnt = (((UARTHUB_REG_READ(DEBUG_5(
					dev2_base_remap_addr_mt6985)) & 0xF0) >> 4) +
				((UARTHUB_REG_READ(DEBUG_6(
					dev2_base_remap_addr_mt6985)) & 0x3) << 4));
		} else if (trx == TX) {
			byte_cnt = (((UARTHUB_REG_READ(DEBUG_2(
					dev2_base_remap_addr_mt6985)) & 0xF0) >> 4) +
				((UARTHUB_REG_READ(DEBUG_3(
					dev2_base_remap_addr_mt6985)) & 0x3) << 4));
		}
	}

	return byte_cnt;
}

int uarthub_get_cmm_byte_cnt_mt6985(enum uarthub_trx_type trx)
{
	int byte_cnt = -1;

	if (trx == TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return -1;
	}

	if (trx == RX) {
		byte_cnt = (((UARTHUB_REG_READ(DEBUG_5(cmm_base_remap_addr_mt6985)) & 0xF0) >> 4) +
			((UARTHUB_REG_READ(DEBUG_6(cmm_base_remap_addr_mt6985)) & 0x3) << 4));
	} else {
		byte_cnt = (((UARTHUB_REG_READ(DEBUG_2(cmm_base_remap_addr_mt6985)) & 0xF0) >> 4) +
			((UARTHUB_REG_READ(DEBUG_3(cmm_base_remap_addr_mt6985)) & 0x3) << 4));
	}

	return byte_cnt;
}

int uarthub_config_crc_ctrl_mt6985(int enable)
{
	CON2_SET_crc_en(CON2_ADDR, enable);
	return 0;
}

int uarthub_config_bypass_ctrl_mt6985(int enable)
{
	CON2_SET_intfhub_bypass(CON2_ADDR, enable);
	return 0;
}

int uarthub_config_host_fifoe_ctrl_mt6985(int dev_index, int enable)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6985), (0x80 | enable));
#if UARTHUB_ENABLE_MD_CHANNEL
	else if (dev_index == 1)
		UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6985), (0x80 | enable));
#endif
	else if (dev_index == 2)
		UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6985), (0x80 | enable));

	return 0;
}

int uarthub_get_rx_error_crc_info_mt6985(int dev_index, int *p_crc_error_data, int *p_crc_result)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		if (p_crc_error_data)
			*p_crc_error_data =
				DEV0_RX_ERR_CRC_STA_GET_dev0_rx_err_crc_data(
					DEV0_RX_ERR_CRC_STA_ADDR);
		if (p_crc_result)
			*p_crc_result =
				DEV0_RX_ERR_CRC_STA_GET_dev0_rx_err_crc_result(
					DEV0_RX_ERR_CRC_STA_ADDR);
	} else if (dev_index == 1) {
		if (p_crc_error_data)
			*p_crc_error_data =
				DEV1_RX_ERR_CRC_STA_GET_dev1_rx_err_crc_data(
					DEV1_RX_ERR_CRC_STA_ADDR);
		if (p_crc_result)
			*p_crc_result =
				DEV1_RX_ERR_CRC_STA_GET_dev1_rx_err_crc_result(
					DEV1_RX_ERR_CRC_STA_ADDR);
	} else if (dev_index == 2) {
		if (p_crc_error_data)
			*p_crc_error_data =
				DEV2_RX_ERR_CRC_STA_GET_dev2_rx_err_crc_data(
					DEV2_RX_ERR_CRC_STA_ADDR);
		if (p_crc_result)
			*p_crc_result =
				DEV2_RX_ERR_CRC_STA_GET_dev2_rx_err_crc_result(
					DEV2_RX_ERR_CRC_STA_ADDR);
	}

	return 0;
}

int uarthub_get_trx_timeout_info_mt6985(
	int dev_index, enum uarthub_trx_type trx,
	int *p_timeout_counter, int *p_pkt_counter)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx == TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return -1;
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

int uarthub_get_irq_err_type_mt6985(void)
{
	return UARTHUB_REG_READ(DEV0_IRQ_STA_ADDR);
}

int uarthub_get_spm_res_info_mt6985(unsigned int *pspm_res1, unsigned int *pspm_res2)
{
	unsigned int spm_res1 = 0, spm_res2 = 0;

	if (!spm_remap_addr_mt6985) {
		pr_notice("[%s] spm_remap_addr_mt6985 is NULL\n", __func__);
		return -1;
	}

	spm_res1 = UARTHUB_REG_READ_BIT(spm_remap_addr_mt6985 + SPM_REQ_STA_9,
		SPM_REQ_STA_9_UARTHUB_REQ_MASK) >> SPM_REQ_STA_9_UARTHUB_REQ_SHIFT;

	spm_res2 = UARTHUB_REG_READ_BIT(spm_remap_addr_mt6985 + MD32PCM_SCU_CTRL1,
		MD32PCM_SCU_CTRL1_MASK) >> MD32PCM_SCU_CTRL1_SHIFT;

	if (pspm_res1)
		*pspm_res1 = spm_res1;

	if (pspm_res2)
		*pspm_res2 = spm_res2;

	if (spm_res1 != 0x1D || spm_res2 != 0x17)
		return 0;

	return 1;
}

int uarthub_dump_apuart_debug_ctrl_mt6985(int enable)
{
	g_enable_apuart_debug_info_mt6985 = enable;
	return 0;
}

int uarthub_get_apuart_debug_ctrl_sta_mt6985(void)
{
	return g_enable_apuart_debug_info_mt6985;
}

static int uarthub_get_intfhub_base_addr_mt6985(void)
{
	return UARTHUB_INTFHUB_BASE_ADDR(UARTHUB_BASE_ADDR);
}

static int uarthub_get_uartip_base_addr_mt6985(int dev_index)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		return UARTHUB_DEV_0_BASE_ADDR(UARTHUB_BASE_ADDR);
	else if (dev_index == 1)
		return UARTHUB_DEV_1_BASE_ADDR(UARTHUB_BASE_ADDR);
	else if (dev_index == 2)
		return UARTHUB_DEV_2_BASE_ADDR(UARTHUB_BASE_ADDR);

	return UARTHUB_CMM_BASE_ADDR(UARTHUB_BASE_ADDR);
}

int uarthub_trigger_dvt_testing_mt6985(int type)
{
	pr_info("[%s] type=[%d]\n", __func__, type);
	return 0;
}

