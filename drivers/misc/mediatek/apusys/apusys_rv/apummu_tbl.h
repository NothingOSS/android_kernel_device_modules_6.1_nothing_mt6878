/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUMMU_TABLE_H__
#define __APUMMU_TABLE_H__

/*
 * This file is platform dependent
 * Note the fileds of vsid descirpt between iommu and smmu are different !
 */
#define linux_ep


#ifdef linux_ep
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/seq_file.h>

#define APUMMU_PREFIX "[apummu]"

#define LOG_ERR(x, args...) \
		pr_info(APUMMU_PREFIX "[error] %s " x, __func__, ##args)
#define LOG_WARN(x, args...) \
		pr_info(APUMMU_PREFIX "[warn] %s " x, __func__, ##args)
#define LOG_INFO(x, args...) \
		pr_info(APUMMU_PREFIX "%s " x, __func__, ##args)

#define printf LOG_INFO

#define DRV_Reg32(addr)            ioread32((void __iomem *) (uintptr_t) (addr))
#define DRV_WriteReg32(addr,  val) iowrite32(val, (void __iomem *) (uintptr_t) addr)
//void *ammu_cmu_top_base        = NULL;
//void *apummu_rcx_uprv_tcu_base = NULL;
//void *apummu_rcx_extm_tcu_base = NULL;



/***CMU***/
#define APUMMU_CMU_TOP_BASE_PTR     ammu_cmu_top_base  //apb register, 4k
#define APUMMU_CMU_TOP_TOPOLOGY_PTR (APUMMU_CMU_TOP_BASE_PTR + 0x04)
#define APUMMU_VISD_ENABLE_BASE_PTR(vsid_idx) \
			(APUMMU_CMU_TOP_BASE_PTR + ((vsid_idx) >> 5)*0x4 + 0x50)
#define APUMMU_VISD_VALID_BASE_PTR(vsid_idx) \
			(APUMMU_CMU_TOP_BASE_PTR + ((vsid_idx) >> 5)*0x4 + 0xb0)
/***VSID SRAM***/
#define APUMMU_VSID_SRAM_SZIE 0x3C00 //15K
#define APUMMU_VSID_TBL_SZIE  0x118 //280

/* apb sram , 0x19068000, vsid index */
#define APUMMU_VSID_BASE_PTR      (APUMMU_CMU_TOP_BASE_PTR + 0x1000)
#define APUMMU_VSID_DESC_BASE_PTR (APUMMU_VSID_BASE_PTR + 0x400) //apb sram
#define APUMMU_VSID_PTR(vsid_idx) (APUMMU_VSID_BASE_PTR + (vsid_idx)*4)
#define APUMMU_VSID_DESC_PTR(vsid_idx) \
		(APUMMU_VSID_DESC_BASE_PTR + (vsid_idx)*APUMMU_VSID_TBL_SZIE)
/* 0:0x00, 1:0x04, 2:0x08, 3:0x0C; seg_idx: 0-9 */
#define APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, seg_offset) \
		(APUMMU_VSID_DESC_PTR(vsid_idx) + (seg_idx)*0x10 + (seg_offset)*0x04)

#define SEC_LEVEL_NORMAL 0

#endif //Endif of #ifdef linux_ep



/***CMU***/
#define APUMMU_CMU_TOP_BASE     0x19067000  //apb register, 1k
#define APUMMU_CMU_TOP_TOPOLOGY (APUMMU_CMU_TOP_BASE + 0x04)
#define APUMMU_VISD_ENABLE_BASE(vsid_idx) (APUMMU_CMU_TOP_BASE + ((vsid_idx) >> 5)*0x4 + 0x50)
#define APUMMU_VISD_VALID_BASE(vsid_idx) (APUMMU_CMU_TOP_BASE + ((vsid_idx) >> 5)*0x4 + 0xb0)

/***VSID SRAM***/
#define APUMMU_VSID_SRAM_SZIE 0x3C00 //15K
#define APUMMU_VSID_TBL_SZIE  0x118 //280
#define APUMMU_VSID_BASE      (APUMMU_CMU_TOP_BASE + 0x1000)  //apb sram , 0x19068000, vsid index
#define APUMMU_VSID_DESC_BASE (APUMMU_VSID_BASE + 0x400) //apb sram
#define APUMMU_VSID(vsid_idx) (APUMMU_VSID_BASE + (vsid_idx)*4)
#define APUMMU_VSID_DESC(vsid_idx) (APUMMU_VSID_DESC_BASE + (vsid_idx)*APUMMU_VSID_TBL_SZIE)

/***TCU***/
//ACX0
#define APUMMU_ACX0_MVPU_TCU_BASE 0x1910E000
#define APUMMU_ACX0_MDLA_TCU_BASE 0x1910F000
//RCX
#define APUMMU_RCX_UPRV_TCU_BASE  0x19060000
#define APUMMU_RCX_EXTM_TCU_BASE  0x19061000
#define APUMMU_RCX_EDPA_TCU_BASE  0x19062000
#define APUMMU_RCX_MDLA_TCU_BASE  0x19063000

//segment page len
/*
 * Page length:This field indicate the segment size selection. There are several options
 * page length=0-> size 128KB
 * page length=1-> size 256KB
 * page length=2-> size 512KB
 * page length=3-> size 1MB
 * page length=4-> size 128MB
 * page length=5-> size 256MB
 * page length=6-> size 512MB
 * page length=7-> size 4GB
 */
enum eAPUMMUPAGESIZE {
	eAPUMMU_PAGE_LEN_128KB = 0,
	eAPUMMU_PAGE_LEN_256KB,
	eAPUMMU_PAGE_LEN_512KB,
	eAPUMMU_PAGE_LEN_1MB, //=3
	eAPUMMU_PAGE_LEN_128MB,
	eAPUMMU_PAGE_LEN_256MB,
	eAPUMMU_PAGE_LEN_512MB,//=6
	eAPUMMU_PAGE_LEN_4GB, //=7
};

// VSID table descript base
// #define APUMMU_VSID_SEGMENT_00_BASE(vsid_idx, seg_idx) \
//			(APUMMU_VSID_DESC(vsid_idx) + seg_idx*0x10 + 0x00)
// #define APUMMU_VSID_SEGMENT_04_BASE(vsid_idx, seg_idx) \
//			(APUMMU_VSID_DESC(vsid_idx) + seg_idx*0x10 + 0x04)
// #define APUMMU_VSID_SEGMENT_08_BASE(vsid_idx, seg_idx) \
//			(APUMMU_VSID_DESC(vsid_idx) + seg_idx*0x10 + 0x08)
// #define APUMMU_VSID_SEGMENT_0C_BASE(vsid_idx, seg_idx) \
//			(APUMMU_VSID_DESC(vsid_idx) + seg_idx*0x10 + 0x0C)

/* 0:0x00, 1:0x04, 2:0x08, 3:0x0C; seg_idx: 0-9 */
#define APUMMU_VSID_SEGMENT_BASE(vsid_idx, seg_idx, seg_offset) \
			(APUMMU_VSID_DESC(vsid_idx) + (seg_idx)*0x10 + (seg_offset)*0x04)


//VSID
#define APUMMU_VISD_RSV        6 //uP, logger, external cpu(cpu,gpu), SAPU, AOV
/* (15*1024/280 = 54, 0-53) */
#define APUMMU_VSID_SRAM_TOTAL (APUMMU_VSID_SRAM_SZIE / APUMMU_VSID_TBL_SZIE)
/* (15*1024/280 = 54, range from 0~53) */
#define APUMMU_RSV_VSID_DESC_IDX_END (APUMMU_VSID_SRAM_TOTAL - 1)
#define APUMMU_UPRV_RSV_DESC_IDX    APUMMU_RSV_VSID_DESC_IDX_END    //53
#define APUMMU_LOGGER_RSV_DESC_IDX (APUMMU_RSV_VSID_DESC_IDX_END-1) //52
#define APUMMU_APMCU_RSV_DESC_IDX  (APUMMU_RSV_VSID_DESC_IDX_END-2) //51
#define APUMMU_GPU_RSV_DESC_IDX    (APUMMU_RSV_VSID_DESC_IDX_END-3) //
#define APUMMU_SAPU_RSV_DESC_IDX   (APUMMU_RSV_VSID_DESC_IDX_END-4) //
#define APUMMU_AOV_RSV_DESC_IDX    (APUMMU_RSV_VSID_DESC_IDX_END-5) //48


#define APUMMU_VSID_ACTIVE		(32) //32 for ponsot
#define APUMMU_VISD_UNUSED		(12) //unused vsid
#define APUMMU_VSID_USE_MAX		(APUMMU_VSID_ACTIVE + APUMMU_VISD_RSV) //32+6
//TODO: check for preprocessor
// (APUMMU_VISD_RSV+APUMMU_VSID_ACTIVE+APUMMU_VISD_UNUSED + 1) <= APUMMU_VSID_TOTAL


#define APUMMU_RSV_VSID_IDX_END    254 //no-use 255 (number form 0-255)
#define APUMMU_RSV_VSID_IDX_START  (APUMMU_RSV_VSID_IDX_END - APUMMU_VISD_RSV + 1) //254-6+1 = 249
//TODO: check for preprocessor
//(APUMMU_RSV_VSID_END - APUMMU_RSV_VSID_START + 1) <= APUMMU_VISD_RSV

//Reserve
#define APUMMU_UPRV_RSV_VSID    APUMMU_RSV_VSID_IDX_END    //254
#define APUMMU_LOGGER_RSV_VSID (APUMMU_RSV_VSID_IDX_END-1) //253
#define APUMMU_APMCU_RSV_VSID  (APUMMU_RSV_VSID_IDX_END-2) //252
#define APUMMU_GPU_RSV_VSID    (APUMMU_RSV_VSID_IDX_END-3) //
#define APUMMU_SAPU_RSV_VSID   (APUMMU_RSV_VSID_IDX_END-4) //
#define APUMMU_AOV_RSV_VSID    (APUMMU_RSV_VSID_IDX_END-5) //249
//TODO: check the minimal rsv vsid >= APUMMU_RSV_VSID_START

//VSID bit mask
#define APUMMU_VSID_MAX_MASK_WORD   ((APUMMU_VSID_USE_MAX+32-1)/32) //ceiling word


// VSID fields
// Get segment offset 0 data - 0x00
#define APUMMU_SEGMENT_GET_INPUT(vsid_idx, seg_idx) \
	((DRV_Reg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 0)) >> 10) & 0x3fffff)
#define APUMMU_SEGMENT_GET_OFFSET0_RSRV(vsid_idx, seg_idx) \
	((DRV_Reg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 0)) >> 6)  & 0xf)
