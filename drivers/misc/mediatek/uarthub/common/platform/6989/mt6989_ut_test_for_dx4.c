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

#if UARTHUB_SUPPORT_DX4_FPGA
static int uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
	int dev_index,
	int verify_index,
	unsigned char *p_tx_data,
	int tx_data_len,
	enum uarthub_trx_type trx_pkt_type_irq);

static int uarthub_ut_ip_verify_trx_not_ready_by_unit_mt6989(
	int dev_index,
	int verify_index,
	unsigned char *p_tx_data,
	int tx_data_len,
	int verify_bypass_mode);
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_request_host_sema_own_sta_mt6989(int dev_index)
{
	int state = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		state = DEV0_SEMAPHORE_STA_GET_dev0_sema_own_sta(DEV0_SEMAPHORE_STA_ADDR);
	else if (dev_index == 1)
		state = DEV1_SEMAPHORE_STA_GET_dev1_sema_own_sta(DEV1_SEMAPHORE_STA_ADDR);
	else if (dev_index == 2)
		state = DEV2_SEMAPHORE_STA_GET_dev2_sema_own_sta(DEV2_SEMAPHORE_STA_ADDR);

	return state;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_set_host_sema_own_rel_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		DEV0_SEMAPHORE_CON_SET_dev0_sema_own_rel(DEV0_SEMAPHORE_CON_ADDR, 0x1);
	else if (dev_index == 1)
		DEV1_SEMAPHORE_CON_SET_dev1_sema_own_rel(DEV1_SEMAPHORE_CON_ADDR, 0x1);
	else if (dev_index == 2)
		DEV2_SEMAPHORE_CON_SET_dev2_sema_own_rel(DEV2_SEMAPHORE_CON_ADDR, 0x1);

	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_get_host_sema_own_rel_irq_sta_mt6989(int dev_index)
{
	int state = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		state = DEV0_IRQ_STA_GET_dev0_sema_own_rel_irq(DEV0_IRQ_STA_ADDR);
	else if (dev_index == 1)
		state = DEV1_IRQ_GET_dev1_sema_own_rel_irq(DEV1_IRQ_ADDR);
	else if (dev_index == 2)
		state = DEV2_IRQ_GET_dev2_sema_own_rel_irq(DEV2_IRQ_ADDR);

	return state;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_clear_host_sema_own_rel_irq_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		DEV0_IRQ_CLR_SET_dev0_sema_own_rel_clr(DEV0_IRQ_CLR_ADDR, 0x1);
	else if (dev_index == 1)
		DEV1_IRQ_SET_dev1_sema_own_rel_clr(DEV1_IRQ_ADDR, 0x1);
	else if (dev_index == 2)
		DEV2_IRQ_SET_dev2_sema_own_rel_clr(DEV2_IRQ_ADDR, 0x1);

	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_reset_host_sema_own_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		DEV0_SEMAPHORE_CON_SET_dev0_sema_own_rst(DEV0_SEMAPHORE_CON_ADDR, 0x1);
	else if (dev_index == 1)
		DEV1_SEMAPHORE_CON_SET_dev1_sema_own_rst(DEV1_SEMAPHORE_CON_ADDR, 0x1);
	else if (dev_index == 2)
		DEV2_SEMAPHORE_CON_SET_dev2_sema_own_rst(DEV2_SEMAPHORE_CON_ADDR, 0x1);

	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_get_host_sema_own_tmo_irq_sta_mt6989(int dev_index)
{
	int state = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		state = DEV0_IRQ_STA_GET_dev0_sema_own_timeout_irq(DEV0_IRQ_STA_ADDR);
	else if (dev_index == 1)
		state = DEV0_IRQ_STA_GET_dev1_sema_own_timeout_irq(DEV0_IRQ_STA_ADDR);
	else if (dev_index == 2)
		state = DEV0_IRQ_STA_GET_dev2_sema_own_timeout_irq(DEV0_IRQ_STA_ADDR);

	return state;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_clear_host_sema_own_tmo_irq_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		DEV0_IRQ_CLR_SET_dev0_sema_own_timeout_clr(DEV0_IRQ_CLR_ADDR, 0x1);
	else if (dev_index == 1)
		DEV0_IRQ_CLR_SET_dev1_sema_own_timeout_clr(DEV0_IRQ_CLR_ADDR, 0x1);
	else if (dev_index == 2)
		DEV0_IRQ_CLR_SET_dev2_sema_own_timeout_clr(DEV0_IRQ_CLR_ADDR, 0x1);

	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_reset_host_sema_own_tmo_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		DEV0_SEMAPHORE_CON_SET_dev0_sema_own_timeout_rst(DEV0_SEMAPHORE_CON_ADDR, 0x1);
	else if (dev_index == 1)
		DEV1_SEMAPHORE_CON_SET_dev1_sema_own_timeout_rst(DEV1_SEMAPHORE_CON_ADDR, 0x1);
	else if (dev_index == 2)
		DEV2_SEMAPHORE_CON_SET_dev2_sema_own_timeout_rst(DEV2_SEMAPHORE_CON_ADDR, 0x1);

	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_config_inband_esc_char_mt6989(int esc_char)
{
	INB_ESC_CHAR_SET_INB_ESC_CHAR(INB_ESC_CHAR_ADDR(cmm_base_remap_addr_mt6989), esc_char);
	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_config_inband_esc_sta_mt6989(int esc_sta)
{
	INB_STA_CHAR_SET_INB_STA_CHAR(INB_STA_CHAR_ADDR(cmm_base_remap_addr_mt6989), esc_sta);
	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_config_inband_enable_ctrl_mt6989(int enable)
{
	INB_IRQ_CTL_SET_INB_EN(
		INB_IRQ_CTL_ADDR(cmm_base_remap_addr_mt6989), ((enable == 0) ? 0x0 : 0x1));
	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_config_inband_irq_enable_ctrl_mt6989(int enable)
{
	INB_IRQ_CTL_SET_INB_IRQ_EN(
		INB_IRQ_CTL_ADDR(cmm_base_remap_addr_mt6989), ((enable == 0) ? 0x0 : 0x1));
	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_config_inband_trigger_mt6989(void)
{
	INB_IRQ_CTL_SET_INB_TRIG(INB_IRQ_CTL_ADDR(cmm_base_remap_addr_mt6989), 0x1);
	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_is_inband_tx_complete_mt6989(void)
{
	return INB_IRQ_CTL_GET_INB_TX_COMP(INB_IRQ_CTL_ADDR(cmm_base_remap_addr_mt6989));
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_get_inband_irq_sta_mt6989(void)
{
	int state = 0;

	state = INB_IRQ_CTL_GET_INB_IRQ_IND(INB_IRQ_CTL_ADDR(cmm_base_remap_addr_mt6989));
	return ((state == 0x0) ? 0x1 : 0x0);
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_clear_inband_irq_mt6989(void)
{
	INB_IRQ_CTL_SET_INB_IRQ_CLR(INB_IRQ_CTL_ADDR(cmm_base_remap_addr_mt6989), 0x1);
	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_get_received_inband_sta_mt6989(void)
{
	return INB_STA_GET_INB_STA(INB_STA_ADDR(cmm_base_remap_addr_mt6989));
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_clear_received_inband_sta_mt6989(void)
{
	INB_IRQ_CTL_SET_INB_STA_CLR(INB_IRQ_CTL_ADDR(cmm_base_remap_addr_mt6989), 0x1);
	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
	int dev_index,
	int verify_index,
	unsigned char *p_tx_data,
	int tx_data_len,
	enum uarthub_trx_type trx_pkt_type_irq)
{
	int state = 0;
	unsigned char dmp_info_buf_recv[TRX_BUF_LEN];
	unsigned char dmp_info_buf_recv_2[TRX_BUF_LEN];
	unsigned char dmp_info_buf[TRX_BUF_LEN];
	int len = 0;
	int i = 0;
	unsigned char evtBuf[TRX_BUF_LEN] = { 0 };
	unsigned char evtBuf_2[TRX_BUF_LEN] = { 0 };
	int recv_rx_len = 0;
	int mask_bit = 0, value_bit = 0;
	int rx_index_2nd = 0;

	if (!p_tx_data || tx_data_len <= 0)
		return UARTHUB_ERR_PARA_WRONG;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	uarthub_reset_intfhub_mt6989();
	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 0);

	uarthub_mask_host_irq_mt6989(0, 0, 1);
	uarthub_mask_host_irq_mt6989(1, 0, 1);
	uarthub_mask_host_irq_mt6989(2, 0, 1);
	uarthub_clear_host_irq_mt6989(0, 0);
	uarthub_clear_host_irq_mt6989(1, 0);
	uarthub_clear_host_irq_mt6989(2, 0);
	g_dev0_irq_sta = 0;
	g_dev0_sema_own_rel_irq_sta = 0;
	g_dev0_sema_own_tmo_irq_sta = 0;
	g_dev1_sema_own_tmo_irq_sta = 0;
	g_dev2_sema_own_tmo_irq_sta = 0;
	g_dev0_inband_irq_sta = 0;
	uarthub_mask_host_irq_mt6989(0, 0, 0);
	uarthub_mask_host_irq_mt6989(1, 0, 0);
	uarthub_mask_host_irq_mt6989(2, 0, 0);

	len = 0;
	for (i = 0; i < tx_data_len; i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), p_tx_data[i]);
	}

	pr_info("[%s] uart_%d send [%s] to uart_cmm",
		__func__, dev_index, dmp_info_buf);

	pr_info("[%s] before 1st tx, DBG_STATE=[0x%x]",
		__func__, UARTHUB_REG_READ(DBG_STATE_ADDR));

	state = uarthub_uartip_write_tx_data_mt6989(dev_index, p_tx_data, tx_data_len);

	pr_info("[%s] after 1st tx, DBG_STATE=[0x%x]",
		__func__, UARTHUB_REG_READ(DBG_STATE_ADDR));

	if (state != 0) {
		pr_notice("[%s] TX FAIL(%d): uart_%d send [%s] to uart_cmm, state=[%d] (dev[%d])",
			__func__, verify_index, dev_index, dmp_info_buf, state, dev_index);
		goto verify_err;
	}

	pr_info("[%s] before 1st rx, DBG_STATE=[0x%x]",
		__func__, UARTHUB_REG_READ(DBG_STATE_ADDR));

	recv_rx_len = 0;
	state = uarthub_uartip_read_rx_data_mt6989(3, evtBuf, TRX_BUF_LEN, &recv_rx_len);

	pr_info("[%s] after 1st rx, DBG_STATE=[0x%x]",
		__func__, UARTHUB_REG_READ(DBG_STATE_ADDR));

	if (recv_rx_len > 0) {
		len = 0;
		for (i = 0; i < recv_rx_len; i++) {
			len += snprintf(dmp_info_buf_recv + len, TRX_BUF_LEN - len,
				((i == 0) ? "%02X" : ",%02X"), evtBuf[i]);
		}
		pr_info("[%s] uart_cmm received [%s]", __func__, dmp_info_buf_recv);
	} else
		pr_info("[%s] uart_cmm received nothing", __func__);

	if ((tx_data_len > 1) && (recv_rx_len != (tx_data_len + 2))) {
		pr_notice("[%s] RX FAIL(%d): uart_cmm received size=[%d] is different with expect size=[%d] (dev[%d])",
			__func__, verify_index,
			recv_rx_len, (tx_data_len + 2), dev_index);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	if ((tx_data_len == 1) && (recv_rx_len != tx_data_len)) {
		pr_notice("[%s] RX FAIL(%d): uart_cmm received size=[%d] is different with expect size=[%d] (dev[%d])",
			__func__, verify_index,
			recv_rx_len, (tx_data_len + 2), dev_index);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	if (state != 0) {
		pr_notice("[%s] RX FAIL(%d): uart_cmm received [%s], size=[%d], state=[%d] (dev[%d])",
			__func__, verify_index, dmp_info_buf_recv,
			recv_rx_len, state, dev_index);
		goto verify_err;
	}

	for (i = 0; i < tx_data_len; i++) {
		if (p_tx_data[i] != evtBuf[i]) {
			pr_notice("[%s] RX FAIL(%d): uart_cmm received [%s] is different with rx_expect_data=[%s] (dev[%d])",
				__func__, verify_index, dmp_info_buf_recv,
				dmp_info_buf, dev_index);
			state = UARTHUB_UT_ERR_RX_FAIL;
			goto verify_err;
		}
	}

	mdelay(1);

	uarthub_config_uartip_dma_en_ctrl_mt6989(3, RX, 1);
	uarthub_config_uartip_dma_en_ctrl_mt6989(0, RX, 0);
	uarthub_config_uartip_dma_en_ctrl_mt6989(1, RX, 0);
	uarthub_config_uartip_dma_en_ctrl_mt6989(2, RX, 0);

	rx_index_2nd = dev_index;
	if (tx_data_len == 1 && p_tx_data[0] == 0xF1)
		rx_index_2nd = 1;
	else if (tx_data_len == 1 && p_tx_data[0] == 0xF2)
		rx_index_2nd = 2;

	pr_info("[%s] uart_cmm send [%s] to uart_%d",
		__func__, dmp_info_buf_recv, rx_index_2nd);

	pr_info("[%s] before 2nd tx, DBG_STATE=[0x%x]",
		__func__, UARTHUB_REG_READ(DBG_STATE_ADDR));

	state = uarthub_uartip_write_tx_data_mt6989(3, evtBuf, recv_rx_len);

	pr_info("[%s] after 2nd tx, DBG_STATE=[0x%x]",
		__func__, UARTHUB_REG_READ(DBG_STATE_ADDR));

	if (state != 0) {
		pr_notice("[%s] TX FAIL(%d): uart_cmm send [%s] to uart_%d, state=[%d] (dev[%d])",
			__func__, verify_index, dmp_info_buf_recv_2,
			rx_index_2nd, state, dev_index);
		goto verify_err;
	}

	pr_info("[%s] before 2nd rx, DBG_STATE=[0x%x]",
		__func__, UARTHUB_REG_READ(DBG_STATE_ADDR));

	recv_rx_len = 0;
	state = uarthub_uartip_read_rx_data_mt6989(
		rx_index_2nd, evtBuf_2, TRX_BUF_LEN, &recv_rx_len);

	pr_info("[%s] after 2nd rx, DBG_STATE=[0x%x]",
		__func__, UARTHUB_REG_READ(DBG_STATE_ADDR));

	if (recv_rx_len > 0) {
		len = 0;
		for (i = 0; i < recv_rx_len; i++) {
			len += snprintf(dmp_info_buf_recv_2 + len, TRX_BUF_LEN - len,
				((i == 0) ? "%02X" : ",%02X"), evtBuf_2[i]);
		}
		pr_info("[%s] uart_%d received [%s]", __func__, rx_index_2nd, dmp_info_buf_recv_2);
	} else
		pr_info("[%s] uart_%d received nothing", __func__, rx_index_2nd);

	if (tx_data_len == 1 && p_tx_data[0] == 0xFF) {
		if (recv_rx_len != 0) {
			pr_notice("[%s] RX FAIL(%d): uart_%d received size=[%d] is different with expect size=[0] (dev[%d])",
				__func__, verify_index, dev_index,
				recv_rx_len, dev_index);
			state = UARTHUB_UT_ERR_RX_FAIL;
			goto verify_err;
		}
		state = 0;
		goto verify_err;
	} else if (recv_rx_len != tx_data_len) {
		pr_notice("[%s] RX FAIL(%d): uart_%d received size=[%d] is different with expect size=[%d] (dev[%d])",
			__func__, verify_index, rx_index_2nd,
			recv_rx_len, tx_data_len, dev_index);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	if (state != 0) {
		pr_notice("[%s] RX FAIL(%d): uart_%d received [%s], size=[%d], state=[%d] (dev[%d])",
			__func__, verify_index, rx_index_2nd, dmp_info_buf_recv_2,
			recv_rx_len, state, dev_index);
		goto verify_err;
	}

	if (tx_data_len > 1) {
		for (i = 0; i < tx_data_len; i++) {
			if (p_tx_data[i] != evtBuf_2[i]) {
				pr_notice("[%s] RX FAIL(%d): uart_%d received [%s] is different with rx_expect_data=[%s] (dev[%d])",
					__func__, verify_index, dev_index, dmp_info_buf_recv_2,
					dmp_info_buf_recv, dev_index);
				state = UARTHUB_UT_ERR_RX_FAIL;
				goto verify_err;
			}
		}
	} else {
		if (p_tx_data[0] == 0xF0 || p_tx_data[0] == 0xF1 || p_tx_data[0] == 0xF2) {
			if (evtBuf_2[0] != 0xFF) {
				pr_notice("[%s] RX FAIL(%d): uart_%d received [%02X] is different with rx_expect_data=[FF] (dev[%d])",
					__func__, verify_index, rx_index_2nd,
					evtBuf_2[0], dev_index);
				state = UARTHUB_UT_ERR_RX_FAIL;
				goto verify_err;
			}
		} else {
			if (p_tx_data[0] != evtBuf_2[0]) {
				pr_notice("[%s] RX FAIL(%d): uart_%d received [%02X] is different with rx_expect_data=[%02X] (dev[%d])",
					__func__, verify_index, dev_index, evtBuf_2[0],
					p_tx_data[0], dev_index);
				state = UARTHUB_UT_ERR_RX_FAIL;
				goto verify_err;
			}
		}
	}

	mdelay(15);

	mask_bit = DEV0_IRQ_STA_VAL_rx_pkt_type_err(1);
	value_bit = 0;
	if (trx_pkt_type_irq == RX || trx_pkt_type_irq == TRX)
		value_bit = DEV0_IRQ_STA_VAL_rx_pkt_type_err(1);

	state = uarthub_core_get_host_irq_sta(0);
	if ((state & mask_bit) != value_bit) {
		pr_notice("[%s] FAIL(%d): rx_pkt_type_err has%s been triggered, irq_sta=[0x%x], mask_bit=[0x%x], value_bit=[0x%x] (dev[%d],trx_irq[%d])",
			__func__, verify_index,
			((value_bit != 0) ? " NOT" : ""),
			state, mask_bit, value_bit, dev_index, trx_pkt_type_irq);
		state = UARTHUB_UT_ERR_IRQ_STA_FAIL;
		goto verify_err;
	}

	mask_bit = DEV0_IRQ_STA_VAL_dev0_tx_pkt_type_err(1);
	if (dev_index == 1)
		mask_bit = DEV0_IRQ_STA_VAL_dev1_tx_pkt_type_err(1);
	else if (dev_index == 2)
		mask_bit = DEV0_IRQ_STA_VAL_dev2_tx_pkt_type_err(1);
	value_bit = mask_bit;
	if (trx_pkt_type_irq == RX || trx_pkt_type_irq == TRX_NONE)
		value_bit = 0;

	if ((state & mask_bit) != value_bit) {
		pr_notice("[%s] FAIL(%d): dev%d_tx_pkt_type_err has%s been triggered, irq_sta=[0x%x], mask_bit=[0x%x], value_bit=[0x%x] (dev[%d],trx_irq[%d])",
			__func__, verify_index, dev_index,
			((value_bit != 0) ? " NOT" : ""),
			state, mask_bit, value_bit, dev_index, trx_pkt_type_irq);
		state = UARTHUB_UT_ERR_IRQ_STA_FAIL;
		goto verify_err;
	}

	mask_bit = DEV0_IRQ_STA_VAL_dev0_tx_timeout_err(1);
	if (dev_index == 1)
		mask_bit = DEV0_IRQ_STA_VAL_dev1_tx_timeout_err(1);
	else if (dev_index == 2)
		mask_bit = DEV0_IRQ_STA_VAL_dev2_tx_timeout_err(1);
	value_bit = mask_bit;
	if (tx_data_len > 1 || trx_pkt_type_irq == RX || trx_pkt_type_irq == TRX_NONE)
		value_bit = 0;

	if ((state & mask_bit) != value_bit) {
		pr_notice("[%s] FAIL(%d): dev%d_tx_timeout_err has%s been triggered, irq_sta=[0x%x], mask_bit=[0x%x], value_bit=[0x%x] (dev[%d],trx_irq[%d])",
			__func__, verify_index, dev_index,
			((value_bit != 0) ? " NOT" : ""),
			state, mask_bit, value_bit, dev_index, trx_pkt_type_irq);
		state = UARTHUB_UT_ERR_IRQ_STA_FAIL;
		goto verify_err;
	}

	mask_bit = DEV0_IRQ_STA_VAL_dev0_rx_timeout_err(1);
	value_bit = 0;
	if ((tx_data_len == 1) && (trx_pkt_type_irq == RX || trx_pkt_type_irq == TRX))
		value_bit = DEV0_IRQ_STA_VAL_dev0_rx_timeout_err(1);

	if ((state & mask_bit) != value_bit) {
		pr_notice("[%s] FAIL(%d): dev0_rx_timeout_err has%s been triggered, irq_sta=[0x%x], mask_bit=[0x%x], value_bit=[0x%x] (dev[%d],trx_irq[%d])",
			__func__, verify_index,
			((value_bit != 0) ? " NOT" : ""),
			state, mask_bit, value_bit, dev_index, trx_pkt_type_irq);
		state = UARTHUB_UT_ERR_IRQ_STA_FAIL;
		goto verify_err;
	}

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL, 0);
	}

	uarthub_mask_host_irq_mt6989(0, 0, 1);
	uarthub_mask_host_irq_mt6989(1, 0, 1);
	uarthub_mask_host_irq_mt6989(2, 0, 1);
	uarthub_clear_host_irq_mt6989(0, 0);
	uarthub_clear_host_irq_mt6989(1, 0);
	uarthub_clear_host_irq_mt6989(2, 0);
	g_dev0_irq_sta = 0;
	g_dev0_sema_own_rel_irq_sta = 0;
	g_dev0_sema_own_tmo_irq_sta = 0;
	g_dev1_sema_own_tmo_irq_sta = 0;
	g_dev2_sema_own_tmo_irq_sta = 0;
	g_dev0_inband_irq_sta = 0;
	uarthub_mask_host_irq_mt6989(0, 0, 0);
	uarthub_mask_host_irq_mt6989(1, 0, 0);
	uarthub_mask_host_irq_mt6989(2, 0, 0);

	uarthub_config_uartip_dma_en_ctrl_mt6989(0, RX, 1);
	uarthub_config_uartip_dma_en_ctrl_mt6989(1, RX, 1);
	uarthub_config_uartip_dma_en_ctrl_mt6989(2, RX, 1);

	uarthub_reset_intfhub_mt6989();

	return state;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_ut_ip_verify_pkt_hdr_fmt_mt6989(void)
{
	int state = 0;
	int verify_index = 0;
	unsigned char TEST_TX_CMD[] = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x00 };
	int tx_data_len = 0;

	/* Initialize */
	uarthub_set_host_trx_request_mt6989(0, TRX);
	mdelay(3);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
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

	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 1);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 1);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);

	/* Verify */
	TEST_TX_CMD[0] = 0x01;
	tx_data_len = 5;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX_NONE);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x05;
	tx_data_len = 6;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX_NONE);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x40;
	tx_data_len = 6;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX_NONE);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x41;
	tx_data_len = 6;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX_NONE);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x81;
	tx_data_len = 5;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		2, ++verify_index, TEST_TX_CMD, tx_data_len, TRX_NONE);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x87;
	tx_data_len = 6;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		1, ++verify_index, TEST_TX_CMD, tx_data_len, TRX_NONE);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x06;
	tx_data_len = 6;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x3F;
	tx_data_len = 6;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x42;
	tx_data_len = 6;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x80;
	tx_data_len = 6;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x88;
	tx_data_len = 6;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0xFF;
	tx_data_len = 1;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX_NONE);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0xF0;
	tx_data_len = 1;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TX);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0xF2;
	tx_data_len = 1;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TX);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0xF3;
	tx_data_len = 1;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0xFE;
	tx_data_len = 1;
	state = uarthub_ut_ip_verify_pkt_hdr_fmt_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, TRX);
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL, 0);
	}

	/* Uninitialize */
	uarthub_set_host_loopback_ctrl_mt6989(0, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(1, 1, 0);
	uarthub_set_host_loopback_ctrl_mt6989(2, 1, 0);
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);
	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	mdelay(3);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	mdelay(1);
