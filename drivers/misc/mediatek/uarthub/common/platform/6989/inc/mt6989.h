/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef MT6989_H
#define MT6989_H

#define UARTHUB_SUPPORT_FPGA           0
#define UARTHUB_SUPPORT_DVT            0
#define UARTHUB_SUPPORT_UT_API         0
#define UARTHUB_SUPPORT_UT_CASE        0
#define SPM_RES_CHK_EN                 1

#if (UARTHUB_SUPPORT_FPGA) || (UARTHUB_SUPPORT_DVT)
#ifdef UARTHUB_SUPPORT_UT_CASE
#undef UARTHUB_SUPPORT_UT_CASE
#endif
#define UARTHUB_SUPPORT_UT_CASE        1
#endif

#if UARTHUB_SUPPORT_UT_CASE
#ifdef UARTHUB_SUPPORT_UT_API
#undef UARTHUB_SUPPORT_UT_API
#endif
#define UARTHUB_SUPPORT_UT_API         1
#endif

#define SSPM_DRIVER_EN                 1
#define SSPM_DRIVER_PLL_CLK_CTRL_EN    1
#define UNIVPLL_CTRL_EN                1
#define MD_CHANNEL_EN                  1

#if !(UNIVPLL_CTRL_EN)
#ifdef SSPM_DRIVER_PLL_CLK_CTRL_EN
#undef SSPM_DRIVER_PLL_CLK_CTRL_EN
#endif
#define SSPM_DRIVER_PLL_CLK_CTRL_EN    1
#endif

#include "INTFHUB_c_header.h"
#include "UARTHUB_UART0_c_header.h"

#include "common_def_id.h"
#include "platform_def_id.h"

extern void __iomem *gpio_base_remap_addr_mt6989;
extern void __iomem *pericfg_ao_remap_addr_mt6989;
extern void __iomem *topckgen_base_remap_addr_mt6989;
extern void __iomem *apdma_uart_tx_int_remap_addr_mt6989;
extern void __iomem *spm_remap_addr_mt6989;
extern void __iomem *apmixedsys_remap_addr_mt6989;
extern void __iomem *iocfg_rm_remap_addr_mt6989;
extern void __iomem *sys_sram_remap_addr_mt6989;

enum uarthub_uartip_id {
	uartip_id_ap = 0,
	uartip_id_md,
	uartip_id_adsp,
	uartip_id_cmm,
};

enum uarthub_clk_opp {
	uarthub_clk_topckgen = 0,
	uarthub_clk_26m,
	uarthub_clk_52m,
	uarthub_clk_104m,
	uarthub_clk_208m,
};

#define UARTHUB_MAX_NUM_DEV_HOST   3

extern void __iomem *uartip_base_map[UARTHUB_MAX_NUM_DEV_HOST + 1];
extern void __iomem *apuart_base_map[4];

extern struct uarthub_core_ops_struct mt6989_plat_core_data;
extern struct uarthub_debug_ops_struct mt6989_plat_debug_data;
extern struct uarthub_ut_test_ops_struct mt6989_plat_ut_test_data;

#define UARTHUB_CMM_BASE_ADDR      0x11005000
#define UARTHUB_DEV_0_BASE_ADDR    0x11005100
#define UARTHUB_DEV_1_BASE_ADDR    0x11005200
#define UARTHUB_DEV_2_BASE_ADDR    0x11005300
#define UARTHUB_INTFHUB_BASE_ADDR  0x11005400

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

#define TRX_BUF_LEN                64

int uarthub_uarthub_init_mt6989(struct platform_device *pdev);
int uarthub_uarthub_exit_mt6989(void);
int uarthub_uarthub_open_mt6989(void);
int uarthub_uarthub_close_mt6989(void);
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
#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_spm_res_info_mt6989(
	int *p_spm_res_uarthub, int *p_spm_res_internal, int *p_spm_res_26m_off);
int uarthub_get_uarthub_cg_info_mt6989(int *p_topckgen_cg, int *p_peri_cg);
int uarthub_get_uart_src_clk_info_mt6989(void);
#endif
int uarthub_get_uart_mux_info_mt6989(void);
int uarthub_get_uarthub_mux_info_mt6989(void);

/* debug API */
int uarthub_dump_apuart_debug_ctrl_mt6989(int enable);
int uarthub_get_apuart_debug_ctrl_sta_mt6989(void);
int uarthub_get_intfhub_base_addr_mt6989(void);
int uarthub_get_uartip_base_addr_mt6989(int dev_index);
int uarthub_dump_uartip_debug_info_mt6989(
	const char *tag, struct mutex *uartip_lock);
int uarthub_dump_intfhub_debug_info_mt6989(const char *tag);
int uarthub_dump_debug_monitor_mt6989(const char *tag);
int uarthub_debug_monitor_ctrl_mt6989(int enable, int mode, int ctrl);
int uarthub_debug_monitor_stop_mt6989(int stop);
int uarthub_debug_monitor_clr_mt6989(void);
int uarthub_dump_debug_tx_rx_count_mt6989(const char *tag, int trigger_point);
int uarthub_dump_debug_clk_info_mt6989(const char *tag);
int uarthub_dump_debug_byte_cnt_info_mt6989(const char *tag);
int uarthub_dump_debug_apdma_uart_info_mt6989(const char *tag);
int uarthub_dump_sspm_log_mt6989(const char *tag);
int uarthub_trigger_fpga_testing_mt6989(int type);
int uarthub_trigger_dvt_testing_mt6989(int type);
#if UARTHUB_SUPPORT_UT_API
int uarthub_verify_combo_connect_sta_mt6989(int type, int rx_delay_ms);
#endif

/* UT Test API */
int uarthub_is_ut_testing_mt6989(void);
#if UARTHUB_SUPPORT_UT_API
int uarthub_is_host_uarthub_ready_state_mt6989(int dev_index);
int uarthub_get_host_irq_sta_mt6989(int dev_index);
int uarthub_get_intfhub_active_sta_mt6989(void);
int uarthub_clear_host_irq_mt6989(int dev_index, int irq_type);
int uarthub_mask_host_irq_mt6989(int dev_index, int irq_type, int is_mask);
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
int uarthub_clear_all_ut_irq_sta_mt6989(void);
enum uarthub_pkt_fmt_type uarthub_check_packet_format(int dev_index, unsigned char byte1);
int uarthub_check_packet_is_complete(int dev_index, unsigned char *pData, int length);
int uarthub_get_crc(unsigned char *pData, int length, unsigned char *pData_CRC);
int uarthub_uartip_send_data_internal_mt6989(
	int dev_index, unsigned char *p_tx_data, int tx_len, int dump_trxlog);
#endif

/* UT Test API for IP level test */
#if UARTHUB_SUPPORT_UT_CASE
int uarthub_ut_ip_timeout_init_fsm_ctrl_mt6989(void);
int uarthub_ut_ip_clear_rx_data_irq_mt6989(void);
int uarthub_ut_ip_host_tx_packet_loopback_mt6989(void);
int uarthub_ut_ip_verify_debug_monitor_packet_info_mode_mt6989(void);
int uarthub_ut_ip_verify_debug_monitor_check_data_mode_mt6989(void);
int uarthub_ut_ip_verify_debug_monitor_crc_result_mode_mt6989(void);
#endif

#if UARTHUB_SUPPORT_UT_API
int uarthub_verify_cmm_loopback_sta_mt6989(void);
int uarthub_verify_cmm_trx_connsys_sta_mt6989(int rx_delay_ms);
#endif

#endif /* MT6989_H */
