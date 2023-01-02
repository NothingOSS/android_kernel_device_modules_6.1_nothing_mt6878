// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/wakeup_reason.h>
#include <linux/syscore_ops.h>

#include <lpm.h>
#include <lpm_module.h>
#include <lpm_spm_comm.h>
#include <lpm_dbg_common_v2.h>
#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_trace_event.h>
#include <lpm_dbg_logger.h>
#include <lpm_trace_event/lpm_trace_event.h>
#include <spm_reg.h>
#include <pwr_ctrl.h>
#include <mt-plat/mtk_ccci_common.h>
#include <lpm_timer.h>
#include <mtk_lpm_sysfs.h>
#include <mtk_cpupm_dbg.h>

#define MT6985_LOG_DEFAULT_MS		5000

#define PCM_32K_TICKS_PER_SEC		(32768)
#define PCM_TICK_TO_SEC(TICK)	(TICK / PCM_32K_TICKS_PER_SEC)

#define aee_sram_printk pr_info

#define SPM_HW_CG_CHECK_MASK (0x7f)
#define SPM_HW_CG_CHECK_SHIFT (12)


const char *wakesrc_str[32] = {
	[0] = " R12_PCM_TIMER",
	[1] = " R12_TWAM_PMSR_DVFSRC_IRQ",
	[2] = " R12_KP_IRQ_B",
	[3] = " R12_APWDT_EVENT_B",
	[4] = " R12_APXGPT1_EVENT_B",
	[5] = " R12_CONN2AP_SPM_WAKEUP_B",
	[6] = " R12_EINT_EVENT_B",
	[7] = " R12_CONN_WDT_IRQ_B",
	[8] = " R12_CCIF0_EVENT_B",
	[9] = " R12_LOWBATTERY_IRQ_B",
	[10] = " R12_SC_SSPM2SPM_WAKEUP_B",
	[11] = " R12_SC_SCP2SPM_WAKEUP_B",
	[12] = " R12_SC_ADSP2SPM_WAKEUP_B",
	[13] = " R12_PCM_WDT_WAKEUP_B",
	[14] = " R12_USB_CDSC_B",
	[15] = " R12_USB_POWERDWN_B",
	[16] = " R12_UART_EVENT",
	[17] = " R12_RESERVED_BIT",
	[18] = " R12_SYSTIMER",
	[19] = " R12_EINT_SECURED",
	[20] = " R12_AFE_IRQ_MCU_B",
	[21] = " R12_THERM_CTRL_EVENT_B",
	[22] = " R12_SYS_CIRQ_IRQ_B",
	[23] = " R12_MD2AP_PEER_EVENT_B",
	[24] = " R12_CSYSPWREQ_B",
	[25] = " R12_MD1_WDT_B",
	[26] = " R12_AP2AP_PEER_WAKEUPEVENT_B",
	[27] = " R12_SEJ_EVENT_B",
	[28] = " R12_SPM_CPU_WAKEUPEVENT_B",
	[29] = " R12_APUSYS",
	[30] = " R12_PCIE",
	[31] = " R12_MSDC",
};

