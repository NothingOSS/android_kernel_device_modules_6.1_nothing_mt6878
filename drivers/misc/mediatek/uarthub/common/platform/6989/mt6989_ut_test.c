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

struct uarthub_ut_test_ops_struct mt6989_plat_ut_test_data = {
	.uarthub_plat_is_ut_testing = uarthub_is_ut_testing_mt6989,

#if UARTHUB_SUPPORT_UT_API
	.uarthub_plat_is_host_uarthub_ready_state = uarthub_is_host_uarthub_ready_state_mt6989,
	.uarthub_plat_get_host_irq_sta = uarthub_get_host_irq_sta_mt6989,
	.uarthub_plat_clear_host_irq = uarthub_clear_host_irq_mt6989,
	.uarthub_plat_mask_host_irq = uarthub_mask_host_irq_mt6989,
	.uarthub_plat_config_host_irq_ctrl = uarthub_config_host_irq_ctrl_mt6989,
	.uarthub_plat_get_host_rx_fifo_size = uarthub_get_host_rx_fifo_size_mt6989,
	.uarthub_plat_get_cmm_rx_fifo_size = uarthub_get_cmm_rx_fifo_size_mt6989,
	.uarthub_plat_config_uartip_dma_en_ctrl = uarthub_config_uartip_dma_en_ctrl_mt6989,
	.uarthub_plat_reset_fifo_trx = uarthub_reset_fifo_trx_mt6989,
	.uarthub_plat_uartip_write_data_to_tx_buf = uarthub_uartip_write_data_to_tx_buf_mt6989,
	.uarthub_plat_uartip_read_data_from_rx_buf = uarthub_uartip_read_data_from_rx_buf_mt6989,
	.uarthub_plat_is_uartip_tx_buf_empty_for_write =
		uarthub_is_uartip_tx_buf_empty_for_write_mt6989,
	.uarthub_plat_is_uartip_rx_buf_ready_for_read =
		uarthub_is_uartip_rx_buf_ready_for_read_mt6989,
	.uarthub_plat_is_uartip_throw_xoff = uarthub_is_uartip_throw_xoff_mt6989,
	.uarthub_plat_config_uartip_rx_fifo_trig_thr =
		uarthub_config_uartip_rx_fifo_trig_thr_mt6989,
#endif

#if UARTHUB_SUPPORT_DX4_FPGA
	.uarthub_plat_request_host_sema_own_sta = uarthub_request_host_sema_own_sta_mt6989,
	.uarthub_plat_set_host_sema_own_rel = uarthub_set_host_sema_own_rel_mt6989,
	.uarthub_plat_get_host_sema_own_rel_irq_sta = uarthub_get_host_sema_own_rel_irq_sta_mt6989,
	.uarthub_plat_clear_host_sema_own_rel_irq = uarthub_clear_host_sema_own_rel_irq_mt6989,
	.uarthub_plat_reset_host_sema_own = uarthub_reset_host_sema_own_mt6989,
	.uarthub_plat_get_host_sema_own_tmo_irq_sta = uarthub_get_host_sema_own_tmo_irq_sta_mt6989,
	.uarthub_plat_clear_host_sema_own_tmo_irq = uarthub_clear_host_sema_own_tmo_irq_mt6989,
	.uarthub_plat_reset_host_sema_own_tmo = uarthub_reset_host_sema_own_tmo_mt6989,
	.uarthub_plat_config_inband_esc_char = uarthub_config_inband_esc_char_mt6989,
	.uarthub_plat_config_inband_esc_sta = uarthub_config_inband_esc_sta_mt6989,
	.uarthub_plat_config_inband_enable_ctrl = uarthub_config_inband_enable_ctrl_mt6989,
	.uarthub_plat_config_inband_irq_enable_ctrl = uarthub_config_inband_irq_enable_ctrl_mt6989,
	.uarthub_plat_config_inband_trigger = uarthub_config_inband_trigger_mt6989,
	.uarthub_plat_is_inband_tx_complete = uarthub_is_inband_tx_complete_mt6989,
	.uarthub_plat_get_inband_irq_sta = uarthub_get_inband_irq_sta_mt6989,
	.uarthub_plat_clear_inband_irq = uarthub_clear_inband_irq_mt6989,
	.uarthub_plat_get_received_inband_sta = uarthub_get_received_inband_sta_mt6989,
	.uarthub_plat_clear_received_inband_sta = uarthub_clear_received_inband_sta_mt6989,
	.uarthub_plat_ut_ip_verify_pkt_hdr_fmt = uarthub_ut_ip_verify_pkt_hdr_fmt_mt6989,
	.uarthub_plat_ut_ip_verify_trx_not_ready = uarthub_ut_ip_verify_trx_not_ready_mt6989,
	.uarthub_plat_sspm_irq_handle = uarthub_sspm_irq_handle_mt6989,
#endif
};