#define APUMMU_SEGMENT_GET_PAGESEL(vsid_idx, seg_idx) \
	((DRV_Reg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 0)) >> 3)  & 0x7)
#define APUMMU_SEGMENT_GET_PAGELEN(vsid_idx, seg_idx) \
	((DRV_Reg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 0)) >> 0)  & 0x7)
// Get segment offset 1 data - 0x04
#define APUMMU_SEGMENT_GET_OUTPUT(vsid_idx, seg_idx) \
	((DRV_Reg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 1)) >> 10) & 0x3fffff)
#define APUMMU_SEGMENT_GET_OFFSET1_RSRV0(vsid_idx, seg_idx) \
	((DRV_Reg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 1)) >> 2)  & 0xff)
#define APUMMU_SEGMENT_GET_IOMMUEN(vsid_idx, seg_idx) \
	((DRV_Reg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 1)) >> 1)  & 0x1)
#define APUMMU_SEGMENT_GET_OFFSET1_RSRV1(vsid_idx, seg_idx) \
	((DRV_Reg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 1)) >> 0)  & 0x1)
// Get segment offset 2 data - 0x08
//TBD
// Get segment offset 3 data - 0x0c
//TBD

/***Build segment data***/
// Build segment offset 0 (0x00) data
#define APUMMU_VSID_SEGMENT_00_INPUT(input_adr)   (((input_adr) & 0x3fffff) << 10)  // 22bits
#define APUMMU_VSID_SEGMENT_00_RESV(resv)         (((resv) & 0xf) << 6)             // 4bits
#define APUMMU_VSID_SEGMENT_00_PAGESEL(page_sel)  (((page_sel) & 0x7) << 3)         // 3bits
#define APUMMU_VSID_SETMENT_00_PAGELEN(page_len)  (((page_len) & 0x7) << 0)         // 3bits
#define APUMMU_BUILD_SEGMENT_OFFSET0(input_adr, resv, page_sel, page_len) \
	(APUMMU_VSID_SEGMENT_00_INPUT(input_adr) | APUMMU_VSID_SEGMENT_00_RESV(resv) | \
	APUMMU_VSID_SEGMENT_00_PAGESEL(page_sel) | APUMMU_VSID_SETMENT_00_PAGELEN(page_len))