static char *pwr_ctrl_str[PW_MAX_COUNT] = {
	[PW_PCM_FLAGS] = "pcm_flags",
	[PW_PCM_FLAGS_CUST] = "pcm_flags_cust",
	[PW_PCM_FLAGS_CUST_SET] = "pcm_flags_cust_set",
	[PW_PCM_FLAGS_CUST_CLR] = "pcm_flags_cust_clr",
	[PW_PCM_FLAGS1] = "pcm_flags1",
	[PW_PCM_FLAGS1_CUST] = "pcm_flags1_cust",
	[PW_PCM_FLAGS1_CUST_SET] = "pcm_flags1_cust_set",
	[PW_PCM_FLAGS1_CUST_CLR] = "pcm_flags1_cust_clr",
	[PW_TIMER_VAL] = "timer_val",
	[PW_TIMER_VAL_CUST] = "timer_val_cust",
	[PW_TIMER_VAL_RAMP_EN] = "timer_val_ramp_en",
	[PW_TIMER_VAL_RAMP_EN_SEC] = "timer_val_ramp_en_sec",
	[PW_WAKE_SRC] = "wake_src",
	[PW_WAKE_SRC_CUST] = "wake_src_cust",
	[PW_WAKELOCK_TIMER_VAL] = "wakelock_timer_val",
	[PW_WDT_DISABLE] = "wdt_disable",

	/* SPM_SRC_REQ */
	[PW_REG_SPM_ADSP_MAILBOX_REQ] = "reg_spm_adsp_mailbox_req",
	[PW_REG_SPM_APSRC_REQ] = "reg_spm_apsrc_req",
	[PW_REG_SPM_DDREN_REQ] = "reg_spm_ddren_req",
	[PW_REG_SPM_DVFS_REQ] = "reg_spm_dvfs_req",
	[PW_REG_SPM_EMI_REQ] = "reg_spm_emi_req",
	[PW_REG_SPM_F26M_REQ] = "reg_spm_f26m_req",
	[PW_REG_SPM_INFRA_REQ] = "reg_spm_infra_req",
	[PW_REG_SPM_PMIC_REQ] = "reg_spm_pmic_req",
	[PW_REG_SPM_SCP_MAILBOX_REQ] = "reg_spm_scp_mailbox_req",
	[PW_REG_SPM_SSPM_MAILBOX_REQ] = "reg_spm_sspm_mailbox_req",
	[PW_REG_SPM_SW_MAILBOX_REQ] = "reg_spm_sw_mailbox_req",
	[PW_REG_SPM_VCORE_REQ] = "reg_spm_vcore_req",
	[PW_REG_SPM_VRF18_REQ] = "reg_spm_vrf18_req",

	/* SPM_SRC_MASK_0 */
	[PW_REG_AFE_APSRC_REQ_MASK_B] = "reg_afe_apsrc_req_mask_b",
	[PW_REG_AFE_DDREN_REQ_MASK_B] = "reg_afe_ddren_req_mask_b",
	[PW_REG_AFE_EMI_REQ_MASK_B] = "reg_afe_emi_req_mask_b",
	[PW_REG_AFE_INFRA_REQ_MASK_B] = "reg_afe_infra_req_mask_b",
	[PW_REG_AFE_PMIC_REQ_MASK_B] = "reg_afe_pmic_req_mask_b",
	[PW_REG_AFE_SRCCLKENA_MASK_B] = "reg_afe_srcclkena_mask_b",
	[PW_REG_AFE_VCORE_REQ_MASK_B] = "reg_afe_vcore_req_mask_b",
	[PW_REG_AFE_VRF18_REQ_MASK_B] = "reg_afe_vrf18_req_mask_b",
	[PW_REG_APU_APSRC_REQ_MASK_B] = "reg_apu_apsrc_req_mask_b",
	[PW_REG_APU_DDREN_REQ_MASK_B] = "reg_apu_ddren_req_mask_b",
	[PW_REG_APU_EMI_REQ_MASK_B] = "reg_apu_emi_req_mask_b",
	[PW_REG_APU_INFRA_REQ_MASK_B] = "reg_apu_infra_req_mask_b",
	[PW_REG_APU_PMIC_REQ_MASK_B] = "reg_apu_pmic_req_mask_b",
	[PW_REG_APU_SRCCLKENA_MASK_B] = "reg_apu_srcclkena_mask_b",
	[PW_REG_APU_VRF18_REQ_MASK_B] = "reg_apu_vrf18_req_mask_b",
	[PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B] = "reg_audio_dsp_apsrc_req_mask_b",
	[PW_REG_AUDIO_DSP_DDREN_REQ_MASK_B] = "reg_audio_dsp_ddren_req_mask_b",
	[PW_REG_AUDIO_DSP_EMI_REQ_MASK_B] = "reg_audio_dsp_emi_req_mask_b",
	[PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B] = "reg_audio_dsp_infra_req_mask_b",
	[PW_REG_AUDIO_DSP_PMIC_REQ_MASK_B] = "reg_audio_dsp_pmic_req_mask_b",
	[PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B] = "reg_audio_dsp_srcclkena_mask_b",
	[PW_REG_AUDIO_DSP_VCORE_REQ_MASK_B] = "reg_audio_dsp_vcore_req_mask_b",
	[PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B] = "reg_audio_dsp_vrf18_req_mask_b",
	[PW_REG_CAM_APSRC_REQ_MASK_B] = "reg_cam_apsrc_req_mask_b",
	[PW_REG_CAM_DDREN_REQ_MASK_B] = "reg_cam_ddren_req_mask_b",
	[PW_REG_CAM_EMI_REQ_MASK_B] = "reg_cam_emi_req_mask_b",

	/* SPM_SRC_MASK_1 */
	[PW_REG_CCIF_APSRC_REQ_MASK_B] = "reg_ccif_apsrc_req_mask_b",
	[PW_REG_CCIF_EMI_REQ_MASK_B] = "reg_ccif_emi_req_mask_b",

	/* SPM_SRC_MASK_2 */
	[PW_REG_CCIF_INFRA_REQ_MASK_B] = "reg_ccif_infra_req_mask_b",
	[PW_REG_CCIF_PMIC_REQ_MASK_B] = "reg_ccif_pmic_req_mask_b",

	/* SPM_SRC_MASK_3 */
	[PW_REG_CCIF_SRCCLKENA_MASK_B] = "reg_ccif_srcclkena_mask_b",
	[PW_REG_CG_CHECK_APSRC_REQ_MASK_B] = "reg_cg_check_apsrc_req_mask_b",
	[PW_REG_CG_CHECK_DDREN_REQ_MASK_B] = "reg_cg_check_ddren_req_mask_b",
	[PW_REG_CG_CHECK_EMI_REQ_MASK_B] = "reg_cg_check_emi_req_mask_b",
	[PW_REG_CG_CHECK_PMIC_REQ_MASK_B] = "reg_cg_check_pmic_req_mask_b",
	[PW_REG_CG_CHECK_SRCCLKENA_MASK_B] = "reg_cg_check_srcclkena_mask_b",
	[PW_REG_CG_CHECK_VCORE_REQ_MASK_B] = "reg_cg_check_vcore_req_mask_b",
	[PW_REG_CG_CHECK_VRF18_REQ_MASK_B] = "reg_cg_check_vrf18_req_mask_b",
	[PW_REG_CONN_APSRC_REQ_MASK_B] = "reg_conn_apsrc_req_mask_b",
	[PW_REG_CONN_DDREN_REQ_MASK_B] = "reg_conn_ddren_req_mask_b",
	[PW_REG_CONN_EMI_REQ_MASK_B] = "reg_conn_emi_req_mask_b",
	[PW_REG_CONN_INFRA_REQ_MASK_B] = "reg_conn_infra_req_mask_b",
	[PW_REG_CONN_PMIC_REQ_MASK_B] = "reg_conn_pmic_req_mask_b",
	[PW_REG_CONN_SRCCLKENA_MASK_B] = "reg_conn_srcclkena_mask_b",
	[PW_REG_CONN_SRCCLKENB_MASK_B] = "reg_conn_srcclkenb_mask_b",
	[PW_REG_CONN_VCORE_REQ_MASK_B] = "reg_conn_vcore_req_mask_b",
	[PW_REG_CONN_VRF18_REQ_MASK_B] = "reg_conn_vrf18_req_mask_b",
	[PW_REG_MCUPM_APSRC_REQ_MASK_B] = "reg_mcupm_apsrc_req_mask_b",
	[PW_REG_MCUPM_DDREN_REQ_MASK_B] = "reg_mcupm_ddren_req_mask_b",
	[PW_REG_MCUPM_EMI_REQ_MASK_B] = "reg_mcupm_emi_req_mask_b",
	[PW_REG_MCUPM_INFRA_REQ_MASK_B] = "reg_mcupm_infra_req_mask_b",

	/* SPM_SRC_MASK_4 */
	[PW_REG_MCUPM_PMIC_REQ_MASK_B] = "reg_mcupm_pmic_req_mask_b",
	[PW_REG_MCUPM_SRCCLKENA_MASK_B] = "reg_mcupm_srcclkena_mask_b",
	[PW_REG_MCUPM_VRF18_REQ_MASK_B] = "reg_mcupm_vrf18_req_mask_b",
	[PW_REG_DISP0_APSRC_REQ_MASK_B] = "reg_disp0_apsrc_req_mask_b",
	[PW_REG_DISP0_DDREN_REQ_MASK_B] = "reg_disp0_ddren_req_mask_b",
	[PW_REG_DISP0_EMI_REQ_MASK_B] = "reg_disp0_emi_req_mask_b",
	[PW_REG_DISP1_APSRC_REQ_MASK_B] = "reg_disp1_apsrc_req_mask_b",
	[PW_REG_DISP1_DDREN_REQ_MASK_B] = "reg_disp1_ddren_req_mask_b",
	[PW_REG_DISP1_EMI_REQ_MASK_B] = "reg_disp1_emi_req_mask_b",
	[PW_REG_DPM_APSRC_REQ_MASK_B] = "reg_dpm_apsrc_req_mask_b",
	[PW_REG_DPM_EMI_REQ_MASK_B] = "reg_dpm_emi_req_mask_b",
	[PW_REG_DPM_INFRA_REQ_MASK_B] = "reg_dpm_infra_req_mask_b",
	[PW_REG_DPM_VRF18_REQ_MASK_B] = "reg_dpm_vrf18_req_mask_b",
	[PW_REG_DPMAIF_APSRC_REQ_MASK_B] = "reg_dpmaif_apsrc_req_mask_b",
	[PW_REG_DPMAIF_DDREN_REQ_MASK_B] = "reg_dpmaif_ddren_req_mask_b",
	[PW_REG_DPMAIF_EMI_REQ_MASK_B] = "reg_dpmaif_emi_req_mask_b",
	[PW_REG_DPMAIF_INFRA_REQ_MASK_B] = "reg_dpmaif_infra_req_mask_b",
	[PW_REG_DPMAIF_PMIC_REQ_MASK_B] = "reg_dpmaif_pmic_req_mask_b",
	[PW_REG_DPMAIF_SRCCLKENA_MASK_B] = "reg_dpmaif_srcclkena_mask_b",
	[PW_REG_DPMAIF_VRF18_REQ_MASK_B] = "reg_dpmaif_vrf18_req_mask_b",

	/* SPM_SRC_MASK_5 */
	[PW_REG_DVFSRC_LEVEL_REQ_MASK_B] = "reg_dvfsrc_level_req_mask_b",
	[PW_REG_GCE_APSRC_REQ_MASK_B] = "reg_gce_apsrc_req_mask_b",
	[PW_REG_GCE_DDREN_REQ_MASK_B] = "reg_gce_ddren_req_mask_b",
	[PW_REG_GCE_EMI_REQ_MASK_B] = "reg_gce_emi_req_mask_b",
	[PW_REG_GCE_INFRA_REQ_MASK_B] = "reg_gce_infra_req_mask_b",
	[PW_REG_GCE_VRF18_REQ_MASK_B] = "reg_gce_vrf18_req_mask_b",
	[PW_REG_GPUEB_APSRC_REQ_MASK_B] = "reg_gpueb_apsrc_req_mask_b",
	[PW_REG_GPUEB_DDREN_REQ_MASK_B] = "reg_gpueb_ddren_req_mask_b",
	[PW_REG_GPUEB_EMI_REQ_MASK_B] = "reg_gpueb_emi_req_mask_b",
	[PW_REG_GPUEB_INFRA_REQ_MASK_B] = "reg_gpueb_infra_req_mask_b",
	[PW_REG_GPUEB_PMIC_REQ_MASK_B] = "reg_gpueb_pmic_req_mask_b",
	[PW_REG_GPUEB_SRCCLKENA_MASK_B] = "reg_gpueb_srcclkena_mask_b",
	[PW_REG_GPUEB_VRF18_REQ_MASK_B] = "reg_gpueb_vrf18_req_mask_b",
	[PW_REG_IMG_APSRC_REQ_MASK_B] = "reg_img_apsrc_req_mask_b",
	[PW_REG_IMG_DDREN_REQ_MASK_B] = "reg_img_ddren_req_mask_b",
	[PW_REG_IMG_EMI_REQ_MASK_B] = "reg_img_emi_req_mask_b",
	[PW_REG_INFRASYS_APSRC_REQ_MASK_B] = "reg_infrasys_apsrc_req_mask_b",
	[PW_REG_INFRASYS_DDREN_REQ_MASK_B] = "reg_infrasys_ddren_req_mask_b",
	[PW_REG_INFRASYS_EMI_REQ_MASK_B] = "reg_infrasys_emi_req_mask_b",
	[PW_REG_EMISYS_APSRC_REQ_MASK_B] = "reg_emisys_apsrc_req_mask_b",
	[PW_REG_EMISYS_DDREN_REQ_MASK_B] = "reg_emisys_ddren_req_mask_b",
	[PW_REG_EMISYS_EMI_REQ_MASK_B] = "reg_emisys_emi_req_mask_b",
	[PW_REG_IPIC_INFRA_REQ_MASK_B] = "reg_ipic_infra_req_mask_b",
	[PW_REG_IPIC_VRF18_REQ_MASK_B] = "reg_ipic_vrf18_req_mask_b",
	[PW_REG_MCUSYS_APSRC_REQ_MASK_B] = "reg_mcusys_apsrc_req_mask_b",

	/* SPM_SRC_MASK_6 */
	[PW_REG_MCUSYS_DDREN_REQ_MASK_B] = "reg_mcusys_ddren_req_mask_b",
	[PW_REG_MCUSYS_EMI_REQ_MASK_B] = "reg_mcusys_emi_req_mask_b",
	[PW_REG_MD_APSRC_REQ_MASK_B] = "reg_md_apsrc_req_mask_b",
	[PW_REG_MD_DDREN_REQ_MASK_B] = "reg_md_ddren_req_mask_b",
	[PW_REG_MD_EMI_REQ_MASK_B] = "reg_md_emi_req_mask_b",
	[PW_REG_MD_INFRA_REQ_MASK_B] = "reg_md_infra_req_mask_b",
	[PW_REG_MD_PMIC_REQ_MASK_B] = "reg_md_pmic_req_mask_b",
	[PW_REG_MD_SRCCLKENA_MASK_B] = "reg_md_srcclkena_mask_b",
	[PW_REG_MD_SRCCLKENA1_MASK_B] = "reg_md_srcclkena1_mask_b",
	[PW_REG_MD_VCORE_REQ_MASK_B] = "reg_md_vcore_req_mask_b",
	[PW_REG_MD_VRF18_REQ_MASK_B] = "reg_md_vrf18_req_mask_b",
	[PW_REG_MDP0_APSRC_REQ_MASK_B] = "reg_mdp0_apsrc_req_mask_b",
	[PW_REG_MDP0_DDREN_REQ_MASK_B] = "reg_mdp0_ddren_req_mask_b",
	[PW_REG_MDP0_EMI_REQ_MASK_B] = "reg_mdp0_emi_req_mask_b",
	[PW_REG_MDP1_APSRC_REQ_MASK_B] = "reg_mdp1_apsrc_req_mask_b",
	[PW_REG_MDP1_DDREN_REQ_MASK_B] = "reg_mdp1_ddren_req_mask_b",
	[PW_REG_MDP1_EMI_REQ_MASK_B] = "reg_mdp1_emi_req_mask_b",
	[PW_REG_MM_PROC_APSRC_REQ_MASK_B] = "reg_mm_proc_apsrc_req_mask_b",

	/* SPM_SRC_MASK_7 */
	[PW_REG_MM_PROC_DDREN_REQ_MASK_B] = "reg_mm_proc_ddren_req_mask_b",
	[PW_REG_MM_PROC_EMI_REQ_MASK_B] = "reg_mm_proc_emi_req_mask_b",
	[PW_REG_MM_PROC_INFRA_REQ_MASK_B] = "reg_mm_proc_infra_req_mask_b",
	[PW_REG_MM_PROC_PMIC_REQ_MASK_B] = "reg_mm_proc_pmic_req_mask_b",
	[PW_REG_MM_PROC_SRCCLKENA_MASK_B] = "reg_mm_proc_srcclkena_mask_b",
	[PW_REG_MM_PROC_VRF18_REQ_MASK_B] = "reg_mm_proc_vrf18_req_mask_b",
	[PW_REG_MSDC1_APSRC_REQ_MASK_B] = "reg_msdc1_apsrc_req_mask_b",
	[PW_REG_MSDC1_DDREN_REQ_MASK_B] = "reg_msdc1_ddren_req_mask_b",
	[PW_REG_MSDC1_EMI_REQ_MASK_B] = "reg_msdc1_emi_req_mask_b",
	[PW_REG_MSDC1_INFRA_REQ_MASK_B] = "reg_msdc1_infra_req_mask_b",
	[PW_REG_MSDC1_PMIC_REQ_MASK_B] = "reg_msdc1_pmic_req_mask_b",
	[PW_REG_MSDC1_SRCCLKENA_MASK_B] = "reg_msdc1_srcclkena_mask_b",
	[PW_REG_MSDC1_VRF18_REQ_MASK_B] = "reg_msdc1_vrf18_req_mask_b",
	[PW_REG_MSDC2_APSRC_REQ_MASK_B] = "reg_msdc2_apsrc_req_mask_b",
	[PW_REG_MSDC2_DDREN_REQ_MASK_B] = "reg_msdc2_ddren_req_mask_b",
	[PW_REG_MSDC2_EMI_REQ_MASK_B] = "reg_msdc2_emi_req_mask_b",
	[PW_REG_MSDC2_INFRA_REQ_MASK_B] = "reg_msdc2_infra_req_mask_b",
	[PW_REG_MSDC2_PMIC_REQ_MASK_B] = "reg_msdc2_pmic_req_mask_b",
	[PW_REG_MSDC2_SRCCLKENA_MASK_B] = "reg_msdc2_srcclkena_mask_b",
	[PW_REG_MSDC2_VRF18_REQ_MASK_B] = "reg_msdc2_vrf18_req_mask_b",
	[PW_REG_OVL0_APSRC_REQ_MASK_B] = "reg_ovl0_apsrc_req_mask_b",
	[PW_REG_OVL0_DDREN_REQ_MASK_B] = "reg_ovl0_ddren_req_mask_b",
	[PW_REG_OVL0_EMI_REQ_MASK_B] = "reg_ovl0_emi_req_mask_b",
	[PW_REG_OVL1_APSRC_REQ_MASK_B] = "reg_ovl1_apsrc_req_mask_b",
	[PW_REG_OVL1_DDREN_REQ_MASK_B] = "reg_ovl1_ddren_req_mask_b",
	[PW_REG_OVL1_EMI_REQ_MASK_B] = "reg_ovl1_emi_req_mask_b",
	[PW_REG_PCIE0_APSRC_REQ_MASK_B] = "reg_pcie0_apsrc_req_mask_b",
	[PW_REG_PCIE0_DDREN_REQ_MASK_B] = "reg_pcie0_ddren_req_mask_b",
	[PW_REG_PCIE0_EMI_REQ_MASK_B] = "reg_pcie0_emi_req_mask_b",
	[PW_REG_PCIE0_INFRA_REQ_MASK_B] = "reg_pcie0_infra_req_mask_b",
	[PW_REG_PCIE0_PMIC_REQ_MASK_B] = "reg_pcie0_pmic_req_mask_b",
	[PW_REG_PCIE0_SRCCLKENA_MASK_B] = "reg_pcie0_srcclkena_mask_b",

	/* SPM_SRC_MASK_8 */
	[PW_REG_PCIE0_VCORE_REQ_MASK_B] = "reg_pcie0_vcore_req_mask_b",
	[PW_REG_PCIE0_VRF18_REQ_MASK_B] = "reg_pcie0_vrf18_req_mask_b",
	[PW_REG_PCIE1_APSRC_REQ_MASK_B] = "reg_pcie1_apsrc_req_mask_b",
	[PW_REG_PCIE1_DDREN_REQ_MASK_B] = "reg_pcie1_ddren_req_mask_b",
	[PW_REG_PCIE1_EMI_REQ_MASK_B] = "reg_pcie1_emi_req_mask_b",
	[PW_REG_PCIE1_INFRA_REQ_MASK_B] = "reg_pcie1_infra_req_mask_b",
	[PW_REG_PCIE1_PMIC_REQ_MASK_B] = "reg_pcie1_pmic_req_mask_b",
	[PW_REG_PCIE1_SRCCLKENA_MASK_B] = "reg_pcie1_srcclkena_mask_b",
	[PW_REG_PCIE1_VCORE_REQ_MASK_B] = "reg_pcie1_vcore_req_mask_b",
	[PW_REG_PCIE1_VRF18_REQ_MASK_B] = "reg_pcie1_vrf18_req_mask_b",
	[PW_REG_SCP_APSRC_REQ_MASK_B] = "reg_scp_apsrc_req_mask_b",
	[PW_REG_SCP_DDREN_REQ_MASK_B] = "reg_scp_ddren_req_mask_b",
	[PW_REG_SCP_EMI_REQ_MASK_B] = "reg_scp_emi_req_mask_b",
	[PW_REG_SCP_INFRA_REQ_MASK_B] = "reg_scp_infra_req_mask_b",
	[PW_REG_SCP_PMIC_REQ_MASK_B] = "reg_scp_pmic_req_mask_b",
	[PW_REG_SCP_SRCCLKENA_MASK_B] = "reg_scp_srcclkena_mask_b",
	[PW_REG_SCP_VCORE_REQ_MASK_B] = "reg_scp_vcore_req_mask_b",
	[PW_REG_SCP_VRF18_REQ_MASK_B] = "reg_scp_vrf18_req_mask_b",
	[PW_REG_SRCCLKENI_INFRA_REQ_MASK_B] = "reg_srcclkeni_infra_req_mask_b",
	[PW_REG_SRCCLKENI_PMIC_REQ_MASK_B] = "reg_srcclkeni_pmic_req_mask_b",
	[PW_REG_SRCCLKENI_SRCCLKENA_MASK_B] = "reg_srcclkeni_srcclkena_mask_b",
	[PW_REG_SSPM_APSRC_REQ_MASK_B] = "reg_sspm_apsrc_req_mask_b",
	[PW_REG_SSPM_DDREN_REQ_MASK_B] = "reg_sspm_ddren_req_mask_b",
	[PW_REG_SSPM_EMI_REQ_MASK_B] = "reg_sspm_emi_req_mask_b",
	[PW_REG_SSPM_INFRA_REQ_MASK_B] = "reg_sspm_infra_req_mask_b",
	[PW_REG_SSPM_PMIC_REQ_MASK_B] = "reg_sspm_pmic_req_mask_b",
	[PW_REG_SSPM_SRCCLKENA_MASK_B] = "reg_sspm_srcclkena_mask_b",
	[PW_REG_SSPM_VRF18_REQ_MASK_B] = "reg_sspm_vrf18_req_mask_b",
	[PW_REG_SSR_INFRA_REQ_MASK_B] = "reg_ssr_infra_req_mask_b",

	/* SPM_SRC_MASK_9 */
	[PW_REG_SSR_PMIC_REQ_MASK_B] = "reg_ssr_pmic_req_mask_b",
	[PW_REG_SSR_SRCCLKENA_MASK_B] = "reg_ssr_srcclkena_mask_b",
	[PW_REG_SSR_VRF18_REQ_MASK_B] = "reg_ssr_vrf18_req_mask_b",
	[PW_REG_SSUSB0_APSRC_REQ_MASK_B] = "reg_ssusb0_apsrc_req_mask_b",
	[PW_REG_SSUSB0_DDREN_REQ_MASK_B] = "reg_ssusb0_ddren_req_mask_b",
	[PW_REG_SSUSB0_EMI_REQ_MASK_B] = "reg_ssusb0_emi_req_mask_b",
	[PW_REG_SSUSB0_INFRA_REQ_MASK_B] = "reg_ssusb0_infra_req_mask_b",
	[PW_REG_SSUSB0_PMIC_REQ_MASK_B] = "reg_ssusb0_pmic_req_mask_b",
	[PW_REG_SSUSB0_SRCCLKENA_MASK_B] = "reg_ssusb0_srcclkena_mask_b",
	[PW_REG_SSUSB0_VRF18_REQ_MASK_B] = "reg_ssusb0_vrf18_req_mask_b",
	[PW_REG_SSUSB1_APSRC_REQ_MASK_B] = "reg_ssusb1_apsrc_req_mask_b",
	[PW_REG_SSUSB1_DDREN_REQ_MASK_B] = "reg_ssusb1_ddren_req_mask_b",
	[PW_REG_SSUSB1_EMI_REQ_MASK_B] = "reg_ssusb1_emi_req_mask_b",
	[PW_REG_SSUSB1_INFRA_REQ_MASK_B] = "reg_ssusb1_infra_req_mask_b",
	[PW_REG_SSUSB1_PMIC_REQ_MASK_B] = "reg_ssusb1_pmic_req_mask_b",
	[PW_REG_SSUSB1_SRCCLKENA_MASK_B] = "reg_ssusb1_srcclkena_mask_b",
	[PW_REG_SSUSB1_VRF18_REQ_MASK_B] = "reg_ssusb1_vrf18_req_mask_b",
	[PW_REG_UART_HUB_INFRA_REQ_MASK_B] = "reg_uart_hub_infra_req_mask_b",
	[PW_REG_UART_HUB_PMIC_REQ_MASK_B] = "reg_uart_hub_pmic_req_mask_b",
	[PW_REG_UART_HUB_SRCCLKENA_MASK_B] = "reg_uart_hub_srcclkena_mask_b",
	[PW_REG_UART_HUB_VCORE_REQ_MASK_B] = "reg_uart_hub_vcore_req_mask_b",
	[PW_REG_UART_HUB_VRF18_REQ_MASK_B] = "reg_uart_hub_vrf18_req_mask_b",
	[PW_REG_UFS_APSRC_REQ_MASK_B] = "reg_ufs_apsrc_req_mask_b",
	[PW_REG_UFS_DDREN_REQ_MASK_B] = "reg_ufs_ddren_req_mask_b",
	[PW_REG_UFS_EMI_REQ_MASK_B] = "reg_ufs_emi_req_mask_b",
	[PW_REG_UFS_INFRA_REQ_MASK_B] = "reg_ufs_infra_req_mask_b",
	[PW_REG_UFS_PMIC_REQ_MASK_B] = "reg_ufs_pmic_req_mask_b",
	[PW_REG_UFS_SRCCLKENA_MASK_B] = "reg_ufs_srcclkena_mask_b",
	[PW_REG_UFS_VRF18_REQ_MASK_B] = "reg_ufs_vrf18_req_mask_b",
	[PW_REG_VDEC_APSRC_REQ_MASK_B] = "reg_vdec_apsrc_req_mask_b",
	[PW_REG_VDEC_DDREN_REQ_MASK_B] = "reg_vdec_ddren_req_mask_b",
	[PW_REG_VDEC_EMI_REQ_MASK_B] = "reg_vdec_emi_req_mask_b",

	/* SPM_SRC_MASK_10 */
	[PW_REG_VENC_APSRC_REQ_MASK_B] = "reg_venc_apsrc_req_mask_b",
	[PW_REG_VENC_DDREN_REQ_MASK_B] = "reg_venc_ddren_req_mask_b",
	[PW_REG_VENC_EMI_REQ_MASK_B] = "reg_venc_emi_req_mask_b",
	[PW_REG_VENC1_APSRC_REQ_MASK_B] = "reg_venc1_apsrc_req_mask_b",
	[PW_REG_VENC1_DDREN_REQ_MASK_B] = "reg_venc1_ddren_req_mask_b",
	[PW_REG_VENC1_EMI_REQ_MASK_B] = "reg_venc1_emi_req_mask_b",
	[PW_REG_VENC2_APSRC_REQ_MASK_B] = "reg_venc2_apsrc_req_mask_b",
	[PW_REG_VENC2_DDREN_REQ_MASK_B] = "reg_venc2_ddren_req_mask_b",
	[PW_REG_VENC2_EMI_REQ_MASK_B] = "reg_venc2_emi_req_mask_b",
	[PW_REG_MCU_APSRC_REQ_MASK_B] = "reg_mcu_apsrc_req_mask_b",
	[PW_REG_MCU_DDREN_REQ_MASK_B] = "reg_mcu_ddren_req_mask_b",
	[PW_REG_MCU_EMI_REQ_MASK_B] = "reg_mcu_emi_req_mask_b",
	/* SPM_EVENT_CON_MISC */
	[PW_REG_SRCCLKEN_FAST_RESP] = "reg_srcclken_fast_resp",
	[PW_REG_CSYSPWRUP_ACK_MASK] = "reg_csyspwrup_ack_mask",

	/* SPM_WAKEUP_EVENT_MASK */
	[PW_REG_WAKEUP_EVENT_MASK] = "reg_wakeup_event_mask",
	/* SPM_WAKEUP_EVENT_EXT_MASK */
	[PW_REG_EXT_WAKEUP_EVENT_MASK] = "reg_ext_wakeup_event_mask",
};

