/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Author: Shu-wei Hsu <Shu-wei Hsu@mediatek.com>
 */

#ifndef MTK_AFE_CM_H_
#define MTK_AFE_CM_H_
enum {
	CM0,
	CM1,
	CM_NUM,
};

void mt6878_set_cm_rate(int id, unsigned int rate);

int mt6878_set_cm(struct mtk_base_afe *afe, int id, unsigned int update,
				bool swap, unsigned int ch);
int mt6878_enable_cm_bypass(struct mtk_base_afe *afe, int id, bool en);
int mt6878_cm_output_mux(struct mtk_base_afe *afe, int id, bool sel);
int mt6878_enable_cm(struct mtk_base_afe *afe, int id, bool en);
int mt6878_is_need_enable_cm(struct mtk_base_afe *afe, int id);

#endif /* MTK_AFE_CM_H_ */