// Build segment offset 1 (0x04) data
#define APUMMU_VSID_SEGMENT_04_OUTPUT(output_adr) (((output_adr) & 0x3fffff) << 10) // 22bits
#define APUMMU_VSID_SEGMENT_04_RESV0(resv0)       (((resv0) & 0xff) << 2)           // bits
#define APUMMU_VSID_SEGMENT_04_IOMMU_EN(iommu_en) (((iommu_en) & 0x1) << 1)         // bits
#define APUMMU_VSID_SEGMENT_04_RESV1(resv1)       (((resv1) & 0x1) << 0)            // bits
#define APUMMU_BUILD_SEGMENT_OFFSET1(output_adr, resv0, iommu_en, resv1) \
	(APUMMU_VSID_SEGMENT_04_OUTPUT(output_adr) | APUMMU_VSID_SEGMENT_04_RESV0(resv0) | \
	APUMMU_VSID_SEGMENT_04_IOMMU_EN(iommu_en) | APUMMU_VSID_SEGMENT_04_RESV1(resv1))
// Build segment offset 2 (0x08) data
// mask and shift
#define APUMMU_VSID_SEGMENT_08_RESV_MASK   0x7fff
#define APUMMU_VSID_SEGMENT_08_RESV_SHIFT      17
#define APUMMU_VSID_SEGMENT_08_DOMAIN_MASK    0xf
#define APUMMU_VSID_SEGMENT_08_DOMAIN_SHIFT    13
//
#define APUMMU_VSID_SEGMENT_08_RESV(resv) \
	(((resv) & APUMMU_VSID_SEGMENT_08_RESV_MASK) << APUMMU_VSID_SEGMENT_08_RESV_SHIFT)
