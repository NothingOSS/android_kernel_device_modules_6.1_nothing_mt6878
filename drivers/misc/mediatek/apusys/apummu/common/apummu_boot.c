// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/io.h>

#include "apummu_tbl.h"


/*
 * CMU
 *    #define APUMMU_CMU_TOP_BASE 0x19067000, apb register, 1k
 * TCU
 *  ACX0
 *    #define APUMMU_ACX0_MVPU_TCU_BASE 0x1910E000
 *    #define APUMMU_ACX0_MDLA_TCU_BASE 0x1910F000
 *  RCX
 *    #define APUMMU_RCX_UPRV_TCU_BASE  0x19060000
 *    #define APUMMU_RCX_EXTM_TCU_BASE  0x19061000
 *    #define APUMMU_RCX_EDPA_TCU_BASE  0x19062000
 *    #define APUMMU_RCX_MDLA_TCU_BASE  0x19063000
 */

#ifdef linux_ep
void *ammu_cmu_top_base;
void *apummu_rcx_uprv_tcu_base;
void *apummu_rcx_extm_tcu_base;

int apummu_ioremap(void)
{
	ammu_cmu_top_base        = ioremap(APUMMU_CMU_TOP_BASE, 0x5000); // 4k + 16k = 20K (ponsot)
	apummu_rcx_uprv_tcu_base = ioremap(APUMMU_RCX_UPRV_TCU_BASE, 0x1000); // 4k (ponsot)
	apummu_rcx_extm_tcu_base = ioremap(APUMMU_RCX_EXTM_TCU_BASE, 0x1000); // 4k (ponsot)

	printf("ammu remap adr: CMUL(0x%llx), RV TCU(0x%llx) , EXTM TCU(0x%llx)\n",
		(uint64_t)ammu_cmu_top_base,
		(uint64_t)apummu_rcx_uprv_tcu_base,
		(uint64_t)apummu_rcx_extm_tcu_base);

	return 0;
}
#endif


//Get vsid segment 0x00
int apummu_get_segment_offset0(uint32_t vsid_idx, uint8_t seg_idx, uint32_t *input_adr,
					uint8_t *res_bits, uint8_t *page_sel, uint8_t *page_len)
{
	//check abnormal parameter
	//printf parameters
	*input_adr = APUMMU_SEGMENT_GET_INPUT(vsid_idx, seg_idx);
	*res_bits  = APUMMU_SEGMENT_GET_OFFSET0_RSRV(vsid_idx, seg_idx);
	*page_sel  = APUMMU_SEGMENT_GET_PAGESEL(vsid_idx, seg_idx);
	*page_len  = APUMMU_SEGMENT_GET_PAGELEN(vsid_idx, seg_idx);

	return 0; //indicate success or fail
}

//get vsid segment 0x04
//TBD

//get vsid segment 0x08
//TBD

//get vsid segment 0x0c
//TBD


//Set vsid segment 0x00
int apummu_set_segment_offset0(uint32_t vsid_idx, uint8_t seg_idx, uint32_t input_adr,
					uint8_t res_bits, uint8_t page_sel, uint8_t page_len)
{
	//check abnormal parameter
	//printf parameters
#ifdef linux_ep
	DRV_WriteReg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 0),
			APUMMU_BUILD_SEGMENT_OFFSET0(input_adr, res_bits, page_sel, page_len));
#else
	DRV_WriteReg32(APUMMU_VSID_SEGMENT_BASE(vsid_idx, seg_idx, 0),
		APUMMU_BUILD_SEGMENT_OFFSET0(input_adr, res_bits, page_sel, page_len));
#endif

	return 0; //indicate success or fail
}


//Set vsid segment 0x04
int apummu_set_segment_offset1(uint32_t vsid_idx, uint8_t seg_idx, uint32_t output_adr,
					uint8_t res0, uint8_t iommu_en, uint8_t res1)
{
	//check abnormal parameter
	//printf parameters
#ifdef linux_ep
	DRV_WriteReg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 1),
				APUMMU_BUILD_SEGMENT_OFFSET1(output_adr, res0, iommu_en, res1));
