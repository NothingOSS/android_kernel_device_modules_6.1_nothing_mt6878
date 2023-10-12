// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
/* #include <mt-plat/mtk_io.h> */
/* #include <mt-plat/sync_write.h> */
/* include <mt-plat/mtk_secure_api.h> */
#include "mt6878_dcm_internal.h"
#include "mtk_dcm.h"

#define DEBUGLINE dcm_pr_info("%s %d\n", __func__, __LINE__)

unsigned int init_dcm_type = ALL_DCM_TYPE;

#if defined(__KERNEL__) && IS_ENABLED(CONFIG_OF)
unsigned long dcm_mcusys_par_wrap_base;
unsigned long dcm_infracfg_ao_base;
unsigned long dcm_infra_ao_bcrm_base;
unsigned long dcm_peri_ao_bcrm_base;
unsigned long dcm_vlp_ao_bcrm_base;

#define DCM_NODE "mediatek,mt6878-dcm"

#endif /* #if defined(__KERNEL__) && IS_ENABLED(CONFIG_OF) */

/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function is 1-argu with ON/OFF/MODE option.
 *****************************************/
static int dcm_armcore(int on)
{
	dcm_mcusys_par_wrap_cpu_pll_div_0_dcm(on);
	dcm_mcusys_par_wrap_cpu_pll_div_1_dcm(on);
	return 0;
}

static int dcm_infra(int on)
{
	dcm_infra_ao_bcrm_infra_bus_dcm(on);
	dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm(on);
	dcm_infracfg_ao_aximem_bus_dcm(on);
	dcm_infracfg_ao_infra_bus_dcm(on);
	dcm_infracfg_ao_infra_rx_p2p_dcm(on);
	dcm_infracfg_ao_peri_bus_dcm(on);
	dcm_infracfg_ao_peri_module_dcm(on);
	return 0;
}

static int dcm_mcusys(int on)
{
	dcm_mcusys_par_wrap_cpubiu_dcm(on);
	dcm_mcusys_par_wrap_emi_wfifo(on);
	dcm_mcusys_par_wrap_last_cor_idle_dcm(on);
	dcm_mcusys_par_wrap_misc_dcm(on);
	dcm_mcusys_par_wrap_mp0_qdcm(on);
	return 0;
}

static int dcm_mcusys_adb(int on)
{
	dcm_mcusys_par_wrap_adb_dcm(on);
	return 0;
}

static int dcm_mcusys_apb(int on)
{
	dcm_mcusys_par_wrap_apb_dcm(on);
	return 0;
}

static int dcm_mcusys_stall(int on)
{
	dcm_mcusys_par_wrap_core_stall_dcm(on);
	dcm_mcusys_par_wrap_fcm_stall_dcm(on);
	return 0;
}

static int dcm_peri(int on)
{
	dcm_peri_ao_bcrm_peri_bus_dcm(on);
	return 0;
}

static int dcm_vlp(int on)
{
	dcm_vlp_ao_bcrm_vlp_bus_dcm(on);
	return 0;
}


/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function will return DCM status (On/Off)
 *****************************************/
static int dcm_armcore_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_cpu_pll_div_0_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_cpu_pll_div_1_dcm_is_on();

	return ret;
}

static int dcm_infra_is_on(void)
{
	int ret = 1;

	ret &= dcm_infra_ao_bcrm_infra_bus_dcm_is_on();
	ret &= dcm_infra_ao_bcrm_infra_bus_fmem_sub_dcm_is_on();
	ret &= dcm_infracfg_ao_aximem_bus_dcm_is_on();
	ret &= dcm_infracfg_ao_infra_bus_dcm_is_on();
	ret &= dcm_infracfg_ao_infra_rx_p2p_dcm_is_on();
	ret &= dcm_infracfg_ao_peri_bus_dcm_is_on();
	ret &= dcm_infracfg_ao_peri_module_dcm_is_on();

	return ret;
}

static int dcm_mcusys_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_cpubiu_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_emi_wfifo_is_on();
	ret &= dcm_mcusys_par_wrap_last_cor_idle_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_misc_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_mp0_qdcm_is_on();

	return ret;
}

static int dcm_mcusys_adb_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_adb_dcm_is_on();

	return ret;
}

static int dcm_mcusys_apb_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_apb_dcm_is_on();

	return ret;
}

static int dcm_mcusys_stall_is_on(void)
{
	int ret = 1;

	ret &= dcm_mcusys_par_wrap_core_stall_dcm_is_on();
	ret &= dcm_mcusys_par_wrap_fcm_stall_dcm_is_on();

	return ret;
}

static int dcm_peri_is_on(void)
{
	int ret = 1;

	ret &= dcm_peri_ao_bcrm_peri_bus_dcm_is_on();

	return ret;
}

static int dcm_vlp_is_on(void)
{
	int ret = 1;

	ret &= dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on();

	return ret;
}


