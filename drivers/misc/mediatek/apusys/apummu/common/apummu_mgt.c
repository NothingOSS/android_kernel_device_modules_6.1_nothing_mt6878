// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/kref.h>

#include "apummu_mgt.h"
#include "apummu_mem.h"
#include "apummu_remote_cmd.h"
#include "apummu_cmn.h"

extern struct apummu_dev_info *g_adv;

struct apummu_tbl {
	struct list_head g_stable_head;
	struct kref session_tbl_cnt;
	struct mutex table_lock;
	bool is_stable_exist;
};

struct apummu_tbl g_ammu_table_set;
struct apummu_session_tbl *g_ammu_session_table_ptr;

#define IOVA2EVA_ENCODE_EN	(1)
#define PAGE_ARRAY_CNT_EN	(1)
#define SHIFT_BITS			(12)

#if IOVA2EVA_ENCODE_EN
#define ENCODE_OFFSET	(0x20000000)
#define IOVA2EVA(input_addr)	(input_addr - ENCODE_OFFSET) // -512M
#define EVA2IOVA(input_addr)	(input_addr + ENCODE_OFFSET) // +512M
#else
#define IOVA2EVA(input_addr)	(input_addr)
#define EVA2IOVA(input_addr)	(input_addr)
#endif

/**
 * @input:
 *  type -> buffer type
 *  input_addr -> addr to encode (IOVA)
 *  output_addr -> encoded address (EVA)
 * @output:
 *  if encode succeeded
 * @description:
 *  encode input addr according to type
 */
static int addr_encode(uint64_t input_addr, enum AMMU_BUF_TYPE type, uint32_t *output_addr)
{
	int ret = 0;
	uint32_t ret_addr;

	switch (type) {
	case AMMU_DATA_BUF:
		ret_addr = IOVA2EVA(input_addr);
		break;
	case AMMU_CMD_BUF:
	case AMMU_VLM_BUF:
		ret_addr = input_addr;
		break;
	default:
		AMMU_LOG_ERR("APUMMU invalid buffer type(%u)\n", type);
		ret = -EINVAL;
		goto out;
	}

	*output_addr = ret_addr;
out:
	return ret;
}

/**
 * @input:
 *  session -> for session check
 * @output:
 *  if the stable of input session is exist
 * @description:
 *  Check if session table of input session exist
 *  also bind exist stable to g_ammu_session_table_ptr
 */
static bool is_session_table_exist(uint64_t session)
{
	bool isExist = false;
	struct list_head *list_ptr;

	list_for_each(list_ptr, &g_ammu_table_set.g_stable_head) {
		g_ammu_session_table_ptr = list_entry(list_ptr, struct apummu_session_tbl, list);
		if (g_ammu_session_table_ptr->session == session) {
			isExist = true;
			break;
		}
	}

	return isExist;
}

/**
 * @input:
 *  None
 * @output:
 *  if stable alloc succeeded
 * @description:
 *  bind stable to g_ammu_session_table_ptr if unused table exist
 */
static int session_table_alloc(void)
{
	int ret = 0;
	struct apummu_session_tbl *sTable_ptr = NULL;

	sTable_ptr = kvmalloc(sizeof(struct apummu_session_tbl), GFP_KERNEL);
	if (!sTable_ptr) {
		AMMU_LOG_ERR("Session table alloc failed, kvmalloc failed\n");
		ret = -ENOMEM;
		goto out;
	}

	list_add_tail(&sTable_ptr->list, &g_ammu_table_set.g_stable_head);
	g_ammu_session_table_ptr = sTable_ptr;

	if (!g_ammu_table_set.is_stable_exist) {
		AMMU_LOG_DBG("kref init\n");
		kref_init(&g_ammu_table_set.session_tbl_cnt);
		g_ammu_table_set.is_stable_exist = true;
	#if DRAM_FALL_BACK_IN_RUNTIME
		apummu_dram_remap_runtime_alloc(g_adv);
		apummu_remote_set_hw_default_iova_one_shot(g_adv);
	#endif
	} else {
		AMMU_LOG_VERBO("kref get\n");
		kref_get(&g_ammu_table_set.session_tbl_cnt);
	}

out:
	return ret;
}