#endif
	return state;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_ut_ip_verify_trx_not_ready_by_unit_mt6989(
	int dev_index,
	int verify_index,
	unsigned char *p_tx_data,
	int tx_data_len,
	int verify_bypass_mode)
{
	int state = 0;
	unsigned char dmp_info_buf[TRX_BUF_LEN];
	int len = 0;
	int i = 0;
	int mask_bit = 0, value_bit = 0;
	int bypass_backup = 0;
	int uartip_rx_fifo_size[4] = { 0 };
	int apuart_portNo = 0;
	int md_irq_en = 0, adsp_irq_en = 0;

	if (!p_tx_data || tx_data_len <= 0)
		return UARTHUB_ERR_PARA_WRONG;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		apuart_portNo = 3;
	else if (dev_index == 1)
		apuart_portNo = 2;
	else if (dev_index == 2)
		apuart_portNo = 1;

	uarthub_config_uartip_dma_en_ctrl_mt6989(-1, TRX, 0);
	uarthub_reset_fifo_trx_mt6989();
	uarthub_reset_intfhub_mt6989();

	uarthub_mask_host_irq_mt6989(0, 0, 1);
	uarthub_mask_host_irq_mt6989(1, 0, 1);
	uarthub_mask_host_irq_mt6989(2, 0, 1);
	uarthub_clear_host_irq_mt6989(0, 0);
	uarthub_clear_host_irq_mt6989(1, 0);
	uarthub_clear_host_irq_mt6989(2, 0);
	g_dev0_irq_sta = 0;
	g_dev0_sema_own_rel_irq_sta = 0;
	g_dev0_sema_own_tmo_irq_sta = 0;
	g_dev1_sema_own_tmo_irq_sta = 0;
	g_dev2_sema_own_tmo_irq_sta = 0;
	g_dev0_inband_irq_sta = 0;
	uarthub_mask_host_irq_mt6989(0, 0, 0);
	uarthub_mask_host_irq_mt6989(1, 0, 0);
	uarthub_mask_host_irq_mt6989(2, 0, 0);

	md_irq_en = RX_DATA_REQ_MASK_GET_uarthub_to_md_eint_en(RX_DATA_REQ_MASK_ADDR);
	adsp_irq_en = RX_DATA_REQ_MASK_GET_uarthub_to_adsp_eint_en(RX_DATA_REQ_MASK_ADDR);
	RX_DATA_REQ_MASK_SET_uarthub_to_md_eint_en(RX_DATA_REQ_MASK_ADDR, 1);
	RX_DATA_REQ_MASK_SET_uarthub_to_adsp_eint_en(RX_DATA_REQ_MASK_ADDR, 1);

	if (verify_bypass_mode == 1) {
		bypass_backup = uarthub_is_bypass_mode_mt6989();
		uarthub_config_bypass_ctrl_mt6989(1);
	}

	for (i = 0; i < 3; i++) {
		state = uarthub_is_host_uarthub_ready_state_mt6989(i);
		if (state != 0) {
			pr_notice("[%s] FAIL(%d): dev%d uarthub is ready(%d) (dev[%d])",
				__func__, verify_index, i, state, dev_index);
			state = UARTHUB_UT_ERR_HUB_READY_STA;
			goto verify_err;
		}
	}

	uartip_rx_fifo_size[0] = uarthub_get_host_rx_fifo_size_mt6989(0);
	uartip_rx_fifo_size[1] = uarthub_get_host_rx_fifo_size_mt6989(1);
	uartip_rx_fifo_size[2] = uarthub_get_host_rx_fifo_size_mt6989(2);
	uartip_rx_fifo_size[3] = uarthub_get_cmm_rx_fifo_size_mt6989();

	len = 0;
	for (i = 0; i < tx_data_len; i++) {
		len += snprintf(dmp_info_buf + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), p_tx_data[i]);
	}

	pr_info("[%s] before ap_uart_%d (dev%d) TX(%d), IRQ_STA_1/2=[0x%x-0x%x]",
				__func__, apuart_portNo, dev_index, verify_index,
				UARTHUB_REG_READ(DEV1_IRQ_ADDR),
				UARTHUB_REG_READ(DEV2_IRQ_ADDR));

	state = uarthub_apuart_write_tx_data_mt6989(apuart_portNo, p_tx_data, tx_data_len);

	if (state != 0) {
		pr_notice("[%s] TX FAIL(%d): uart_%d send [%s] to uart_cmm, state=[%d] (dev[%d])",
			__func__, verify_index, dev_index, dmp_info_buf, state, dev_index);
		goto verify_err;
	}

	mdelay(1);

	pr_info("[%s] after ap_uart_%d (dev%d) TX(%d), IRQ_STA_1/2=[0x%x-0x%x]",
				__func__, apuart_portNo, dev_index, verify_index,
				UARTHUB_REG_READ(DEV1_IRQ_ADDR),
				UARTHUB_REG_READ(DEV2_IRQ_ADDR));

	for (i = 0; i <= 3; i++) {
		if (i == 3)
			state = uarthub_get_cmm_rx_fifo_size_mt6989();
		else
			state = uarthub_get_host_rx_fifo_size_mt6989(i);
		if (state != uartip_rx_fifo_size[i]) {
			pr_notice("[%s] FAIL(%d): %s rx fifo size is not empty, size=[%d], expect size=[%d] (dev[%d])",
				__func__, verify_index,
				((i == 3) ? "uart_cmm" : ((i == 0) ? "uart_0" :
					((i == 1) ? "uart_1" : "uart_2"))),
				state, uartip_rx_fifo_size[i], dev_index);
			state = UARTHUB_UT_ERR_FIFO_SIZE_FAIL;
			goto verify_err;
		}
	}

	mdelay(1);

	mask_bit = DEV0_IRQ_STA_VAL_intfhub_dev0_tx_err(1);
	if (dev_index == 1)
		mask_bit = DEV0_IRQ_STA_VAL_intfhub_dev1_tx_err(1);
	else if (dev_index == 2)
		mask_bit = DEV0_IRQ_STA_VAL_intfhub_dev2_tx_err(1);
	value_bit = mask_bit;

	state = uarthub_core_get_host_irq_sta(0);
	if ((state & mask_bit) != value_bit) {
		pr_notice("[%s] FAIL(%d): intfhub_dev%d_tx_err has NOT been triggered, irq_sta=[0x%x], mask_bit=[0x%x], value_bit=[0x%x] (dev[%d])",
			__func__, verify_index, dev_index,
			state, mask_bit, value_bit, dev_index);
		state = UARTHUB_UT_ERR_IRQ_STA_FAIL;
		goto verify_err;
	}

	if (dev_index == 1)
		state = DEV1_IRQ_GET_intfhub_dev1_tx_err_for_dev1(DEV1_IRQ_ADDR);
	else if (dev_index == 2)
		state = DEV2_IRQ_GET_intfhub_dev2_tx_err_for_dev2(DEV2_IRQ_ADDR);

	if (dev_index != 0 && state == 0) {
		pr_notice("[%s] FAIL(%d): intfhub_dev%d_tx_err_for_dev%d has NOT been triggered, irq_sta=[0x%x], mask_bit=[0x%x], value_bit=[0x%x] (dev[%d])",
			__func__, verify_index, dev_index, dev_index,
			state, mask_bit, value_bit, dev_index);
		state = UARTHUB_UT_ERR_IRQ_STA_FAIL;
		goto verify_err;
	}

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL, 0);
	}

	if (verify_bypass_mode == 1)
		uarthub_config_bypass_ctrl_mt6989(bypass_backup);

	RX_DATA_REQ_MASK_SET_uarthub_to_md_eint_en(RX_DATA_REQ_MASK_ADDR, md_irq_en);
	RX_DATA_REQ_MASK_SET_uarthub_to_adsp_eint_en(RX_DATA_REQ_MASK_ADDR, adsp_irq_en);

	uarthub_mask_host_irq_mt6989(0, 0, 1);
	uarthub_mask_host_irq_mt6989(1, 0, 1);
	uarthub_mask_host_irq_mt6989(2, 0, 1);
	uarthub_clear_host_irq_mt6989(0, 0);
	uarthub_clear_host_irq_mt6989(1, 0);
	uarthub_clear_host_irq_mt6989(2, 0);
	g_dev0_irq_sta = 0;
	g_dev0_sema_own_rel_irq_sta = 0;
	g_dev0_sema_own_tmo_irq_sta = 0;
	g_dev1_sema_own_tmo_irq_sta = 0;
	g_dev2_sema_own_tmo_irq_sta = 0;
	g_dev0_inband_irq_sta = 0;
	uarthub_mask_host_irq_mt6989(0, 0, 0);
	uarthub_mask_host_irq_mt6989(1, 0, 0);
	uarthub_mask_host_irq_mt6989(2, 0, 0);

	uarthub_reset_fifo_trx_mt6989();
	uarthub_reset_intfhub_mt6989();

	uarthub_config_uartip_dma_en_ctrl_mt6989(-1, TRX, 1);

	return state;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_ut_ip_verify_trx_not_ready_mt6989(void)
{
	int state = 0;
	int verify_index = 0;
	unsigned char TEST_TX_CMD[] = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x00 };
	int tx_data_len = 0;
	int i = 0;

	/* Initialize */
	uarthub_init_default_apuart_config_mt6989();
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 1);

	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	mdelay(3);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	mdelay(1);
