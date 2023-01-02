/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#ifndef __CLKBUF_UTIL_H
#define __CLKBUF_UTIL_H

#define CLKBUF_DBG(fmt, args...)                                               \
	pr_notice("[CLKBUF], <%s(), %d> " fmt, __func__, __LINE__, ##args)
#define CLKBUF_ERR(fmt, args...)                                               \
	pr_info("[CLKBUG], <%s(), %d> " fmt, __func__, __LINE__, ##args)

struct reg_t {
	char *name;
	u32 ofs;
	u32 mask;
	u32 shift;
};

#define SET_REG(reg, offset, msk, bit)                                         \
	._##reg = {                                                            \
		.name = #reg,                                                  \
		.ofs = offset,                                                 \
		.mask = msk,                                                   \
		.shift = bit,                                                  \
	},

#define SET_REG_BY_NAME(reg, name)                                             \
	SET_REG(reg, name##_ADDR, name##_MASK, name##_SHIFT)

#define DBG_REG(reg, offset, msk, bit)                                         \
	{                                                                      \
		.name = #reg,                                                  \
		.ofs = offset,                                                 \
		.mask = msk,                                                   \
		.shift = bit,                                                  \
	},

#define SET_DBG_REG(reg, name) DBG_REG(reg, name##_ADDR, 0xff, 0x0)

enum clkbuf_err_code {
	EREG_NOT_SUPPORT = 1000,
	EHW_WRONG_TYPE,
	EHW_NOT_SUPPORT,
	EHW_INIT_FAIL,
	EFIND_DTS_ERR,
	EGET_BASE_FAILED,
	ECHIP_NOT_FOUND,
	ENO_PMIC_REGMAP_FOUND,
	EXO_NUM_CONFIG_ERR,
	ERC_INIT_TIMEOUT,
	EXO_NOT_SW_CTRL,
	EXO_NOT_FOUND,
};

#endif
