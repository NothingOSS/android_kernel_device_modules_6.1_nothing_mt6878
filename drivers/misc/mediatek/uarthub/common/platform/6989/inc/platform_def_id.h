/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef PLATFORM_DEF_ID_H
#define PLATFORM_DEF_ID_H

#define GPIO_BASE_ADDR                   0x10005000
#define GPIO_HUB_MODE_TX                 0x410
#define GPIO_HUB_MODE_TX_MASK            0x7000
#define GPIO_HUB_MODE_TX_VALUE           0x1000
#define GPIO_HUB_MODE_RX                 0x410
#define GPIO_HUB_MODE_RX_MASK            0x70000
#define GPIO_HUB_MODE_RX_VALUE           0x10000
#define GPIO_HUB_DIR_TX                  0x40
#define GPIO_HUB_DIR_TX_MASK             (0x1 << 11)
#define GPIO_HUB_DIR_TX_SHIFT            11
#define GPIO_HUB_DIR_RX                  0x40
#define GPIO_HUB_DIR_RX_MASK             (0x1 << 12)
#define GPIO_HUB_DIR_RX_SHIFT            12
#define GPIO_HUB_DIN_RX                  0x240
#define GPIO_HUB_DIN_RX_MASK             (0x1 << 12)
#define GPIO_HUB_DIN_RX_SHIFT            12

#define IOCFG_RM_BASE_ADDR               0x11C30000
#define GPIO_HUB_IES_TX                  0x50
#define GPIO_HUB_IES_TX_MASK             (0x1 << 5)
#define GPIO_HUB_IES_TX_SHIFT            5
#define GPIO_HUB_IES_RX                  0x50
#define GPIO_HUB_IES_RX_MASK             (0x1 << 4)
#define GPIO_HUB_IES_RX_SHIFT            4
#define GPIO_HUB_PU_TX                   0x90
#define GPIO_HUB_PU_TX_MASK              (0x1 << 5)
#define GPIO_HUB_PU_TX_SHIFT             5
#define GPIO_HUB_PU_RX                   0x90
#define GPIO_HUB_PU_RX_MASK              (0x1 << 4)
#define GPIO_HUB_PU_RX_SHIFT             4
#define GPIO_HUB_PD_TX                   0x80
#define GPIO_HUB_PD_TX_MASK              (0x1 << 5)
#define GPIO_HUB_PD_TX_SHIFT             5
#define GPIO_HUB_PD_RX                   0x80
#define GPIO_HUB_PD_RX_MASK              (0x1 << 4)
#define GPIO_HUB_PD_RX_SHIFT             4
#define GPIO_HUB_DRV_TX                  0x0
#define GPIO_HUB_DRV_TX_MASK             (0x7 << 15)
#define GPIO_HUB_DRV_TX_SHIFT            15
#define GPIO_HUB_DRV_RX                  0x0
#define GPIO_HUB_DRV_RX_MASK             (0x7 << 15)
#define GPIO_HUB_DRV_RX_SHIFT            15
#define GPIO_HUB_SMT_TX                  0xB0
#define GPIO_HUB_SMT_TX_MASK             (0x1 << 5)
#define GPIO_HUB_SMT_TX_SHIFT            5
#define GPIO_HUB_SMT_RX                  0xB0
#define GPIO_HUB_SMT_RX_MASK             (0x1 << 5)
#define GPIO_HUB_SMT_RX_SHIFT            5
#define GPIO_HUB_TDSEL_TX                0xC0
#define GPIO_HUB_TDSEL_TX_MASK           (0xF << 12)
#define GPIO_HUB_TDSEL_TX_SHIFT          12
#define GPIO_HUB_TDSEL_RX                0xC0
#define GPIO_HUB_TDSEL_RX_MASK           (0xF << 12)
#define GPIO_HUB_TDSEL_RX_SHIFT          12
#define GPIO_HUB_RDSEL_TX                0xA0
#define GPIO_HUB_RDSEL_TX_MASK           (0x3 << 6)
#define GPIO_HUB_RDSEL_TX_SHIFT          6
#define GPIO_HUB_RDSEL_RX                0xA0
#define GPIO_HUB_RDSEL_RX_MASK           (0x3 << 6)
#define GPIO_HUB_RDSEL_RX_SHIFT          6
#define GPIO_HUB_SEC_EN_TX               0xA00
#define GPIO_HUB_SEC_EN_TX_MASK          (0x1 << 3)
#define GPIO_HUB_SEC_EN_TX_SHIFT         3
#define GPIO_HUB_SEC_EN_RX               0xA00
#define GPIO_HUB_SEC_EN_RX_MASK          (0x1 << 4)
#define GPIO_HUB_SEC_EN_RX_SHIFT         4

#define PERICFG_AO_BASE_ADDR             0x11036000
#define PERI_CG_1                        0x14
#define PERI_CG_1_UARTHUB_CG_MASK        (0x7 << 23)
#define PERI_CG_1_UARTHUB_CG_SHIFT       23
#define PERI_CG_1_UARTHUB_CLK_CG_MASK    (0x3 << 24)
#define PERI_CG_1_UARTHUB_CLK_CG_SHIFT   24
#define PERI_CLOCK_CON                   0x20
#define PERI_UART_FBCLK_CKSEL            0xF00
#define PERI_UART_WAKEUP                 0x50
#define PERI_UART_WAKEUP_MASK            0x10
#define PERI_UART_WAKEUP_SHIFT           4

#define APMIXEDSYS_BASE_ADDR             0x1000C000
#define UNIVPLL_CON0                     0x308
#define UNIVPLL_CON0_UNIVPLL_EN_MASK     (0x1 << 0)
#define UNIVPLL_CON0_UNIVPLL_EN_SHIFT    0

#define TOPCKGEN_BASE_ADDR               0x10000000
#define CLK_CFG_6                        0x70
#define CLK_CFG_6_MASK                   0x3
#define CLK_CFG_6_SHIFT                  0x0

#define SPM_BASE_ADDR                    0x1C001000
#define MD32PCM_SCU_CTRL1                0x104
#define MD32PCM_SCU_CTRL1_MASK           (0x17 << 17)
#define MD32PCM_SCU_CTRL1_SHIFT          17

#define SPM_SYS_TIMER_L                  0x504
#define SPM_SYS_TIMER_H                  0x508

#define SPM_REQ_STA_9                    0x86C
#define SPM_REQ_STA_9_UARTHUB_REQ_MASK   (0x1F << 17)
#define SPM_REQ_STA_9_UARTHUB_REQ_SHIFT  17

#define UART1_BASE_ADDR                  0x11002000
#define UART2_BASE_ADDR                  0x11003000
#define UART3_BASE_ADDR                  0x11004000
#define AP_DMA_UART_TX_INT_FLAG_ADDR     0x11300F80

#define SYS_SRAM_BASE_ADDR               0x00113C00

#endif /* PLATFORM_DEF_ID_H */
