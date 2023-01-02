// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>

#include "apusys_secure.h"
#include "aputop_rpmsg.h"
#include "apu_top.h"
#include "mt6985_apupwr.h"
#include "mt6985_apupwr_prot.h"

#define LOCAL_DBG	(1)
#define RPC_ALIVE_DBG	(0)
/* Below reg_name has to 1-1 mapping DTS's name */
static const char *reg_name[APUPW_MAX_REGS] = {
	"sys_vlp", "sys_spm", "apu_rcx", "apu_vcore", "apu_md32_mbox",
	"apu_rpc", "apu_pcu", "apu_ao_ctl", "apu_pll", "apu_acc",
	"apu_are", "apu_acx0", "apu_acx0_rpc_lite",
	"apu_acx1", "apu_acx1_rpc_lite", "apu_ncx", "apu_ncx_rpc_lite"
};

static struct apu_power apupw = {
	.env = FPGA,
	.rcx = CE_FW,
};

/* backup/restore opp limit */
static uint32_t g_opp_cfg_acx0;
static uint32_t g_opp_cfg_acx1;

static int init_reg_base(struct platform_device *pdev)
{
	struct resource *res;
	int idx = 0;

	pr_info("%s %d APUPW_MAX_REGS = %d\n",
			__func__, __LINE__, APUPW_MAX_REGS);

	for (idx = 0; idx < APUPW_MAX_REGS; idx++) {

		res = platform_get_resource_byname(
				pdev, IORESOURCE_MEM, reg_name[idx]);

		if (res == NULL) {
			pr_info("%s: get resource \"%s\" fail\n",
					__func__, reg_name[idx]);
			return -ENODEV;
		}

		pr_info("%s: get resource \"%s\" pass\n",
				__func__, reg_name[idx]);

		apupw.regs[idx] = ioremap(res->start,
				res->end - res->start + 1);

		if (IS_ERR_OR_NULL(apupw.regs[idx])) {
			pr_info("%s: %s remap base fail\n",
					__func__, reg_name[idx]);
			return -ENOMEM;
		}

		pr_info("%s: %s remap base 0x%x to 0x%x\n",
				__func__, reg_name[idx],
				res->start, apupw.regs[idx]);

		apupw.phy_addr[idx] = res->start;
	}

	return 0;
}

static uint32_t apusys_pwr_smc_call(struct device *dev, uint32_t smc_id,
		uint32_t a2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
			a2, 0, 0, 0, 0, 0, &res);
	if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%d)\n",
				__func__,
				smc_id, res.a0);
	return res.a0;
}

static void aputop_dump_pwr_reg(struct device *dev)
{
	// dump reg in ATF log
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_DUMP,
			SMC_PWR_DUMP_ALL);
	// dump reg in AEE db
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_REGDUMP, 0);
}

static void aputop_dump_rpc_data(void)
{
	char buf[32];

	// reg dump for RPC
	memset(buf, 0, sizeof(buf));
	snprintf(buf, 32, "phys 0x%08x: ",
			(u32)(apupw.phy_addr[apu_rpc]));
	print_hex_dump(KERN_INFO, buf, DUMP_PREFIX_OFFSET, 16, 4,
			apupw.regs[apu_rpc], 0x20, true);
}

static void aputop_dump_pcu_data(struct device *dev)
{
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_DUMP,
			SMC_PWR_DUMP_PCU);
}