struct subsys_req plat_subsys_req[SUBSYS_REQ_MAX] = {
        {SPM_REQ_STA_4, 0x1F, 0, 0},
        {SPM_REQ_STA_2, 0xF, SPM_REQ_STA_1, (0x7 << 29)},
        {SPM_REQ_STA_5, (0x1F << 3), 0, 0},
        {SPM_REQ_STA_0, (0x1F << 10), 0, 0},
        {SPM_REQ_STA_5, (0x1F << 27), 0, 0},
        {SPM_REQ_STA_4, (0x3FF << 15), 0, 0},
        {SPM_REQ_STA_2, (0xF << 9), 0, 0},
        {SPM_REQ_STA_0, (0x1F << 5), 0, 0},
        {SPM_SRC_REQ, 0x63E, 0, 0},
};

#define plat_mmio_read(offset)	__raw_readl(lpm_spm_base + offset)

u64 ap_pd_count;
u64 ap_slp_duration;
u64 spm_26M_off_count;
u64 spm_26M_off_duration;
u64 spm_vcore_off_count;
u64 spm_vcore_off_duration;
u32 before_ap_slp_duration;

struct logger_timer {
	struct lpm_timer tm;
	unsigned int fired;
};
#define	STATE_NUM	10
#define	STATE_NAME_SIZE	15
struct logger_fired_info {
	unsigned int fired;
	unsigned int state_index;
	char state_name[STATE_NUM][STATE_NAME_SIZE];
	int fired_index;
};