/* device_va == iova */
int addr_encode_and_write_stable(enum AMMU_BUF_TYPE type, uint64_t session, uint64_t device_va,
								uint32_t buf_size, uint32_t *eva)
{
	int ret = 0;
	uint32_t cur_stable_entry, ret_eva, cross_page_array_num = 0;

	if (device_va & 0xFFF) {
		AMMU_LOG_ERR("device_va is not 4K alignment!!!\n");
		ret = -EINVAL;
		goto out_before_lock;
	}

	/* addr encode and CHECK input type */
	ret = addr_encode(device_va, type, &ret_eva);
	if (ret)
		goto out_before_lock;

	AMMU_LOG_VERBO("session   = 0x%llx\n", session);
	AMMU_LOG_VERBO("device_va = 0x%llx\n", device_va);
	AMMU_LOG_VERBO("buf_size  = 0x%x\n", buf_size);
	AMMU_LOG_VERBO("ret_eva   = 0x%x\n", ret_eva);
	AMMU_LOG_VERBO("type      = %u\n", type);

	/* lock for g_ammu_session_table_ptr */
	mutex_lock(&g_ammu_table_set.table_lock);

	/* check if session table exist by session */
	if (!is_session_table_exist(session)) {
		/* if session table not exist alloc a session table */
		ret = session_table_alloc();
		if (ret)
			goto out_after_lock;

		g_ammu_session_table_ptr->session = session;
	}

	/* session table mapping count check */
	cur_stable_entry = g_ammu_session_table_ptr->session_entry_cnt;
	if (cur_stable_entry >= APUMMU_MAX_TBL_ENTRY) {
		AMMU_LOG_ERR("APUMMU session table entry reach maximum(%u >= %u)\n",
				cur_stable_entry, APUMMU_MAX_TBL_ENTRY);
		ret = -ENOMEM;
		goto out_after_lock;
	}

	/* Hint for RV APUMMU fill VSID table */
#if AMMU_DRAM2PAGE_ARRAY
	/* NOTE: cross_page_array_num is for secnario the given buffer cross differnet page array */
	if ((ret_eva >> SHIFT_BITS) & 0x300000) { // mask for 34-bit
		cross_page_array_num = (((device_va + buf_size) / (0x20000000))
							- ((device_va) / (0x20000000)));
		AMMU_LOG_DBG("DBG footprint 4~16G, cross_page_array_num = %u\n",
					cross_page_array_num);
		do {
		#if IOVA2EVA_ENCODE_EN
			/* >> 29 = 512M / 0x20000000 */
			g_ammu_session_table_ptr->page_idx_mask[1] |=
				(1 << (((device_va >> 29) + cross_page_array_num) & (0x1f)));
		#else
			g_ammu_session_table_ptr->page_idx_mask[1] |=
				(1 << ((device_va >> 29) + cross_page_array_num));
		#endif
			AMMU_LOG_VERBO("DBG footprint 4~16G cnt\n");
		} while (cross_page_array_num--);
		g_ammu_session_table_ptr->dram_idx_mask |= 2;
	} else {
		cross_page_array_num =
			(((device_va + buf_size) / (0x8000000)) - ((device_va) / (0x8000000)));
		AMMU_LOG_DBG("DBG footprint 1~5G, cross_page_array_num = %u\n",
				cross_page_array_num);
		do {
		#if IOVA2EVA_ENCODE_EN
			/* - (0x40000000) because mapping start from 1G */
			/* >> 27 = 128M / 0x20000000 */
			g_ammu_session_table_ptr->page_idx_mask[0] |=
				(1 << ((((device_va - (0x40000000)) >> 27)
				+ cross_page_array_num) & (0x1f)));
		#else
			g_ammu_session_table_ptr->page_idx_mask[0] |=
				(1 << ((device_va >> 27) + cross_page_array_num));
		#endif
			AMMU_LOG_VERBO("DBG footprint 0~4G cnt\n");
		} while (cross_page_array_num--);
		g_ammu_session_table_ptr->dram_idx_mask |= 1;
	}
#else
	/* TODO: Should DRAM block be hardcored or HS from RV??? */
	g_ammu_session_table_ptr->page_idx_mask |= (1 << (device_va / 0x20000000)); // 512M
#endif

	AMMU_LOG_VERBO("g_ammu_session_table_ptr->page_idx_mask[0] = 0x%08x\n",
			g_ammu_session_table_ptr->page_idx_mask[0]);
	AMMU_LOG_VERBO("g_ammu_session_table_ptr->page_idx_mask[1] = 0x%08x\n",
			g_ammu_session_table_ptr->page_idx_mask[1]);
	AMMU_LOG_VERBO("g_ammu_session_table_ptr->dram_idx_mask = 0x%08x\n",
			g_ammu_session_table_ptr->dram_idx_mask);

	g_ammu_session_table_ptr->adr[cur_stable_entry].type = type;
	g_ammu_session_table_ptr->adr[cur_stable_entry].iova = device_va;
	g_ammu_session_table_ptr->adr[cur_stable_entry].eva  = ret_eva;
	g_ammu_session_table_ptr->session_entry_cnt += 1;

	*eva = ret_eva;

out_after_lock:
	mutex_unlock(&g_ammu_table_set.table_lock);
out_before_lock:
	return ret;
}

