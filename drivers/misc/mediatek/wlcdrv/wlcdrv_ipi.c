// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include "wlcdrv_ipi.h"
#include "../mcupm/include/mcupm_ipi_id.h"
#include "../mcupm/include/mcupm_driver.h"
#include "wlcdrv_pmu.h"

/*****************************************************************************
 * internal variable declaration
 *****************************************************************************/
static struct mtk_ipi_device *wlcdrv_ipidev;

int wlc_ipi_to_mcupm_init(void)
{
	wlcdrv_ipidev = get_mcupm_ipidev();

	if (wlcdrv_ipidev == NULL) {
		pr_info("[WLCDrv]wlcdrv_ipidev_symbol is NULL, get ipi device fail\n");
		return -1;
	}

	return 0;
}


int wlc_ipi_to_mcupm_deinit(void)
{
	return 0;
}


int wlc_ipi_to_mcupm_send(int ctrl_cmd)
{
	struct wlc_ipi_data cmd_packet;

	cmd_packet.cmd = IPI_WLC_SUSPEND;

	pr_info("[WLCDrv]%s cmd:0x%x\n", __func__, ctrl_cmd);
	switch (ctrl_cmd) {
	case WLCIPI_CMD_RESET:
		cmd_packet.cmd = IPI_WLC_RESET;
		wlc_mcu_pmu_deinit();
		wlc_sampler_stop();
		break;
	case WLCIPI_CMD_SUSPEND:
		cmd_packet.cmd = IPI_WLC_SUSPEND;
		wlc_mcu_pmu_deinit();
		wlc_sampler_stop();
		break;
	case WLCIPI_CMD_RESUME:
		cmd_packet.cmd = IPI_WLC_RESUME;
		wlc_mcu_pmu_init();
		wlc_sampler_start();
		break;
	default:
		pr_info("[WLCDrv]default cmd:0x%x\n", ctrl_cmd);
		return 0;
	}

	if (wlcdrv_ipidev != NULL) {
		mtk_ipi_send_compl(wlcdrv_ipidev, CH_S_PLATFORM,
						IPI_SEND_WAIT, &cmd_packet,
						sizeof(cmd_packet)/MBOX_SLOT_SIZE, 2000);
	}
	return 0;
}

