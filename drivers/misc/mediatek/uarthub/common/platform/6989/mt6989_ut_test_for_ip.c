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
	int dev_index, enum uarthub_trx_type trx, int enable_fsm, int verify_index);
static int uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(int dev_index, int verify_index);
static int uarthub_ut_ip_host_tx_packet_loopback_by_unit_mt6989(int dev_index, int verify_index);
#endif

#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(
	int dev_index,
	enum uarthub_trx_type trx,
	int enable_fsm,
	int verify_index)
{
	int state = 0;
	unsigned char dmp_info_buf[TRX_BUF_LEN];
	int len = 0;
	int i = 0;
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
	uarthub_reset_intfhub_mt6989();
	uarthub_set_host_loopback_ctrl_mt6989(dev_index, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);
	uarthub_config_uartip_dma_en_ctrl_mt6989(((trx == TX) ? 3 : dev_index), RX, 0);

	if (dev_index == 1) {
		TEST_1_CMD[0] = 0x86;
		TEST_2_RX_CMD[2] = 0x7E;
		TEST_2_RX_CMD[3] = 0x13;
	} else if (dev_index == 2) {
		TEST_1_CMD[0] = 0x82;
		TEST_2_RX_CMD[2] = 0xF4;
		TEST_2_RX_CMD[3] = 0xFB;
	}

	len = 0;
	for (i = 0; i < sizeof(TEST_1_CMD); i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), TEST_1_CMD[i]);
	}

	/* Verify */
	state = uarthub_uartip_write_tx_data_mt6989(
		((trx == TX) ? dev_index : 3), TEST_1_CMD, sizeof(TEST_1_CMD));

	if (state != 0) {
		pr_notice("[%s] TX FAIL(%d): %s send [%s] to %s, state=[%d] (dev[%d],trx[%d],fsm[%d])",
			__func__, verify_index,
			(trx == RX) ? "uart_cmm" : ((dev_index == 0) ? "uart_0" :
				((dev_index == 1) ? "uart_1" : "uart_2")),
			dmp_info_buf,
			(trx == TX) ? "uart_cmm" : ((dev_index == 0) ? "uart_0" :
				((dev_index == 1) ? "uart_1" : "uart_2")),
			state, dev_index, trx, enable_fsm);
		goto verify_err;
	}

	msleep(20);

	len = 0;
	if (trx == RX) {
		test_2_cmd_len = sizeof(TEST_2_RX_CMD);
		p_TEST_2_CMD = TEST_2_RX_CMD;
	} else {
		test_2_cmd_len = sizeof(TEST_2_TX_CMD);
		p_TEST_2_CMD = TEST_2_TX_CMD;
	}

	len = 0;
	for (i = 0; i < test_2_cmd_len; i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), p_TEST_2_CMD[i]);
	}

	state = uarthub_uartip_write_tx_data_mt6989(
		((trx == TX) ? dev_index : 3), p_TEST_2_CMD, test_2_cmd_len);

	if (state != 0) {
		pr_notice("[%s] TX FAIL(%d): %s send [%s] to %s, state=[%d] (dev[%d],trx[%d],fsm[%d])",
			__func__, verify_index,
			(trx == RX) ? "uart_cmm" : ((dev_index == 0) ? "uart_0" :
				((dev_index == 1) ? "uart_1" : "uart_2")),
			dmp_info_buf,
			(trx == TX) ? "uart_cmm" : ((dev_index == 0) ? "uart_0" :
				((dev_index == 1) ? "uart_1" : "uart_2")),
			state, dev_index, trx, enable_fsm);
		goto verify_err;
	}

	usleep_range(1000, 1010);

	if (trx == TX) {
		if (dev_index == 0)
			mask_bit = DEV0_IRQ_STA_VAL_dev0_tx_timeout_err(1);
		else if (dev_index == 1)
			mask_bit = DEV0_IRQ_STA_VAL_dev1_tx_timeout_err(1);
		else if (dev_index == 2)
			mask_bit = DEV0_IRQ_STA_VAL_dev2_tx_timeout_err(1);
	} else {
		if (dev_index == 0)
			mask_bit = DEV0_IRQ_STA_VAL_dev0_rx_timeout_err(1);
		else if (dev_index == 1)
			mask_bit = DEV0_IRQ_STA_VAL_dev1_rx_timeout_err(1);
		else if (dev_index == 2)
			mask_bit = DEV0_IRQ_STA_VAL_dev2_rx_timeout_err(1);
	}
	value_bit = mask_bit;
	state = uarthub_core_get_host_irq_sta(0);
	if ((state & mask_bit) != value_bit) {
		pr_notice("[%s] FAIL(%d): dev%d_%s_timeout_err has NOT been triggered, mask_bit=[0x%x], value_bit=[0x%x] (dev[%d],trx[%d],fsm[%d])",
			__func__, verify_index, dev_index,
			((trx == TX) ? "tx" : "rx"),
			mask_bit, value_bit, dev_index, trx, enable_fsm);
		state = UARTHUB_UT_ERR_IRQ_STA_FAIL;
		goto verify_err;
	}

	if (trx == TX) {
		if (dev_index == 0)
			mask_bit = DEV0_IRQ_STA_VAL_dev0_tx_pkt_type_err(1);
		else if (dev_index == 1)
			mask_bit = DEV0_IRQ_STA_VAL_dev1_tx_pkt_type_err(1);
		else if (dev_index == 2)
			mask_bit = DEV0_IRQ_STA_VAL_dev2_tx_pkt_type_err(1);
	} else
		mask_bit = DEV0_IRQ_STA_VAL_rx_pkt_type_err(1);
	if (enable_fsm == 0)
		value_bit = 0;
	else
		value_bit = mask_bit;
	if ((state & mask_bit) != value_bit) {
		pr_notice("[%s] FAIL(%d):%s%s_pkt_type_err has %sbeen triggered, mask_bit=[0x%x], value_bit=[0x%x] (dev[%d],trx[%d],fsm[%d])",
			__func__, verify_index,
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
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL, 0);
	}
	uarthub_set_host_loopback_ctrl_mt6989(dev_index, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);
	uarthub_reset_intfhub_mt6989();
	uarthub_config_uartip_dma_en_ctrl_mt6989(((trx == TX) ? 3 : dev_index), RX, 1);
	TIMEOUT_SET_dev0_tx_timeout_init_fsm_en(TIMEOUT_ADDR, fsm_backup);

	return state;
}
#endif /* UARTHUB_SUPPORT_UT_CASE */