static void aputop_dump_pll_data(void)
{
	// need to 1-1 in order mapping with array in __apu_pll_init func
	uint32_t pll_base_arr[] = {MNOC_PLL_BASE, UP_PLL_BASE};
	uint32_t pll_offset_arr[] = {
				PLL1UPLL_FHCTL_HP_EN, PLL1UPLL_FHCTL_RST_CON,
				PLL1UPLL_FHCTL_CLK_CON, PLL1UPLL_FHCTL0_CFG,
				PLL1U_PLL1_CON1, PLL1UPLL_FHCTL0_DDS};
	int base_arr_size = sizeof(pll_base_arr) / sizeof(uint32_t);
	int offset_arr_size = sizeof(pll_offset_arr) / sizeof(uint32_t);
	int pll_idx;
	int ofs_idx;
	uint32_t phy_addr = 0x0;
	char buf[256];

	for (pll_idx = 0 ; pll_idx < base_arr_size ; pll_idx++) {

		memset(buf, 0, sizeof(buf));

		for (ofs_idx = 0 ; ofs_idx < offset_arr_size ; ofs_idx++) {

			phy_addr = apupw.phy_addr[apu_pll] +
				pll_base_arr[pll_idx] +
				pll_offset_arr[ofs_idx];

			snprintf(buf + strlen(buf),
					sizeof(buf) - strlen(buf),
					" 0x%08x",
					apu_readl(apupw.regs[apu_pll] +
						pll_base_arr[pll_idx] +
						pll_offset_arr[ofs_idx]));

		}

		pr_info("%s pll_base:0x%08x = %s\n", __func__,
				apupw.phy_addr[apu_pll] + pll_base_arr[pll_idx],
				buf);
	}
}

static void aputop_check_pwr_data(void)
{
	uint32_t magicNum = 0x11223344;
	uint32_t clearNum = 0x55667788;
	uint32_t regValue = 0x0;

	apu_writel(magicNum, apupw.regs[apu_md32_mbox] + PWR_DBG_REG);
	regValue = apu_readl(apupw.regs[apu_md32_mbox] + PWR_DBG_REG);
	pr_info("%s mbox_dbg_reg readback = 0x%08x\n", __func__, regValue);
	apu_writel(clearNum, apupw.regs[apu_md32_mbox] + PWR_DBG_REG);
}

static int __apu_wake_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;

	dev_info(dev, "%s before wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			SMC_RCX_PWR_AFC_EN);

	/* wake up RPC */
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			SMC_RCX_PWR_WAKEUP_RPC);
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);
	if (ret) {
		pr_info("%s polling RPC RDY timeout, ret %d\n", __func__, ret);
		goto out;
	}

	dev_info(dev, "%s after wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret |= readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_STATUS),
			val, (val & (0x1 << 29)), 50, 10000);
	if (ret) {
		pr_info("%s polling ARE FSM timeout, ret %d\n", __func__, ret);
		goto out;
	}

	/* clear vcore/rcx cgs */
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			SMC_RCX_PWR_CG_EN);
out:
	return ret;
}

static int mt6985_apu_top_on(struct device *dev)
{
	int ret = 0;

	if (apupw.env != MP)
		return 0;

	pr_info("%s +\n", __func__);

	ret = __apu_wake_rpc_rcx(dev);
	if (ret) {
		pr_info("%s fail to wakeup RPC, ret %d\n", __func__, ret);
		aputop_dump_pwr_reg(dev);
		aputop_dump_rpc_data();
		aputop_dump_pcu_data(dev);
		aputop_dump_pll_data();
		aputop_check_pwr_data();
		if (ret == -EIO)
			apupw_aee_warn("APUSYS_POWER",
					"APUSYS_POWER_RPC_CFG_ERR");
		else
			apupw_aee_warn("APUSYS_POWER",
					"APUSYS_POWER_WAKEUP_FAIL");
		return -1;
	}

	pr_info("%s -\n", __func__);
	return 0;
}

static int mt6985_apu_top_off(struct device *dev)
{
	int ret = 0, val = 0;
	int rpc_timeout_val = 500000; // 500 ms

	if (apupw.env != MP)
		return 0;

	pr_info("%s +\n", __func__);
	mt6985_pwr_flow_remote_sync(1); // tell remote side I am ready to off

	// blocking until sleep success or timeout, delay 50 us per round
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL) == 0x0, 50, rpc_timeout_val);
	if (ret) {
		pr_info("%s polling PWR RDY timeout\n", __func__);
	} else {
		ret = readl_relaxed_poll_timeout_atomic(
				(apupw.regs[apu_rpc] + APU_RPC_STATUS),
				val, (val & 0x1UL) == 0x1, 50, 10000);
		if (ret) {
			pr_info("%s polling PWR STATUS timeout\n", __func__);
			return -1;
		}
	}

	if (ret) {
		pr_info(
		"%s timeout to wait RPC sleep (val:%d), ret %d\n", __func__, rpc_timeout_val, ret);
		aputop_dump_pwr_reg(dev);
		aputop_dump_rpc_data();
		aputop_dump_pcu_data(dev);
		aputop_dump_pll_data();
		aputop_check_pwr_data();
		apupw_aee_warn("APUSYS_POWER", "APUSYS_POWER_SLEEP_TIMEOUT");
		return -1;
	}

	pr_info("%s -\n", __func__);
	return 0;
}