int uarthub_is_ut_testing_mt6989(void)
{
#if (UARTHUB_SUPPORT_FPGA) || (UARTHUB_SUPPORT_DVT)
	return 1;
#else
	return 0;
#endif
}

#if UARTHUB_SUPPORT_UT_API
int uarthub_is_host_uarthub_ready_state_mt6989(int dev_index)
{
	int state = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		state = DEV0_STA_GET_dev0_intfhub_ready(DEV0_STA_ADDR);
	else if (dev_index == 1)
		state = DEV1_STA_GET_dev1_intfhub_ready(DEV1_STA_ADDR);
	else if (dev_index == 2)
		state = DEV2_STA_GET_dev2_intfhub_ready(DEV2_STA_ADDR);

	return state;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_get_host_irq_sta_mt6989(int dev_index)
{
	int state = 0;
#if UARTHUB_SUPPORT_DX4_FPGA
	int mask = 0;
#endif

#if UARTHUB_SUPPORT_DX4_FPGA
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
#else
	if (dev_index != 0) {
#endif
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

#if UARTHUB_SUPPORT_DX4_FPGA
	if (dev_index == 0)
#endif
		state = UARTHUB_REG_READ(DEV0_IRQ_STA_ADDR);
#if UARTHUB_SUPPORT_DX4_FPGA
	else if (dev_index == 1) {
		mask = DEV1_IRQ_VAL_dev1_crc_err_for_dev1(1) |
			DEV1_IRQ_VAL_dev1_tx_timeout_err_for_dev1(1) |
			DEV1_IRQ_VAL_dev1_tx_pkt_type_err_for_dev1(1) |
			DEV1_IRQ_VAL_dev1_rx_timeout_err_for_dev1(1) |
			DEV1_IRQ_VAL_intfhub_dev1_tx_err_for_dev1(1) |
			DEV1_IRQ_VAL_dev1_sema_own_rel_irq(1) |
			DEV1_IRQ_VAL_uarthub_to_md_eint_b(1);
		state = UARTHUB_REG_READ_BIT(DEV1_IRQ_ADDR, mask);
	} else if (dev_index == 2) {
		mask = DEV2_IRQ_VAL_dev2_crc_err_for_dev2(1) |
			DEV2_IRQ_VAL_dev2_tx_timeout_err_for_dev2(1) |
			DEV2_IRQ_VAL_dev2_tx_pkt_type_err_for_dev2(1) |
			DEV2_IRQ_VAL_dev2_rx_timeout_err_for_dev2(1) |
			DEV2_IRQ_VAL_intfhub_dev2_tx_err_for_dev2(1) |
			DEV2_IRQ_VAL_dev2_sema_own_rel_irq(1) |
			DEV2_IRQ_VAL_uarthub_to_adsp_eint_b(1);
		state = UARTHUB_REG_READ_BIT(DEV2_IRQ_ADDR, mask);
	}
#endif
	return state;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_clear_host_irq_mt6989(int dev_index, int mask_bit)
{
#if UARTHUB_SUPPORT_DX4_FPGA
	int mask = 0;
#endif

#if UARTHUB_SUPPORT_DX4_FPGA
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
#else
	if (dev_index != 0) {
#endif
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

#if UARTHUB_SUPPORT_DX4_FPGA
	if (dev_index == 0) {
#endif
		if (mask_bit > 0)
			UARTHUB_REG_WRITE(DEV0_IRQ_CLR_ADDR, mask_bit);
		else
			UARTHUB_REG_WRITE(DEV0_IRQ_CLR_ADDR, 0xFFFFFFFF);
#if UARTHUB_SUPPORT_DX4_FPGA
	} else if (dev_index == 1) {
		if (mask_bit > 0)
			UARTHUB_REG_WRITE(DEV1_IRQ_ADDR, mask_bit);
		else {
			mask = DEV1_IRQ_VAL_dev1_crc_err_clr_for_dev1(1) |
				DEV1_IRQ_VAL_dev1_tx_timeout_err_clr_for_dev1(1) |
				DEV1_IRQ_VAL_dev1_tx_pkt_type_err_clr_for_dev1(1) |
				DEV1_IRQ_VAL_dev1_rx_timeout_err_clr_for_dev1(1) |
				DEV1_IRQ_VAL_intfhub_dev1_tx_err_clr_for_dev1(1) |
				DEV1_IRQ_VAL_dev1_sema_own_rel_clr(1);
			UARTHUB_SET_BIT(DEV1_IRQ_ADDR, mask);
		}
	} else if (dev_index == 2) {
		if (mask_bit > 0)
			UARTHUB_REG_WRITE(DEV2_IRQ_ADDR, mask_bit);
		else {
			mask = DEV2_IRQ_VAL_dev2_crc_err_clr_for_dev2(1) |
				DEV2_IRQ_VAL_dev2_tx_timeout_err_clr_for_dev2(1) |
				DEV2_IRQ_VAL_dev2_tx_pkt_type_err_clr_for_dev2(1) |
				DEV2_IRQ_VAL_dev2_rx_timeout_err_clr_for_dev2(1) |
				DEV2_IRQ_VAL_intfhub_dev2_tx_err_clr_for_dev2(1) |
				DEV2_IRQ_VAL_dev2_sema_own_rel_clr(1);
			UARTHUB_SET_BIT(DEV2_IRQ_ADDR, mask);
		}
	}
#endif

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_mask_host_irq_mt6989(int dev_index, int mask_bit, int is_mask)
{
#if UARTHUB_SUPPORT_DX4_FPGA
	int mask = 0;
#endif

#if UARTHUB_SUPPORT_DX4_FPGA
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
#else
	if (dev_index != 0) {
#endif
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

#if UARTHUB_SUPPORT_DX4_FPGA
	if (dev_index == 0) {
#endif
		if (mask_bit > 0) {
			UARTHUB_REG_WRITE_MASK(
				DEV0_IRQ_MASK_ADDR, ((is_mask == 1) ? mask_bit : 0x0), mask_bit);
		} else
			UARTHUB_REG_WRITE(DEV0_IRQ_MASK_ADDR, ((is_mask == 1) ? 0xFFFFFFFF : 0x0));
#if UARTHUB_SUPPORT_DX4_FPGA
	} else if (dev_index == 1) {
		if (mask_bit > 0) {
			UARTHUB_REG_WRITE_MASK(
				DEV1_IRQ_ADDR, ((is_mask == 1) ? mask_bit : 0x0), mask_bit);
		} else {
			mask = DEV1_IRQ_VAL_dev1_crc_err_mask_for_dev1(1) |
				DEV1_IRQ_VAL_dev1_tx_timeout_err_mask_for_dev1(1) |
				DEV1_IRQ_VAL_dev1_tx_pkt_type_err_mask_for_dev1(1) |
				DEV1_IRQ_VAL_dev1_rx_timeout_err_mask_for_dev1(1) |
				DEV1_IRQ_VAL_intfhub_dev1_tx_err_mask_for_dev1(1) |
				DEV1_IRQ_VAL_dev1_sema_own_rel_mask(1);
			UARTHUB_REG_WRITE_MASK(DEV1_IRQ_ADDR, ((is_mask == 1) ? mask : 0x0), mask);
		}
	} else if (dev_index == 2) {
		if (mask_bit > 0) {
			UARTHUB_REG_WRITE_MASK(
				DEV2_IRQ_ADDR, ((is_mask == 1) ? mask_bit : 0x0), mask_bit);
		} else {
			mask = DEV2_IRQ_VAL_dev2_crc_err_mask_for_dev2(1) |
				DEV2_IRQ_VAL_dev2_tx_timeout_err_mask_for_dev2(1) |
				DEV2_IRQ_VAL_dev2_tx_pkt_type_err_mask_for_dev2(1) |
				DEV2_IRQ_VAL_dev2_rx_timeout_err_mask_for_dev2(1) |
				DEV2_IRQ_VAL_intfhub_dev2_tx_err_mask_for_dev2(1) |
				DEV2_IRQ_VAL_dev2_sema_own_rel_mask(1);
			UARTHUB_REG_WRITE_MASK(DEV2_IRQ_ADDR, ((is_mask == 1) ? mask : 0x0), mask);
		}
	}
#endif

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_config_host_irq_ctrl_mt6989(int dev_index, int enable)
{
#if UARTHUB_SUPPORT_DX4_FPGA
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
#else
	if (dev_index != 0) {
#endif
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

#if UARTHUB_SUPPORT_DX4_FPGA
	if (dev_index == 0)
#endif
		UARTHUB_REG_WRITE(DEV0_IRQ_MASK_ADDR, ((enable == 1) ? 0x0 : 0xFFFFFFFF));
#if UARTHUB_SUPPORT_DX4_FPGA
	else if (dev_index == 1)
		RX_DATA_REQ_MASK_SET_uarthub_to_md_eint_en(
			RX_DATA_REQ_MASK_ADDR, ((enable == 1) ? 0x1 : 0x0));
	else if (dev_index == 2)
		RX_DATA_REQ_MASK_SET_uarthub_to_adsp_eint_en(
			RX_DATA_REQ_MASK_ADDR, ((enable == 1) ? 0x1 : 0x0));
#endif

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_get_host_rx_fifo_size_mt6989(int dev_index)
{
	int state = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		state = UARTHUB_REG_READ_BIT(DEBUG_7(dev0_base_remap_addr_mt6989), 0x3F);
	else if (dev_index == 1)
		state = UARTHUB_REG_READ_BIT(DEBUG_7(dev1_base_remap_addr_mt6989), 0x3F);
	else if (dev_index == 2)
		state = UARTHUB_REG_READ_BIT(DEBUG_7(dev2_base_remap_addr_mt6989), 0x3F);

	return state;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_get_cmm_rx_fifo_size_mt6989(void)
{
	return UARTHUB_REG_READ_BIT(DEBUG_7(cmm_base_remap_addr_mt6989), 0x3F);
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_config_uartip_dma_en_ctrl_mt6989(int dev_index, enum uarthub_trx_type trx, int enable)
{
	void __iomem *uarthub_dev_base = dev0_base_remap_addr_mt6989;
	int i = 0;

	if (dev_index < -1 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++) {
		if (dev_index >= 0 && dev_index != i)
			continue;

		if (i == 0)
			uarthub_dev_base = dev0_base_remap_addr_mt6989;
		else if (i == 1)
			uarthub_dev_base = dev1_base_remap_addr_mt6989;
		else if (i == 2)
			uarthub_dev_base = dev2_base_remap_addr_mt6989;
		else if (i == UARTHUB_MAX_NUM_DEV_HOST)
			uarthub_dev_base = cmm_base_remap_addr_mt6989;

		if (trx == RX)
			DMA_EN_SET_RX_DMA_EN(DMA_EN_ADDR(uarthub_dev_base),
				((enable == 0) ? 0x0 : 0x1));
		else if (trx == TX)
			DMA_EN_SET_TX_DMA_EN(DMA_EN_ADDR(uarthub_dev_base),
				((enable == 0) ? 0x0 : 0x1));
		else if (trx == TRX)
			UARTHUB_REG_WRITE_MASK(DMA_EN_ADDR(uarthub_dev_base),
				(DMA_EN_VAL_RX_DMA_EN(((enable == 0) ? 0x0 : 0x1)) |
					DMA_EN_VAL_TX_DMA_EN(((enable == 0) ? 0x0 : 0x1))),
				(DMA_EN_VAL_RX_DMA_EN(1) | DMA_EN_VAL_TX_DMA_EN(1)));
	}

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_reset_fifo_trx_mt6989(void)
{
	UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6989), 0x80);
	UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6989), 0x80);
	UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6989), 0x80);
	UARTHUB_REG_WRITE(FCR_ADDR(cmm_base_remap_addr_mt6989), 0x80);

	usleep_range(50, 60);

	UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6989), 0x87);
	UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6989), 0x87);
	UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6989), 0x87);
	UARTHUB_REG_WRITE(FCR_ADDR(cmm_base_remap_addr_mt6989), 0x87);

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_reset_intfhub_mt6989(void)
{
	UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6989), 0x80);
	UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6989), 0x80);
	UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6989), 0x80);
	UARTHUB_REG_WRITE(FCR_ADDR(cmm_base_remap_addr_mt6989), 0x80);

	CON4_SET_sw4_rst(CON4_ADDR, 1);

	UARTHUB_REG_WRITE(FCR_ADDR(dev0_base_remap_addr_mt6989), 0x81);
	UARTHUB_REG_WRITE(FCR_ADDR(dev1_base_remap_addr_mt6989), 0x81);
	UARTHUB_REG_WRITE(FCR_ADDR(dev2_base_remap_addr_mt6989), 0x81);
	UARTHUB_REG_WRITE(FCR_ADDR(cmm_base_remap_addr_mt6989), 0x81);

	return 0;
}
#endif