#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_timeout_init_fsm_ctrl_mt6989(void)
{
	int state = 0;
	int verify_index = 0;

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
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

	/* Verify */
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(0, TX, 0, ++verify_index);
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(0, TX, 1, ++verify_index);
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(0, RX, 0, ++verify_index);
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(0, RX, 1, ++verify_index);
	if (state != 0)
		goto verify_err;

	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(1, TX, 0, ++verify_index);
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(1, TX, 1, ++verify_index);
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(1, RX, 0, ++verify_index);
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(1, RX, 1, ++verify_index);
	if (state != 0)
		goto verify_err;

	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(2, TX, 0, ++verify_index);
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(2, TX, 1, ++verify_index);
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(2, RX, 0, ++verify_index);
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_timeout_init_fsm_ctrl_by_unit_mt6989(2, RX, 1, ++verify_index);
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	/* Uninitialize */
	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return state;
}
#endif /* UARTHUB_SUPPORT_UT_CASE */

#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(int dev_index, int verify_index)
{
	int state = 0;
	unsigned char dmp_info_buf_recv[TRX_BUF_LEN];
	unsigned char dmp_info_buf[TRX_BUF_LEN];
	int len = 0;
	int i = 0;
	unsigned char TEST_FF_CMD[] = { 0xFF };
	unsigned char TEST_F0_CMD[] = { 0xF0 };
	unsigned char TEST_SEND_CMD[] = {
		0x02, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68, 0x92, 0x06 };
	unsigned char TEST_RECV_CMD[] = {
		0x02, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68 };
	unsigned char evtBuf[TRX_BUF_LEN] = { 0 };
	int recv_rx_len = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(dev_index, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(dev_index);
	if (state != 1) {
		pr_notice("[%s] FAIL(%d): dev%d uarthub is NOT ready(%d)",
			__func__, verify_index, dev_index, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}

	uarthub_reset_intfhub_mt6989();
	uarthub_set_host_loopback_ctrl_mt6989(dev_index, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);
	uarthub_config_uartip_dma_en_ctrl_mt6989(dev_index, RX, 0);

	/* Verify */
	len = 0;
	for (i = 0; i < sizeof(TEST_FF_CMD); i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), TEST_FF_CMD[i]);
	}

	state = uarthub_uartip_write_tx_data_mt6989(3, TEST_FF_CMD, sizeof(TEST_FF_CMD));

	if (state != 0) {
		pr_notice("[%s] TX FAIL(%d): uart_cmm send [%s] to uart_%d, state=[%d] (dev[%d])",
			__func__, verify_index, dmp_info_buf, dev_index, state, dev_index);
		goto verify_err;
	}

	len = 0;
	for (i = 0; i < sizeof(TEST_F0_CMD); i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), TEST_F0_CMD[i]);
	}

	state = uarthub_uartip_write_tx_data_mt6989(3, TEST_F0_CMD, sizeof(TEST_F0_CMD));

	if (state != 0) {
		pr_notice("[%s] TX FAIL(%d): uart_cmm send [%s] to uart_%d, state=[%d] (dev[%d])",
			__func__, verify_index, dmp_info_buf, dev_index, state, dev_index);
		goto verify_err;
	}

	usleep_range(1000, 1010);

	recv_rx_len = 0;
	state = uarthub_uartip_read_rx_data_mt6989(dev_index, evtBuf, TRX_BUF_LEN, &recv_rx_len);

	if (recv_rx_len != 1) {
		pr_notice("[%s] RX FAIL(%d): uart_%d received size=[%d] is different with expect size=[1] (dev[%d])",
			__func__, verify_index, dev_index, recv_rx_len, dev_index);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	if (state != 0) {
		pr_notice("[%s] RX FAIL(%d): uart_%d received [%02X], size=[%d], state=[%d] (dev[%d])",
			__func__, verify_index, dev_index, evtBuf[0],
			recv_rx_len, state, dev_index);
		goto verify_err;
	}

	if (TEST_FF_CMD[0] != evtBuf[0]) {
		pr_notice("[%s] RX FAIL(%d): uart_%d received [%02X] is different with rx_expect_data=[%02X] (dev[%d])",
			__func__, verify_index, dev_index, evtBuf[0], TEST_FF_CMD[0], dev_index);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	if (dev_index == 1) {
		TEST_SEND_CMD[0] = 0x86;
		TEST_SEND_CMD[8] = 0x7E;
		TEST_SEND_CMD[9] = 0x13;
		TEST_RECV_CMD[0] = 0x86;
	} else if (dev_index == 2) {
		TEST_SEND_CMD[0] = 0x82;
		TEST_SEND_CMD[8] = 0xF4;
		TEST_SEND_CMD[9] = 0xFB;
		TEST_RECV_CMD[0] = 0x82;
	}

	len = 0;
	for (i = 0; i < sizeof(TEST_SEND_CMD); i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), TEST_SEND_CMD[i]);
	}

	state = uarthub_uartip_write_tx_data_mt6989(3, TEST_SEND_CMD, sizeof(TEST_SEND_CMD));

	if (state != 0) {
		pr_notice("[%s] TX FAIL(%d): uart_cmm send [%s] to uart_%d, state=[%d] (dev[%d])",
			__func__, verify_index, dmp_info_buf, dev_index, state, dev_index);
		goto verify_err;
	}

	for (i = 0; i < TRX_BUF_LEN; i++)
		evtBuf[i] = 0;

	recv_rx_len = 0;
	state = uarthub_uartip_read_rx_data_mt6989(dev_index, evtBuf, TRX_BUF_LEN, &recv_rx_len);

	if (recv_rx_len != sizeof(TEST_RECV_CMD)) {
		pr_notice("[%s] RX FAIL(%d): uart_%d received size=[%d] is different with expect size=[%lu] (dev[%d])",
			__func__, verify_index, dev_index,
			recv_rx_len, sizeof(TEST_RECV_CMD), dev_index);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	len = 0;
	for (i = 0; i < sizeof(TEST_RECV_CMD); i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), TEST_RECV_CMD[i]);
	}

	len = 0;
	for (i = 0; i < recv_rx_len; i++) {
		len += snprintf(dmp_info_buf_recv + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), evtBuf[i]);
	}

	if (state != 0) {
		pr_notice("[%s] RX FAIL(%d): uart_%d received [%s], size=[%d], state=[%d] (dev[%d])",
			__func__, verify_index, dev_index, dmp_info_buf,
			recv_rx_len, state, dev_index);
		goto verify_err;
	}

	for (i = 0; i < sizeof(TEST_RECV_CMD); i++) {
		if (TEST_RECV_CMD[i] != evtBuf[i]) {
			pr_notice("[%s] RX FAIL(%d): uart_%d received [%s] is different with rx_expect_data=[%s] (dev[%d])",
				__func__, verify_index, dev_index, dmp_info_buf_recv,
				dmp_info_buf, dev_index);
			state = UARTHUB_UT_ERR_RX_FAIL;
			goto verify_err;
		}
	}

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL, 0);
	}

	uarthub_set_host_loopback_ctrl_mt6989(dev_index, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);
	uarthub_config_uartip_dma_en_ctrl_mt6989(dev_index, RX, 1);
	uarthub_reset_intfhub_mt6989();

	/* Uninitialize */
	uarthub_clear_host_trx_request_mt6989(dev_index, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif

	if (state == 0) {
		if (STA0_GET_intfhub_active(STA0_ADDR) != 0x0) {
			pr_notice("[%s] FAIL(%d): intfhub_active is NOT 0x0 (dev[%d])",
				__func__, verify_index, dev_index);
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
	int verify_index = 0;

	/* Initialize */
	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif

	/* Verify */
	state = uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(0, ++verify_index);
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(1, ++verify_index);
	if (state != 0)
		goto verify_err;
	state = uarthub_ut_ip_clear_rx_data_irq_by_unit_mt6989(2, ++verify_index);
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	/* Uninitialize */
	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return state;
}
#endif /* UARTHUB_SUPPORT_UT_CASE */

#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_host_tx_packet_loopback_by_unit_mt6989(int dev_index, int verify_index)
{
	int state = 0;
	unsigned char dmp_info_buf_recv[TRX_BUF_LEN];
	unsigned char dmp_info_buf[TRX_BUF_LEN];
	int len = 0;
	int i = 0;
	unsigned char TEST_SEND_CMD[] = {
		0x02, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68 };
	unsigned char TEST_RECV_CMD[] = {
		0x02, 0x00, 0x00, 0x03, 0x00, 0x66, 0x67, 0x68, 0x92, 0x06 };
	unsigned char evtBuf[TRX_BUF_LEN] = { 0 };
	int recv_rx_len = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	uarthub_reset_intfhub_mt6989();
	uarthub_set_host_loopback_ctrl_mt6989(dev_index, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);
	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 0);

	/* Verify */
	if (dev_index == 1) {
		TEST_RECV_CMD[0] = 0x86;
		TEST_RECV_CMD[8] = 0x7E;
		TEST_RECV_CMD[9] = 0x13;
		TEST_SEND_CMD[0] = 0x86;
	} else if (dev_index == 2) {
		TEST_RECV_CMD[0] = 0x82;
		TEST_RECV_CMD[8] = 0xF4;
		TEST_RECV_CMD[9] = 0xFB;
		TEST_SEND_CMD[0] = 0x82;
	}

	len = 0;
	for (i = 0; i < sizeof(TEST_SEND_CMD); i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), TEST_SEND_CMD[i]);
	}

	state = uarthub_uartip_write_tx_data_mt6989(
		dev_index, TEST_SEND_CMD, sizeof(TEST_SEND_CMD));

	if (state != 0) {
		pr_notice("[%s] TX FAIL(%d): uart_%d send [%s] to uart_cmm, state=[%d] (dev[%d])",
			__func__, verify_index, dev_index, dmp_info_buf, state, dev_index);
		goto verify_err;
	}

	recv_rx_len = 0;
	state = uarthub_uartip_read_rx_data_mt6989(3, evtBuf, TRX_BUF_LEN, &recv_rx_len);

	if (recv_rx_len != sizeof(TEST_RECV_CMD)) {
		pr_notice("[%s] RX FAIL(%d): uart_cmm received size=[%d] is different with expect size=[%lu] (dev[%d])",
			__func__, verify_index,
			recv_rx_len, sizeof(TEST_RECV_CMD), dev_index);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	len = 0;
	for (i = 0; i < sizeof(TEST_RECV_CMD); i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), TEST_RECV_CMD[i]);
	}

	len = 0;
	for (i = 0; i < recv_rx_len; i++) {
		len += snprintf(dmp_info_buf_recv + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), evtBuf[i]);
	}

	if (state != 0) {
		pr_notice("[%s] RX FAIL(%d): uart_cmm received [%s], size=[%d], state=[%d] (dev[%d])",
			__func__, verify_index, dmp_info_buf_recv,
			recv_rx_len, state, dev_index);
		goto verify_err;
	}

	for (i = 0; i < sizeof(TEST_RECV_CMD); i++) {
		if (TEST_RECV_CMD[i] != evtBuf[i]) {
			pr_notice("[%s] RX FAIL(%d): uart_cmm received [%s] is different with rx_expect_data=[%s] (dev[%d])",
				__func__, verify_index, dmp_info_buf_recv,
				dmp_info_buf, dev_index);
			state = UARTHUB_UT_ERR_RX_FAIL;
			goto verify_err;
		}
	}

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL, 0);
	}

	uarthub_set_host_loopback_ctrl_mt6989(dev_index, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);
	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 1);
	uarthub_reset_intfhub_mt6989();

	return state;
}
#endif /* UARTHUB_SUPPORT_UT_CASE */