static void count_page_array_en_num(void)
{
	uint32_t i, dram_mask_idx;

	for (dram_mask_idx = 0; dram_mask_idx < 2; dram_mask_idx++) {
	#if PAGE_ARRAY_CNT_EN
		if (g_ammu_session_table_ptr->page_idx_mask[dram_mask_idx] != 0) {
			for (i = 31; i >= 0; i--) {
				if (g_ammu_session_table_ptr->page_idx_mask[dram_mask_idx]
					& (1 << i))
					break;
			}

			g_ammu_session_table_ptr->page_mask_en_num[dram_mask_idx] = i+1;
		} else {
			g_ammu_session_table_ptr->page_mask_en_num[dram_mask_idx] = 0;
		}
	#else
		g_ammu_session_table_ptr->page_mask_en_num[dram_mask_idx] = 32;
	#endif
	}
}

/* get session table by session */
int get_session_table(uint64_t session, void **tbl_kva, uint32_t *size)
{
	int ret = 0;

	mutex_lock(&g_ammu_table_set.table_lock);

	if (!is_session_table_exist(session)) {
		AMMU_LOG_ERR("Session table NOT exist!!!(0x%llx)\n", session);
		ret = -ENOMEM;
		goto out;
	}

	count_page_array_en_num();

	AMMU_LOG_VERBO("g_ammu_session_table_ptr->page_idx_mask[0] = 0x%08x\n",
			g_ammu_session_table_ptr->page_idx_mask[0]);
	AMMU_LOG_VERBO("g_ammu_session_table_ptr->page_idx_mask[1] = 0x%08x\n",
			g_ammu_session_table_ptr->page_idx_mask[1]);
	AMMU_LOG_VERBO("g_ammu_session_table_ptr->page_mask_en_num[0] = 0x%08x\n",
			g_ammu_session_table_ptr->page_mask_en_num[0]);
	AMMU_LOG_VERBO("g_ammu_session_table_ptr->page_mask_en_num[1] = 0x%08x\n",
			g_ammu_session_table_ptr->page_mask_en_num[1]);
	AMMU_LOG_VERBO("g_ammu_session_table_ptr->dram_idx_mask = 0x%08x\n",
			g_ammu_session_table_ptr->dram_idx_mask);

	*tbl_kva = (void *) g_ammu_session_table_ptr;
	*size = sizeof(struct apummu_session_tbl);

out:
	mutex_unlock(&g_ammu_table_set.table_lock);
	return ret;
}

static void free_DRAM(struct kref *kref)
{
	AMMU_LOG_INFO("kref destroy\n");
#if DRAM_FALL_BACK_IN_RUNTIME
	apummu_dram_remap_runtime_free(g_adv);
#endif
}

/* free session table by session */
int session_table_free(uint64_t session)
{
	int ret = 0;

	mutex_lock(&g_ammu_table_set.table_lock);
	if (!is_session_table_exist(session)) {
		ret = -EFAULT;
		AMMU_LOG_ERR("free session table FAILED!!!, session table 0x%llx not found\n",
				session);
		goto out;
	}

	list_del(&g_ammu_session_table_ptr->list);
	kvfree(g_ammu_session_table_ptr);
	AMMU_LOG_VERBO("kref put\n");
	kref_put(&g_ammu_table_set.session_tbl_cnt, free_DRAM);

out:
	mutex_unlock(&g_ammu_table_set.table_lock);
	return ret;
}