#if UARTHUB_SUPPORT_UT_API
int uarthub_uartip_write_data_to_tx_buf_mt6989(int dev_index, int tx_data)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		UARTHUB_REG_WRITE(THR_ADDR(dev0_base_remap_addr_mt6989), tx_data);
	else if (dev_index == 1)
		UARTHUB_REG_WRITE(THR_ADDR(dev1_base_remap_addr_mt6989), tx_data);
	else if (dev_index == 2)
		UARTHUB_REG_WRITE(THR_ADDR(dev2_base_remap_addr_mt6989), tx_data);
	else if (dev_index == UARTHUB_MAX_NUM_DEV_HOST)
		UARTHUB_REG_WRITE(THR_ADDR(cmm_base_remap_addr_mt6989), tx_data);

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_uartip_read_data_from_rx_buf_mt6989(int dev_index)
{
	int rx_data = 0;

	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		rx_data = UARTHUB_REG_READ(RBR_ADDR(dev0_base_remap_addr_mt6989));
	else if (dev_index == 1)
		rx_data = UARTHUB_REG_READ(RBR_ADDR(dev1_base_remap_addr_mt6989));
	else if (dev_index == 2)
		rx_data = UARTHUB_REG_READ(RBR_ADDR(dev2_base_remap_addr_mt6989));
	else if (dev_index == UARTHUB_MAX_NUM_DEV_HOST)
		rx_data = UARTHUB_REG_READ(RBR_ADDR(cmm_base_remap_addr_mt6989));

	return rx_data;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_is_uartip_tx_buf_empty_for_write_mt6989(int dev_index)
{
	int is_empty = 0;

	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		is_empty = LSR_GET_TEMT(LSR_ADDR(dev0_base_remap_addr_mt6989));
	else if (dev_index == 1)
		is_empty = LSR_GET_TEMT(LSR_ADDR(dev1_base_remap_addr_mt6989));
	else if (dev_index == 2)
		is_empty = LSR_GET_TEMT(LSR_ADDR(dev2_base_remap_addr_mt6989));
	else if (dev_index == UARTHUB_MAX_NUM_DEV_HOST)
		is_empty = LSR_GET_TEMT(LSR_ADDR(cmm_base_remap_addr_mt6989));

	return is_empty;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_is_uartip_rx_buf_ready_for_read_mt6989(int dev_index)
{
	int is_ready = 0;

	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		is_ready = LSR_GET_DR(LSR_ADDR(dev0_base_remap_addr_mt6989));
	else if (dev_index == 1)
		is_ready = LSR_GET_DR(LSR_ADDR(dev1_base_remap_addr_mt6989));
	else if (dev_index == 2)
		is_ready = LSR_GET_DR(LSR_ADDR(dev2_base_remap_addr_mt6989));
	else if (dev_index == UARTHUB_MAX_NUM_DEV_HOST)
		is_ready = LSR_GET_DR(LSR_ADDR(cmm_base_remap_addr_mt6989));

	return is_ready;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_is_uartip_throw_xoff_mt6989(int dev_index)
{
	int is_xoff = 0;

	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		is_xoff = ((((UARTHUB_REG_READ(
			DEBUG_1(dev0_base_remap_addr_mt6989)) & 0xE0) >> 5) == 1) ? 0 : 1);
	else if (dev_index == 1)
		is_xoff = ((((UARTHUB_REG_READ(
			DEBUG_1(dev1_base_remap_addr_mt6989)) & 0xE0) >> 5) == 1) ? 0 : 1);
	else if (dev_index == 2)
		is_xoff = ((((UARTHUB_REG_READ(
			DEBUG_1(dev2_base_remap_addr_mt6989)) & 0xE0) >> 5) == 1) ? 0 : 1);
	else if (dev_index == UARTHUB_MAX_NUM_DEV_HOST)
		is_xoff = ((((UARTHUB_REG_READ(
			DEBUG_1(cmm_base_remap_addr_mt6989)) & 0xE0) >> 5) == 1) ? 0 : 1);

	return is_xoff;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_config_uartip_rx_fifo_trig_thr_mt6989(int dev_index, int size)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0) {
		pr_info("[%s] FCR_RD_ADDR_1=[0x%x]\n",
			__func__, UARTHUB_REG_READ(FCR_RD_ADDR(dev0_base_remap_addr_mt6989)));
		REG_FLD_RD_SET(FCR_FLD_RFTL1_RFTL0,
			FCR_RD_ADDR(dev0_base_remap_addr_mt6989),
			FCR_ADDR(dev0_base_remap_addr_mt6989), size);
		pr_info("[%s] FCR_RD_ADDR_2=[0x%x]\n",
			__func__, UARTHUB_REG_READ(FCR_RD_ADDR(dev0_base_remap_addr_mt6989)));
	} else if (dev_index == 1) {
		pr_info("[%s] FCR_RD_ADDR_1=[0x%x]\n",
			__func__, UARTHUB_REG_READ(FCR_RD_ADDR(dev0_base_remap_addr_mt6989)));
		REG_FLD_RD_SET(FCR_FLD_RFTL1_RFTL0,
			FCR_RD_ADDR(dev1_base_remap_addr_mt6989),
			FCR_ADDR(dev1_base_remap_addr_mt6989), size);
		pr_info("[%s] FCR_RD_ADDR_2=[0x%x]\n",
			__func__, UARTHUB_REG_READ(FCR_RD_ADDR(dev0_base_remap_addr_mt6989)));
	} else if (dev_index == 2) {
		pr_info("[%s] FCR_RD_ADDR_1=[0x%x]\n",
			__func__, UARTHUB_REG_READ(FCR_RD_ADDR(dev0_base_remap_addr_mt6989)));
		REG_FLD_RD_SET(FCR_FLD_RFTL1_RFTL0,
			FCR_RD_ADDR(dev2_base_remap_addr_mt6989),
			FCR_ADDR(dev2_base_remap_addr_mt6989), size);
		pr_info("[%s] FCR_RD_ADDR_2=[0x%x]\n",
			__func__, UARTHUB_REG_READ(FCR_RD_ADDR(dev0_base_remap_addr_mt6989)));
	} else if (dev_index == UARTHUB_MAX_NUM_DEV_HOST) {
		pr_info("[%s] FCR_RD_ADDR_1=[0x%x]\n",
			__func__, UARTHUB_REG_READ(FCR_RD_ADDR(dev0_base_remap_addr_mt6989)));
		REG_FLD_RD_SET(FCR_FLD_RFTL1_RFTL0,
			FCR_RD_ADDR(cmm_base_remap_addr_mt6989),
			FCR_ADDR(cmm_base_remap_addr_mt6989), size);
		pr_info("[%s] FCR_RD_ADDR_2=[0x%x]\n",
			__func__, UARTHUB_REG_READ(FCR_RD_ADDR(dev0_base_remap_addr_mt6989)));
	}

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_uartip_write_tx_data_mt6989(int dev_index, unsigned char *p_tx_data, int tx_len)
{
	int retry = 0;
	int i = 0;

	if (!p_tx_data || tx_len == 0)
		return UARTHUB_ERR_PARA_WRONG;

	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	for (i = 0; i < tx_len; i++) {
		retry = 200;
		while (retry-- > 0) {
			if (uarthub_is_uartip_tx_buf_empty_for_write_mt6989(dev_index) == 1) {
				uarthub_uartip_write_data_to_tx_buf_mt6989(dev_index, p_tx_data[i]);
				break;
			}
			usleep_range(5, 6);
		}

		if (retry <= 0)
			return UARTHUB_UT_ERR_TX_FAIL;
	}

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_uartip_read_rx_data_mt6989(
	int dev_index, unsigned char *p_rx_data, int rx_len, int *p_recv_rx_len)
{
	int retry = 0;
	int i = 0;
	int state = 0;

	if (!p_rx_data || !p_recv_rx_len || rx_len <= 0)
		return UARTHUB_ERR_PARA_WRONG;

	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	for (i = 0; i < rx_len; i++) {
		retry = 200;
		while (retry-- > 0) {
			if (uarthub_is_uartip_rx_buf_ready_for_read_mt6989(dev_index) == 1) {
				p_rx_data[i] =
					uarthub_uartip_read_data_from_rx_buf_mt6989(dev_index);
				*p_recv_rx_len = i+1;
				break;
			}
			usleep_range(5, 6);
		}

		if (retry <= 0) {
			state = 0;
			if (dev_index == 3)
				state = uarthub_get_cmm_rx_fifo_size_mt6989();
			else
				state = uarthub_get_host_rx_fifo_size_mt6989(dev_index);
			if (state > 0)
				retry = 200;
			else
				break;
		}
	}

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_is_apuart_tx_buf_empty_for_write_mt6989(int port_no)
{
	int is_empty = 0;

	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	if (port_no == 1)
		is_empty = LSR_GET_TEMT(LSR_ADDR(apuart1_base_remap_addr_mt6989));
	else if (port_no == 2)
		is_empty = LSR_GET_TEMT(LSR_ADDR(apuart2_base_remap_addr_mt6989));
	else if (port_no == 3)
		is_empty = LSR_GET_TEMT(LSR_ADDR(apuart3_base_remap_addr_mt6989));

	return is_empty;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_is_apuart_rx_buf_ready_for_read_mt6989(int port_no)
{
	int is_ready = 0;

	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	if (port_no == 1)
		is_ready = LSR_GET_DR(LSR_ADDR(apuart1_base_remap_addr_mt6989));
	else if (port_no == 2)
		is_ready = LSR_GET_DR(LSR_ADDR(apuart2_base_remap_addr_mt6989));
	else if (port_no == 3)
		is_ready = LSR_GET_DR(LSR_ADDR(apuart3_base_remap_addr_mt6989));

	return is_ready;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_apuart_write_data_to_tx_buf_mt6989(int port_no, int tx_data)
{
	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	if (port_no == 1)
		UARTHUB_REG_WRITE(THR_ADDR(apuart1_base_remap_addr_mt6989), tx_data);
	else if (port_no == 2)
		UARTHUB_REG_WRITE(THR_ADDR(apuart2_base_remap_addr_mt6989), tx_data);
	else if (port_no == 3)
		UARTHUB_REG_WRITE(THR_ADDR(apuart3_base_remap_addr_mt6989), tx_data);

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_apuart_read_data_from_rx_buf_mt6989(int port_no)
{
	int rx_data = 0;

	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	if (port_no == 1)
		rx_data = UARTHUB_REG_READ(RBR_ADDR(apuart1_base_remap_addr_mt6989));
	else if (port_no == 2)
		rx_data = UARTHUB_REG_READ(RBR_ADDR(apuart2_base_remap_addr_mt6989));
	else if (port_no == 3)
		rx_data = UARTHUB_REG_READ(RBR_ADDR(apuart3_base_remap_addr_mt6989));

	return rx_data;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_apuart_write_tx_data_mt6989(int port_no, unsigned char *p_tx_data, int tx_len)
{
	int retry = 0;
	int i = 0;

	if (!p_tx_data || tx_len == 0)
		return UARTHUB_ERR_PARA_WRONG;

	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	for (i = 0; i < tx_len; i++) {
		retry = 200;
		while (retry-- > 0) {
			if (uarthub_is_apuart_tx_buf_empty_for_write_mt6989(port_no) == 1) {
				uarthub_apuart_write_data_to_tx_buf_mt6989(port_no, p_tx_data[i]);
				break;
			}
			usleep_range(5, 6);
		}

		if (retry <= 0)
			return UARTHUB_UT_ERR_TX_FAIL;
	}

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_apuart_read_rx_data_mt6989(
	int port_no, unsigned char *p_rx_data, int rx_len, int *p_recv_rx_len)
{
	int retry = 0;
	int i = 0;
	int state = 0;

	if (!p_rx_data || !p_recv_rx_len || rx_len <= 0)
		return UARTHUB_ERR_PARA_WRONG;

	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	for (i = 0; i < rx_len; i++) {
		retry = 200;
		while (retry-- > 0) {
			if (uarthub_is_apuart_rx_buf_ready_for_read_mt6989(port_no) == 1) {
				p_rx_data[i] =
					uarthub_apuart_read_data_from_rx_buf_mt6989(port_no);
				*p_recv_rx_len = i+1;
				break;
			}
			usleep_range(5, 6);
		}

		if (retry <= 0) {
			state = 0;
			if (port_no == 1)
				state = UARTHUB_REG_READ_BIT(
					DEBUG_7(apuart1_base_remap_addr_mt6989), 0x3F);
			else if (port_no == 2)
				state = UARTHUB_REG_READ_BIT(
					DEBUG_7(apuart2_base_remap_addr_mt6989), 0x3F);
			else if (port_no == 3)
				state = UARTHUB_REG_READ_BIT(
					DEBUG_7(apuart3_base_remap_addr_mt6989), 0x3F);
			if (state > 0)
				retry = 200;
			else
				break;
		}
	}

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */

#if UARTHUB_SUPPORT_UT_API
int uarthub_init_default_apuart_config_mt6989(void)
{
	void __iomem *uarthub_dev_base = NULL;
	int baud_rate = 0;
	int i = 0;

	if (!apuart1_base_remap_addr_mt6989 || !apuart2_base_remap_addr_mt6989 ||
			!apuart3_base_remap_addr_mt6989) {
		pr_notice("[%s] apuart1/2/3_base_remap_addr_mt6989 is not all init\n",
			__func__);
		return -1;
	}

	uarthub_usb_rx_pin_ctrl_mt6989(apuart1_base_remap_addr_mt6989, 1);
	uarthub_usb_rx_pin_ctrl_mt6989(apuart2_base_remap_addr_mt6989, 1);
	uarthub_usb_rx_pin_ctrl_mt6989(apuart3_base_remap_addr_mt6989, 1);
	baud_rate = 115200;

	for (i = 0; i < 3; i++) {
		if (i == 0)
			uarthub_dev_base = apuart1_base_remap_addr_mt6989;
		else if (i == 1)
			uarthub_dev_base = apuart2_base_remap_addr_mt6989;
		else if (i == 2)
			uarthub_dev_base = apuart3_base_remap_addr_mt6989;

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

	uarthub_usb_rx_pin_ctrl_mt6989(apuart1_base_remap_addr_mt6989, 0);
	uarthub_usb_rx_pin_ctrl_mt6989(apuart2_base_remap_addr_mt6989, 0);
	uarthub_usb_rx_pin_ctrl_mt6989(apuart3_base_remap_addr_mt6989, 0);

	usleep_range(2000, 2010);

	/* 0x4c = 0x3,  rx/tx channel dma enable */
	UARTHUB_REG_WRITE(DMA_EN_ADDR(apuart1_base_remap_addr_mt6989), 0x3);
	UARTHUB_REG_WRITE(DMA_EN_ADDR(apuart2_base_remap_addr_mt6989), 0x3);
	UARTHUB_REG_WRITE(DMA_EN_ADDR(apuart3_base_remap_addr_mt6989), 0x3);

	/* 0x08 = 0x87, fifo control register */
	UARTHUB_REG_WRITE(FCR_ADDR(apuart1_base_remap_addr_mt6989), 0x87);
	UARTHUB_REG_WRITE(FCR_ADDR(apuart2_base_remap_addr_mt6989), 0x87);
	UARTHUB_REG_WRITE(FCR_ADDR(apuart3_base_remap_addr_mt6989), 0x87);

	return 0;
}
#endif /* UARTHUB_SUPPORT_UT_API */
