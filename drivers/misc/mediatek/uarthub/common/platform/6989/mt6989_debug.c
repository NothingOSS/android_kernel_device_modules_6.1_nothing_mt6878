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

#include <linux/regmap.h>

static int g_enable_apuart_debug_info;

static int uarthub_get_uart_mux_info_mt6989(void);
static int uarthub_get_uarthub_cg_info_mt6989(void);
#if !(UARTHUB_SUPPORT_FPGA)
static int uarthub_get_peri_uart_pad_mode_mt6989(void);
#endif
static int uarthub_get_peri_clk_info_mt6989(void);
static int uarthub_get_spm_res_info_mt6989(unsigned int *pspm_res1, unsigned int *pspm_res2);
#if !(UARTHUB_SUPPORT_FPGA)
static int uarthub_get_gpio_trx_info_mt6989(struct uarthub_gpio_trx_info *info);
#endif
static int uarthub_clk_univpll_ctrl_mt6989(int clk_on);

struct uarthub_debug_ops_struct mt6989_plat_debug_data = {
	.uarthub_plat_dump_apuart_debug_ctrl = uarthub_dump_apuart_debug_ctrl_mt6989,
	.uarthub_plat_get_apuart_debug_ctrl_sta = uarthub_get_apuart_debug_ctrl_sta_mt6989,
	.uarthub_plat_get_intfhub_base_addr = uarthub_get_intfhub_base_addr_mt6989,
	.uarthub_plat_get_uartip_base_addr = uarthub_get_uartip_base_addr_mt6989,
	.uarthub_plat_dump_uartip_debug_info = uarthub_dump_uartip_debug_info_mt6989,
	.uarthub_plat_dump_intfhub_debug_info = uarthub_dump_intfhub_debug_info_mt6989,
	.uarthub_plat_dump_debug_tx_rx_count = uarthub_dump_debug_tx_rx_count_mt6989,
	.uarthub_plat_dump_debug_clk_info = uarthub_dump_debug_clk_info_mt6989,
	.uarthub_plat_dump_debug_byte_cnt_info = uarthub_dump_debug_byte_cnt_info_mt6989,
	.uarthub_plat_dump_debug_apdma_uart_info = uarthub_dump_debug_apdma_uart_info_mt6989,
	.uarthub_plat_dump_sspm_log = uarthub_dump_sspm_log_mt6989,
	.uarthub_plat_trigger_fpga_testing = uarthub_trigger_fpga_testing_mt6989,
	.uarthub_plat_trigger_dvt_testing = uarthub_trigger_dvt_testing_mt6989,
	.uarthub_plat_verify_combo_connect_sta = uarthub_verify_combo_connect_sta_mt6989,
};

int uarthub_get_uart_mux_info_mt6989(void)
{
	if (!topckgen_base_remap_addr_mt6989) {
		pr_notice("[%s] topckgen_base_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(topckgen_base_remap_addr_mt6989 + CLK_CFG_6,
		CLK_CFG_6_MASK) >> CLK_CFG_6_SHIFT);
}

int uarthub_get_uarthub_cg_info_mt6989(void)
{
	if (!pericfg_ao_remap_addr_mt6989) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6989 + PERI_CG_1,
		PERI_CG_1_UARTHUB_CG_MASK) >> PERI_CG_1_UARTHUB_CG_SHIFT);
}

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_peri_uart_pad_mode_mt6989(void)
{
	if (!pericfg_ao_remap_addr_mt6989) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	/* 1: UART_PAD mode */
	/* 0: UARTHUB mode */
	return (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6989 + PERI_UART_WAKEUP,
		PERI_UART_WAKEUP_MASK) >> PERI_UART_WAKEUP_SHIFT);
}
#endif

int uarthub_get_peri_clk_info_mt6989(void)
{
	if (!pericfg_ao_remap_addr_mt6989) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	return UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6989 + PERI_CLOCK_CON,
		PERI_UART_FBCLK_CKSEL);
}