static struct lpm_spm_wake_status wakesrc;

static struct lpm_log_helper log_help = {
	.wakesrc = &wakesrc,
	.cur = 0,
	.prev = 0,
};

static int lpm_get_wakeup_status(void)
{
	struct lpm_log_helper *help = &log_help;

	if (!help->wakesrc || !lpm_spm_base)
		return -EINVAL;

	help->wakesrc->r12 = plat_mmio_read(SPM_BK_WAKE_EVENT);
	help->wakesrc->r12_ext = plat_mmio_read(SPM_WAKEUP_STA);
	help->wakesrc->raw_sta = plat_mmio_read(SPM_WAKEUP_STA);
	help->wakesrc->raw_ext_sta = plat_mmio_read(SPM_WAKEUP_EXT_STA);
	help->wakesrc->md32pcm_wakeup_sta = plat_mmio_read(MD32PCM_WAKEUP_STA);
	help->wakesrc->md32pcm_event_sta = plat_mmio_read(MD32PCM_EVENT_STA);

	help->wakesrc->src_req = plat_mmio_read(SPM_SRC_REQ);

	/* backup of SPM_WAKEUP_MISC */
	help->wakesrc->wake_misc = plat_mmio_read(SPM_BK_WAKE_MISC);

	/* get sleep time */
	/* backup of PCM_TIMER_OUT */
	help->wakesrc->timer_out = plat_mmio_read(SPM_BK_PCM_TIMER);

	/* get other SYS and co-clock status */
	help->wakesrc->r13 = plat_mmio_read(MD32PCM_SCU_STA0);
	help->wakesrc->req_sta0 = plat_mmio_read(SPM_REQ_STA_0);
	help->wakesrc->req_sta1 = plat_mmio_read(SPM_REQ_STA_1);
	help->wakesrc->req_sta2 = plat_mmio_read(SPM_REQ_STA_2);
	help->wakesrc->req_sta3 = plat_mmio_read(SPM_REQ_STA_3);
	help->wakesrc->req_sta4 = plat_mmio_read(SPM_REQ_STA_4);
	help->wakesrc->req_sta5 = plat_mmio_read(SPM_REQ_STA_5);
	help->wakesrc->req_sta6 = plat_mmio_read(SPM_REQ_STA_6);
	help->wakesrc->req_sta7 = plat_mmio_read(SPM_REQ_STA_7);
	help->wakesrc->req_sta8 = plat_mmio_read(SPM_REQ_STA_8);
	help->wakesrc->req_sta9 = plat_mmio_read(SPM_REQ_STA_9);
	help->wakesrc->req_sta10 = plat_mmio_read(SPM_REQ_STA_10);

	/* get HW CG check status */
	help->wakesrc->cg_check_sta =
		((plat_mmio_read(SPM_REQ_STA_3) >> SPM_HW_CG_CHECK_SHIFT) & SPM_HW_CG_CHECK_MASK);

	/* get debug flag for PCM execution check */
	help->wakesrc->debug_flag = plat_mmio_read(PCM_WDT_LATCH_SPARE_0);
	help->wakesrc->debug_flag1 = plat_mmio_read(PCM_WDT_LATCH_SPARE_1);

	/* get backup SW flag status */
	help->wakesrc->b_sw_flag0 = plat_mmio_read(SPM_SW_RSV_7);
	help->wakesrc->b_sw_flag1 = plat_mmio_read(SPM_SW_RSV_8);

	help->wakesrc->rt_req_sta0 = plat_mmio_read(SPM_SW_RSV_2);
	help->wakesrc->rt_req_sta1 = plat_mmio_read(SPM_SW_RSV_3);
	help->wakesrc->rt_req_sta2 = plat_mmio_read(SPM_SW_RSV_4);
	help->wakesrc->rt_req_sta3 = plat_mmio_read(SPM_SW_RSV_5);
	help->wakesrc->rt_req_sta4 = plat_mmio_read(SPM_SW_RSV_6);
	/* get ISR status */
	help->wakesrc->isr = plat_mmio_read(SPM_IRQ_STA);

	/* get debug spare 5 && 6 */
	help->wakesrc->debug_spare5 = plat_mmio_read(PCM_WDT_LATCH_SPARE_5);
	help->wakesrc->debug_spare6 = plat_mmio_read(PCM_WDT_LATCH_SPARE_6);

	/* get SW flag status */
	help->wakesrc->sw_flag0 = plat_mmio_read(SPM_SW_FLAG_0);
	help->wakesrc->sw_flag1 = plat_mmio_read(SPM_SW_FLAG_1);

	/* get CLK SETTLE */
	help->wakesrc->clk_settle = plat_mmio_read(SPM_CLK_SETTLE);
	/* check abort */

	help->cur += 1;
	return 0;
}

