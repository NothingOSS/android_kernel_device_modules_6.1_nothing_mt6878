/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTKFB_DEBUG_H
#define __MTKFB_DEBUG_H

#define LOGGER_BUFFER_SIZE (16 * 1024)
#define ERROR_BUFFER_COUNT 4
#define FENCE_BUFFER_COUNT 20
#define DEBUG_BUFFER_COUNT 30
#define DUMP_BUFFER_COUNT 10
#define STATUS_BUFFER_COUNT 3
#define _DRM_P_H_
#if defined(CONFIG_MT_ENG_BUILD) || !defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
#define DEBUG_BUFFER_SIZE                                                      \
	(4096 +                                                                \
	 (ERROR_BUFFER_COUNT + FENCE_BUFFER_COUNT + DEBUG_BUFFER_COUNT +       \
	  DUMP_BUFFER_COUNT + STATUS_BUFFER_COUNT) *                           \
		 LOGGER_BUFFER_SIZE)
#else
#define DEBUG_BUFFER_SIZE 10240
#endif

extern int mtk_disp_hrt_bw_dbg(void);

struct cb_data_store {
	struct cmdq_cb_data data;
	struct list_head link;
};
#ifdef _DRM_P_H_
struct disp_rect {
	u32 x;
	u32 y;
	u32 width;
	u32 height;
};
void disp_dbg_probe(void);
void disp_dbg_init(struct drm_device *drm_dev);
void disp_dbg_deinit(void);
void mtk_wakeup_pf_wq(unsigned int m_id);
void mtk_drm_cwb_backup_copy_size(void);
int mtk_dprec_mmp_dump_ovl_layer(struct mtk_plane_state *plane_state);
int mtk_dprec_mmp_dump_wdma_layer(struct drm_crtc *crtc,
	struct drm_framebuffer *wb_fb);
int mtk_dprec_mmp_dump_cwb_buffer(struct drm_crtc *crtc,
	void *buffer, unsigned int buf_idx);
int disp_met_set(void *data, u64 val);
void mtk_drm_idlemgr_kick_ext(const char *source);
unsigned int mtk_dbg_get_lfr_mode_value(void);
unsigned int mtk_dbg_get_lfr_type_value(void);
unsigned int mtk_dbg_get_lfr_enable_value(void);
unsigned int mtk_dbg_get_lfr_update_value(void);
unsigned int mtk_dbg_get_lfr_vse_dis_value(void);
unsigned int mtk_dbg_get_lfr_skip_num_value(void);
unsigned int mtk_dbg_get_lfr_dbg_value(void);
int mtk_drm_add_cb_data(struct cb_data_store *cb_data, unsigned int crtc_id);
struct cb_data_store *mtk_drm_get_cb_data(unsigned int crtc_id);
void mtk_drm_del_cb_data(struct cmdq_cb_data data, unsigned int crtc_id);
int hrt_lp_switch_get(void);
void mtk_dprec_snapshot(void);

void mtkfb_set_force_partial_roi(int en);
bool mtkfb_is_force_partial_roi(void);
int mtkfb_force_partial_y_offset(void);
int mtkfb_force_partial_height(void);
#endif

enum mtk_drm_mml_dbg {
	DISP_MML_DBG_LOG = 0x0001,
	DISP_MML_MMCLK_UNLIMIT = 0x0002,
	DISP_MML_IR_CLEAR = 0x0004,
	MMP_ADDON_CONNECT = 0x1000,
	MMP_ADDON_DISCONNECT = 0x2000,
	MMP_MML_SUBMIT = 0x4000,
	MMP_MML_IDLE = 0x8000,
	MMP_MML_REPAINT = 0x10000,
};

#if IS_ENABLED(CONFIG_MTK_DISP_DEBUG)
struct reg_dbg {
	uint32_t addr;
	uint32_t val;
	uint32_t mask;
};

struct wr_online_dbg {
	struct reg_dbg reg[64];
	uint32_t index;
	uint32_t after_commit;
};

extern struct wr_online_dbg g_wr_reg;
#endif

enum GCE_COND_REVERSE_COND {
	R_CMDQ_NOT_EQUAL = CMDQ_EQUAL,
	R_CMDQ_EQUAL = CMDQ_NOT_EQUAL,
	R_CMDQ_LESS = CMDQ_GREATER_THAN_AND_EQUAL,
	R_CMDQ_GREATER = CMDQ_LESS_THAN_AND_EQUAL,
	R_CMDQ_LESS_EQUAL = CMDQ_GREATER_THAN,
	R_CMDQ_GREATER_EQUAL = CMDQ_LESS_THAN,
};

#define GCE_COND_DECLARE \
	u32 _inst_condi_jump, _inst_jump_end; \
	u64 _jump_pa; \
	u64 *_inst; \
	struct cmdq_pkt *_cond_pkt; \
	u16 _gpr, _reg_jump

#define GCE_COND_ASSIGN(pkt, addr, gpr) do { \
	_cond_pkt = pkt; \
	_reg_jump = addr; \
	_gpr = gpr; \
} while (0)

#define GCE_IF(lop, cond, rop) do { \
	_inst_condi_jump = _cond_pkt->cmd_buf_size; \
	cmdq_pkt_assign_command(_cond_pkt, _reg_jump, 0); \
	cmdq_pkt_cond_jump_abs(_cond_pkt, _reg_jump, &lop, &rop, (enum CMDQ_CONDITION_ENUM) cond); \
	_inst_jump_end = _inst_condi_jump; \
} while (0)

#define GCE_ELSE do { \
	_inst_jump_end = _cond_pkt->cmd_buf_size; \
	cmdq_pkt_jump_addr(_cond_pkt, 0); \
	_inst = cmdq_pkt_get_va_by_offset(_cond_pkt, _inst_condi_jump); \
	_jump_pa = cmdq_pkt_get_pa_by_offset(_cond_pkt, _cond_pkt->cmd_buf_size); \
	*_inst = *_inst | CMDQ_REG_SHIFT_ADDR(_jump_pa); \
} while (0)

#define GCE_FI do { \
	_inst = cmdq_pkt_get_va_by_offset(_cond_pkt, _inst_jump_end); \
	_jump_pa = cmdq_pkt_get_pa_by_offset(_cond_pkt, _cond_pkt->cmd_buf_size); \
	*_inst = *_inst | CMDQ_REG_SHIFT_ADDR(_jump_pa); \
} while (0)

#define GCE_DO(act, name) cmdq_pkt_##act(_cond_pkt, mtk_crtc->gce_obj.event[name])

#define GCE_SLEEP(us) cmdq_pkt_sleep(_cond_pkt, us, _gpr)


#endif
