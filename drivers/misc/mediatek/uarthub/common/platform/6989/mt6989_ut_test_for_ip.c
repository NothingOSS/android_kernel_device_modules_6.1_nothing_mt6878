// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_core.h"
#include "uarthub_drv_export.h"
#include "common_def_id.h"
#include "inc/mt6989.h"

#include <linux/regmap.h>

#if UARTHUB_SUPPORT_UT_CASE
static int uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(
	int dev_index, enum uarthub_trx_type trx, int enable_fsm);
static int uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(int dev_index);
static int uarthub_ut_ip_host_tx_packet_loopback_by_unit_mt6989(int dev_index);
#endif

#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(
	int dev_index,
	enum uarthub_trx_type trx,
	int enable_fsm)
{
	int state = 0;
	unsigned char TEST_1_CMD[] = { 0x02, 0x00, 0x00, 0x03, 0x00, 0x66 };
	unsigned char TEST_2_TX_CMD[] = { 0x67, 0x68 };
	unsigned char TEST_2_RX_CMD[] = { 0x67, 0x68, 0x92, 0x06 };
	unsigned char *p_TEST_2_CMD = NULL;
	int test_2_cmd_len = 0;
	int fsm_backup = 0;
	int mask_bit = 0, value_bit = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx >= TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	fsm_backup = TIMEOUT_GET_dev0_tx_timeout_init_fsm_en(TIMEOUT_ADDR);
	TIMEOUT_SET_dev0_tx_timeout_init_fsm_en(TIMEOUT_ADDR, enable_fsm);

	uarthub_clear_all_ut_irq_sta_mt6989();

	if (dev_index == 1) {
		TEST_1_CMD[0] = 0x86;
		TEST_2_RX_CMD[2] = 0x7E;
		TEST_2_RX_CMD[3] = 0xF4;
	} else if (dev_index == 2) {
		TEST_1_CMD[0] = 0x82;
		TEST_2_RX_CMD[2] = 0x13;
		TEST_2_RX_CMD[3] = 0xFB;
	}

	state = uarthub_uartip_send_data_internal_mt6989(
		((trx == TX) ? dev_index : 3),
		TEST_1_CMD, sizeof(TEST_1_CMD), 1);
	if (state != 0)
		goto verify_err;

	pr_info("[%s] delay 20ms ", __func__);
	mdelay(20);

	if (trx == RX) {
		test_2_cmd_len = sizeof(TEST_2_RX_CMD);
		p_TEST_2_CMD = TEST_2_RX_CMD;
	} else {
		test_2_cmd_len = sizeof(TEST_2_TX_CMD);
		p_TEST_2_CMD = TEST_2_TX_CMD;
	}

	state = uarthub_uartip_send_data_internal_mt6989(
		((trx == TX) ? dev_index : 3),
		p_TEST_2_CMD, test_2_cmd_len, 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	if (trx == TX) {
		if (dev_index == 0)
			mask_bit = REG_FLD_MASK(DEV0_IRQ_STA_FLD_dev0_tx_timeout_err);
		else if (dev_index == 1)
			mask_bit = REG_FLD_MASK(DEV0_IRQ_STA_FLD_dev1_tx_timeout_err);
		else if (dev_index == 2)
			mask_bit = REG_FLD_MASK(DEV0_IRQ_STA_FLD_dev2_tx_timeout_err);
	} else {
		if (dev_index == 0)
			mask_bit = REG_FLD_MASK(DEV0_IRQ_STA_FLD_dev0_rx_timeout_err);
		else if (dev_index == 1)
			mask_bit = REG_FLD_MASK(DEV0_IRQ_STA_FLD_dev1_rx_timeout_err);
		else if (dev_index == 2)
			mask_bit = REG_FLD_MASK(DEV0_IRQ_STA_FLD_dev2_rx_timeout_err);
	}
	value_bit = mask_bit;
	state = uarthub_core_get_host_irq_sta(0);
	if ((state & mask_bit) != value_bit) {
		pr_notice("[%s] FAIL: dev%d_%s_timeout_err has NOT been triggered, mask_bit=[0x%x], value_bit=[0x%x] (dev[%d],trx[%d],fsm[%d])",
			__func__, dev_index,
			((trx == TX) ? "tx" : "rx"),
			mask_bit, value_bit, dev_index, trx, enable_fsm);
		state = UARTHUB_UT_ERR_IRQ_STA_FAIL;
		goto verify_err;
	}

	if (trx == TX) {
		if (dev_index == 0)
			mask_bit = REG_FLD_MASK(DEV0_IRQ_STA_FLD_dev0_tx_pkt_type_err);
		else if (dev_index == 1)
			mask_bit = REG_FLD_MASK(DEV0_IRQ_STA_FLD_dev1_tx_pkt_type_err);
		else if (dev_index == 2)
			mask_bit = REG_FLD_MASK(DEV0_IRQ_STA_FLD_dev2_tx_pkt_type_err);
	} else
		mask_bit = REG_FLD_MASK(DEV0_IRQ_STA_FLD_rx_pkt_type_err);
	if (enable_fsm == 0)
		value_bit = 0;
	else
		value_bit = mask_bit;
	if ((state & mask_bit) != value_bit) {
		pr_notice("[%s] FAIL: %s%s_pkt_type_err has %sbeen triggered, mask_bit=[0x%x], value_bit=[0x%x] (dev[%d],trx[%d],fsm[%d])",
			__func__,
			((trx == RX) ? " " : ((dev_index == 0) ? " dev0_" :
				((dev_index == 1) ? " dev1_" : " dev2_"))),
			((trx == TX) ? "tx" : "rx"),
			((enable_fsm == 0) ? "" : "NOT "),
			mask_bit, value_bit, dev_index, trx, enable_fsm);
		state = UARTHUB_UT_ERR_IRQ_STA_FAIL;
		goto verify_err;
	}

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL);
		uarthub_dump_debug_monitor_mt6989(__func__);
	}

	uarthub_clear_all_ut_irq_sta_mt6989();
	TIMEOUT_SET_dev0_tx_timeout_init_fsm_en(TIMEOUT_ADDR, fsm_backup);

	return state;
}
#endif /* UARTHUB_SUPPORT_UT_CASE */