#define APUMMU_VSID_SEGMENT_08_DOMAIN(domain) \
	(((domain) & APUMMU_VSID_SEGMENT_08_DOMAIN_MASK) << APUMMU_VSID_SEGMENT_08_DOMAIN_SHIFT)
#define APUMMU_VSID_SEGMENT_08_ACP_EN(acp_en)          (((acp_en) & 0x1) << 12)  //bool type?
#define APUMMU_VSID_SEGMENT_08_AW_CLR(aw_clr)          (((aw_clr) & 0x1) << 11)
#define APUMMU_VSID_SEGMENT_08_AW_INVALID(aw_invalid)  (((aw_invalid) & 0x1) << 10)
#define APUMMU_VSID_SEGMENT_08_AR_EXCLU(ar_exclu)      (((ar_exclu) & 0x1) << 9)
#define APUMMU_VSID_SEGMENT_08_AR_SEPCU(ar_sepcu)      (((ar_sepcu) & 0x1) << 8)
#define APUMMU_VSID_SEGMENT_08_AW_CACHE_ALLOCATE(aw_cache_allocate) \
			(((aw_cache_allocate) & 0x1) << 7)
#define APUMMU_VSID_SEGMENT_08_AW_SLC_EN(aw_slc_en)    (((aw_slc_en) & 0x1) << 6)
#define APUMMU_VSID_SEGMENT_08_AW_SLB_EN(aw_slb_en)    (((aw_slb_en) & 0x1) << 5)
#define APUMMU_VSID_SEGMENT_08_AR_CACHE_ALLOCATE(ar_cache_allocate) \
			(((ar_cache_allocate) & 0x1) << 4)
#define APUMMU_VSID_SEGMENT_08_AR_SLC_EN(ar_slc_en)    (((ar_slc_en) & 0x1) << 3)
#define APUMMU_VSID_SEGMENT_08_AR_SLB_EN(ar_slb_en)    (((ar_slb_en) & 0x1) << 2)
#define APUMMU_VSID_SEGMENT_08_RO(ro)                  (((ro) & 0x1) << 1)
#define APUMMU_VSID_SEGMENT_08_NS(ns)                  (((ns) & 0x1) << 0)