static int init_plat_chip_data(struct platform_device *pdev)
{
	struct plat_cfg_data plat_cfg;
	uint32_t aging_attr = 0x0;
	uint32_t vsram_vb_attr = 0x0;

	memset(&plat_cfg, 0, sizeof(plat_cfg));

	of_property_read_u32(pdev->dev.of_node, "aging_load", &aging_attr);
	of_property_read_u32(pdev->dev.of_node, "vsram_vb_en", &vsram_vb_attr);

	plat_cfg.aging_flag = (aging_attr & 0xf);
	plat_cfg.hw_id = 0x0;
	plat_cfg.vsram_vb_en = (vsram_vb_attr & 0xf);

	pr_info("%s 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", __func__,
		aging_attr, plat_cfg.aging_flag,
		vsram_vb_attr, plat_cfg.vsram_vb_en,
		plat_cfg.hw_id);

	return mt6985_chip_data_remote_sync(&plat_cfg);
}

static int mt6985_apu_top_pb(struct platform_device *pdev)
{

	int ret = 0;

	init_reg_base(pdev);
	if (apupw.env < MP)
		ret = mt6985_all_on(pdev, &apupw);
	mt6985_init_remote_data_sync(apupw.regs[apu_md32_mbox]);
	init_plat_chip_data(pdev);
	return ret;
}

static int mt6985_apu_top_rm(struct platform_device *pdev)
{
	int idx;

	pr_info("%s +\n", __func__);

	if (apupw.env < MP)
		mt6985_all_off(pdev);
	for (idx = 0; idx < APUPW_MAX_REGS; idx++)
		iounmap(apupw.regs[idx]);
	pr_info("%s -\n", __func__);
	return 0;
}

static int mt6985_apu_top_suspend(struct device *dev)
{
	g_opp_cfg_acx0 = apu_readl(
			apupw.regs[apu_md32_mbox] + ACX0_LIMIT_OPP_REG);
	g_opp_cfg_acx1 = apu_readl(
			apupw.regs[apu_md32_mbox] + ACX1_LIMIT_OPP_REG);

	pr_info("%s backup data 0x%08x 0x%08x\n", __func__,
			g_opp_cfg_acx0, g_opp_cfg_acx1);
	return 0;
}

static int mt6985_apu_top_resume(struct device *dev)
{
	pr_info("%s restore data 0x%08x 0x%08x\n", __func__,
			g_opp_cfg_acx0, g_opp_cfg_acx1);

	apu_writel(g_opp_cfg_acx0,
			apupw.regs[apu_md32_mbox] + ACX0_LIMIT_OPP_REG);
	apu_writel(g_opp_cfg_acx1,
			apupw.regs[apu_md32_mbox] + ACX1_LIMIT_OPP_REG);
	return 0;
}

static int mt6985_apu_top_func(struct platform_device *pdev,
		enum aputop_func_id func_id, struct aputop_func_param *aputop)
{
	pr_info("%s func_id : %d\n", __func__, aputop->func_id);