#else
	DRV_WriteReg32(APUMMU_VSID_SEGMENT_BASE(vsid_idx, seg_idx, 1),
				APUMMU_BUILD_SEGMENT_OFFSET1(output_adr, res0, iommu_en, res1));
#endif

	return 0; //indicate success or fail
}


//set vsid segment 0x08 -prefer bit fields as input parameters
int apummu_set_segment_offset2(
	uint32_t vsid_idx, uint8_t seg_idx, uint8_t resv, uint8_t domain, uint8_t acp_en,
	uint8_t aw_clr, uint8_t aw_invalid, uint8_t ar_exclu, uint8_t ar_sepcu,
	uint8_t aw_cache_allocate, uint8_t aw_slc_en, uint8_t aw_slb_en,
	uint8_t ar_cache_allocate, uint8_t ar_slc_en, uint8_t ar_slb_en, uint8_t ro, uint8_t ns)
{
	#ifdef linux_ep
	DRV_WriteReg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 2),
			APUMMU_BUILD_SEGMENT_OFFSET2(resv, domain, acp_en, aw_clr, aw_invalid,
							ar_exclu, ar_sepcu, aw_cache_allocate,
							aw_slc_en, aw_slb_en, ar_cache_allocate,
							ar_slc_en, ar_slb_en, ro, ns));
	#else
	DRV_WriteReg32(APUMMU_VSID_SEGMENT_BASE(vsid_idx, seg_idx, 2),
			APUMMU_BUILD_SEGMENT_OFFSET2(
				resv, domain, acp_en, aw_clr, aw_invalid,
				ar_exclu, ar_sepcu, aw_cache_allocate,
				aw_slc_en, aw_slb_en, ar_cache_allocate,
				ar_slc_en, ar_slb_en, ro, ns));
	#endif

	return 0;
}


//set vsid segment 0x0c
int apummu_set_segment_offset3(uint32_t vsid_idx, uint8_t seg_idx, uint8_t seg_valid, uint8_t rsv)
{
#ifdef linux_ep
	DRV_WriteReg32(APUMMU_VSID_SEGMENT_BASE_PTR(vsid_idx, seg_idx, 3),
					APUMMU_BUILD_SEGMENT_OFFSET3(seg_valid, rsv));
#else
	DRV_WriteReg32(APUMMU_VSID_SEGMENT_BASE(vsid_idx, seg_idx, 3),
					APUMMU_BUILD_SEGMENT_OFFSET3(seg_valid, rsv));
#endif

	return 0;
}


/*
 * apummu_enable_vsid()

 * VSID in CMU Configuration

 * 0x50 VSID_enable0_set 32 1  VSID enable0 set BX; SCU;
 * RAND 31 0 the_VSID31_0_enable_set_register

 * 0x54 VSID_enable1_set 32 1  VSID enable1 set BX; SCU;
 * RAND 31 0 the_VSID63_32_enable_set_register

 * *((UINT32P)(APU_RCX_AMU_CMU_TOP+(vsid/32)*0x4+0x50)) = 0x1 << (vsid%32);
 * *((UINT32P)(APU_RCX_AMU_CMU_TOP+(vsid/32)*0x4+0xb0)) = 0x1 << (vsid%32);

*/
int apummu_enable_vsid(uint32_t vsid_idx)
{
	if (vsid_idx > (APUMMU_VSID_ACTIVE-1) &&
			vsid_idx < (APUMMU_RSV_VSID_IDX_END - APUMMU_VISD_RSV+1)) {
		printf("invalid vsid index %d\n", vsid_idx);
		return -1;
	}

#ifdef linux_ep
	/* this vsid is distributed */
	DRV_WriteReg32(APUMMU_VISD_ENABLE_BASE_PTR(vsid_idx), 0x1 << (vsid_idx & 0x1f));
	/* this vsid is ready for used */
	DRV_WriteReg32(APUMMU_VISD_VALID_BASE_PTR(vsid_idx), 0x1 << (vsid_idx & 0x1f));
#else
	/* this vsid is distributed */
	DRV_WriteReg32(APUMMU_VISD_ENABLE_BASE(vsid_idx), 0x1 << (vsid_idx & 0x1f));
	/* this vsid is ready for used */
	DRV_WriteReg32(APUMMU_VISD_VALID_BASE(vsid_idx),  0x1 << (vsid_idx & 0x1f));
#endif

	return 0;
}