static void dump_hw_cg_status(void)
{
#undef LOG_BUF_SIZE
#define LOG_BUF_SIZE	(128)
	char log_buf[LOG_BUF_SIZE] = { 0 };
	unsigned int log_size = 0;
	unsigned int hwcg_num, setting_num;
	unsigned int sta, setting;
	int i, j;

	hwcg_num = (unsigned int)lpm_smc_spm_dbg(MT_SPM_DBG_SMC_HWCG_NUM,
				MT_LPM_SMC_ACT_GET, 0, 0);

	setting_num = (unsigned int)lpm_smc_spm_dbg(MT_SPM_DBG_SMC_HWCG_NUM,
				MT_LPM_SMC_ACT_GET, 0, 1);

	log_size = scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size,
		"HWCG sta :");

	for (i = 0 ; i < hwcg_num; i++) {
		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				"[%d] ", i);
		for (j = 0 ; j < setting_num; j++) {
			sta =  (unsigned int)lpm_smc_spm_dbg(
					MT_SPM_DBG_SMC_HWCG_STATUS,
					MT_LPM_SMC_ACT_GET, i, j);

			setting = (unsigned int)lpm_smc_spm_dbg(
						MT_SPM_DBG_SMC_HWCG_SETTING,
						MT_LPM_SMC_ACT_GET, i, j);

			log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				"0x%x ", setting & sta);
		}
		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				i < hwcg_num - 1 ? "|" : ".");

	}
	WARN_ON(strlen(log_buf) >= LOG_BUF_SIZE);
	pr_info("[name:spm&][SPM] %s\n", log_buf);

}

