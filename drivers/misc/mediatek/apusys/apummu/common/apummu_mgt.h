/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUMMU_TABLE_H__
#define __APUMMU_TABLE_H__

/* config define */
#define AMMU_DRAM2PAGE_ARRAY		(1)
#define DRAM_FALL_BACK_IN_RUNTIME	(1)

#define APUMMU_MAX_BUF_CNT		(16)
/* midware has max 64 SCs */
#define APUMMU_MAX_SC_CNT		(64)

#define APUMMU_MAX_TBL_ENTRY	(APUMMU_MAX_SC_CNT*APUMMU_MAX_BUF_CNT)

/* NOTE: DATA_BUF, CMD_BUF aligned MDW */
enum AMMU_BUF_TYPE {
	AMMU_DATA_BUF,	// (non 1-1 mapping)
	AMMU_CMD_BUF,	// (1-1 mapping)
	AMMU_VLM_BUF,	// (1-1 mapping)
};

struct apummu_adr {
	enum AMMU_BUF_TYPE type;
	uint32_t iova;	// the frist 22~ 24bits due to 4k alignment(shift 10)
	uint32_t eva;	// the encrypted iova
};

/* apummu iova-eva mapping table */
struct apummu_session_tbl {
	/* header */
	uint64_t session;			// the session
	uint32_t session_entry_cnt;	// max: APUMMU_MAX_TBL_ENTRY

	/*
	 * Issue: there's only 5 page arraies, so we cannot use 4 page arraies for 0-16G
	 * 1 segment for 4-16G with page array size is 512M (24 fields)
	 * 1~4 4~16G
	 */
#if AMMU_DRAM2PAGE_ARRAY
	uint32_t page_idx_mask[2];		// 1 for 0~4G, 1 for 4~16G
	uint8_t  page_mask_en_num[2];	// 0-1, for page_idx_mask[x] enable
	uint8_t  dram_idx_mask;			// 0-1, for page_idx_mask[x] enable
#else
	uint32_t page_idx_mask;		// Whole 16G use 1 mask -> 512M for a block
#endif

	/* payload */
	struct apummu_adr adr[APUMMU_MAX_TBL_ENTRY];

	struct list_head list;
};

int addr_encode_and_write_stable(enum AMMU_BUF_TYPE type, uint64_t session,
			uint64_t iova, uint32_t buf_size, uint32_t *eva);
int get_session_table(uint64_t session, void **tbl_kva, uint32_t *size);
int session_table_free(uint64_t session);
void dump_session_table_set(void);
void apummu_mgt_init(void);
void apummu_mgt_destroy(void);


#endif //Endof __APUMMU_TABLE_H__