#define APUMMU_BUILD_SEGMENT_OFFSET2(resv, domain, acp_en, aw_clr, \
		aw_invalid, ar_exclu, ar_sepcu, \
		aw_cache_allocate, aw_slc_en, aw_slb_en, ar_cache_allocate, \
		ar_slc_en, ar_slb_en, ro, ns) \
		((APUMMU_VSID_SEGMENT_08_RESV(resv)) |\
		(APUMMU_VSID_SEGMENT_08_DOMAIN(domain)) |\
		(APUMMU_VSID_SEGMENT_08_ACP_EN(acp_en)) |\
		(APUMMU_VSID_SEGMENT_08_AW_CLR(aw_clr)) |\
		(APUMMU_VSID_SEGMENT_08_AW_INVALID(aw_invalid)) |\
		(APUMMU_VSID_SEGMENT_08_AR_EXCLU(ar_exclu)) |\
		(APUMMU_VSID_SEGMENT_08_AR_SEPCU(ar_sepcu)) |\
		(APUMMU_VSID_SEGMENT_08_AW_CACHE_ALLOCATE(aw_cache_allocate)) |\
		(APUMMU_VSID_SEGMENT_08_AW_SLC_EN(aw_slc_en)) |\
		(APUMMU_VSID_SEGMENT_08_AW_SLB_EN(aw_slb_en)) |\
		(APUMMU_VSID_SEGMENT_08_AR_CACHE_ALLOCATE(ar_cache_allocate)) |\
		(APUMMU_VSID_SEGMENT_08_AR_SLC_EN(ar_slc_en)) |\
		(APUMMU_VSID_SEGMENT_08_AR_SLB_EN(ar_slb_en)) |\
		(APUMMU_VSID_SEGMENT_08_RO(ro)) | (APUMMU_VSID_SEGMENT_08_NS(ns)))

// Build segment offset 3 (0x0c) data
#define APUMMU_VSID_SEGMENT_0C_SEG_VALID(seg_valid)  (((seg_valid) & 0x1) << 31)  //1bit
#define APUMMU_VSID_SEGMENT_0C_RESV(rsv)             (((rsv) & 0x7fffffff) << 0)  //31bit
#define APUMMU_BUILD_SEGMENT_OFFSET3(seg_valid, rsv) \
	(APUMMU_VSID_SEGMENT_0C_SEG_VALID(seg_valid) | APUMMU_VSID_SEGMENT_0C_RESV(rsv))



#ifdef APUUMU_RV
struct apummu_vsid_tlb {
	uint32_t cmd_id;      //apucmd id by midware
	uint32_t vsid_num;    //vsid_num = APUMMU_VSID_MAX
	uint32_t sram_mask;   //tcm , slb , or tcm+sbl for this visd
	uint32_t dram_mask_l; //low dram usaeg for this vsid
	uint32_t dram_mask_l; //high dram usage for this vsid
	apummu_apucmd_tbl *apummu_apucmd_tb; //point to apummu_apucmd_tbl_t[index]
} apummu_vsid_tbl_t;


//Global vsid
struct apummu_vsid {
		//uint32_t tcm_mask;   //granularity 512k, 16M
		//uint32_t slb_mask;   //granularity 512K, 16M
		uint32_t tsram_mask;   //sram = tcm + slb, granularity 512k, 8+8
		uint32_t tdram_mask_l; //granularity 128M, 0-4G
		uint32_t tdram_mask_h; //granularity 512M, 16G, 4-16G
		uint32_t vsid_mask[APUMMU_VSID_MAX_MASK_WORD];  //((32+10)+31) /32=2
		//e.g. vsid=3, use apummu_vsid_tbl_t[3]
		apummu_vsid_tbl_t apummu_vsid_tbl_t[APUMMU_VSID_USE_MAX];
} apummu_vsid_t;


//Global status.
struct apummu_drv {
		//config
		uint32_t tcm_scale;
		uint32_t dram_scale;
		uint64_t tcm_pa;
		///.....etc.

		//vsid
		apummu_vsid_t *apummu_vsid;
} apummu_drv_t;

//Global data structure
apummu_drv_t gApummu_drv;

//int rv_boot(uint32_t seg_output0, uint32_t seg_output1, uint32_t seg_output2, uint8_t hw_thread);

//@pare table
//vsid table (= dram table + sram table)

struct apummu_vsid_tlb {
	uint32_t intput_adr:22
	uint32_t reserv:4;
	uint32_t page_selt: 3;
	....

	//
	*ptr = &apummu_session_tbl_t  //locate dram
} apummu_vsid_tbl_t[16];

#endif  //Endof #ifdef APUUMU_RV

int rv_boot(uint32_t seg_output0, uint32_t seg_output1, uint32_t seg_output2, uint8_t hw_thread);
// got from peter
#define APUMMU_INT_D2T_TBL0_OFS 0x40
enum {
	APUMMU_THD_ID_APMCU_NORMAL = 0,
	APUMMU_THD_ID_TEE,
	APUMMU_THD_ID_MAX
};



#endif //Endof __APUMMU_TABLE_H__