static void dump_peri_cg_status(void)
{
#undef LOG_BUF_SIZE
#define LOG_BUF_SIZE	(128)
	char log_buf[LOG_BUF_SIZE] = { 0 };
	unsigned int log_size = 0;
	unsigned int peri_cg_num, setting_num;
	unsigned int sta, setting;
	int i, j;

	peri_cg_num = (unsigned int)lpm_smc_spm_dbg(MT_SPM_DBG_SMC_PERI_REQ_NUM,
				MT_LPM_SMC_ACT_GET, 0, 0);

	setting_num = (unsigned int)lpm_smc_spm_dbg(MT_SPM_DBG_SMC_PERI_REQ_NUM,
				MT_LPM_SMC_ACT_GET, 0, 1);

	log_size = scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size,
		"PERI_CG sta :");

	for (i = 0 ; i < peri_cg_num; i++) {
		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				"[%d] ", i);
		for (j = 0 ; j < setting_num; j++) {
			sta =  (unsigned int)lpm_smc_spm_dbg(
					MT_SPM_DBG_SMC_PERI_REQ_STATUS,
					MT_LPM_SMC_ACT_GET, i, j);

			setting = (unsigned int)lpm_smc_spm_dbg(
						MT_SPM_DBG_SMC_PERI_REQ_SETTING,
						MT_LPM_SMC_ACT_GET, i, j);

			log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				"0x%x ", setting & sta);
		}
		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_SIZE - log_size,
				i < peri_cg_num - 1 ? "|" : ".");

	}
	WARN_ON(strlen(log_buf) >= LOG_BUF_SIZE);
	pr_info("[name:spm&][SPM] %s\n", log_buf);

}