/*
 * apummu_enable()
 * CMU init
 * ((UINT32P)(0x19067000)) = 0x1; //bit0: apu_mmu_en
 * 0x0	cmu_con	32	1	cmu control register
 *				0	0	APU_MMU_enable
 *				1	1	tcu_apb_dbg_en
 *				2	2	tcu_secure_chk_en
 *				3	3	DCM_en
 *				4	4	sw_slp_prot_en_override
 */
int apummu_enable(void)
{
	uint32_t flag = 0;

	#ifdef linux_ep
	//need to read first, only set bit 0 (keep the default value)
	flag = DRV_Reg32(APUMMU_CMU_TOP_BASE_PTR);
	flag |= 0x1;
	DRV_WriteReg32(APUMMU_CMU_TOP_BASE_PTR, flag);
	#else
	//need to read first, only set bit 0 (keep the default value)
	flag = DRV_Reg32(APUMMU_CMU_TOP_BASE);
	flag |= 0x1;
	DRV_WriteReg32(APUMMU_CMU_TOP_BASE, 0x1);
	#endif

	return 0;
}


/*
 * apummu_topology_init()
 * 0x4 cmu_sys_con0	32	1	cmu topology setting 0
 *					6	0	socket0_tcu_bit_map
 *					13	7	socket1_tcu_bit_map
 *					20	14	socket2_tcu_bit_map
 *						27	21	socket3_tcu_bit_map
 * ponsot-> socket0:acx0 , socket1:rcx
 * leroy -> socket0:acx0 , socket1:acx1 , socket2:ncx , socket3:rcx
 * ponsot :  *((UINT32P)(0x19067004)) = (0x3) | (0xf<<7)
 * leroy  :  *((UINT32P)(0x19067004)) = (0x3) | (0x3 <<7) | (0x3 << 14) | (0xf<<21)
 */
int apummu_topology_init(void)
{
	#ifdef linux_ep
	DRV_WriteReg32(APUMMU_CMU_TOP_TOPOLOGY_PTR, ((0xf << 7) | 0x03));
	#else
	DRV_WriteReg32(APUMMU_CMU_TOP_TOPOLOGY, ((0xf << 7) | 0x03));
	#endif

	return 0;
}


// apummu_vsid_sram_config() @ drv_init
int apummu_vsid_sram_config(void)
{
	uint32_t idx;

#ifdef linux_ep
	//cofnig reserverd vsid idx: 254~249 , sram position: 53~48 (ponsot)
	for (idx = 0; idx < APUMMU_VISD_RSV; idx++) {
		/* 1round: (249, 48). final round: (254, 53), (config addr, desc addr) */
		DRV_WriteReg32(APUMMU_VSID_PTR(APUMMU_RSV_VSID_IDX_START+idx),
			APUMMU_VSID_DESC(APUMMU_VSID_SRAM_TOTAL - APUMMU_VISD_RSV + idx));
	}
#else
	//cofnig reserverd vsid idx: 254~249 , sram position: 53~48 (ponsot)
	/* 1round: (249, 48). final round: (254, 53) */
	for (idx = 0; idx < APUMMU_VISD_RSV; idx++) {
		DRV_WriteReg32(APUMMU_VSID(APUMMU_RSV_VSID_IDX_START+idx),
			APUMMU_VSID_DESC(APUMMU_VSID_SRAM_TOTAL - APUMMU_VISD_RSV + idx));
	}

#endif


#ifdef SHOW_COMMENT
	//config active vsid's vsid desc 0-31 (ponsot)
	for (idx = 0; idx < APUMMU_VSID_ACTIVE; idx++)
		DRV_WriteReg32(APUMMU_VSID(idx), APUMMU_VSID_DESC(idx));

	//set valid bit = 0 of 10 segment to avoid abnormal operations
	//TBD
#endif

	return 0;
}