#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_timeout_init_fsm_ctrl_mt6989(void)
{
	int state = 0;
	int cg_en_backup = 0;
	int dbg_mon_sel_backup = 0;
	int chk_data_mode_sel_backup = 0;

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(0);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}
	uarthub_set_host_trx_request_mt6989(1, TRX);
	uarthub_set_host_trx_request_mt6989(2, TRX);

	cg_en_backup = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	dbg_mon_sel_backup =
		DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
	chk_data_mode_sel_backup =
		DEBUG_MODE_CRTL_GET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);

	/* Verify */
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(0, TX, 0);
	pr_info("[ITEM_1]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(0, TX, 1);
	pr_info("[ITEM_2]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(0, RX, 0);
	pr_info("[ITEM_3]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(0, RX, 1);
	pr_info("[ITEM_4]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(1, TX, 0);
	pr_info("[ITEM_5]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(1, TX, 1);
	pr_info("[ITEM_6]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(1, RX, 0);
	pr_info("[ITEM_7]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(1, RX, 1);
	pr_info("[ITEM_8]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(2, TX, 0);
	pr_info("[ITEM_9]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(2, TX, 1);
	pr_info("[ITEM_10]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(2, RX, 0);
	pr_info("[ITEM_11]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(2, RX, 1);
	pr_info("[ITEM_12]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	/* Uninitialize */
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, cg_en_backup);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, dbg_mon_sel_backup);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(
		DEBUG_MODE_CRTL_ADDR, chk_data_mode_sel_backup);

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return state;
}
#endif /* UARTHUB_SUPPORT_UT_CASE */

#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(int dev_index)
{
	int state = 0;
	unsigned char TEST_FF_CMD[] = { 0xFF };
	unsigned char TEST_F012_CMD[] = { 0xF0 };
	unsigned char TEST_SEND_CMD[] = {
		0x02, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68, 0x92, 0x06 };

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(dev_index, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(dev_index);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev%d uarthub is NOT ready(%d)",
			__func__, dev_index, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}

	if (dev_index == 1) {
		TEST_F012_CMD[0] = 0xF1;
		TEST_SEND_CMD[0] = 0x86;
		TEST_SEND_CMD[8] = 0x7E;
		TEST_SEND_CMD[9] = 0xF4;
	} else if (dev_index == 2) {
		TEST_F012_CMD[0] = 0xF2;
		TEST_SEND_CMD[0] = 0x82;
		TEST_SEND_CMD[8] = 0x13;
		TEST_SEND_CMD[9] = 0xFB;
	}

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_FF_CMD, sizeof(TEST_FF_CMD), 1);
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_F012_CMD, sizeof(TEST_F012_CMD), 1);
	if (state != 0)
		goto verify_err;

	usleep_range(1000, 1010);

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_SEND_CMD, sizeof(TEST_SEND_CMD), 1);
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL);
		uarthub_dump_debug_monitor_mt6989(__func__);
	}

	/* Uninitialize */
	uarthub_clear_host_trx_request_mt6989(dev_index, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif

	if (state == 0) {
		if (STA0_GET_intfhub_active(STA0_ADDR) != 0x0) {
			pr_notice("[%s] FAIL: intfhub_active is NOT 0x0 (dev[%d])",
				__func__, dev_index);
			state = UARTHUB_UT_ERR_INTFHUB_ACTIVE;
		}
	}

	return state;
}
#endif /* UARTHUB_SUPPORT_UT_CASE */

#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_clear_rx_data_irq_mt6989(void)
{
	int state = 0;
	int cg_en_backup = 0;
	int dbg_mon_sel_backup = 0;
	int chk_data_mode_sel_backup = 0;

	/* Initialize */
	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif

	cg_en_backup = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	dbg_mon_sel_backup =
		DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
	chk_data_mode_sel_backup =
		DEBUG_MODE_CRTL_GET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);

	/* Verify */
	state = uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(0);
	pr_info("[ITEM_1]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(1);
	pr_info("[ITEM_2]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(2);
	pr_info("[ITEM_3]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	/* Uninitialize */
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, cg_en_backup);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, dbg_mon_sel_backup);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(
		DEBUG_MODE_CRTL_ADDR, chk_data_mode_sel_backup);

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return state;
}
#endif /* UARTHUB_SUPPORT_UT_CASE */

#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_host_tx_packet_loopback_by_unit_mt6989(int dev_index)
{
	int state = 0;
	unsigned char TEST_SEND_CMD[] = {
		0x02, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68 };

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 1)
		TEST_SEND_CMD[0] = 0x86;
	else if (dev_index == 2)
		TEST_SEND_CMD[0] = 0x82;

	state = uarthub_uartip_send_data_internal_mt6989(
		dev_index, TEST_SEND_CMD, sizeof(TEST_SEND_CMD), 1);
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL);
		uarthub_dump_debug_monitor_mt6989(__func__);
	}

	return state;
}
#endif /* UARTHUB_SUPPORT_UT_CASE */

#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_host_tx_packet_loopback_mt6989(void)
{
	int state = 0;
	int cg_en_backup = 0;
	int dbg_mon_sel_backup = 0;
	int chk_data_mode_sel_backup = 0;

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(0);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}
	uarthub_set_host_trx_request_mt6989(1, TRX);
	uarthub_set_host_trx_request_mt6989(2, TRX);

	cg_en_backup = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	dbg_mon_sel_backup =
		DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
	chk_data_mode_sel_backup =
		DEBUG_MODE_CRTL_GET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);

	/* Verify */
	state = uarthub_ut_ip_host_tx_packet_loopback_by_unit_mt6989(0);
	pr_info("[ITEM_1]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_ut_ip_host_tx_packet_loopback_by_unit_mt6989(1);
	pr_info("[ITEM_2]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_ut_ip_host_tx_packet_loopback_by_unit_mt6989(2);
	pr_info("[ITEM_3]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	/* Uninitialize */
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, cg_en_backup);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, dbg_mon_sel_backup);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(
		DEBUG_MODE_CRTL_ADDR, chk_data_mode_sel_backup);

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return state;
}
#endif /* UARTHUB_SUPPORT_UT_CASE */

#if UARTHUB_SUPPORT_UT_API
int uarthub_verify_cmm_loopback_sta_mt6989(void)
{
	int state = 0;
	unsigned char BT_TEST_CMD[] = {
		0x01, 0x6F, 0xFC, 0x05, 0x01, 0x04, 0x01, 0x00, 0x02 };
	int cg_en_backup = 0;
	int dbg_mon_sel_backup = 0;
	int chk_data_mode_sel_backup = 0;

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(0);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}

	cg_en_backup = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	dbg_mon_sel_backup =
		DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
	chk_data_mode_sel_backup =
		DEBUG_MODE_CRTL_GET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);

	state = uarthub_uartip_send_data_internal_mt6989(
		0, BT_TEST_CMD, sizeof(BT_TEST_CMD), 1);
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	pr_info("[CMM_LOOPBACK]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL);
		uarthub_dump_debug_monitor_mt6989(__func__);
	}
	/* Uninitialize */
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, cg_en_backup);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, dbg_mon_sel_backup);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(
		DEBUG_MODE_CRTL_ADDR, chk_data_mode_sel_backup);

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)

	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return state;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_verify_cmm_trx_connsys_sta_mt6989(int rx_delay_ms)
{
	int state = 0;
	unsigned char dmp_info_buf_tx[TRX_BUF_LEN];
	unsigned char dmp_info_buf_rx[TRX_BUF_LEN];
	unsigned char dmp_info_buf_rx_expect[TRX_BUF_LEN];
	int len = 0;
	int i = 0;
	unsigned char BT_TEST_CMD[] = {
		0x01, 0x6F, 0xFC, 0x05, 0x01, 0x04, 0x01, 0x00, 0x02 };
	unsigned char BT_TEST_EVT[] = {
		0x04, 0xE4, 0x0A, 0x02, 0x04, 0x06, 0x00, 0x00, 0x02 };
	unsigned char evtBuf[TRX_BUF_LEN] = { 0 };
	int recv_rx_len = 0;
	int expect_rx_len = 15;
	int cg_en_backup = 0;
	int dbg_mon_sel_backup = 0;
	int chk_data_mode_sel_backup = 0;

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(0);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}

	cg_en_backup = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	dbg_mon_sel_backup =
		DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
	chk_data_mode_sel_backup =
		DEBUG_MODE_CRTL_GET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_reset_intfhub_mt6989();
	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 0);

	len = 0;
	for (i = 0; i < sizeof(BT_TEST_CMD); i++) {
		len += snprintf(dmp_info_buf_tx + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), BT_TEST_CMD[i]);
	}

	/* Verify */
	pr_info("[%s] uart_0 send [%s] to connsys chip", __func__, dmp_info_buf_tx);
	state = uarthub_uartip_write_tx_data_mt6989(0, BT_TEST_CMD, sizeof(BT_TEST_CMD));

	if (state != 0) {
		pr_notice("[%s] TX FAIL: uart_0 send [%s] to uart_cmm, state=[%d]",
			__func__, dmp_info_buf_tx, state);
		goto verify_err;
	}

	pr_info("[%s] TX PASS: uart_0 send [%s] to uart_cmm", __func__, dmp_info_buf_tx);

	if (rx_delay_ms >= 20)
		msleep(rx_delay_ms);
	else
		usleep_range(rx_delay_ms*1000, (rx_delay_ms*1000 + 10));

	recv_rx_len = 0;
	state = uarthub_uartip_read_rx_data_mt6989(3, evtBuf, TRX_BUF_LEN, &recv_rx_len);
	if (recv_rx_len > 0) {
		len = 0;
		for (i = 0; i < recv_rx_len; i++) {
			len += snprintf(dmp_info_buf_rx + len, TRX_BUF_LEN - len,
				((i == 0) ? "%02X" : ",%02X"), evtBuf[i]);
		}
		pr_info("[%s] uart_cmm received [%s] from connsys chip", __func__, dmp_info_buf_rx);
	} else
		pr_info("[%s] uart_cmm received nothing from connsys chip", __func__);

	if (recv_rx_len != expect_rx_len) {
		pr_notice("[%s] RX FAIL: uart_cmm received size=[%d] is different with expect size=[%d]",
			__func__, recv_rx_len, expect_rx_len);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	len = 0;
	for (i = 0; i < sizeof(BT_TEST_EVT); i++) {
		len += snprintf(dmp_info_buf_rx_expect + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), BT_TEST_EVT[i]);
	}

	if (state != 0) {
		pr_notice("[%s] RX FAIL: uart_cmm received [%s], size=[%d], state=[%d]",
			__func__, dmp_info_buf_rx, recv_rx_len, state);
		goto verify_err;
	}

	for (i = 0; i < sizeof(BT_TEST_EVT); i++) {
		if (BT_TEST_EVT[i] != evtBuf[i]) {
			pr_notice("[%s] RX FAIL: uart_cmm received [%s] is different with rx_expect_data=[%s]",
				__func__, dmp_info_buf_rx, dmp_info_buf_rx_expect);
			state = UARTHUB_UT_ERR_RX_FAIL;
			goto verify_err;
		}
	}

	state = 0;