static void lpm_save_sleep_info(void)
{
}

static void suspend_show_detailed_wakeup_reason
	(struct lpm_spm_wake_status *wakesta)
{
}

static unsigned int is_lp_blocked_threshold;
static void suspend_spm_rsc_req_check
	(struct lpm_spm_wake_status *wakesta)
{
#undef LOG_BUF_SIZE
#define LOG_BUF_SIZE		        256
#undef AVOID_OVERFLOW
#define AVOID_OVERFLOW (0xF0000000)
static u32 is_blocked_cnt;
	char log_buf[LOG_BUF_SIZE] = { 0 };
	int log_size = 0;
	u32 is_no_blocked = 0;
	u32 req_sta_0, req_sta_1, req_sta_2;
	u32 req_sta_3, req_sta_4, req_sta_5;
	u32 req_sta_6, req_sta_7, req_sta_8;
	u32 req_sta_9, req_sta_10;
	u32 src_req;

	if (is_blocked_cnt >= AVOID_OVERFLOW)
		is_blocked_cnt = 0;

	/* Check if ever enter deepest System LPM */
	is_no_blocked = wakesta->debug_flag & 0x200;

	/* Check if System LPM ever is blocked over 10 times */
	if (!is_no_blocked)
		is_blocked_cnt++;
	else
		is_blocked_cnt = 0;

	if (is_blocked_cnt < is_lp_blocked_threshold)
		return;

	if (!lpm_spm_base)
		return;

	/* Show who is blocking system LPM */
	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size,
		"suspend warning:(OneShot) System LPM is blocked by ");

	req_sta_0 = plat_mmio_read(SPM_REQ_STA_0);
	req_sta_1 = plat_mmio_read(SPM_REQ_STA_1);
	req_sta_2 = plat_mmio_read(SPM_REQ_STA_2);
	req_sta_3 = plat_mmio_read(SPM_REQ_STA_3);
	req_sta_4 = plat_mmio_read(SPM_REQ_STA_4);
	req_sta_5 = plat_mmio_read(SPM_REQ_STA_5);
	req_sta_6 = plat_mmio_read(SPM_REQ_STA_6);
	req_sta_7 = plat_mmio_read(SPM_REQ_STA_7);
	req_sta_8 = plat_mmio_read(SPM_REQ_STA_8);
	req_sta_9 = plat_mmio_read(SPM_REQ_STA_9);
	req_sta_10 = plat_mmio_read(SPM_REQ_STA_10);
	if (req_sta_6 & (0x1FF << 16))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "md ");
	if (req_sta_3 & (0x1FF << 19))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "conn ");
	if (req_sta_4 & (0x3F << 3))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "disp ");

	if (req_sta_8 & (0xFF << 10))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "scp ");

	if (req_sta_0 & (0xFF << 15))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "adsp ");

	if (req_sta_9 & (0x7F << 22))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "ufs ");

	if (req_sta_7 & (0x3FFF << 6))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "msdc ");

	if (req_sta_0 & (0x3F << 8))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "apu ");

	if (req_sta_5 & (0x1F << 1))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "gce ");

	/* FIXME: add other request check? */

	src_req = plat_mmio_read(SPM_SRC_REQ);
	if (src_req & 0x18F6) {
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "spm ");
	}
	WARN_ON(strlen(log_buf) >= LOG_BUF_SIZE);
	pr_info("[name:spm&][SPM] %s\n", log_buf);
	dump_hw_cg_status();
	dump_peri_cg_status();
}

