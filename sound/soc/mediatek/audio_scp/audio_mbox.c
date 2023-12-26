// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/soc/mediatek/mtk-mbox.h>
#include "scp_audio_logger.h"
#include "audio_mbox.h"
#include "adsp_helper.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include "scp_ipi_pin.h"
#endif

static u32 audio_mbox_pin_buf[AUDIO_MBOX_RECV_SLOT_SIZE];
static bool mbox_init_done;

struct mtk_mbox_info audio_mbox_table[AUDIO_TOTAL_MBOX] = {
	{ .opt = MBOX_OPT_QUEUE_DIR, .is64d = true},
	{ .opt = MBOX_OPT_QUEUE_DIR, .is64d = true}
};

static struct mtk_mbox_pin_send audio_mbox_pin_send[AUDIO_TOTAL_SEND_PIN] = {
	{
		.mbox = AUDIO_MBOX0_CH_ID,
		.offset = AUDIO_MBOX_SEND_SLOT_OFFSET,
		.msg_size = AUDIO_MBOX_SEND_SLOT_SIZE,
		.pin_index = 0,
	},
};

static struct mtk_mbox_pin_recv audio_mbox_pin_recv[AUDIO_TOTAL_RECV_PIN] = {
	{
		.mbox = AUDIO_MBOX1_CH_ID,
		.offset = AUDIO_MBOX_RECV_SLOT_OFFSET,
		.msg_size = AUDIO_MBOX_RECV_SLOT_SIZE,
		.pin_index = 0,
		.mbox_pin_cb = audio_mbox_pin_cb,
		.pin_buf = audio_mbox_pin_buf,
	},
};

struct mtk_mbox_device audio_mboxdev = {
	.name = "audio_mailbox",
	.pin_recv_table = audio_mbox_pin_recv,
	.pin_send_table = audio_mbox_pin_send,
	.info_table = audio_mbox_table,
	.count = AUDIO_TOTAL_MBOX,
	.recv_count = AUDIO_TOTAL_RECV_PIN,
	.send_count = AUDIO_TOTAL_SEND_PIN,
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	.post_cb = (mbox_rx_cb_t)scp_clr_spm_reg,
#endif
};

static struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "scp_audio_ipi",
};

bool is_audio_mbox_init_done(void)
{
	return mbox_init_done;
}
EXPORT_SYMBOL_GPL(is_audio_mbox_init_done);

int audio_mbox_send(void *msg, unsigned int wait)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	int ret;
	struct mtk_mbox_device *mbdev = &audio_mboxdev;
	struct mtk_mbox_pin_send *pin_send = &audio_mbox_pin_send[0];
	ktime_t ts;

	if (!mbox_init_done) {
		pr_info_ratelimited("%s, not implemented", __func__);
		return -1;
	}

	if (mutex_trylock(&pin_send->mutex_send) == 0) {
		pr_info("%s, mbox %d mutex_trylock busy", __func__, pin_send->mbox);
		return MBOX_PIN_BUSY;
	}

	/* TODO : maybe move to audio_ipi_queue */
	if (scp_awake_lock((void *)SCP_A_ID)) {
		/* leave without doing scp_awake_unlock, since the function trigger warning */
		mutex_unlock(&pin_send->mutex_send);
		return MBOX_PLT_ERR;
	}

	if (mtk_mbox_check_send_irq(mbdev, pin_send->mbox, pin_send->pin_index)) {
		ret = MBOX_PIN_BUSY;
		goto EXIT;
	}

	ret = mtk_mbox_write_hd(mbdev, pin_send->mbox, pin_send->offset, msg);
	if (ret)
		goto EXIT;

	dsb(SY);

	ret = mtk_mbox_trigger_irq(mbdev, pin_send->mbox, 0x1 << pin_send->pin_index);
	if (!wait || ret)
		goto EXIT;

	ts = ktime_get();
	while (mtk_mbox_check_send_irq(mbdev, pin_send->mbox, pin_send->pin_index)) {
		if (ktime_us_delta(ktime_get(), ts) > 1000LL) {/* 1 ms */
			pr_warn("%s, time_ipc_us > 1000", __func__);
			break;
		}
	}
EXIT:
	if (ret && ret != MBOX_PIN_BUSY)
		pr_err("%s() fail, mbox pin %d error = %d\n", __func__, pin_send->mbox, ret);

	/* TODO : maybe move to audio_ipi_queue */
	scp_awake_unlock((void *)SCP_A_ID);

	mutex_unlock(&pin_send->mutex_send);
	return ret;
