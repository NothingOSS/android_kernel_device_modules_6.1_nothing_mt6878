/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef MT6989_H
#define MT6989_H

#define UARTHUB_SUPPORT_DX4_FPGA      1
#define UARTHUB_SUPPORT_FPGA          0
#define UARTHUB_SUPPORT_DVT           0
#define UARTHUB_SUPPORT_UT_API        1
#define UARTHUB_SUPPORT_UT_CASE       1

#if UARTHUB_SUPPORT_DX4_FPGA
#ifdef UARTHUB_SUPPORT_FPGA
#undef UARTHUB_SUPPORT_FPGA
#endif
#define UARTHUB_SUPPORT_FPGA          1
#endif

#if (UARTHUB_SUPPORT_FPGA) || (UARTHUB_SUPPORT_DVT)
#ifdef UARTHUB_SUPPORT_UT_CASE
#undef UARTHUB_SUPPORT_UT_CASE
#endif
#define UARTHUB_SUPPORT_UT_CASE       1
#endif

#if UARTHUB_SUPPORT_UT_CASE
#ifdef UARTHUB_SUPPORT_UT_API
#undef UARTHUB_SUPPORT_UT_API
#endif
#define UARTHUB_SUPPORT_UT_API        1
#endif

#define UARTHUB_SUPPORT_SSPM_DRIVER   0
#define UARTHUB_CONFIG_TRX_GPIO       0
#define UARTHUB_SUPPORT_UNIVPLL_CTRL  0
#define UARTHUB_ENABLE_MD_CHANNEL     1

#if UARTHUB_SUPPORT_DX4_FPGA
#include "dx4_fpga/INTFHUB_c_header.h"
#include "dx4_fpga/UARTHUB_UART0_c_header.h"
#else
#include "INTFHUB_c_header.h"
#include "UARTHUB_UART0_c_header.h"
#endif

#include "platform_def_id.h"

extern void __iomem *gpio_base_remap_addr_mt6989;
extern void __iomem *pericfg_ao_remap_addr_mt6989;
extern void __iomem *topckgen_base_remap_addr_mt6989;
extern void __iomem *dev0_base_remap_addr_mt6989;
extern void __iomem *dev1_base_remap_addr_mt6989;
extern void __iomem *dev2_base_remap_addr_mt6989;
extern void __iomem *cmm_base_remap_addr_mt6989;
extern void __iomem *apuart1_base_remap_addr_mt6989;
extern void __iomem *apuart2_base_remap_addr_mt6989;
extern void __iomem *apuart3_base_remap_addr_mt6989;
extern void __iomem *apdma_uart_tx_int_remap_addr_mt6989;
extern void __iomem *spm_remap_addr_mt6989;
extern void __iomem *apmixedsys_remap_addr_mt6989;
extern void __iomem *iocfg_rm_remap_addr_mt6989;
extern void __iomem *sys_sram_remap_addr_mt6989;


extern struct uarthub_core_ops_struct mt6989_plat_core_data;
extern struct uarthub_debug_ops_struct mt6989_plat_debug_data;
extern struct uarthub_ut_test_ops_struct mt6989_plat_ut_test_data;

#define UARTHUB_CMM_BASE_ADDR      0x11005000
#define UARTHUB_DEV_0_BASE_ADDR    0x11005100
#define UARTHUB_DEV_1_BASE_ADDR    0x11005200
#define UARTHUB_DEV_2_BASE_ADDR    0x11005300
#define UARTHUB_INTFHUB_BASE_ADDR  0x11005400

#define UARTHUB_MAX_NUM_DEV_HOST   3

#if UARTHUB_SUPPORT_FPGA
#define UARTHUB_DEV_0_BAUD_RATE    115200
#define UARTHUB_DEV_1_BAUD_RATE    115200
#define UARTHUB_DEV_2_BAUD_RATE    115200
#define UARTHUB_CMM_BAUD_RATE      115200
#else
#define UARTHUB_DEV_0_BAUD_RATE    12000000
#define UARTHUB_DEV_1_BAUD_RATE    4000000
#define UARTHUB_DEV_2_BAUD_RATE    12000000
#define UARTHUB_CMM_BAUD_RATE      12000000
#endif

#define TRX_BUF_LEN                32

int uarthub_uarthub_init_mt6989(struct platform_device *pdev);
int uarthub_is_apb_bus_clk_enable_mt6989(void);
int uarthub_get_hwccf_univpll_on_info_mt6989(void);
int uarthub_set_host_loopback_ctrl_mt6989(int dev_index, int tx_to_rx, int enable);
int uarthub_set_cmm_loopback_ctrl_mt6989(int tx_to_rx, int enable);
int uarthub_is_bypass_mode_mt6989(void);
int uarthub_set_host_trx_request_mt6989(int dev_index, enum uarthub_trx_type trx);
int uarthub_clear_host_trx_request_mt6989(int dev_index, enum uarthub_trx_type trx);
int uarthub_config_bypass_ctrl_mt6989(int enable);
int uarthub_config_baud_rate_m6989(void __iomem *dev_base, int rate_index);
int uarthub_usb_rx_pin_ctrl_mt6989(void __iomem *dev_base, int enable);

/* debug API */
int uarthub_dump_apuart_debug_ctrl_mt6989(int enable);
int uarthub_get_apuart_debug_ctrl_sta_mt6989(void);
int uarthub_get_intfhub_base_addr_mt6989(void);
int uarthub_get_uartip_base_addr_mt6989(int dev_index);
int uarthub_dump_uartip_debug_info_mt6989(
	const char *tag, struct mutex *uartip_lock, int force_dump);