void dcm_dump_regs(void)
{
	dcm_pr_info("\n******** dcm dump register *********\n");

	REG_DUMP(BUS_PLLDIV_CFG);
	REG_DUMP(CPU_PLLDIV_CFG0);
	REG_DUMP(CPU_PLLDIV_CFG1);
	REG_DUMP(EMI_WFIFO);
	REG_DUMP(INFRA_AXIMEM_IDLE_BIT_EN_0);
	REG_DUMP(INFRA_BUS_DCM_CTRL);
	REG_DUMP(MCSI_DCM0);
	REG_DUMP(MCUSYS_DCM_CFG0);
	REG_DUMP(MP0_DCM_CFG0);
	REG_DUMP(MP0_DCM_CFG7);
	REG_DUMP(MP_ADB_DCM_CFG0);
	REG_DUMP(MP_ADB_DCM_CFG4);
	REG_DUMP(MP_MISC_DCM_CFG0);
	REG_DUMP(P2P_RX_CLK_ON);
	REG_DUMP(PERI_BUS_DCM_CTRL);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1);
	REG_DUMP(VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2);
	REG_DUMP(VDNR_PWR_PROT_VLP_PAR_BUS_u_spm_CTRL_0);
}

void get_init_state_and_type(unsigned int *type, int *state)
{
#if defined(DCM_DEFAULT_ALL_OFF)
	*type = ALL_DCM_TYPE;
	*state = DCM_OFF;
#elif defined(ENABLE_DCM_IN_LK)
	*type = INIT_DCM_TYPE_BY_K;
	*state = DCM_INIT;
#else
	*type = init_dcm_type;
	*state = DCM_INIT;
#endif
}
struct DCM_OPS dcm_ops = {
	.dump_regs = (DCM_FUNC_VOID_VOID) dcm_dump_regs,
	.get_init_state_and_type = (DCM_FUNC_VOID_UINTR_INTR) get_init_state_and_type,
};

struct DCM_BASE dcm_base_array[] = {
	DCM_BASE_INFO(dcm_mcusys_par_wrap_base),
	DCM_BASE_INFO(dcm_infracfg_ao_base),
	DCM_BASE_INFO(dcm_infra_ao_bcrm_base),
	DCM_BASE_INFO(dcm_peri_ao_bcrm_base),
	DCM_BASE_INFO(dcm_vlp_ao_bcrm_base),
};

static struct DCM dcm_array[] = {
	{
	 .typeid = ARMCORE_DCM_TYPE,
	 .name = "ARMCORE_DCM",
	 .func = dcm_armcore,
	 .is_on_func = dcm_armcore_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = INFRA_DCM_TYPE,
	 .name = "INFRA_DCM",
	 .func = dcm_infra,
	 .is_on_func = dcm_infra_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_DCM_TYPE,
	 .name = "MCUSYS_DCM",
	 .func = dcm_mcusys,
	 .is_on_func = dcm_mcusys_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_ADB_DCM_TYPE,
	 .name = "MCUSYS_ADB_DCM",
	 .func = dcm_mcusys_adb,
	 .is_on_func = dcm_mcusys_adb_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_APB_DCM_TYPE,
	 .name = "MCUSYS_APB_DCM",
	 .func = dcm_mcusys_apb,
	 .is_on_func = dcm_mcusys_apb_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = MCUSYS_STALL_DCM_TYPE,
	 .name = "MCUSYS_STALL_DCM",
	 .func = dcm_mcusys_stall,
	 .is_on_func = dcm_mcusys_stall_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = PERI_DCM_TYPE,
	 .name = "PERI_DCM",
	 .func = dcm_peri,
	 .is_on_func = dcm_peri_is_on,
	 .default_state = DCM_ON,
	},
	{
	 .typeid = VLP_DCM_TYPE,
	 .name = "VLP_DCM",
	 .func = dcm_vlp,
	 .is_on_func = dcm_vlp_is_on,
	 .default_state = DCM_ON,
	},
	/* Keep this NULL element for array traverse */
	{0},
};

/*From DCM COMMON*/

#if IS_ENABLED(CONFIG_OF)
int mt_dcm_dts_map(void)
{
	struct device_node *node;
	unsigned int i;
	/* dcm */
	node = of_find_compatible_node(NULL, NULL, DCM_NODE);
	if (!node) {
		dcm_pr_info("error: cannot find node %s\n", DCM_NODE);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(dcm_base_array); i++) {
		*(dcm_base_array[i].base) = (unsigned long)of_iomap(node, i);

		if (!*(dcm_base_array[i].base)) {
			dcm_pr_info("error: cannot iomap base %s\n",
				dcm_base_array[i].name);
			return -1;
		}
	}
	return 0;
}
#else
int mt_dcm_dts_map(void)
{
	return 0;
}
#endif /* #if IS_ENABLED(CONFIG_OF) */


void dcm_pre_init(void)
{
	dcm_pr_info("weak function of %s\n", __func__);
}

static int __init mt6878_dcm_init(void)
{
	int ret = 0;

	if (mt_dcm_dts_map()) {
		dcm_pr_notice("%s: failed due to DTS failed\n", __func__);
		return -1;
	}

	mt_dcm_array_register(dcm_array, &dcm_ops);

	ret = mt_dcm_common_init();

	return ret;
}

static void __exit mt6878_dcm_exit(void)
{
}
MODULE_SOFTDEP("pre:mtk_dcm.ko");
module_init(mt6878_dcm_init);
module_exit(mt6878_dcm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek DCM driver");
