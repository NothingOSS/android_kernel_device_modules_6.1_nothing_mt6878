/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __LPM_DBG_TRACE_EVENT_H__
#define __LPM_DBG_TRACE_EVENT_H__

enum subsys_req_index {
        SUBSYS_REQ_MD = 0,
        SUBSYS_REQ_CONN,
        SUBSYS_REQ_SCP,
        SUBSYS_REQ_ADSP,
        SUBSYS_REQ_UFS,
        SUBSYS_REQ_MSDC,
        SUBSYS_REQ_DISP,
        SUBSYS_REQ_APU,
        SUBSYS_REQ_SPM,
        SUBSYS_REQ_MAX,
};

struct subsys_req {
        u32 req_addr1;
        u32 req_mask1;
        u32 req_addr2;
        u32 req_mask2;
};

int lpm_trace_event_init(struct subsys_req *lpm_subsys_req);
void lpm_trace_event_deinit(void);

#endif /* __LPM_DBG_TRACE_EVENT_H__ */