/*
 * apummu_bind_vsid()
 * //thread mapping to VSID table
 * e.g
 * //bit17 to bit11: COR_ID, bit10 to bit3: VSID, bit0: VSID valid, bit1: COR_ID valid
 * *((UINT32P)(base + (thread*0x4))) =  (cor_id << 11) + (vsid << 3) + 0x1 + 0x2;
 *   => VSDI valid:1 -> means the enigne bind this visd
 *   => VSID valid:0 -> interurpt will be triggered if transcation is sent
 * 17	11	cor_id
 * 10	3	vsid
 * 2	2	vsid_prefetch_trigger
 * 1	1	corid_vld
 * 0	0	vsid_vld
 */
#ifdef linux_ep
int apummu_bind_vsid(void *tcu_base, uint32_t vsid_idx, uint8_t cor_id, uint8_t hw_thread,
					uint8_t cor_valid, uint8_t vsid_valid)
#else
int apummu_bind_vsid(uint32_t tcu_base, uint32_t vsid_idx, uint8_t cor_id, uint8_t hw_thread,
					uint8_t cor_valid, uint8_t vsid_valid)
#endif
{
#ifdef linux_ep
	printf("TCU BASE:0x%llx, vsid=%d, core_id=%d, thread=%d, cor_valid=%d, vsid_valid=%d\n",
			(uint64_t)tcu_base, vsid_idx, cor_id, hw_thread, cor_valid, vsid_valid);
#else
	printf("TCU BASE:0x%x, vsid=%d, core_id=%d, thread=%d, cor_valid=%d, vsid_valid=%d\n",
			tcu_base, vsid_idx, cor_id, hw_thread, cor_valid, vsid_valid);
#endif
	DRV_WriteReg32((tcu_base + hw_thread*0x4), (((cor_id & 0x7f) << 11)
				| ((vsid_idx & 0xff) << 3)
				| ((cor_valid & 0x1) << 1) | ((vsid_valid & 0x1) << 0)));

	return 0;
}

int apummu_rv_bind_vsid(uint8_t hw_thread)
{
	//UINT32 thread	= 2;//md32 user->thread0, logger -> thread2 ?
	if (hw_thread > 7) {
		printf("the hw thread id (%d) is not valid for rv/logger\n", hw_thread);
		return -1;
	}

	/* for rV */
	apummu_bind_vsid(apummu_rcx_uprv_tcu_base, APUMMU_UPRV_RSV_VSID, 0, 0, 0, 1);
#ifdef linux_ep
	apummu_bind_vsid(apummu_rcx_uprv_tcu_base, APUMMU_UPRV_RSV_VSID, 0, hw_thread, 0, 1);
#else
	apummu_bind_vsid(APUMMU_RCX_UPRV_TCU_BASE, APUMMU_UPRV_RSV_VSID, 0, hw_thread, 0, 1);
#endif
	printf("Binding APUMMU_UPRV_RSV_VSID successfully (%d)\n", APUMMU_UPRV_RSV_VSID);

	return 0;
}

int apummu_logger_bind_vsid(uint8_t hw_thread)
{

	if (hw_thread > 7) {
		printf("the hw thread id (%d) is not valid for rv/logger\n", hw_thread);
		return -1;
	}
	/* for logger */
#ifdef linux_ep
	apummu_bind_vsid(apummu_rcx_uprv_tcu_base, APUMMU_LOGGER_RSV_VSID, 0, hw_thread, 0, 1);
#else
	apummu_bind_vsid(APUMMU_RCX_UPRV_TCU_BASE, APUMMU_LOGGER_RSV_VSID, 0, hw_thread, 0, 1);
#endif
	printf("Binding APUMMU_LOGGER_RSV_VSID successfully(%d)\n", APUMMU_LOGGER_RSV_VSID);

	return 0;
}

int apummu_apmcu_bind_vsid(uint8_t hw_thread)
{

	if (hw_thread > 7) {
		printf("the hw thread id (%d) is not valid for rv/logger\n", hw_thread);
		return -1;
	}
	/* for APmcu */
#ifdef linux_ep
	apummu_bind_vsid(apummu_rcx_extm_tcu_base, APUMMU_APMCU_RSV_VSID, 0, hw_thread, 0, 1);
#else
	apummu_bind_vsid(APUMMU_RCX_EXTM_TCU_BASE, APUMMU_APMCU_RSV_VSID, 0, hw_thread, 0, 1);
#endif
	printf("Binding APUMMU_LOGGER_RSV_VSID successfully(%d)\n", APUMMU_APMCU_RSV_VSID);

	return 0;
}