int uarthub_dump_intfhub_debug_info_mt6989(const char *tag);
int uarthub_dump_debug_tx_rx_count_mt6989(const char *tag, int trigger_point);
int uarthub_dump_debug_clk_info_mt6989(const char *tag);
int uarthub_dump_debug_byte_cnt_info_mt6989(const char *tag);
int uarthub_dump_debug_apdma_uart_info_mt6989(const char *tag);
int uarthub_dump_sspm_log_mt6989(const char *tag);
int uarthub_trigger_fpga_testing_mt6989(int type);
int uarthub_trigger_dvt_testing_mt6989(int type);
int uarthub_verify_combo_connect_sta_mt6989(int type, int rx_delay_ms);

/* UT Test API */
#if UARTHUB_SUPPORT_UT_API
int uarthub_is_ut_testing_mt6989(void);
int uarthub_is_host_uarthub_ready_state_mt6989(int dev_index);
int uarthub_get_host_irq_sta_mt6989(int dev_index);
int uarthub_clear_host_irq_mt6989(int dev_index, int mask_bit);
int uarthub_mask_host_irq_mt6989(int dev_index, int mask_bit, int is_mask);
int uarthub_config_host_irq_ctrl_mt6989(int dev_index, int enable);
int uarthub_get_host_rx_fifo_size_mt6989(int dev_index);
int uarthub_get_cmm_rx_fifo_size_mt6989(void);
int uarthub_config_uartip_dma_en_ctrl_mt6989(int dev_index, enum uarthub_trx_type trx, int enable);
int uarthub_reset_fifo_trx_mt6989(void);
int uarthub_reset_intfhub_mt6989(void);
int uarthub_uartip_write_data_to_tx_buf_mt6989(int dev_index, int tx_data);
int uarthub_uartip_read_data_from_rx_buf_mt6989(int dev_index);
int uarthub_is_uartip_tx_buf_empty_for_write_mt6989(int dev_index);
int uarthub_is_uartip_rx_buf_ready_for_read_mt6989(int dev_index);
int uarthub_is_uartip_throw_xoff_mt6989(int dev_index);
int uarthub_config_uartip_rx_fifo_trig_thr_mt6989(int dev_index, int size);
int uarthub_uartip_write_tx_data_mt6989(int dev_index, unsigned char *p_tx_data, int tx_len);
int uarthub_uartip_read_rx_data_mt6989(
	int dev_index, unsigned char *p_rx_data, int rx_len, int *p_recv_rx_len);
int uarthub_is_apuart_tx_buf_empty_for_write_mt6989(int port_no);
int uarthub_is_apuart_rx_buf_ready_for_read_mt6989(int port_no);
int uarthub_apuart_write_data_to_tx_buf_mt6989(int port_no, int tx_data);
int uarthub_apuart_read_data_from_rx_buf_mt6989(int port_no);
int uarthub_apuart_write_tx_data_mt6989(int port_no, unsigned char *p_tx_data, int tx_len);
int uarthub_apuart_read_rx_data_mt6989(
	int port_no, unsigned char *p_rx_data, int rx_len, int *p_recv_rx_len);
int uarthub_init_default_apuart_config_mt6989(void);
#endif

/* UT Test API for DX4 project */
#if UARTHUB_SUPPORT_DX4_FPGA
int uarthub_request_host_sema_own_sta_mt6989(int dev_index);
int uarthub_set_host_sema_own_rel_mt6989(int dev_index);
int uarthub_get_host_sema_own_rel_irq_sta_mt6989(int dev_index);
int uarthub_clear_host_sema_own_rel_irq_mt6989(int dev_index);
int uarthub_reset_host_sema_own_mt6989(int dev_index);
int uarthub_get_host_sema_own_tmo_irq_sta_mt6989(int dev_index);
int uarthub_clear_host_sema_own_tmo_irq_mt6989(int dev_index);
int uarthub_reset_host_sema_own_tmo_mt6989(int dev_index);
int uarthub_config_inband_esc_char_mt6989(int esc_char);
int uarthub_config_inband_esc_sta_mt6989(int esc_sta);
int uarthub_config_inband_enable_ctrl_mt6989(int enable);
int uarthub_config_inband_irq_enable_ctrl_mt6989(int enable);
int uarthub_config_inband_trigger_mt6989(void);
int uarthub_is_inband_tx_complete_mt6989(void);
int uarthub_get_inband_irq_sta_mt6989(void);
int uarthub_clear_inband_irq_mt6989(void);
int uarthub_get_received_inband_sta_mt6989(void);
int uarthub_clear_received_inband_sta_mt6989(void);
int uarthub_ut_ip_verify_pkt_hdr_fmt_mt6989(void);
int uarthub_ut_ip_verify_trx_not_ready_mt6989(void);
int uarthub_sspm_irq_handle_mt6989(int sspm_irq);
#endif

/* UT Test API for IP level test */
#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_timeout_init_fsm_ctrl_mt6989(void);
int uarthub_ut_ip_clear_rx_data_irq_mt6989(void);
int uarthub_ut_ip_host_tx_packet_loopback_mt6989(void);
#endif

#if UARTHUB_SUPPORT_UT_API
int uarthub_verify_cmm_loopback_sta_mt6989(void);
int uarthub_verify_cmm_trx_combo_sta_mt6989(int rx_delay_ms);
#endif

#endif /* MT6989_H */