	switch (aputop->func_id) {
	case APUTOP_FUNC_PWR_OFF:
		pm_runtime_put_sync(&pdev->dev);
		break;
	case APUTOP_FUNC_PWR_ON:
		pm_runtime_get_sync(&pdev->dev);
		break;
	case APUTOP_FUNC_OPP_LIMIT_HAL:
		mt6985_aputop_opp_limit(aputop, OPP_LIMIT_HAL);
		break;
	case APUTOP_FUNC_OPP_LIMIT_DBG:
		mt6985_aputop_opp_limit(aputop, OPP_LIMIT_DEBUG);
		break;
	case APUTOP_FUNC_DUMP_REG:
		aputop_dump_pwr_reg(&pdev->dev);
		break;
	case APUTOP_FUNC_DRV_CFG:
		mt6985_drv_cfg_remote_sync(aputop);
		break;
	case APUTOP_FUNC_IPI_TEST:
		test_ipi_wakeup_apu();
		break;
	case APUTOP_FUNC_ARE_DUMP1:
		//TODO
		break;
	case APUTOP_FUNC_ARE_DUMP2:
		//TODO
		break;
	default:
		pr_info("%s invalid func_id : %d\n", __func__, aputop->func_id);
		return -EINVAL;
	}

	return 0;
}

/* call by mt6985_pwr_func.c */
void mt6985_apu_dump_rpc_status(enum t_acx_id id, struct rpc_status_dump *dump)
{
	uint32_t status1 = 0x0;
	uint32_t status2 = 0x0;
	uint32_t status3 = 0x0;

	if (id == ACX0) {
		status1 = apu_readl(apupw.regs[apu_acx0_rpc_lite]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_acx0]
				+ APU_ACX_CONN_CG_CON);
		pr_info("%s ACX0 RPC_PWR_RDY:0x%08x APU_ACX_CONN_CG_CON:0x%08x\n",
			__func__, status1, status2);

	} else if (id == ACX1) {
		status1 = apu_readl(apupw.regs[apu_acx1_rpc_lite]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_acx1]
				+ APU_ACX_CONN_CG_CON);
		pr_info("%s ACX1 RPC_PWR_RDY:0x%08x APU_ACX_CONN_CG_CON:0x%08x\n",
			__func__, status1, status2);

	} else if (id == NCX) {
		status1 = apu_readl(apupw.regs[apu_ncx_rpc_lite]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_ncx]
				+ APU_ACX_CONN_CG_CON);
		pr_info("%s NCX RPC_PWR_RDY:0x%08x APU_NCX_CONN_CG_CON:0x%08x\n",
			__func__, status1, status2);
	} else {
		status1 = apu_readl(apupw.regs[apu_rpc]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_vcore]
				+ APUSYS_VCORE_CG_CON);
		status3 = apu_readl(apupw.regs[apu_rcx]
				+ APU_RCX_CG_CON);
		pr_info("%s RCX RPC_PWR_RDY:0x%08x VCORE_CG_CON:0x%08x RCX_CG_CON:0x%08x\n",
			__func__, status1, status2, status3);
		/*
		 * print_hex_dump(KERN_ERR, "rpc: ", DUMP_PREFIX_OFFSET,
		 *              16, 4, apupw.regs[apu_rpc], 0x100, 1);
		 */
	}

	if (!IS_ERR_OR_NULL(dump)) {
		dump->rpc_reg_status = status1;
		dump->conn_reg_status = status2;
		if (id == RCX)
			dump->vcore_reg_status = status3;
	}
}

const struct apupwr_plat_data mt6985_plat_data = {
	.plat_name = "mt6985_apupwr",
	.plat_aputop_on = mt6985_apu_top_on,
	.plat_aputop_off = mt6985_apu_top_off,
	.plat_aputop_pb = mt6985_apu_top_pb,
	.plat_aputop_rm = mt6985_apu_top_rm,
	.plat_aputop_suspend = mt6985_apu_top_suspend,
	.plat_aputop_resume = mt6985_apu_top_resume,
	.plat_aputop_func = mt6985_apu_top_func,
#if IS_ENABLED(CONFIG_DEBUG_FS)
	.plat_aputop_dbg_open = mt6985_apu_top_dbg_open,
	.plat_aputop_dbg_write = mt6985_apu_top_dbg_write,
#endif
	.plat_rpmsg_callback = mt6985_apu_top_rpmsg_cb,
	.bypass_pwr_on = 0,
	.bypass_pwr_off = 0,
};