#endif

	for (i = 0; i < 3; i++) {
		state = uarthub_is_host_uarthub_ready_state_mt6989(i);
		if (state != 0) {
			pr_notice("[%s] FAIL: dev%d uarthub is ready(%d)",
				__func__, i, state);
			state = UARTHUB_UT_ERR_HUB_READY_STA;
			goto verify_err;
		}
	}

	/* Verify */
	TEST_TX_CMD[0] = 0x01;
	tx_data_len = 5;
	state = uarthub_ut_ip_verify_trx_not_ready_by_unit_mt6989(
		0, ++verify_index, TEST_TX_CMD, tx_data_len, 0);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x87;
	tx_data_len = 6;
	state = uarthub_ut_ip_verify_trx_not_ready_by_unit_mt6989(
		1, ++verify_index, TEST_TX_CMD, tx_data_len, 0);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x81;
	tx_data_len = 5;
	state = uarthub_ut_ip_verify_trx_not_ready_by_unit_mt6989(
		2, ++verify_index, TEST_TX_CMD, tx_data_len, 0);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x87;
	tx_data_len = 6;
	state = uarthub_ut_ip_verify_trx_not_ready_by_unit_mt6989(
		1, ++verify_index, TEST_TX_CMD, tx_data_len, 1);
	if (state != 0)
		goto verify_err;

	TEST_TX_CMD[0] = 0x81;
	tx_data_len = 5;
	state = uarthub_ut_ip_verify_trx_not_ready_by_unit_mt6989(
		2, ++verify_index, TEST_TX_CMD, tx_data_len, 1);
	if (state != 0)
		goto verify_err;

	state = 0;
