/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Mediatek Inc.
 */

#ifndef MTK_UART_APDMA_H
#define MTK_UART_APDMA_H

#define KERNEL_mtk_save_uart_apdma_reg    mtk_save_uart_apdma_reg
#define KERNEL_mtk_uart_apdma_data_dump   mtk_uart_apdma_data_dump
#define KERNEL_mtk_uart_rx_setting   mtk_uart_rx_setting
#define KERNEL_mtk_uart_apdma_start_record  mtk_uart_apdma_start_record
#define KERNEL_mtk_uart_apdma_end_record	mtk_uart_apdma_end_record
#define KERNEL_mtk_uart_get_apdma_rpt	mtk_uart_get_apdma_rpt
#define KERNEL_mtk_uart_set_res_status	mtk_uart_set_res_status
#define KERNEL_mtk_uart_get_res_status	mtk_uart_get_res_status
#define KERNEL_mtk_uart_apdma_polling_rx_finish mtk_uart_apdma_polling_rx_finish
#define KERNEL_mtk_uart_set_apdma_clk  mtk_uart_set_apdma_clk
#define KERNEL_mtk_uart_set_apdma_rx_irq  mtk_uart_set_apdma_rx_irq
#define KERNEL_mtk_uart_get_apdma_rx_state  mtk_uart_get_apdma_rx_state
#define KERNEL_mtk_uart_set_apdma_rx_state  mtk_uart_set_apdma_rx_state
#define KERNEL_mtk_uart_set_apdma_idle  mtk_uart_set_apdma_idle

void mtk_save_uart_apdma_reg(struct dma_chan *chan, unsigned int *reg_buf);
void mtk_uart_apdma_data_dump(struct dma_chan *chan);
void mtk_uart_rx_setting(struct dma_chan *chan, int copied, int total);
void mtk_uart_apdma_start_record(struct dma_chan *chan);
void mtk_uart_apdma_end_record(struct dma_chan *chan);
void mtk_uart_get_apdma_rpt(struct dma_chan *chan, unsigned int *rpt);
void mtk_uart_set_res_status(unsigned int status);
unsigned int mtk_uart_get_res_status(void);
void mtk_uart_apdma_polling_rx_finish(void);
void mtk_uart_set_apdma_clk (bool enable);
void mtk_uart_set_apdma_rx_irq (bool enable);
int mtk_uart_get_apdma_rx_state (void);
void mtk_uart_set_apdma_rx_state (int value);
void mtk_uart_set_apdma_idle (int value);

#endif /* MTK_UART_APDMA_H */