#endif
	return 0;
}

static bool audio_mbox_table_init(struct mtk_mbox_device *mbdev, struct platform_device *pdev)
{
	int ret = 0;
	struct mtk_mbox_pin_send *pin_send = NULL;
	struct mtk_mbox_pin_recv *pin_recv = NULL;

	/* Get mbox count */
	ret = of_property_read_u32(pdev->dev.of_node, "mbox-num", &mbdev->count);
	if (ret < 0 || mbdev->count <= 0 || mbdev->count > AUDIO_TOTAL_MBOX) {
		pr_warn("[audio_mbox] mbox-num %d invalid\n", mbdev->count);
		return false;
	}

	/* Setup send and receive table when mbox count is 1, which is true for legacy plaform.
	 * Use the same mbox channel for send and receive.
	 */
	if (mbdev->count == 1) {
		// send table
		pin_send = mbdev->pin_send_table;
		pin_send[0].mbox = AUDIO_MBOX0_CH_ID;
		pin_send[0].offset = AUDIO_MBOX_SEND_SLOT_OFFSET_1CH;
		pin_send[0].msg_size = AUDIO_MBOX_SLOT_SIZE_1CH;
		// recv table
		pin_recv = mbdev->pin_recv_table;
		pin_recv[0].mbox = AUDIO_MBOX0_CH_ID;
		pin_recv[0].offset = AUDIO_MBOX_RECV_SLOT_OFFSET_1CH;
		pin_recv[0].msg_size = AUDIO_MBOX_SLOT_SIZE_1CH;
	}
	return true;
}

static int scp_audio_mbox_dev_probe(struct platform_device *pdev)
{
	int ret = -1;
	int idx;
	struct mtk_mbox_device *mbdev = &audio_mboxdev;

	/* Setup mbox info for different set of mbox channels */
	if (!audio_mbox_table_init(&audio_mboxdev, pdev)) {
		pr_warn("%s, audio_mbox_table_init fail\n", __func__);
		return -ENODEV;
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)

	for (idx = 0; idx < mbdev->count; idx++) {
		ret = mtk_mbox_probe(pdev, mbdev, idx);
		if (ret) {
			pr_warn("%s, mtk_mbox_probe mboxdev id %d fail ret(%d)", __func__, idx, ret);
			goto EXIT;
		}

		ret = enable_irq_wake(mbdev->info_table[idx].irq_num);
		if (ret) {
			pr_warn("%s, enable_irq_wake id %d fail ret (%d)", __func__, idx, ret);
			goto EXIT;
		}
	}

	for (idx = 0; idx < mbdev->send_count; idx++)
		mutex_init(&audio_mbox_pin_send[idx].mutex_send);

	ret = misc_register(&mdev);
	if (ret) {
		pr_info("%s, cannot register misc device\n", __func__);
		goto EXIT;
	}

	ret = device_create_file(mdev.this_device, &dev_attr_audio_ipi_test);
	if (ret) {
		pr_info("%s, cannot create dev_attr_audio_ipi_test\n", __func__);
		goto EXIT;
	}

	mbox_init_done = true;

	/* scp audio logger */
	if (get_adsp_type() == ADSP_TYPE_RV55) {
		ret = scp_audio_logger_init(pdev);
		if (ret) {
			pr_info("%s, init scp audio logger fail\n", __func__);
			goto EXIT;
		}
	}

#else
	ret = 0;
#endif /* CONFIG_MTK_TINYSYS_SCP_SUPPORT */

EXIT:
	pr_info("%s, done ret:%d", __func__, ret);
	return ret;
}

static const struct of_device_id scp_audio_mbox_dt_match[] = {
	{ .compatible = "mediatek,scp-audio-mbox", },
	{},
};
MODULE_DEVICE_TABLE(of, scp_audio_mbox_dt_match);

static struct platform_driver scp_audio_mbox_driver = {
	.driver = {
		   .name = "scp-audio-mbox",
		   .owner = THIS_MODULE,
		   .of_match_table = scp_audio_mbox_dt_match,
	},
	.probe = scp_audio_mbox_dev_probe,
};

module_platform_driver(scp_audio_mbox_driver);

MODULE_DESCRIPTION("Mediatek common driver for scp audio mbox");
MODULE_AUTHOR("Chien-wei Hsu <chien-Wei.Hsu@mediatek.com>");
MODULE_LICENSE("GPL v2");