/*
 * apummu_add_map()

 * vsid_idx:0-255
 * seg_idx :0-9
 * input eva: 22bits
 * output remap_adr: 22bits

 * Page length:This field indicate the segment size selection. There are several options
 * page length=0-> size 128KB
 * page length=1-> size 256KB
 * page length=2-> size 512KB
 * page length=3-> size 1MB
 * page length=4-> size 128MB
 * page length=5-> size 256MB
 * page length=6-> size 512MB
 * page length=7-> size 4GB
 * when page sel=0, total remap size = input base+page length (page enable bit ?¡æ?)
 * when page sel=3~7, total remap size=input base+page length*page enable

 * Segment layout for reserved VSID
 * reservered VSID 254 for uP
 * reservered VSID 253 for logger
 * reservered VSID 252 for ARM
 * reservered VSID 251 for GPU
 * reservered VSID 250 for sAPU
 * reservered VSID 249 for AoV (1-1 mapping, or disable APUMMMU)

 * Up
 * seg_idx 0  - 1M
 * seg_idx 1  - 512M
 * seg_idx 2  - 4G
 * e.g apummu_add_map(254, 0, 0, output_adr, 0,3, SEC_LEVEL_NORMAL);
 * apummu_add_map(254, 1, 0, output_adr, 0,6, SEC_LEVEL_NORMAL);
 * apummu_add_map(254, 2, 0, output_adr, 0,7, SEC_LEVEL_NORMAL);
 * Logger
 * seg_idx 0  - 4G

 * xPU (CPU/GPU)
 * seg_idx 0 - core dump address
 */
#ifndef linux_ep
apummu_vsid_t gApummu_vsid;
#endif
int apummu_add_map(uint32_t vsid_idx, uint8_t seg_idx, uint32_t input_adr, uint32_t output_adr,
				uint8_t page_sel, uint8_t page_len, int sec_level)
{
	uint8_t domain, ns;
#ifdef COMMENT_SHOW
	if ((input_adr & 0x3fffff) != 0 || (output_adr & 0x3fffff) != 0) { // check 4k alignment
		printf("input/output adr is not 4k alignment (%x / %x)\n", input_adr, output_adr);
		return -1; //error code
	}
#endif
	// if (vsid_idx > (APUMMU_VSID_ACTIVE-1) && \
	//        vsid_idx < (APUMMU_RSV_VSID_IDX_END - APUMMU_VISD_RSV+1)) { //check vsid if valid
	//        printf("vsid_idx  is not valid  (0x%x:0x%x)\n", vsid_idx);
	//   return -2;
	// }

	if (seg_idx > 9) { //check segment position if illegal
		printf("seg_idx  is not illegal (0x%x)\n", seg_idx);
		return -3;
	}

	// get domain /ns
#ifndef linux_ep
	sec_get_dns(sec_level, &domain, &ns);
#else
	domain = 7;
	ns = 1;
#endif

	//fill segment
	apummu_set_segment_offset0(vsid_idx, seg_idx, input_adr, 0, page_sel, page_len);
	apummu_set_segment_offset1(vsid_idx, seg_idx, output_adr, 0, 1, 0);
	apummu_set_segment_offset2(vsid_idx, seg_idx, 0, domain,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ns);
	apummu_set_segment_offset3(vsid_idx, seg_idx, 1, 0);

#ifdef COMMENT_SHOW //ndef linux_ep
	//fill page arrary if needs
#endif

	return 0; //success or fail
}


int apummu_boot_init(void)
{

#ifdef linux_ep
	// apummu ioremap
	apummu_ioremap();
#endif
	//vsid sram descript init
	apummu_vsid_sram_config();

#ifdef COMMENT_SHOW//ndef linux_ep
	//topology init
	apummu_topology_init();
#endif

	//enable apummu h/w
	apummu_enable();


	return 0;
}


/*
 * virtual engine thread generator
 */