void dump_session_table_set(void)
{
	struct list_head *list_ptr;
	uint32_t j, i = 0;

	mutex_lock(&g_ammu_table_set.table_lock);

	AMMU_LOG_INFO("== APUMMU dump session table Start ==\n");
	AMMU_LOG_INFO("== APUMMU dump session set info ==\n");
	AMMU_LOG_INFO("== APUMMU dump in dynamic mode ==\n");
	AMMU_LOG_INFO("session_tbl_cnt  = %u\n", kref_read(&g_ammu_table_set.session_tbl_cnt));
	AMMU_LOG_INFO("----------------------------------\n");

	list_for_each(list_ptr, &g_ammu_table_set.g_stable_head) {
		g_ammu_session_table_ptr = list_entry(list_ptr, struct apummu_session_tbl, list);
		AMMU_LOG_INFO("== dump session table info %u ==\n", i++);
		AMMU_LOG_INFO("session           = 0x%llx\n", g_ammu_session_table_ptr->session);
		AMMU_LOG_INFO("session_entry_cnt = %u\n",
				g_ammu_session_table_ptr->session_entry_cnt);
	#if AMMU_DRAM2PAGE_ARRAY
		AMMU_LOG_INFO("dram_idx_mask     = 0x%x\n",
				g_ammu_session_table_ptr->dram_idx_mask);
		AMMU_LOG_INFO("page_idx_mask     = 0x%x 0x%x\n",
				g_ammu_session_table_ptr->page_idx_mask[0],
				g_ammu_session_table_ptr->page_idx_mask[1]);
	#else
		AMMU_LOG_INFO("page_idx_mask     = 0x%x\n",
				g_ammu_session_table_ptr->page_idx_mask);
	#endif
		AMMU_LOG_INFO("== dump session addr table ==\n");
		AMMU_LOG_INFO("           | type  |    IOVA    |     EVA    | EVA->IOVA\n");
		for (j = 0; j < g_ammu_session_table_ptr->session_entry_cnt; j++) {
			/* EVA->IOVA will be used after encode ready */
			if (g_ammu_session_table_ptr->adr[j].type == 1) {
				AMMU_LOG_INFO("> entry%3u |     %u | 0x%8x | 0x%8x | 0x%8x\n", j,
					g_ammu_session_table_ptr->adr[j].type,
					g_ammu_session_table_ptr->adr[j].iova,
					g_ammu_session_table_ptr->adr[j].eva,
					0);
			} else {
				AMMU_LOG_INFO("> entry%3u |     %u | 0x%8x | 0x%8x |\n", j,
					g_ammu_session_table_ptr->adr[j].type,
					g_ammu_session_table_ptr->adr[j].iova,
					g_ammu_session_table_ptr->adr[j].eva);
			}
		}
	}

	mutex_unlock(&g_ammu_table_set.table_lock);
	AMMU_LOG_INFO("== APUMMU dump session table End ==\n");
}

/* Init lust head, lock */
void apummu_mgt_init(void)
{
	g_ammu_table_set.is_stable_exist = false;
	INIT_LIST_HEAD(&g_ammu_table_set.g_stable_head);
	mutex_init(&g_ammu_table_set.table_lock);
}

/* apummu_mgt_destroy session table set */
void apummu_mgt_destroy(void)
{
	struct list_head *list_ptr1, *list_ptr2;

	mutex_lock(&g_ammu_table_set.table_lock);
	list_for_each_safe(list_ptr1, list_ptr2, &g_ammu_table_set.g_stable_head) {
		g_ammu_session_table_ptr = list_entry(list_ptr1, struct apummu_session_tbl, list);
		list_del(&g_ammu_session_table_ptr->list);
		kvfree(g_ammu_session_table_ptr);
		g_ammu_session_table_ptr = NULL;
		AMMU_LOG_VERBO("kref put\n");
		kref_put(&g_ammu_table_set.session_tbl_cnt, free_DRAM);
	}

	mutex_unlock(&g_ammu_table_set.table_lock);
}