int uarthub_get_spm_res_info_mt6989(unsigned int *pspm_res1, unsigned int *pspm_res2)
{
	unsigned int spm_res1 = 0, spm_res2 = 0;

	if (!spm_remap_addr_mt6989) {
		pr_notice("[%s] spm_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	spm_res1 = UARTHUB_REG_READ_BIT(spm_remap_addr_mt6989 + SPM_REQ_STA_9,
		SPM_REQ_STA_9_UARTHUB_REQ_MASK) >> SPM_REQ_STA_9_UARTHUB_REQ_SHIFT;

	spm_res2 = UARTHUB_REG_READ_BIT(spm_remap_addr_mt6989 + MD32PCM_SCU_CTRL1,
		MD32PCM_SCU_CTRL1_MASK) >> MD32PCM_SCU_CTRL1_SHIFT;

	if (pspm_res1)
		*pspm_res1 = spm_res1;

	if (pspm_res2)
		*pspm_res2 = spm_res2;

	if (spm_res1 != 0x1D || spm_res2 != 0x17)
		return 0;

	return 1;
}

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_gpio_trx_info_mt6989(struct uarthub_gpio_trx_info *info)
{
	if (!info) {
		pr_notice("[%s] info is NULL\n", __func__);
		return -1;
	}

	if (!gpio_base_remap_addr_mt6989) {
		pr_notice("[%s] gpio_base_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	if (!iocfg_rm_remap_addr_mt6989) {
		pr_notice("[%s] iocfg_rm_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	UARTHUB_READ_GPIO(info->tx_mode, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6989,
		GPIO_HUB_MODE_TX, GPIO_HUB_MODE_TX_MASK, GPIO_HUB_MODE_TX_VALUE);
	UARTHUB_READ_GPIO(info->rx_mode, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6989,
		GPIO_HUB_MODE_RX, GPIO_HUB_MODE_RX_MASK, GPIO_HUB_MODE_RX_VALUE);
	UARTHUB_READ_GPIO_BIT(info->tx_dir, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6989,
		GPIO_HUB_DIR_TX, GPIO_HUB_DIR_TX_MASK, GPIO_HUB_DIR_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_dir, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6989,
		GPIO_HUB_DIR_RX, GPIO_HUB_DIR_RX_MASK, GPIO_HUB_DIR_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_ies, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_IES_TX, GPIO_HUB_IES_TX_MASK, GPIO_HUB_IES_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_ies, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_IES_RX, GPIO_HUB_IES_RX_MASK, GPIO_HUB_IES_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_pu, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_PU_TX, GPIO_HUB_PU_TX_MASK, GPIO_HUB_PU_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_pu, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_PU_RX, GPIO_HUB_PU_RX_MASK, GPIO_HUB_PU_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_pd, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_PD_TX, GPIO_HUB_PD_TX_MASK, GPIO_HUB_PD_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_pd, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_PD_RX, GPIO_HUB_PD_RX_MASK, GPIO_HUB_PD_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_drv, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_DRV_TX, GPIO_HUB_DRV_TX_MASK, GPIO_HUB_DRV_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_drv, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_DRV_RX, GPIO_HUB_DRV_RX_MASK, GPIO_HUB_DRV_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_smt, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_SMT_TX, GPIO_HUB_SMT_TX_MASK, GPIO_HUB_SMT_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_smt, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_SMT_RX, GPIO_HUB_SMT_RX_MASK, GPIO_HUB_SMT_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_tdsel, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_TDSEL_TX, GPIO_HUB_TDSEL_TX_MASK, GPIO_HUB_TDSEL_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_tdsel, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_TDSEL_RX, GPIO_HUB_TDSEL_RX_MASK, GPIO_HUB_TDSEL_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_rdsel, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_RDSEL_TX, GPIO_HUB_RDSEL_TX_MASK, GPIO_HUB_RDSEL_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_rdsel, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_RDSEL_RX, GPIO_HUB_RDSEL_RX_MASK, GPIO_HUB_RDSEL_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_sec_en, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_SEC_EN_TX, GPIO_HUB_SEC_EN_TX_MASK, GPIO_HUB_SEC_EN_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_sec_en, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_SEC_EN_RX, GPIO_HUB_SEC_EN_RX_MASK, GPIO_HUB_SEC_EN_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_din, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6989,
		GPIO_HUB_DIN_RX, GPIO_HUB_DIN_RX_MASK, GPIO_HUB_DIN_RX_SHIFT);

	return 0;
}
#endif

int uarthub_clk_univpll_ctrl_mt6989(int clk_on)
{
#if UARTHUB_SUPPORT_UNIVPLL_CTRL
	int ret = 0;
#if UARTHUB_DEBUG_LOG
	unsigned int before_pll_sta = 0;
#endif

	if (clk_apmixedsys_univpll_mt6989 == NULL || IS_ERR(clk_apmixedsys_univpll_mt6989)) {
		pr_notice("[%s] clk_apmixedsys_univpll_mt6989 is not init\n", __func__);
		return -1;
	}

	if (mutex_lock_killable(&g_lock_univpll_clk_mt6989)) {
		pr_notice("[%s] mutex_lock_killable(g_lock_univpll_clk_mt6989) fail\n", __func__);
		return UARTHUB_ERR_MUTEX_LOCK_FAIL;
	}

	if (clk_on == 1 && g_univpll_clk_ref_count_mt6989 >= 1) {
		mutex_unlock(&g_lock_univpll_clk_mt6989);
		return 0;
	} else if (clk_on == 0 && g_univpll_clk_ref_count_mt6989 <= 0) {
		mutex_unlock(&g_lock_univpll_clk_mt6989);
		return 0;
	}

#if UARTHUB_DEBUG_LOG
	before_pll_sta = uarthub_get_hwccf_univpll_on_info_mt6989();
#endif

	if (clk_on == 1) {
		ret = clk_prepare_enable(clk_apmixedsys_univpll_mt6989);
		if (ret) {
			pr_notice("[%s] UNIVPLL ON fail(%d)\n", __func__, ret);
			mutex_unlock(&g_lock_univpll_clk_mt6989);
			return ret;
		}
		g_univpll_clk_ref_count_mt6989++;

#if UARTHUB_DEBUG_LOG
		pr_info("[%s] UNIVPLL ON pass, ref_cnt=[%d], pll_sta=[%d --> %d] +\n",
			__func__, g_univpll_clk_ref_count_mt6989,
			before_pll_sta, uarthub_get_hwccf_univpll_on_info_mt6989());
#endif
	} else {
		g_univpll_clk_ref_count_mt6989--;
		clk_disable_unprepare(clk_apmixedsys_univpll_mt6989);

#if UARTHUB_DEBUG_LOG
		pr_info("[%s] UNIVPLL OFF pass, ref_cnt=[%d], pll_sta=[%d --> %d] -\n",
			__func__, g_univpll_clk_ref_count_mt6989,
			before_pll_sta, uarthub_get_hwccf_univpll_on_info_mt6989());
#endif
	}

	mutex_unlock(&g_lock_univpll_clk_mt6989);
#endif
	return 0;
}

int uarthub_dump_apuart_debug_ctrl_mt6989(int enable)
{
	g_enable_apuart_debug_info = enable;
	return 0;
}

int uarthub_get_apuart_debug_ctrl_sta_mt6989(void)
{
	return g_enable_apuart_debug_info;
}

int uarthub_get_intfhub_base_addr_mt6989(void)
{
	return UARTHUB_INTFHUB_BASE_ADDR;
}

int uarthub_get_uartip_base_addr_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		return UARTHUB_DEV_0_BASE_ADDR;
	else if (dev_index == 1)
		return UARTHUB_DEV_1_BASE_ADDR;
	else if (dev_index == 2)
		return UARTHUB_DEV_2_BASE_ADDR;

	return UARTHUB_CMM_BASE_ADDR;
}

int uarthub_dump_uartip_debug_info_mt6989(
	const char *tag, struct mutex *uartip_lock, int force_dump)
{
	const char *def_tag = "HUB_DBG_UIP";
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	int print_ap = 0;

	if (!uartip_lock)
		pr_notice("[%s] uartip_lock is NULL\n", __func__);

	if (uartip_lock) {
		if (mutex_lock_killable(uartip_lock)) {
			pr_notice("[%s] mutex_lock_killable(uartip_lock) fail\n", __func__);
			return UARTHUB_ERR_MUTEX_LOCK_FAIL;
		}
	}

	if (force_dump == 0) {
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
	}

	if (force_dump == 0) {
		uarthub_clk_univpll_ctrl_mt6989(1);
		if (uarthub_get_hwccf_univpll_on_info_mt6989() == 0) {
			pr_notice("[%s] uarthub_get_hwccf_univpll_on_info_mt6989=[0]\n", __func__);
			uarthub_clk_univpll_ctrl_mt6989(0);
			if (uartip_lock)
				mutex_unlock(uartip_lock);
			return -1;
		}
	}

	if (apuart3_base_remap_addr_mt6989 != NULL && g_enable_apuart_debug_info == 1)
		print_ap = 1;

	UARTHUB_DEBUG_PRINT_OP_RX_REQ(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_IP_TX_DMA(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_RX_WOFFSET(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_TX_WOFFSET(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_TX_ROFFSET(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_ROFFSET_DMA(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_ROFFSET_DMA(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_XCSTATE_WSEND_XOFF(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_SWTXDIS_DET_XOFF(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_FEATURE_SEL(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_HIGHSPEEND(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_DLL(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_SAMPLE_CNT(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_SAMPLE_PT(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_FRACDIV_L(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_FRACDIV_M(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_DMA_EN(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_IIR_FCR(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_LCR(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_EFR(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_XON1(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_XOFF1(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_XON2(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_XOFF2(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_ESCAPE_EN(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_ESCAPE_DAT(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_FCR_RD(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_MCR(def_tag, tag, print_ap);
	UARTHUB_DEBUG_PRINT_LSR(def_tag, tag, print_ap);

	uarthub_clk_univpll_ctrl_mt6989(0);
	if (uartip_lock)
		mutex_unlock(uartip_lock);

	return 0;
}

int uarthub_dump_intfhub_debug_info_mt6989(const char *tag)
{
	int val = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int ret = 0;
	const char *def_tag = "HUB_DBG";
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
#if !(UARTHUB_SUPPORT_FPGA)
	unsigned int spm_res1 = 0, spm_res2 = 0;
	struct uarthub_gpio_trx_info gpio_base_addr;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s] IDBG=[0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag), val);
	if (ret > 0)
		len += ret;

	val = uarthub_is_apb_bus_clk_enable_mt6989();
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",APB=[%d]", val);
	if (ret > 0)
		len += ret;

	if (val == 0) {
		pr_info("%s\n", dmp_info_buf);
		return 0;
	}

	val = uarthub_get_uarthub_cg_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x0 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HCG=[0x%x]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_peri_clk_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x800 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UPCLK=[0x%x]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_peri_uart_pad_mode_mt6989();
	if (val >= 0) {
		/* the expect value is 0x0 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UPAD=[0x%x(%s)]",
			val, ((val == 0) ? "HUB" : "UART_PAD"));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_spm_res_info_mt6989(&spm_res1, &spm_res2);
	if (val == 1) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[1]");
		if (ret > 0)
			len += ret;
	} else if (val == 0) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[0(0x%x/0x%x)]", spm_res1, spm_res2);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_hwccf_univpll_on_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UVPLL=[%d]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_mux_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x2 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UMUX=[0x%x]", val);
		if (ret > 0)
			len += ret;
	}

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	val = uarthub_get_gpio_trx_info_mt6989(&gpio_base_addr);
	if (val == 0) {
		ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
			"[%s][%s] GPIO MODE=[T:0x%x,R:0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			gpio_base_addr.tx_mode.gpio_value,
			gpio_base_addr.rx_mode.gpio_value);
		if (ret > 0)
			len += ret;
	}

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ILPBACK(0xe4)=[0x%x]", UARTHUB_REG_READ(LOOPBACK_ADDR));
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG(0xf4)=[0x%x]", UARTHUB_REG_READ(DBG_CTRL_ADDR));
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);
#endif

	len = 0;
	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s] IDEVx_STA(0x0/0x40/0x80)=[0x%x-0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	dev0_sta = UARTHUB_REG_READ(DEV0_PKT_CNT_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_PKT_CNT_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_PKT_CNT_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDEVx_PKT_CNT(0x1c/0x50/0x90)=[0x%x-0x%x-0x%x]",
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = UARTHUB_REG_READ(DEV0_CRC_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_CRC_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_CRC_STA_ADDR);
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s] IDEVx_CRC_STA(0x20/0x54/0x94)=[0x%x-0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	dev0_sta = UARTHUB_REG_READ(DEV0_RX_ERR_CRC_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_RX_ERR_CRC_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_RX_ERR_CRC_STA_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDEVx_RX_ERR_CRC_STA(0x10/0x14/0x18)=[0x%x-0x%x-0x%x]",
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s] IDEV0_IRQ_STA/MASK(0x30/0x38)=[0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(DEV0_IRQ_STA_ADDR),
		UARTHUB_REG_READ(DEV0_IRQ_MASK_ADDR));
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IIRQ_STA/MASK(0xd0/0xd8)=[0x%x-0x%x]",
		UARTHUB_REG_READ(IRQ_STA_ADDR),
		UARTHUB_REG_READ(IRQ_MASK_ADDR));
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0(0xe0)=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(CON2_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ICON2(0xc8)=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_debug_tx_rx_count_mt6989(const char *tag, int trigger_point)
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
	struct uarthub_uartip_debug_info debug4 = {0};
	struct uarthub_uartip_debug_info debug5 = {0};
	struct uarthub_uartip_debug_info debug6 = {0};
	struct uarthub_uartip_debug_info debug7 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	const char *def_tag = "HUB_DBG";
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int ret = 0;

	if (trigger_point != DUMP0 && trigger_point != DUMP1) {
		pr_notice("[%s] trigger_point = %d is invalid\n", __func__, trigger_point);
		return -1;
	}

	if (trigger_point == DUMP1 && pre_trigger_point == 0) {
		len = 0;
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s], dump0, pcnt=[R:%d-%d-%d",
			def_tag, ((tag == NULL) ? "null" : tag),
			cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d-%d-%d]",
			cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;

		if (g_enable_apuart_debug_info == 0) {
			ap_rx_bcnt = 0;
			ap_tx_bcnt = 0;
			ap_wait_for_send_xoff = 0;
			ap_detect_xoff = 0;
		}

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",bcnt=[R:%d-%d-%d-%d-%d",
			d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt,
			cmm_rx_bcnt, ap_rx_bcnt);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d-%d-%d-%d-%d]",
			d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt,
			cmm_tx_bcnt, ap_tx_bcnt);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",wsend_xoff=[%d-%d-%d-%d-%d]",
			d0_wait_for_send_xoff, d1_wait_for_send_xoff,
			d2_wait_for_send_xoff, cmm_wait_for_send_xoff,
			ap_wait_for_send_xoff);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d-%d-%d-%d-%d]",
				d0_detect_xoff, d1_detect_xoff, d2_detect_xoff,
				cmm_detect_xoff, ap_detect_xoff);
		if (ret > 0)
			len += ret;

		pr_info("%s\n", dmp_info_buf);
	}

	if (uarthub_is_apb_bus_clk_enable_mt6989() == 0) {
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

	uarthub_clk_univpll_ctrl_mt6989(1);
	if (uarthub_get_hwccf_univpll_on_info_mt6989() == 1) {
		UARTHUB_DEBUG_READ_DEBUG_REG(dev0, dev0);
		UARTHUB_DEBUG_READ_DEBUG_REG(dev1, dev1);
		UARTHUB_DEBUG_READ_DEBUG_REG(dev2, dev2);
		UARTHUB_DEBUG_READ_DEBUG_REG(cmm, cmm);

		if (apuart3_base_remap_addr_mt6989 != NULL &&
				g_enable_apuart_debug_info == 1) {
			UARTHUB_DEBUG_READ_DEBUG_REG(ap, apuart3);
		} else {
			debug1.ap = 0;
			debug2.ap = 0;
			debug3.ap = 0;
			debug5.ap = 0;
			debug6.ap = 0;
			debug8.ap = 0;
		}
	} else
		pr_notice("[%s] uarthub_get_hwccf_univpll_on_info_mt6989=[0]\n", __func__);
	uarthub_clk_univpll_ctrl_mt6989(0);

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
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s], dump1, pcnt=[R:%d-%d-%d",
			def_tag, ((tag == NULL) ? "null" : tag),
			cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d-%d-%d]",
			cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",bcnt=[R:%d-%d-%d-%d-%d",
			d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt,
			cmm_rx_bcnt, ap_rx_bcnt);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",wsend_xoff=[%d-%d-%d-%d-%d]",
			d0_wait_for_send_xoff, d1_wait_for_send_xoff,
			d2_wait_for_send_xoff, cmm_wait_for_send_xoff,
			ap_wait_for_send_xoff);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",det_xoff=[%d-%d-%d-%d-%d]",
			d0_detect_xoff, d1_detect_xoff,
			d2_detect_xoff, cmm_detect_xoff,
			ap_detect_xoff);
		if (ret > 0)
			len += ret;

		pr_info("%s\n", dmp_info_buf);
	}

	pre_trigger_point = trigger_point;
	return 0;
}

int uarthub_dump_debug_clk_info_mt6989(const char *tag)
{
	int val = 0;
	unsigned int spm_res1 = 0, spm_res2 = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	int len = 0;
	int ret = 0;

	if (uarthub_is_apb_bus_clk_enable_mt6989() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] IDBG=[0x%x]", ((tag == NULL) ? "null" : tag), val);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = uarthub_is_apb_bus_clk_enable_mt6989();
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",APB=[0x%x]", val);
	if (ret > 0)
		len += ret;

	if (val == 0) {
		pr_info("%s\n", dmp_info_buf);
		return -1;
	}

	val = uarthub_get_uarthub_cg_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x0 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HCG=[0x%x]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_peri_clk_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x800 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UPCLK=[0x%x]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_spm_res_info_mt6989(&spm_res1, &spm_res2);
	if (val == 1) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[1]");
		if (ret > 0)
			len += ret;
	} else if (val == 0) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[0(0x%x/0x%x)]", spm_res1, spm_res2);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_hwccf_univpll_on_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UVPLL=[%d]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_mux_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x2 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UMUX=[0x%x]", val);
		if (ret > 0)
			len += ret;
	}

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x]", dev0_sta);
		if (ret > 0)
			len += ret;
	} else {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x-0x%x-0x%x]", dev0_sta, dev1_sta, dev2_sta);
		if (ret > 0)
			len += ret;
	}

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_debug_byte_cnt_info_mt6989(const char *tag)
{
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug2 = {0};
	struct uarthub_uartip_debug_info debug3 = {0};
	struct uarthub_uartip_debug_info debug4 = {0};
	struct uarthub_uartip_debug_info debug5 = {0};
	struct uarthub_uartip_debug_info debug6 = {0};
	struct uarthub_uartip_debug_info debug7 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int val = 0;
	int ret = 0;

	uarthub_clk_univpll_ctrl_mt6989(1);
	if (uarthub_get_hwccf_univpll_on_info_mt6989() == 0) {
		pr_notice("[%s] uarthub_get_hwccf_univpll_on_info_mt6989=[0]\n", __func__);
		uarthub_clk_univpll_ctrl_mt6989(0);
		return -1;
	}

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] IDBG=[0x%x]", ((tag == NULL) ? "null" : tag), val);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	UARTHUB_DEBUG_READ_DEBUG_REG(dev0, dev0);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev1, dev1);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev2, dev2);
	UARTHUB_DEBUG_READ_DEBUG_REG(cmm, cmm);

	if (apuart3_base_remap_addr_mt6989 != NULL && g_enable_apuart_debug_info == 1) {
		UARTHUB_DEBUG_READ_DEBUG_REG(ap, apuart3);
	} else {
		debug1.ap = 0;
		debug2.ap = 0;
		debug3.ap = 0;
		debug4.ap = 0;
		debug5.ap = 0;
		debug6.ap = 0;
		debug7.ap = 0;
		debug8.ap = 0;
	}

	uarthub_clk_univpll_ctrl_mt6989(0);

	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug5, 0xF0, 4, debug6, 0x3, 4, ",bcnt=[R:%d-%d-%d-%d-%d");
	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug2, 0xF0, 4, debug3, 0x3, 4, ",T:%d-%d-%d-%d-%d]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug7, 0x3F, 0, ",fifo_woffset=[R:%d-%d-%d-%d-%d");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug4, 0x3F, 0, ",T:%d-%d-%d-%d-%d]");
	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug4, 0xC0, 6, debug5, 0xF, 2,
		",fifo_tx_roffset=[%d-%d-%d-%d-%d]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug6, 0xFC, 2, ",offset_dma=[R:%d-%d-%d-%d-%d");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug3, 0xFC, 2, ",T:%d-%d-%d-%d-%d]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug1, 0xE0, 5, ",wsend_xoff=[%d-%d-%d-%d-%d]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug8, 0x8, 3, ",det_xoff=[%d-%d-%d-%d-%d]");

	val = uarthub_get_hwccf_univpll_on_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UVPLL=[%d]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_mux_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x2 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UMUX=[0x%x]", val);
		if (ret > 0)
			len += ret;
	}

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDEV_STA=[0x%x]", dev0_sta);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_debug_apdma_uart_info_mt6989(const char *tag)
{
	const char *def_tag = "HUB_DBG_APMDA";

	pr_info("[%s][%s] 0=[0x%x],4=[0x%x],8=[0x%x],c=[0x%x],10=[0x%x],14=[0x%x],18=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x00),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x04),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x08),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x0c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x10),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x14),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x18));

	pr_info("[%s][%s] 1c=[0x%x],20=[0x%x],24=[0x%x],28=[0x%x],2c=[0x%x],30=[0x%x],34=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x1c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x20),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x24),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x28),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x2c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x30),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x34));

	pr_info("[%s][%s] 38=[0x%x],3c=[0x%x],40=[0x%x],44=[0x%x],48=[0x%x],4c=[0x%x],50=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x38),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x3c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x40),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x44),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x48),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x4c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x50));

	pr_info("[%s][%s] 54=[0x%x],58=[0x%x],5c=[0x%x],60=[0x%x],64=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x54),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x58),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x5c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x60),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x64));

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
char g_buf_m6989[UARTHUB_TMP_BUF_SZ];

int uarthub_dump_sspm_log_mt6989(const char *tag)
{
	void __iomem *log_addr = NULL;
	int i, n, used;
	uint32_t val, irq_idx = 0, tsk_idx = 0;
	uint32_t v1, v2, v3;
	uint64_t t;
	char *tmp;
	const char *def_tag = "HUB_DBG_SSPM";

	g_buf_m6989[0] = '\0';
	log_addr = UARTHUB_LOG_IRQ_IDX_ADDR(sys_sram_remap_addr_mt6989);
	irq_idx = UARTHUB_REG_READ(log_addr);
	log_addr += 4;

	tmp = g_buf_m6989;
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
		def_tag, ((tag == NULL) ? "null" : tag), irq_idx, g_buf_m6989);

	log_addr = UARTHUB_LOG_TSK_IDX_ADDR(sys_sram_remap_addr_mt6989);
	tsk_idx = UARTHUB_REG_READ(log_addr);
	log_addr += 4;
	g_buf_m6989[0] = '\0';
	tmp = g_buf_m6989;
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
					tsk_idx, g_buf_m6989);
				g_buf_m6989[0] = '\0';
				tmp = g_buf_m6989;
				used = 0;
			}
		}
		log_addr += UARTHUB_LOG_TSK_PKT_SIZE;
	}

	log_addr = UARTHUB_CK_CNT_ADDR(sys_sram_remap_addr_mt6989);
	val = UARTHUB_REG_READ(log_addr);

	log_addr = UARTHUB_LAST_CK_ON(sys_sram_remap_addr_mt6989);
	v1 = UARTHUB_REG_READ(log_addr);
	v2 = UARTHUB_REG_READ(log_addr + 4);

	log_addr = UARTHUB_LAST_CK_ON_CNT(sys_sram_remap_addr_mt6989);
	v3 = UARTHUB_REG_READ(log_addr);

	pr_info("[%s][%s] off/on cnt=[%d][%d] ckon=[%x][%x] cnt=[%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		(val & 0xFFFF), (val >> 16),
		v1, v2, v3);

	return 0;
}

int uarthub_trigger_fpga_testing_mt6989(int type)
{
#if UARTHUB_SUPPORT_DVT
	pr_info("[%s] FPGA type=[%d]\n", __func__, type);
#else
	pr_info("[%s] NOT support FPGA\n", __func__);
#endif
	return 0;
}

int uarthub_trigger_dvt_testing_mt6989(int type)
{
#if UARTHUB_SUPPORT_DVT
	int state = 0;

	pr_info("[%s] DVT type=[%d]\n", __func__, type);

	state = uarthub_ut_ip_host_tx_packet_loopback_mt6989();
	pr_info("[UT_IP] host_tx_packet_loopback=[%s], state=[%d]\n",
		((state == 0) ? "PASS" : "FAIL"), state);

	state = uarthub_ut_ip_timeout_init_fsm_ctrl_mt6989();
	pr_info("[UT_IP] timeout_init_fsm_ctrl=[%s], state=[%d]\n",
		((state == 0) ? "PASS" : "FAIL"), state);

	state = uarthub_ut_ip_clear_rx_data_irq_mt6989();
	pr_info("[UT_IP] clear_rx_data_irq=[%s], state=[%d]\n",
		((state == 0) ? "PASS" : "FAIL"), state);

#else
	pr_info("[%s] NOT support DVT\n", __func__);
#endif
	return 0;
}

int uarthub_verify_combo_connect_sta_mt6989(int type, int rx_delay_ms)
{
	int state = -1;

	if (type == 0)
		state = uarthub_verify_cmm_trx_combo_sta_mt6989(rx_delay_ms);
	else if (type == 1)
		state = uarthub_verify_cmm_loopback_sta_mt6989();
	else
		pr_notice("[%s] Not support type value=[%d]\n", __func__, type);

	return state;
}