int virtual_engine_thread(void)
{
	uint32_t thread_map;

/*
 * init TEE dns 0 maps thread 1
 * (dns consists of 5 bits xxxy -> xxxx is domain, y is ns)
 */
#ifdef linux_ep
	thread_map = APUMMU_THD_ID_APMCU_NORMAL; //0
	DRV_WriteReg32((apummu_rcx_extm_tcu_base + APUMMU_INT_D2T_TBL0_OFS), thread_map << 3);
#else
	thread_map = APUMMU_THD_ID_TEE; //1
	DRV_WriteReg32((APUMMU_RCX_EXTM_TCU_BASE + APUMMU_INT_D2T_TBL0_OFS), thread_map);
#endif

	return 0;

}

/*
 * for apmcu map
 */
int apummu_add_apmcu_map(uint32_t seg_input0, uint32_t seg_output0, enum eAPUMMUPAGESIZE page_size)
{
	//must be in order
	apummu_add_map(APUMMU_APMCU_RSV_DESC_IDX, 0, seg_input0, seg_output0,
					0, page_size, SEC_LEVEL_NORMAL); //page length=3-> size 1MB

	// enable vsid
	apummu_enable_vsid(APUMMU_APMCU_RSV_VSID);

	return 0;
}


/*
 * for logger map
 */
int apummu_add_logger_map(uint32_t seg_input0, uint32_t seg_output0, enum eAPUMMUPAGESIZE page_size)
{
	//must be in order
	apummu_add_map(APUMMU_LOGGER_RSV_DESC_IDX, 0, seg_input0, seg_output0,
					0, page_size, SEC_LEVEL_NORMAL); //page length=3-> size 1MB

	// enable vsid
	apummu_enable_vsid(APUMMU_LOGGER_RSV_VSID);

	return 0;
}

/*
 * for rv boot map
 */
int apummu_add_rv_boot_map(uint32_t seg_output0, int32_t seg_output1, int32_t seg_output2)
{
	//must be in order
	// eAPUMMU_PAGE_LEN_1MB, = 3
	apummu_add_map(APUMMU_RSV_VSID_DESC_IDX_END, 0, 0, seg_output0,
					0, eAPUMMU_PAGE_LEN_1MB, SEC_LEVEL_NORMAL);
	// eAPUMMU_PAGE_LEN_512MB, 6
	apummu_add_map(APUMMU_RSV_VSID_DESC_IDX_END, 1, 0, seg_output1,
					0, eAPUMMU_PAGE_LEN_512MB, SEC_LEVEL_NORMAL);
	// eAPUMMU_PAGE_LEN_4GB, 7
	apummu_add_map(APUMMU_RSV_VSID_DESC_IDX_END, 2, 0, seg_output2,
					0, eAPUMMU_PAGE_LEN_4GB, SEC_LEVEL_NORMAL);

	// enable vsid
	apummu_enable_vsid(APUMMU_UPRV_RSV_VSID);

	return 0;
}

/*=====Example func. Before RCX power on =========================*/
int rv_boot(uint32_t seg_output0, uint32_t seg_output1, uint32_t seg_output2, uint8_t hw_thread)
{
	// apummu init @ beginning - call this once only
	apummu_boot_init();
	printf("<%s> seg_output0 = 0x%8x seg_output1 = 0x%8x seg_output2 = 0x%8x\n",
		 __func__, seg_output0, seg_output1, seg_output2);
	// 1. add rv map - MUST be in-order for rv booting
	apummu_add_rv_boot_map(seg_output0, seg_output0, seg_output0);
	// bind rv vsid
	apummu_rv_bind_vsid(hw_thread); //thread: 0:normal, 1:secure, 2:logger?; MP flow should be 1
	apummu_rv_bind_vsid(1);
	//2.  add h/w logger map
	apummu_add_logger_map(seg_output1/*input*/, seg_output1/*output*/, eAPUMMU_PAGE_LEN_1MB);
	// bind logger vsid
	apummu_logger_bind_vsid(2); //thread: 0:normal, 1:secure, 2:logger

	//3. add apmcu map
	virtual_engine_thread();
	apummu_add_apmcu_map(seg_output2/*input*/, seg_output2/*output*/, eAPUMMU_PAGE_LEN_256KB);
	apummu_apmcu_bind_vsid(APUMMU_THD_ID_APMCU_NORMAL);


	return 0;
}
/*=====example func.=========================*/



