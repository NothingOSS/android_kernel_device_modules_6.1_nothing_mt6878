// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include "scp_feature_define.h"
#include "scp_helper.h"
#include "scp.h"
#include "scp_chre_manager.h"

/* scp mbox/ipi related */
#include <linux/soc/mediatek/mtk-mbox.h>
#include "scp_ipi.h"

/* scp chre message buffer for IPI slots */
uint32_t scp_chre_ackdata[2];
uint32_t scp_chre_msgdata[2];
int scp_chre_ack2scp[2];

/* scp chre manager payload */
uint64_t scp_chre_payload_to_addr;
uint64_t scp_chre_payload_to_size;
uint64_t scp_chre_payload_from_addr;
uint64_t scp_chre_payload_from_size;

/*
 * IPI for chre
 * @param id:   IPI id
 * @param prdata: callback function parameter
 * @param data:  IPI data
 * @param len: IPI data length
 */
static int scp_chre_ack_handler(unsigned int id, void *prdata, void *data,
				unsigned int len)
{
	return 0;
}

char __user *chre_buf;
static int scp_chre_ipi_handler(unsigned int id, void *prdata, void *data,
				unsigned int len)
{
	struct scp_chre_manager_payload chre_message;
	struct scp_chre_ipi_msg msg = *(struct scp_chre_ipi_msg *)data;

	if (msg.size > SCP_CHRE_MANAGER_PAYLOAD_MAXIMUM) {
		pr_err("[SCP] %s: msg size exceeds maximum\n", __func__);
		scp_chre_ack2scp[0] = IPI_NO_MEMORY;
		scp_chre_ack2scp[1] = -1;
		return -EFAULT;
	}

	if (chre_buf == NULL) {
		pr_debug("[SCP] chre user hasn't wait ipi, try to send again\n");
		scp_chre_ack2scp[0] = IPI_PIN_BUSY;
		scp_chre_ack2scp[1] = -1;
		return -EFAULT;
	}

	chre_message.ptr = scp_chre_payload_from_addr;
	/* copy payload to user */
	if (copy_to_user(chre_buf, (void *)chre_message.ptr, msg.size)) {
		pr_err("[SCP] cher payload copy to user failed\n");
		return -EFAULT;
	}

	scp_chre_ack2scp[0] = IPI_ACTION_DONE;
	scp_chre_ack2scp[1] = msg.size;
	/* reset user buf address */
	chre_buf = NULL;

	return 0;
}
/* CHRE sysfs operations */
static ssize_t scp_chre_manager_read(struct file *filp,
		char __user *buf, size_t count, loff_t *f_pos)
{
	if (count <= 0 || count > SCP_CHRE_MANAGER_PAYLOAD_MAXIMUM) {
		pr_err("[SCP] %s: wrong size(%zd)\n", __func__, count);
		return 0;
	}

	chre_buf = buf;
	scp_chre_ack2scp[0] = 0;
	scp_chre_ack2scp[1] = -1;
	mtk_ipi_recv_reply(&scp_ipidev, IPI_IN_SCP_HOST_CHRE,
			&scp_chre_ack2scp, PIN_IN_SIZE_SCP_HOST_CHRE);

	if (scp_chre_ack2scp[1] == -1)
		return 0;
	else
		return scp_chre_ack2scp[1];
}

static ssize_t scp_chre_manager_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *f_pos)
{
	int ret;
	struct scp_chre_ipi_msg msg;
	struct scp_chre_manager_payload chre_message;

	if (count <= 0 || count > (SCP_CHRE_MANAGER_PAYLOAD_MAXIMUM + sizeof(msg))) {
		pr_err("[SCP] %s: wrong size(%zd)\n", __func__, count);
		return -EFAULT;
	}

	if (copy_from_user(&msg, buf, sizeof(msg))) {
		pr_err("[SCP] msg copy from user failed\n");
		return -EFAULT;
	}

	if (msg.magic != SCP_CHRE_MAGIC) {
		pr_err("[SCP] magic check fail\n");
		return -EFAULT;
	}

	chre_message.ptr = scp_chre_payload_to_addr;

	if (copy_from_user((void *)chre_message.ptr, (char *)(buf+8), msg.size)) {
		pr_err("[SCP] payload copy from user failed\n");
		return -EFAULT;
	}

	ret = mtk_ipi_send_compl(
			&scp_ipidev,
			IPI_OUT_HOST_SCP_CHRE,
			IPI_SEND_WAIT,		//blocking mode
			&msg,
			PIN_OUT_SIZE_HOST_SCP_CHRE,
			500);

	if (ret != IPI_ACTION_DONE)
		pr_notice("[SCP] %s: ipi failed, ret = %d\n", __func__, ret);
	else
		pr_debug("[SCP] %s: ipi ack done(%u)\n", __func__, scp_chre_ackdata[0]);

	return scp_chre_ackdata[1];
}

static unsigned int scp_chre_manager_poll(struct file *filp,
		struct poll_table_struct *wait)
{
	pr_notice("%s+++\n", __func__);

	return POLLIN;
}

static const struct file_operations scp_chre_manager_fops = {
	.owner		= THIS_MODULE,
	.read           = scp_chre_manager_read,
	.write          = scp_chre_manager_write,
	.poll           = scp_chre_manager_poll,
};

static struct miscdevice scp_chre_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "scp_chre_manager",
	.fops = &scp_chre_manager_fops
};

static void scp_chre_channel_init(void)
{
	int ret;

	scp_chre_payload_to_addr = (uint64_t)scp_get_reserve_mem_virt(SCP_CHRE_TO_MEM_ID);
	scp_chre_payload_to_size = (uint64_t)scp_get_reserve_mem_size(SCP_CHRE_TO_MEM_ID);
	scp_chre_payload_from_addr = (uint64_t)scp_get_reserve_mem_virt(SCP_CHRE_FROM_MEM_ID);
	scp_chre_payload_from_size = (uint64_t)scp_get_reserve_mem_size(SCP_CHRE_FROM_MEM_ID);

	/* synchronization for send IPI post callback sequence */
	ret = mtk_ipi_register(
			&scp_ipidev,
			IPI_OUT_HOST_SCP_CHRE,
			NULL,
			(void *)scp_chre_ack_handler,
			(void *)&scp_chre_ackdata);
	if (ret) {
		pr_err("IPI_OUT_HOST_SCP_CHRE register failed %d\n", ret);
		WARN_ON(1);
	}

	/* receive IPI handler */
	ret = mtk_ipi_register(
			&scp_ipidev,
			IPI_IN_SCP_HOST_CHRE,
			(void *)scp_chre_ipi_handler,
			NULL,
			(void *)&scp_chre_msgdata);
	if (ret) {
		pr_err("IPI_IN_SCP_HOST_CHRE register failed %d\n", ret);
		WARN_ON(1);
	}
}

void scp_chre_manager_init(void)
{
	int ret;

	scp_chre_channel_init();

	ret = misc_register(&scp_chre_device);
	if (unlikely(ret != 0))
		pr_err("[SCP] chre misc register failed(%d)\n", ret);
}

void scp_chre_manager_exit(void)
{
	misc_deregister(&scp_chre_device);
}