static int lpm_show_message(int type, const char *prefix, void *data)
{
	struct lpm_spm_wake_status *wakesrc = log_help.wakesrc;

#undef LOG_BUF_SIZE
	#define LOG_BUF_SIZE		256
	#define LOG_BUF_OUT_SZ		768
	#define IS_WAKE_MISC(x)	(wakesrc->wake_misc & x)
	#define IS_LOGBUF(ptr, newstr) \
		((strlen(ptr) + strlen(newstr)) < LOG_BUF_SIZE)

	unsigned int spm_26M_off_pct = 0;
	unsigned int spm_vcore_off_pct = 0;
	char buf[LOG_BUF_SIZE] = { 0 };
	char log_buf[LOG_BUF_OUT_SZ] = { 0 };
	char *local_ptr = NULL;
	int i = 0, log_size = 0, log_type = 0;
	unsigned int wr = WR_UNKNOWN;
	const char *scenario = prefix ?: "UNKNOWN";

	log_type = ((struct lpm_issuer *)data)->log_type;

	if (log_type == LOG_MCUSYS_NOT_OFF) {
		aee_sram_printk("[name:spm&][SPM] %s didn't enter mcusys off, mcusys off cnt is no update\n",
					scenario);
		wr =  WR_ABORT;

		goto end;
	}

	if (wakesrc->is_abort != 0) {
		/* add size check for vcoredvfs */
		aee_sram_printk("SPM ABORT (%s), r13 = 0x%x, ",
			scenario, wakesrc->r13);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"[SPM] ABORT (%s), r13 = 0x%x, ",
			scenario, wakesrc->r13);

		aee_sram_printk(" debug_flag = 0x%x 0x%x",
			wakesrc->debug_flag, wakesrc->debug_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" debug_flag = 0x%x 0x%x",
			wakesrc->debug_flag, wakesrc->debug_flag1);

		aee_sram_printk(" sw_flag = 0x%x 0x%x",
			wakesrc->sw_flag0, wakesrc->sw_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" sw_flag = 0x%x 0x%x\n",
			wakesrc->sw_flag0, wakesrc->sw_flag1);

		aee_sram_printk(" b_sw_flag = 0x%x 0x%x",
			wakesrc->b_sw_flag0, wakesrc->b_sw_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" b_sw_flag = 0x%x 0x%x",
			wakesrc->b_sw_flag0, wakesrc->b_sw_flag1);

		wr =  WR_ABORT;
	} else {
		if (wakesrc->r12 & R12_PCM_TIMER_B) {
			if (wakesrc->wake_misc & WAKE_MISC_PCM_TIMER_EVENT) {
				local_ptr = " PCM_TIMER";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_PCM_TIMER;
			}
		}

		if (wakesrc->r12 & R12_SPM_DEBUG_B) {
			if (IS_WAKE_MISC(WAKE_MISC_DVFSRC_IRQ)) {
				local_ptr = " DVFSRC";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_DVFSRC;
			}
			if (IS_WAKE_MISC(WAKE_MISC_TWAM_IRQ_B)) {
				local_ptr = " TWAM";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_TWAM;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMSR_IRQ_B_SET0)) {
				local_ptr = " PMSR";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_PMSR;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMSR_IRQ_B_SET1)) {
				local_ptr = " PMSR";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_PMSR;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_0)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_1)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_2)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_3)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_ALL)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SRCLKEN_RC_ERR_INT)) {
				local_ptr = " WAKE_MISC_SRCLKEN_RC_ERR_INT";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_VLP_BUS_TIMEOUT_IRQ)) {
				local_ptr = " WAKE_MISC_VLP_BUS_TIMEOUT_IRQ";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMIC_EINT_OUT)) {
				local_ptr = " WAKE_MISC_PMIC_EINT_OUT";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMIC_IRQ_ACK)) {
				local_ptr = " WAKE_MISC_PMIC_IRQ_ACK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMIC_SCP_IRQ)) {
				local_ptr = " WAKE_MISC_PMIC_SCP_IRQ";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
		}
		for (i = 1; i < 32; i++) {
			if (wakesrc->r12 & (1U << i)) {
				if (IS_LOGBUF(buf, wakesrc_str[i]))
					strncat(buf, wakesrc_str[i],
						strlen(wakesrc_str[i]));

				wr = WR_WAKE_SRC;
			}
		}
		WARN_ON(strlen(buf) >= LOG_BUF_SIZE);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"%s wake up by %s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x 0x%x, ",
			scenario, buf, wakesrc->timer_out, wakesrc->r13,
			wakesrc->debug_flag, wakesrc->debug_flag1);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"r12 = 0x%x, r12_ext = 0x%x, raw_sta = 0x%x 0x%x 0x%x, idle_sta = 0x%x, ",
			wakesrc->r12, wakesrc->r12_ext,
			wakesrc->raw_sta,
			wakesrc->md32pcm_wakeup_sta,
			wakesrc->md32pcm_event_sta,
			wakesrc->idle_sta);

		log_size += scnprintf(log_buf + log_size,
			  LOG_BUF_OUT_SZ - log_size,
			  "req_sta =  0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x 0x%x | 0x%x 0x%x 0x%x, ",
			  wakesrc->req_sta0, wakesrc->req_sta1, wakesrc->req_sta2,
			  wakesrc->req_sta3, wakesrc->req_sta4, wakesrc->req_sta5,
			  wakesrc->req_sta6, wakesrc->req_sta7, wakesrc->req_sta8,
			  wakesrc->req_sta9, wakesrc->req_sta10);

		log_size += scnprintf(log_buf + log_size,
			  LOG_BUF_OUT_SZ - log_size,
			  "cg_check_sta =0x%x, isr = 0x%x, rt_req_sta0 = 0x%x rt_req_sta1 = 0x%x rt_req_sta2 = 0x%x rt_req_sta3 = 0x%x dram_sw_con_3 = 0x%x, ",
			  wakesrc->cg_check_sta,
			  wakesrc->isr, wakesrc->rt_req_sta0,
			  wakesrc->rt_req_sta1, wakesrc->rt_req_sta2,
			  wakesrc->rt_req_sta3, wakesrc->rt_req_sta4);

		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				"raw_ext_sta = 0x%x, wake_misc = 0x%x, pcm_flag = 0x%x 0x%x 0x%x 0x%x, req = 0x%x, ",
				wakesrc->raw_ext_sta,
				wakesrc->wake_misc,
				wakesrc->sw_flag0,
				wakesrc->sw_flag1, wakesrc->b_sw_flag0,
				wakesrc->b_sw_flag1,
				wakesrc->src_req);

		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				"clk_settle = 0x%x, ", wakesrc->clk_settle);

		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				"debug_spare_5 = 0x%x, debug_spare_6 = 0x%x, ",
				wakesrc->debug_spare5, wakesrc->debug_spare6);

		if (type == LPM_ISSUER_SUSPEND && lpm_spm_base) {
			/* calculate 26M off percentage in suspend period */
			if (wakesrc->timer_out != 0) {
				spm_26M_off_pct =
					(100 * plat_mmio_read(SPM_BK_VTCXO_DUR))
							/ wakesrc->timer_out;
				spm_vcore_off_pct =
					(100 * plat_mmio_read(SPM_SW_RSV_4))
							/ wakesrc->timer_out;
			}
			log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				"wlk_cntcv_l = 0x%x, wlk_cntcv_h = 0x%x, 26M_off_pct = %d, vcore_off_pct = %d\n",
				plat_mmio_read(SYS_TIMER_VALUE_L),
				plat_mmio_read(SYS_TIMER_VALUE_H),
				spm_26M_off_pct, spm_vcore_off_pct);
		}
	}
	WARN_ON(log_size >= LOG_BUF_OUT_SZ);

	if (type == LPM_ISSUER_SUSPEND) {
		pr_info("[name:spm&][SPM] %s", log_buf);
		suspend_show_detailed_wakeup_reason(wakesrc);
		suspend_spm_rsc_req_check(wakesrc);
		pr_info("[name:spm&][SPM] Suspended for %d.%03d seconds",
			PCM_TICK_TO_SEC(wakesrc->timer_out),
			PCM_TICK_TO_SEC((wakesrc->timer_out %
				PCM_32K_TICKS_PER_SEC)
			* 1000));
		log_md_sleep_info();
		/* Eable rcu lock checking */
//		rcu_irq_exit_irqson();
	} else
		pr_info("[name:spm&][SPM] %s", log_buf);

end:
	return wr;
}


static struct lpm_dbg_plat_ops dbg_ops = {
	.lpm_show_message = lpm_show_message,
	.lpm_save_sleep_info = lpm_save_sleep_info,
	.lpm_get_spm_wakesrc_irq = NULL,
	.lpm_get_wakeup_status = lpm_get_wakeup_status,
};

int dbg_ops_register(void)
{
	int ret;

	ret = lpm_dbg_plat_ops_register(&dbg_ops);

	is_lp_blocked_threshold = lpm_get_lp_blocked_threshold();

	return ret;
}

static int __init mt6985_dbg_device_initcall(void)
{
	lpm_dbg_plat_ops_register(&dbg_ops);
	lpm_spm_fs_init(pwr_ctrl_str, PW_MAX_COUNT);

	return 0;
}

static int __init mt6985_dbg_late_initcall(void)
{
	lpm_trace_event_init(plat_subsys_req);

	return 0;
}

int __init mt6985_dbg_init(void)
{
	int ret = 0;

	ret = mt6985_dbg_device_initcall();
	if (ret)
		goto mt6985_dbg_init_fail;

	ret = mt6985_dbg_late_initcall();
	if (ret)
		goto mt6985_dbg_init_fail;

	return 0;

mt6985_dbg_init_fail:
	return -EAGAIN;
}

void __exit mt6985_dbg_exit(void)
{
	lpm_trace_event_deinit();
}

module_init(mt6985_dbg_init);
module_exit(mt6985_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mt6985 low power debug module");
MODULE_AUTHOR("MediaTek Inc.");