verify_err:
	if (state != 0) {
		uarthub_dump_intfhub_debug_info_mt6989(__func__);
		uarthub_dump_uartip_debug_info_mt6989(__func__, NULL, 0);
	}

	/* Uninitialize */
	uarthub_clear_host_trx_request_mt6989(0, TRX);
	uarthub_clear_host_trx_request_mt6989(1, TRX);
	uarthub_clear_host_trx_request_mt6989(2, TRX);
	mdelay(3);
#if !(UARTHUB_SUPPORT_SSPM_DRIVER) || (UARTHUB_SUPPORT_FPGA)
	UARTHUB_REG_WRITE(IRQ_CLR_ADDR, 0xFFFFFFFF);
	mdelay(1);
#endif
	uarthub_set_cmm_loopback_ctrl_mt6989(1, 0);

	return state;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */

#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_sspm_irq_handle_mt6989(int sspm_irq)
{
	if ((sspm_irq & REG_FLD_MASK(IRQ_STA_FLD_intfhub_restore_req)) != 0) {
		pr_info("[%s] intfhub_restore_req\n", __func__);
		uarthub_uarthub_init_mt6989(NULL);
	} else if ((sspm_irq & REG_FLD_MASK(IRQ_STA_FLD_intfhub_ckon_req)) != 0)
		pr_info("[%s] intfhub_ckon_req\n", __func__);
	else if ((sspm_irq & REG_FLD_MASK(IRQ_STA_FLD_intfhub_ckoff_req)) != 0)
		pr_info("[%s] intfhub_ckoff_req\n", __func__);
	else
		pr_info("[%s] other sspm irq(0x%x)\n", __func__, sspm_irq);

	return 0;
}
#endif /* UARTHUB_SUPPORT_DX4_FPGA */