#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_host_tx_packet_loopback_mt6989(void)
{
	int state = 0;
	int verify_index = 0;

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
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

	/* Verify */
	uarthub_ut_ip_host_tx_packet_loopback_by_unit_mt6989(0, ++verify_index);
	uarthub_ut_ip_host_tx_packet_loopback_by_unit_mt6989(1, ++verify_index);
	uarthub_ut_ip_host_tx_packet_loopback_by_unit_mt6989(2, ++verify_index);

	state = 0;
verify_err:
	/* Uninitialize */
	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
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
	unsigned char dmp_info_buf_recv[TRX_BUF_LEN];
	unsigned char dmp_info_buf[TRX_BUF_LEN];
	int len = 0;
	int i = 0;
	unsigned char BT_TEST_CMD[] = {
		0x01, 0x6F, 0xFC, 0x05, 0x01, 0x04, 0x01, 0x00, 0x02 };
	unsigned char BT_TEST_EVT[] = {
		0x01, 0x6F, 0xFC, 0x05, 0x01, 0x04, 0x01, 0x00, 0x02, 0x0B, 0xEE };
	unsigned char evtBuf[TRX_BUF_LEN] = { 0 };
	int recv_rx_len = 0;

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(0);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}

	uarthub_reset_intfhub_mt6989();
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);
	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 0);

	len = 0;
	for (i = 0; i < sizeof(BT_TEST_CMD); i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), BT_TEST_CMD[i]);
	}

	/* Verify */
	state = uarthub_uartip_write_tx_data_mt6989(0, BT_TEST_CMD, sizeof(BT_TEST_CMD));

	if (state != 0) {
		pr_notice("[%s] TX FAIL: uart_0 send [%s] to uart_cmm, state=[%d]",
			__func__, dmp_info_buf, state);
		goto verify_err;
	}

	pr_info("[%s] TX PASS: uart_0 send [%s] to uart_cmm", __func__, dmp_info_buf);

	usleep_range(1000, 1010);

	recv_rx_len = 0;
	state = uarthub_uartip_read_rx_data_mt6989(3, evtBuf, TRX_BUF_LEN, &recv_rx_len);

	if (recv_rx_len != sizeof(BT_TEST_EVT)) {
		pr_notice("[%s] RX FAIL: uart_cmm received size=[%d] is different with expect size=[%lu]",
			__func__, recv_rx_len, sizeof(BT_TEST_EVT));
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	len = 0;
	for (i = 0; i < sizeof(BT_TEST_EVT); i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), BT_TEST_EVT[i]);
	}

	len = 0;
	for (i = 0; i < recv_rx_len; i++) {
		len += snprintf(dmp_info_buf_recv + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), evtBuf[i]);
	}

	if (state != 0) {
		pr_notice("[%s] RX FAIL: uart_cmm received [%s], size=[%d], state=[%d]",
			__func__, dmp_info_buf_recv, recv_rx_len, state);
		goto verify_err;
	}

	for (i = 0; i < sizeof(BT_TEST_EVT); i++) {
		if (BT_TEST_EVT[i] != evtBuf[i]) {
			pr_notice("[%s] RX FAIL: uart_cmm received [%s] is different with rx_expect_data=[%s]",
				__func__, dmp_info_buf_recv, dmp_info_buf);
			state = UARTHUB_UT_ERR_RX_FAIL;
			goto verify_err;
		}
	}

	pr_notice("[%s] RX PASS: uart_cmm received [%s] is same with rx_expect_data=[%s]",
		__func__, dmp_info_buf_recv, dmp_info_buf);

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL, 0);
	}
	/* Uninitialize */
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);
	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 1);
	uarthub_reset_intfhub_mt6989();

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)

	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return state;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_verify_cmm_trx_combo_sta_mt6989(int rx_delay_ms)
{
	int state = 0;
	unsigned char dmp_info_buf_recv[TRX_BUF_LEN];
	unsigned char dmp_info_buf[TRX_BUF_LEN];
	int len = 0;
	int i = 0;
	unsigned char BT_TEST_CMD[] = {
		0x01, 0x6F, 0xFC, 0x05, 0x01, 0x04, 0x01, 0x00, 0x02 };
	unsigned char BT_TEST_EVT[] = {
		0x04, 0xE4, 0x0A, 0x02, 0x04, 0x06, 0x00, 0x00, 0x02 };
	unsigned char evtBuf[TRX_BUF_LEN] = { 0 };
	int recv_rx_len = 0;
	int expect_rx_len = 15;

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	state = uarthub_is_host_uarthub_ready_state_mt6989(0);
	if (state != 1) {
		pr_notice("[%s] FAIL: dev0 uarthub is NOT ready(%d)", __func__, state);
		state = UARTHUB_UT_ERR_HUB_READY_STA;
		goto verify_err;
	}

	uarthub_reset_intfhub_mt6989();
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 0);

	len = 0;
	for (i = 0; i < sizeof(BT_TEST_CMD); i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), BT_TEST_CMD[i]);
	}

	/* Verify */
	state = uarthub_uartip_write_tx_data_mt6989(0, BT_TEST_CMD, sizeof(BT_TEST_CMD));

	if (state != 0) {
		pr_notice("[%s] TX FAIL: uart_0 send [%s] to uart_cmm, state=[%d]",
			__func__, dmp_info_buf, state);
		goto verify_err;
	}

	pr_info("[%s] TX PASS: uart_0 send [%s] to uart_cmm", __func__, dmp_info_buf);

	if (rx_delay_ms >= 20)
		msleep(rx_delay_ms);
	else
		usleep_range(rx_delay_ms*1000, (rx_delay_ms*1000 + 10));

	recv_rx_len = 0;
	state = uarthub_uartip_read_rx_data_mt6989(3, evtBuf, TRX_BUF_LEN, &recv_rx_len);

	if (recv_rx_len != expect_rx_len) {
		pr_notice("[%s] RX FAIL: uart_cmm received size=[%d] is different with expect size=[%d]",
			__func__, recv_rx_len, expect_rx_len);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	len = 0;
	for (i = 0; i < sizeof(BT_TEST_EVT); i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), BT_TEST_EVT[i]);
	}

	len = 0;
	for (i = 0; i < recv_rx_len; i++) {
		len += snprintf(dmp_info_buf_recv + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), evtBuf[i]);
	}

	if (state != 0) {
		pr_notice("[%s] RX FAIL: uart_cmm received [%s], size=[%d], state=[%d]",
			__func__, dmp_info_buf_recv, recv_rx_len, state);
		goto verify_err;
	}

	for (i = 0; i < sizeof(BT_TEST_EVT); i++) {
		if (BT_TEST_EVT[i] != evtBuf[i]) {
			pr_notice("[%s] RX FAIL: uart_cmm received [%s] is different with rx_expect_data=[%s]",
				__func__, dmp_info_buf_recv, dmp_info_buf);
			state = UARTHUB_UT_ERR_RX_FAIL;
			goto verify_err;
		}
	}

	pr_notice("[%s] RX PASS: uart_cmm received [%s] is same with rx_expect_data=[%s]",
		__func__, dmp_info_buf_recv, dmp_info_buf);

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL, 0);
	}
	/* Uninitialize */
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 1);
	uarthub_reset_intfhub_mt6989();

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	usleep_range(3000, 3010);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)

	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	usleep_range(1000, 1010);
#endif
	return state;
}
#endif /* UARTHUB_SUPPORT_UT_API */