verify_err:
	pr_info("[CMM_TRX_CONN_STA]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL);
		uarthub_dump_debug_monitor_mt6989(__func__);
	}
	/* Uninitialize */
	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 1);
	uarthub_reset_intfhub_mt6989();
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, cg_en_backup);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, dbg_mon_sel_backup);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(
		DEBUG_MODE_CRTL_ADDR, chk_data_mode_sel_backup);

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)

	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return state;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_check_debug_monitor_result_mt6989(
	int tx_mon_ptr_expect, int rx_mon_ptr_expect, int *tx_mon_expect, int *rx_mon_expect)
{
	int tx_mon_ptr = 0;
	int rx_mon_ptr = 0;
	int tx_mon[4] = { 0x0 };
	int rx_mon[4] = { 0x0 };
	int i = 0;
	int dbg_mon_sel = 0;
	int dbg_mon_mask = 0xFFFFFFFF;

	dbg_mon_sel = DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
	if (dbg_mon_sel == 0)
		dbg_mon_mask = 0xF800FFFF;

	if (dbg_mon_sel > 2) {
		pr_notice("[%s] FAIL: debug monitor select error, dbg_mon_sel=[0x%X]\n",
			__func__, dbg_mon_sel);
		return -1;
	}

	if (tx_mon_ptr_expect != -1) {
		if (dbg_mon_sel == 0)
			tx_mon_ptr =
				DEBUG_MODE_CRTL_GET_packet_info_mode_tx_monitor_pointer(
					DEBUG_MODE_CRTL_ADDR);
		else if (dbg_mon_sel == 1)
			tx_mon_ptr =
				DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(
					DEBUG_MODE_CRTL_ADDR);
		else if (dbg_mon_sel == 2)
			tx_mon_ptr =
				DEBUG_MODE_CRTL_GET_crc_result_mode_tx_monitor_pointer(
					DEBUG_MODE_CRTL_ADDR);

		if (tx_mon_ptr != tx_mon_ptr_expect) {
			pr_notice("[%s] FAIL: tx_mon_ptr error, tx_mon_ptr=[0x%X], tx_mon_ptr_expect=[0x%X]\n",
				__func__, tx_mon_ptr, tx_mon_ptr_expect);
			return -1;
		}
	}

	if (rx_mon_ptr_expect != -1) {
		if (dbg_mon_sel == 0)
			rx_mon_ptr =
				DEBUG_MODE_CRTL_GET_packet_info_mode_rx_monitor_pointer(
					DEBUG_MODE_CRTL_ADDR);
		else if (dbg_mon_sel == 1)
			rx_mon_ptr =
				DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(
					DEBUG_MODE_CRTL_ADDR);
		else if (dbg_mon_sel == 2)
			rx_mon_ptr =
				DEBUG_MODE_CRTL_GET_crc_result_mode_rx_monitor_pointer(
					DEBUG_MODE_CRTL_ADDR);

		if (rx_mon_ptr != rx_mon_ptr_expect) {
			pr_notice("[%s] FAIL: rx_mon_ptr error, rx_mon_ptr=[0x%X], rx_mon_ptr_expect=[0x%X]\n",
				__func__, rx_mon_ptr, rx_mon_ptr_expect);
			return -1;
		}
	}

	if (tx_mon_expect != NULL) {
		tx_mon[0] = (DEBUG_TX_MOINTOR_0_GET_intfhub_debug_tx_monitor0(
			DEBUG_TX_MOINTOR_0_ADDR) & dbg_mon_mask);
		tx_mon[1] = (DEBUG_TX_MOINTOR_1_GET_intfhub_debug_tx_monitor1(
			DEBUG_TX_MOINTOR_1_ADDR) & dbg_mon_mask);
		tx_mon[2] = (DEBUG_TX_MOINTOR_2_GET_intfhub_debug_tx_monitor2(
			DEBUG_TX_MOINTOR_2_ADDR) & dbg_mon_mask);
		tx_mon[3] = (DEBUG_TX_MOINTOR_3_GET_intfhub_debug_tx_monitor3(
			DEBUG_TX_MOINTOR_3_ADDR) & dbg_mon_mask);

		for (i = 0; i < 4; i++) {
			if (tx_mon[i] != tx_mon_expect[i]) {
				pr_notice("[%s] FAIL: tx_mon[%d] error, tx_mon[%d]=[0x%08X], tx_mon_expect[%d]=[0x%08X]\n",
					__func__, i, i, tx_mon[i], i, tx_mon_expect[i]);
				return -1;
			}
		}
	}

	if (rx_mon_expect != NULL) {
		rx_mon[0] = (DEBUG_RX_MOINTOR_0_GET_intfhub_debug_rx_monitor0(
			DEBUG_RX_MOINTOR_0_ADDR) & dbg_mon_mask);
		rx_mon[1] = (DEBUG_RX_MOINTOR_1_GET_intfhub_debug_rx_monitor1(
			DEBUG_RX_MOINTOR_1_ADDR) & dbg_mon_mask);
		rx_mon[2] = (DEBUG_RX_MOINTOR_2_GET_intfhub_debug_rx_monitor2(
			DEBUG_RX_MOINTOR_2_ADDR) & dbg_mon_mask);
		rx_mon[3] = (DEBUG_RX_MOINTOR_3_GET_intfhub_debug_rx_monitor3(
			DEBUG_RX_MOINTOR_3_ADDR) & dbg_mon_mask);

		for (i = 0; i < 4; i++) {
			if (rx_mon[i] != rx_mon_expect[i]) {
				pr_notice("[%s] FAIL: rx_mon[%d] error, rx_mon[%d]=[0x%08X], rx_mon_expect[%d]=[0x%08X]\n",
					__func__, i, i, rx_mon[i], i, rx_mon_expect[i]);
				return -1;
			}
		}
	}

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_ut_ip_verify_debug_monitor_packet_info_mode_mt6989(void)
{
	int state = 0;
	int tx_mon_expect[4] = { 0x0 };
	int rx_mon_expect[4] = { 0x0 };
	int cg_en_backup = 0;
	int dbg_mon_sel_backup = 0;
	int esp_pkt_en_backup = 0;

	unsigned char TEST_DEV0_TX_CMD[] = {
		0x01, 0x6F, 0xFC, 0x06, 0x01, 0x03, 0x02, 0x00, 0x3, 0x01 };
	unsigned char TEST_DEV0_RX_WITH_CRC_CMD[] = {
		0x04, 0xE4, 0x07, 0x02, 0x03, 0x03, 0x00, 0x00, 0x03, 0x01, 0xCF, 0x55 };

	unsigned char TEST_DEV1_TX_CMD[] = {
		0x86, 0x6F, 0xFC, 0x06, 0x00, 0x01, 0x03, 0x02, 0x00, 0x03, 0x02 };
	unsigned char TEST_DEV1_RX_WITH_CRC_CMD[] = {
		0x86, 0xE4, 0x00, 0x07, 0x00, 0x02, 0x03,
		0x03, 0x00, 0x00, 0x03, 0x02, 0x4F, 0x97 };

	unsigned char TEST_DEV2_TX_CMD[] = {
		0x81, 0x6F, 0xFC, 0x06, 0x01, 0x03, 0x02, 0x00, 0x03, 0x03 };
	unsigned char TEST_DEV2_RX_WITH_CRC_CMD[] = {
		0x84, 0xE4, 0x07, 0x02, 0x03, 0x03, 0x00, 0x00, 0x03, 0x03, 0xB9, 0x91 };

	unsigned char TEST_CMM_RX_F0_CMD[] = { 0xF0 };
	unsigned char TEST_CMM_RX_F1_CMD[] = { 0xF1 };
	unsigned char TEST_CMM_RX_F2_CMD[] = { 0xF2 };

	uarthub_set_host_trx_request_mt6989(0, TRX);
	mdelay(3);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	mdelay(1);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(0);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}
	uarthub_set_host_trx_request_mt6989(1, TRX);
	uarthub_set_host_trx_request_mt6989(2, TRX);

	cg_en_backup = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	dbg_mon_sel_backup =
		DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
	esp_pkt_en_backup =
		DEBUG_MODE_CRTL_GET_packet_info_bypass_esp_pkt_en(DEBUG_MODE_CRTL_ADDR);

	/* enable debug mode and config debug mode to check data mode */
	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, 0);
	DEBUG_MODE_CRTL_SET_packet_info_bypass_esp_pkt_en(DEBUG_MODE_CRTL_ADDR, 0);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_1]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	uarthub_clear_all_ut_irq_sta_mt6989();

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);

	state = uarthub_uartip_send_data_internal_mt6989(
		0, TEST_DEV0_TX_CMD, sizeof(TEST_DEV0_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = ((0x10 << 27) | 12);
	tx_mon_expect[1] = 0x0;
	tx_mon_expect[2] = 0x0;
	tx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_2]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV0_RX_WITH_CRC_CMD, sizeof(TEST_DEV0_RX_WITH_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = ((0x10 << 27) | 12);
	rx_mon_expect[1] = 0x0;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_3]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		1, TEST_DEV1_TX_CMD, sizeof(TEST_DEV1_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = ((0x10 << 27) | 12);
	tx_mon_expect[1] = ((0x8 << 27) | 13);
	tx_mon_expect[2] = 0x0;
	tx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(1, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_4]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV1_RX_WITH_CRC_CMD, sizeof(TEST_DEV1_RX_WITH_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = ((0x10 << 27) | 12);
	rx_mon_expect[1] = ((0x8 << 27) | 14);
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(1, 1, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_5]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		2, TEST_DEV2_TX_CMD, sizeof(TEST_DEV2_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = ((0x10 << 27) | 12);
	tx_mon_expect[1] = ((0x8 << 27) | 13);
	tx_mon_expect[2] = ((0x4 << 27) | 12);
	tx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(2, 1, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_6]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV2_RX_WITH_CRC_CMD, sizeof(TEST_DEV2_RX_WITH_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = ((0x10 << 27) | 12);
	rx_mon_expect[1] = ((0x8 << 27) | 14);
	rx_mon_expect[2] = ((0x4 << 27) | 12);
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(2, 2, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_7]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_CMM_RX_F0_CMD, sizeof(TEST_CMM_RX_F0_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = ((0x10 << 27) | 12);
	rx_mon_expect[1] = ((0x8 << 27) | 14);
	rx_mon_expect[2] = ((0x4 << 27) | 12);
	rx_mon_expect[3] = ((0x12 << 27) | 1);
	state = uarthub_check_debug_monitor_result_mt6989(2, 3, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_8]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_CMM_RX_F1_CMD, sizeof(TEST_CMM_RX_F1_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = ((0xA << 27) | 1);
	rx_mon_expect[1] = ((0x8 << 27) | 14);
	rx_mon_expect[2] = ((0x4 << 27) | 12);
	rx_mon_expect[3] = ((0x12 << 27) | 1);
	state = uarthub_check_debug_monitor_result_mt6989(2, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_9]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_CMM_RX_F2_CMD, sizeof(TEST_CMM_RX_F2_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = ((0xA << 27) | 1);
	rx_mon_expect[1] = ((0x6 << 27) | 1);
	rx_mon_expect[2] = ((0x4 << 27) | 12);
	rx_mon_expect[3] = ((0x12 << 27) | 1);
	state = uarthub_check_debug_monitor_result_mt6989(2, 1, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_10]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL);
		uarthub_dump_debug_monitor_mt6989(__func__);
	}

	uarthub_clear_all_ut_irq_sta_mt6989();

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, cg_en_backup);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, dbg_mon_sel_backup);
	DEBUG_MODE_CRTL_SET_packet_info_bypass_esp_pkt_en(
		DEBUG_MODE_CRTL_ADDR, esp_pkt_en_backup);

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	mdelay(3);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	mdelay(1);
#endif

	return state;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_ut_ip_verify_debug_monitor_check_data_mode_mt6989(void)
{
	int state = 0;
	int tx_mon_expect[4] = { 0x0 };
	int rx_mon_expect[4] = { 0x0 };
	int cg_en_backup = 0;
	int dbg_mon_sel_backup = 0;
	int chk_data_mode_sel_backup = 0;

	unsigned char TEST_DEV0_TX_CMD[] = {
		0x01, 0x6F, 0xFC, 0x06, 0x01, 0x03, 0x02, 0x00, 0x3, 0x01 };
	unsigned char TEST_DEV0_RX_WITH_CRC_CMD[] = {
		0x04, 0xE4, 0x07, 0x02, 0x03, 0x03, 0x00, 0x00, 0x03, 0x01, 0xCF, 0x55 };

	unsigned char TEST_DEV1_TX_CMD[] = {
		0x86, 0x6F, 0xFC, 0x06, 0x00, 0x01, 0x03, 0x02, 0x00, 0x03, 0x02 };
	unsigned char TEST_DEV1_RX_WITH_CRC_CMD[] = {
		0x86, 0xE4, 0x00, 0x07, 0x00, 0x02, 0x03,
		0x03, 0x00, 0x00, 0x03, 0x02, 0x4F, 0x97 };

	unsigned char TEST_DEV2_TX_CMD[] = {
		0x81, 0x6F, 0xFC, 0x06, 0x01, 0x03, 0x02, 0x00, 0x03, 0x03 };
	unsigned char TEST_DEV2_RX_WITH_CRC_CMD[] = {
		0x84, 0xE4, 0x07, 0x02, 0x03, 0x03, 0x00, 0x00, 0x03, 0x03, 0xB9, 0x91 };

	unsigned char TEST_CMM_RX_F0_CMD[] = { 0xF0 };
	unsigned char TEST_CMM_RX_F1_CMD[] = { 0xF1 };
	unsigned char TEST_CMM_RX_F2_CMD[] = { 0xF2 };

	uarthub_set_host_trx_request_mt6989(0, TRX);
	mdelay(3);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	mdelay(1);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(0);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}
	uarthub_set_host_trx_request_mt6989(1, TRX);
	uarthub_set_host_trx_request_mt6989(2, TRX);

	cg_en_backup = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	dbg_mon_sel_backup =
		DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
	chk_data_mode_sel_backup =
		DEBUG_MODE_CRTL_GET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR);

	/* enable debug mode and config debug mode to check data mode */
	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, 0);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_1]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	uarthub_clear_all_ut_irq_sta_mt6989();

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);

	state = uarthub_uartip_send_data_internal_mt6989(
		0, TEST_DEV0_TX_CMD, sizeof(TEST_DEV0_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x06FC6F01;
	tx_mon_expect[1] = 0x00020301;
	tx_mon_expect[2] = 0x107A0103;
	tx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(11, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_2]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV0_RX_WITH_CRC_CMD, sizeof(TEST_DEV0_RX_WITH_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x0207E404;
	rx_mon_expect[1] = 0x00000303;
	rx_mon_expect[2] = 0x55CF0103;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(11, 11, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_3]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		1, TEST_DEV1_TX_CMD, sizeof(TEST_DEV1_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x06FC6F86;
	tx_mon_expect[1] = 0x02030100;
	tx_mon_expect[2] = 0x4E020300;
	tx_mon_expect[3] = 0x000000A3;
	state = uarthub_check_debug_monitor_result_mt6989(12, 11, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_4]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV1_RX_WITH_CRC_CMD, sizeof(TEST_DEV1_RX_WITH_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x0700E486;
	rx_mon_expect[1] = 0x03030200;
	rx_mon_expect[2] = 0x02030000;
	rx_mon_expect[3] = 0x0000974F;
	state = uarthub_check_debug_monitor_result_mt6989(12, 13, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_5]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		2, TEST_DEV2_TX_CMD, sizeof(TEST_DEV2_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x06FC6F81;
	tx_mon_expect[1] = 0x00020301;
	tx_mon_expect[2] = 0xD40C0303;
	tx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(11, 13, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_6]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV2_RX_WITH_CRC_CMD, sizeof(TEST_DEV2_RX_WITH_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x0207E484;
	rx_mon_expect[1] = 0x00000303;
	rx_mon_expect[2] = 0x91B90303;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(11, 11, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_7]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_CMM_RX_F0_CMD, sizeof(TEST_CMM_RX_F0_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0xF0;
	rx_mon_expect[1] = 0x0;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(11, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_8]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_CMM_RX_F1_CMD, sizeof(TEST_CMM_RX_F1_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0xF1;
	rx_mon_expect[1] = 0x0;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(11, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_9]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_CMM_RX_F2_CMD, sizeof(TEST_CMM_RX_F2_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0xF2;
	rx_mon_expect[1] = 0x0;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(11, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_10]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	DEBUG_MODE_CRTL_SET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	tx_mon_expect[0] = 0x0;
	tx_mon_expect[1] = 0x0;
	tx_mon_expect[2] = 0x0;
	tx_mon_expect[3] = 0x0;
	rx_mon_expect[0] = 0x0;
	rx_mon_expect[1] = 0x0;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_11]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	uarthub_clear_all_ut_irq_sta_mt6989();

	state = uarthub_uartip_send_data_internal_mt6989(
		0, TEST_DEV0_TX_CMD, sizeof(TEST_DEV0_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x06FC6F01;
	tx_mon_expect[1] = 0x00020301;
	tx_mon_expect[2] = 0x107A0103;
	tx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(11, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_12]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV0_RX_WITH_CRC_CMD, sizeof(TEST_DEV0_RX_WITH_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x0207E404;
	rx_mon_expect[1] = 0x00000303;
	rx_mon_expect[2] = 0x55CF0103;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(11, 11, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_13]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		1, TEST_DEV1_TX_CMD, sizeof(TEST_DEV1_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x02030100;
	tx_mon_expect[1] = 0x4E020300;
	tx_mon_expect[2] = 0x107A01A3;
	tx_mon_expect[3] = 0x06FC6F86;
	state = uarthub_check_debug_monitor_result_mt6989(8, 11, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_14]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV1_RX_WITH_CRC_CMD, sizeof(TEST_DEV1_RX_WITH_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x03030200;
	rx_mon_expect[1] = 0x02030000;
	rx_mon_expect[2] = 0x55CF974F;
	rx_mon_expect[3] = 0x0700E486;
	state = uarthub_check_debug_monitor_result_mt6989(8, 9, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_15]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		2, TEST_DEV2_TX_CMD, sizeof(TEST_DEV2_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x0C030300;
	tx_mon_expect[1] = 0x4E0203D4;
	tx_mon_expect[2] = 0xFC6F81A3;
	tx_mon_expect[3] = 0x02030106;
	state = uarthub_check_debug_monitor_result_mt6989(4, 9, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_16]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV2_RX_WITH_CRC_CMD, sizeof(TEST_DEV2_RX_WITH_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x03030000;
	rx_mon_expect[1] = 0x020391B9;
	rx_mon_expect[2] = 0xE484974F;
	rx_mon_expect[3] = 0x03030207;
	state = uarthub_check_debug_monitor_result_mt6989(4, 5, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_17]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_CMM_RX_F0_CMD, sizeof(TEST_CMM_RX_F0_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x03030000;
	rx_mon_expect[1] = 0x02F091B9;
	rx_mon_expect[2] = 0xE484974F;
	rx_mon_expect[3] = 0x03030207;
	state = uarthub_check_debug_monitor_result_mt6989(4, 6, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_18]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_CMM_RX_F1_CMD, sizeof(TEST_CMM_RX_F1_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x03030000;
	rx_mon_expect[1] = 0xF1F091B9;
	rx_mon_expect[2] = 0xE484974F;
	rx_mon_expect[3] = 0x03030207;
	state = uarthub_check_debug_monitor_result_mt6989(4, 7, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_19]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_CMM_RX_F2_CMD, sizeof(TEST_CMM_RX_F2_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x03030000;
	rx_mon_expect[1] = 0xF1F091B9;
	rx_mon_expect[2] = 0xE48497F2;
	rx_mon_expect[3] = 0x03030207;
	state = uarthub_check_debug_monitor_result_mt6989(4, 8, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_20]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL);
		uarthub_dump_debug_monitor_mt6989(__func__);
	}

	uarthub_clear_all_ut_irq_sta_mt6989();

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, cg_en_backup);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, dbg_mon_sel_backup);
	DEBUG_MODE_CRTL_SET_check_data_mode_select(
		DEBUG_MODE_CRTL_ADDR, chk_data_mode_sel_backup);

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	mdelay(3);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	mdelay(1);
#endif

	return state;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_ut_ip_verify_debug_monitor_crc_result_mode_mt6989(void)
{
	int state = 0;
	int tx_mon_expect[4] = { 0x0 };
	int rx_mon_expect[4] = { 0x0 };
	int cg_en_backup = 0;
	int dbg_mon_sel_backup = 0;
	int rx_crc_data_en_backup = 0;

	unsigned char TEST_DEV0_TX_CMD[] = {
		0x01, 0x6F, 0xFC, 0x06, 0x01, 0x03, 0x02, 0x00, 0x3, 0x01 };
	unsigned char TEST_DEV0_RX_WITH_CRC_CMD[] = {
		0x04, 0xE4, 0x07, 0x02, 0x03, 0x03, 0x00, 0x00, 0x03, 0x01, 0xCF, 0x55 };

	unsigned char TEST_DEV1_TX_CMD[] = {
		0x86, 0x6F, 0xFC, 0x06, 0x00, 0x01, 0x03, 0x02, 0x00, 0x03, 0x02 };
	/* CRC should be 0x4F, 0x97 */
	unsigned char TEST_DEV1_RX_WITH_ERROR_CRC_CMD[] = {
		0x86, 0xE4, 0x00, 0x07, 0x00, 0x02, 0x03,
		0x03, 0x00, 0x00, 0x03, 0x02, 0x3A, 0x6F };

	unsigned char TEST_DEV2_TX_CMD[] = {
		0x81, 0x6F, 0xFC, 0x06, 0x01, 0x03, 0x02, 0x00, 0x03, 0x03 };
	/* CRC should be 0xB9, 0x91 */
	unsigned char TEST_DEV2_RX_WITH_ERROR_CRC_CMD[] = {
		0x84, 0xE4, 0x07, 0x02, 0x03, 0x03, 0x00, 0x00, 0x03, 0x03, 0x7C, 0xD4 };

	unsigned char TEST_CMM_RX_F0_CMD[] = { 0xF0 };

	uarthub_set_host_trx_request_mt6989(0, TRX);
	mdelay(3);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	mdelay(1);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(0);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}
	uarthub_set_host_trx_request_mt6989(1, TRX);
	uarthub_set_host_trx_request_mt6989(2, TRX);

	cg_en_backup = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	dbg_mon_sel_backup =
		DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
	rx_crc_data_en_backup =
		DEBUG_MODE_CRTL_GET_tx_monitor_display_rx_crc_data_en(DEBUG_MODE_CRTL_ADDR);

	/* enable debug mode and config debug mode to check data mode */
	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_tx_monitor_display_rx_crc_data_en(DEBUG_MODE_CRTL_ADDR, 0);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_1]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	uarthub_clear_all_ut_irq_sta_mt6989();

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);

	state = uarthub_uartip_send_data_internal_mt6989(
		0, TEST_DEV0_TX_CMD, sizeof(TEST_DEV0_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x00007A10;
	tx_mon_expect[1] = 0x0;
	tx_mon_expect[2] = 0x0;
	tx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_2]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV0_RX_WITH_CRC_CMD, sizeof(TEST_DEV0_RX_WITH_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x0000CF55;
	rx_mon_expect[1] = 0x0;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_3]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		1, TEST_DEV1_TX_CMD, sizeof(TEST_DEV1_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x4EA37A10;
	tx_mon_expect[1] = 0x0;
	tx_mon_expect[2] = 0x0;
	tx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(1, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_4]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV1_RX_WITH_ERROR_CRC_CMD, sizeof(TEST_DEV1_RX_WITH_ERROR_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x4F97CF55;
	rx_mon_expect[1] = 0x0;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(1, 1, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_5]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		2, TEST_DEV2_TX_CMD, sizeof(TEST_DEV2_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x4EA37A10;
	tx_mon_expect[1] = 0x00000CD4;
	tx_mon_expect[2] = 0x0;
	tx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(2, 1, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_6]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV2_RX_WITH_ERROR_CRC_CMD, sizeof(TEST_DEV2_RX_WITH_ERROR_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	rx_mon_expect[0] = 0x4F97CF55;
	rx_mon_expect[1] = 0x0000B991;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(2, 2, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_7]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_CMM_RX_F0_CMD, sizeof(TEST_CMM_RX_F0_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	state = uarthub_check_debug_monitor_result_mt6989(2, 2, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_8]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	DEBUG_MODE_CRTL_SET_tx_monitor_display_rx_crc_data_en(DEBUG_MODE_CRTL_ADDR, 1);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);

	tx_mon_expect[0] = 0x0;
	tx_mon_expect[1] = 0x0;
	tx_mon_expect[2] = 0x0;
	tx_mon_expect[3] = 0x0;
	rx_mon_expect[0] = 0x0;
	rx_mon_expect[1] = 0x0;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_9]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	uarthub_clear_all_ut_irq_sta_mt6989();

	state = uarthub_uartip_send_data_internal_mt6989(
		0, TEST_DEV0_TX_CMD, sizeof(TEST_DEV0_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_10]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV0_RX_WITH_CRC_CMD, sizeof(TEST_DEV0_RX_WITH_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x0000CF55;
	tx_mon_expect[1] = 0x0;
	tx_mon_expect[2] = 0x0;
	tx_mon_expect[3] = 0x0;
	rx_mon_expect[0] = 0x0000CF55;
	rx_mon_expect[1] = 0x0;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_11]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		1, TEST_DEV1_TX_CMD, sizeof(TEST_DEV1_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	state = uarthub_check_debug_monitor_result_mt6989(0, 0, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_12]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV1_RX_WITH_ERROR_CRC_CMD, sizeof(TEST_DEV1_RX_WITH_ERROR_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x3A6FCF55;
	tx_mon_expect[1] = 0x0;
	tx_mon_expect[2] = 0x0;
	tx_mon_expect[3] = 0x0;
	rx_mon_expect[0] = 0x4F97CF55;
	rx_mon_expect[1] = 0x0;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(1, 1, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_13]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		2, TEST_DEV2_TX_CMD, sizeof(TEST_DEV2_TX_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	state = uarthub_check_debug_monitor_result_mt6989(1, 1, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_14]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_DEV2_RX_WITH_ERROR_CRC_CMD, sizeof(TEST_DEV2_RX_WITH_ERROR_CRC_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	tx_mon_expect[0] = 0x3A6FCF55;
	tx_mon_expect[1] = 0x00007CD4;
	tx_mon_expect[2] = 0x0;
	tx_mon_expect[3] = 0x0;
	rx_mon_expect[0] = 0x4F97CF55;
	rx_mon_expect[1] = 0x0000B991;
	rx_mon_expect[2] = 0x0;
	rx_mon_expect[3] = 0x0;
	state = uarthub_check_debug_monitor_result_mt6989(2, 2, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_15]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = uarthub_uartip_send_data_internal_mt6989(
		3, TEST_CMM_RX_F0_CMD, sizeof(TEST_CMM_RX_F0_CMD), 1);
	if (state != 0)
		goto verify_err;

	mdelay(1);

	state = uarthub_check_debug_monitor_result_mt6989(2, 2, tx_mon_expect, rx_mon_expect);
	pr_info("[ITEM_16]: %s", ((state != 0) ? "FAIL" : "PASS"));
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL);
		uarthub_dump_debug_monitor_mt6989(__func__);
	}

	uarthub_clear_all_ut_irq_sta_mt6989();

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, cg_en_backup);
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, dbg_mon_sel_backup);
	DEBUG_MODE_CRTL_SET_tx_monitor_display_rx_crc_data_en(
		DEBUG_MODE_CRTL_ADDR, rx_crc_data_en_backup);

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	mdelay(3);
#if !(SSPM_DRIVER_EN) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	mdelay(1);
#endif

	return state;
}
#endif /* UARTHUB_SUPPORT_UT_API */
