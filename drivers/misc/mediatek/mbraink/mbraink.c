// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

#include "mbraink_power.h"
#include "mbraink_video.h"
#include "mbraink.h"
#include "mbraink_process.h"
#include "mbraink_memory.h"
#include "mbraink_gpu.h"
#include "mbraink_audio.h"
#include "mbraink_cpufreq.h"
#include "mbraink_suspend_info.h"
#include "mbraink_battery.h"

static DEFINE_MUTEX(power_lock);
struct mbraink_data mbraink_priv;

static int mbraink_open(struct inode *inode, struct file *filp)
{
	/*pr_info("[MBK_INFO] %s\n", __func__);*/

	return 0;
}

static int mbraink_release(struct inode *inode, struct file *filp)
{
	/*pr_info("[MBK_INFO] %s\n", __func__);*/

	return 0;
}

static long handleMemoryDdrInfo(unsigned long arg)
{
	long ret = 0;
	struct mbraink_memory_ddrInfo memoryDdrInfo;

	memset(&memoryDdrInfo,
			0,
			sizeof(struct mbraink_memory_ddrInfo));
	ret = mbraink_memory_getDdrInfo(&memoryDdrInfo);
	if (ret == 0) {
		if (copy_to_user((struct mbraink_memory_ddrInfo *)arg,
						&memoryDdrInfo,
						sizeof(memoryDdrInfo))) {
			pr_notice("Copy memory ddr Info to UserSpace error!\n");
			ret = -EPERM;
		}
	}

	return ret;
}

static long handleIdleRatioInfo(unsigned long arg)
{
	long ret = 0;
	struct mbraink_audio_idleRatioInfo audioIdleRatioInfo;

	memset(&audioIdleRatioInfo,
			0,
			sizeof(struct mbraink_audio_idleRatioInfo));
	ret = mbraink_audio_getIdleRatioInfo(&audioIdleRatioInfo);
	if (ret == 0) {
		if (copy_to_user((struct mbraink_audio_idleRatioInfo *)arg,
						&audioIdleRatioInfo,
						sizeof(struct mbraink_audio_idleRatioInfo))) {
			pr_notice("Copy audio idle ratio info from UserSpace Err!\n");
			ret = -EPERM;
		}
	}
	return ret;
}

static long handleVcoreInfo(unsigned long arg)
{
	long ret = 0;
	struct mbraink_power_vcoreInfo powerVcoreInfo;

	memset(&powerVcoreInfo,
			0,
			sizeof(struct mbraink_power_vcoreInfo));
	ret = mbraink_power_getVcoreInfo(&powerVcoreInfo);
	if (ret == 0) {
		if (copy_to_user((struct mbraink_power_vcoreInfo *)arg,
						&powerVcoreInfo,
						sizeof(struct mbraink_power_vcoreInfo))) {
			pr_notice("Copy vcore info from UserSpace Err!\n");
			ret = -EPERM;
		}
	}
	return ret;
}

static long mbraink_ioctl(struct file *filp,
							unsigned int cmd,
							unsigned long arg)
{
	int n = 0;
	long ret = 0;

	switch (cmd) {
	case RO_POWER:
	{
		n = mbraink_get_power_info(mbraink_priv.power_buffer, CURRENT_DATA);
		if (n <= 0) {
			pr_notice("mbraink_get_power_info return failed, err %d\n", n);
		} else {
			mutex_lock(&power_lock);

			if (mbraink_priv.suspend_power_info_en[0] == '1') {
				if (mbraink_priv.suspend_power_buffer[0] != '\0'
				    && mbraink_priv.suspend_power_data_size != 0) {
					memcpy(mbraink_priv.power_buffer+n,
						mbraink_priv.suspend_power_buffer,
						mbraink_priv.suspend_power_data_size+1);
					n = n + mbraink_priv.suspend_power_data_size;
				}
				mbraink_priv.suspend_power_buffer[0] = '\0';
				mbraink_priv.suspend_power_data_size = 0;

				if (mbraink_priv.resume_power_buffer[0] != '\0'
				    && mbraink_priv.resume_power_data_size != 0) {
					memcpy(mbraink_priv.power_buffer+n,
						mbraink_priv.resume_power_buffer,
						mbraink_priv.resume_power_data_size+1);
					n = n + mbraink_priv.resume_power_data_size;
				}
				mbraink_priv.resume_power_buffer[0] = '\0';
				mbraink_priv.resume_power_data_size = 0;
			}

			if (copy_to_user((char *)arg, mbraink_priv.power_buffer, n+1)) {
				pr_notice("Copy Power_info to UserSpace error!\n");
				mutex_unlock(&power_lock);
				return -EPERM;
			}
			mutex_unlock(&power_lock);
		}
		break;
	}
	case RO_VIDEO:
	{
		char buffer[(MAX_BUF_SZ/8)];

		n = mbraink_get_video_info(buffer);
		if (n <= 0) {
			pr_notice("mbraink_get_video_info return failed, err %d\n", n);
		} else if (copy_to_user((char *)arg, buffer, sizeof(buffer))) {
			pr_notice("Copy Video_info to UserSpace error!\n");
			return -EPERM;
		}
		break;
	}
	case WO_SUSPEND_POWER_EN:
	{
		mutex_lock(&power_lock);
		if (copy_from_user(mbraink_priv.suspend_power_info_en,
					 (char *)arg,
					 sizeof(mbraink_priv.suspend_power_info_en))) {
			pr_notice("Data write suspend_power_en from UserSpace Err!\n");
			mutex_unlock(&power_lock);
			return -EPERM;
		}
		pr_info("[MBK_INFO] mbraink_priv.suspend_power_info_en = %c\n",
				mbraink_priv.suspend_power_info_en[0]);
		mutex_unlock(&power_lock);
		break;
	}
	case RO_PROCESS_STAT:
	{
		struct mbraink_process_stat_data process_stat_buffer;

		pid_t pid = 1;

		if (copy_from_user(&process_stat_buffer,
					(struct mbraink_process_stat_data *)arg,
					sizeof(process_stat_buffer))) {
			pr_notice("copy process info from user Err!\n");
			return -EPERM;
		}

		pid = process_stat_buffer.pid;

		mbraink_get_process_stat_info(&pid, &process_stat_buffer);

		if (copy_to_user((struct mbraink_process_stat_data *)arg,
					&process_stat_buffer,
					sizeof(process_stat_buffer))) {
			pr_notice("Copy process_info to UserSpace error!\n");
			return -EPERM;
		}
		break;
	}
	case RO_PROCESS_MEMORY:
	{
		struct mbraink_process_memory_data process_memory_buffer;

		pid_t pid = 1;

		if (copy_from_user(&process_memory_buffer,
					(struct mbraink_process_memory_data *)arg,
					sizeof(process_memory_buffer))) {
			pr_notice("copy process memory info from user Err!\n");
			return -EPERM;
		}

		pid = process_memory_buffer.pid;

		mbraink_get_process_memory_info(&pid, &process_memory_buffer);

		if (copy_to_user((struct mbraink_process_memory_data *)arg,
					&process_memory_buffer,
					sizeof(process_memory_buffer))) {
			pr_notice("Copy process_memory_info to UserSpace error!\n");
				return -EPERM;
		}
		break;
	}
	case WO_MONITOR_PROCESS:
	{
		struct mbraink_monitor_processlist monitor_processlist_buffer;

		if (copy_from_user(&monitor_processlist_buffer,
					(struct mbraink_monitor_processlist *)arg,
					sizeof(monitor_processlist_buffer))) {
			pr_notice("copy mbraink_monitor_processlist from user Err!\n");
			return -EPERM;
		}

		mbraink_processname_to_pid(&monitor_processlist_buffer);
		break;
	}
	case RO_THREAD_STAT:
	{
		struct mbraink_thread_stat_data thread_stat_buffer;

		pid_t pid_idx = 0, tid = 0;

		if (copy_from_user(&thread_stat_buffer,
					(struct mbraink_thread_stat_data *)arg,
					sizeof(thread_stat_buffer))) {
			pr_notice("copy thread_stat_info data from user Err!\n");
			return -EPERM;
		}

		pid_idx = thread_stat_buffer.pid_idx;
		tid = thread_stat_buffer.tid;

		mbraink_get_thread_stat_info(&pid_idx, &tid, &thread_stat_buffer);

		if (copy_to_user((struct mbraink_thread_stat_data *)arg,
					&thread_stat_buffer,
					sizeof(thread_stat_buffer))) {
			pr_notice("Copy thread_stat_info to UserSpace error!\n");
			return -EPERM;
		}
		break;
	}
	case RO_TRACE_PROCESS:
	{
		struct mbraink_tracing_pid_data tracing_pid_buffer;

		unsigned short tracing_idx = 0;

		if (copy_from_user(&tracing_pid_buffer,
					(struct mbraink_tracing_pid_data *)arg,
					sizeof(tracing_pid_buffer))) {
			pr_notice("copy tracing_pid_buffer data from user Err!\n");
			return -EPERM;
		}

		tracing_idx = tracing_pid_buffer.tracing_idx;

		mbraink_get_tracing_pid_info(&tracing_idx, &tracing_pid_buffer);

		if (copy_to_user((struct mbraink_tracing_pid_data *)arg,
					&tracing_pid_buffer,
					sizeof(tracing_pid_buffer))) {
			pr_notice("Copy tracing_pid_buffer to UserSpace error!\n");
			return -EPERM;
		}
		break;
	}
	case RO_CPUFREQ_NOTIFY:
	{
		struct mbraink_cpufreq_notify_struct_data *pcpufreq_notify_buffer;
		unsigned short notify_cluster_idx = 0;
		unsigned short notify_idx = 0;

		pcpufreq_notify_buffer =
			vmalloc(sizeof(struct mbraink_cpufreq_notify_struct_data));

		if (pcpufreq_notify_buffer == NULL) {
			pr_notice("Can't allocate cpufreq_notify_buffer!\n");
			return -EPERM;
		}

		if (copy_from_user(pcpufreq_notify_buffer,
					(struct mbraink_cpufreq_notify_struct_data *)arg,
					sizeof(struct mbraink_cpufreq_notify_struct_data))) {
			pr_notice("Copy cpufreq_notify_buffer from user Err!\n");
			vfree(pcpufreq_notify_buffer);
			return -EPERM;
		}
		notify_cluster_idx = pcpufreq_notify_buffer->notify_cluster_idx;
		notify_idx = pcpufreq_notify_buffer->notify_idx;
		mbraink_get_cpufreq_notifier_info(&notify_cluster_idx,
									&notify_idx,
									pcpufreq_notify_buffer);
		if (copy_to_user((struct mbraink_cpufreq_notify_struct_data *)arg,
					pcpufreq_notify_buffer,
					sizeof(struct mbraink_cpufreq_notify_struct_data))) {
			pr_notice("Copy cpufreq_notify_buffer to UserSpace error!\n");
			vfree(pcpufreq_notify_buffer);
			return -EPERM;
		}
		vfree(pcpufreq_notify_buffer);
		break;
	}
	case RO_MEMORY_DDR_INFO:
	{
		ret = handleMemoryDdrInfo(arg);
		break;
	}
	case RO_IDLE_RATIO:
	{
		ret = handleIdleRatioInfo(arg);
		break;
	}
	case RO_VCORE_INFO:
	{
		ret = handleVcoreInfo(arg);
		break;
	}
	case RO_SUSPEND_INFO:
	{
		struct mbraink_suspend_info_struct_data *psuspend_info_buffer =
			vmalloc(sizeof(struct mbraink_suspend_info_struct_data));

		if (psuspend_info_buffer == NULL) {
			pr_notice("can't allocate suspend_info_buffer\n");
			return -EPERM;
		}

		mbraink_get_suspend_info_list_record(psuspend_info_buffer, (MAX_SUSPEND_INFO_SZ));
		if (copy_to_user((struct mbraink_suspend_info_struct_data *) arg,
					psuspend_info_buffer,
					sizeof(struct mbraink_suspend_info_struct_data))) {
			pr_notice("Copy suspend_info_buffer to UserSpace error!\n");
			vfree(psuspend_info_buffer);
			return -EPERM;
		}
		vfree(psuspend_info_buffer);
		break;
	}
	case RO_BATTERY_INFO:
	{
		struct mbraink_battery_data battery_buffer;

		memset(&battery_buffer,
				0,
				sizeof(struct mbraink_battery_data));
		mbraink_get_battery_info(&battery_buffer);
		if (copy_to_user((struct mbraink_battery_data *) arg,
					&battery_buffer,
					sizeof(battery_buffer))) {
			pr_notice("Copy battery_buffer to UserSpace error!\n");
			return -EPERM;
		}
		break;
	}
	default:
		pr_notice("illegal ioctl number %u.\n", cmd);
		return -EINVAL;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long mbraink_compat_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	return mbraink_ioctl(filp, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static const struct file_operations mbraink_fops = {
	.owner		= THIS_MODULE,
	.open		= mbraink_open,
	.release        = mbraink_release,
	.unlocked_ioctl = mbraink_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = mbraink_compat_ioctl,
#endif
};

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int mbraink_suspend(struct device *dev)
{
	int ret;

	pr_info("[MBK_INFO] %s\n",
			__func__);

	mutex_lock(&power_lock);
	if (mbraink_priv.suspend_power_info_en[0] == '1') {
		mbraink_priv.suspend_power_data_size =
			mbraink_get_power_info(mbraink_priv.suspend_power_buffer, SUSPEND_DATA);
		if (mbraink_priv.suspend_power_data_size <= 0) {
			pr_notice("mbraink_get_power_info return failed, err %d\n",
						mbraink_priv.suspend_power_data_size);
		}
	}
	mutex_unlock(&power_lock);

	mbraink_set_suspend_info_list_record(SUSPEND_DATA);

	ret = pm_generic_suspend(dev);

	return ret;
}

static int mbraink_resume(struct device *dev)
{
	int ret;

	pr_info("[MBK_INFO] %s\n",
			__func__);
	ret = pm_generic_resume(dev);

	mbraink_set_suspend_info_list_record(RESUME_DATA);

	mutex_lock(&power_lock);
	if (mbraink_priv.suspend_power_info_en[0] == '1') {
		mbraink_priv.resume_power_data_size =
			mbraink_get_power_info(mbraink_priv.resume_power_buffer, RESUME_DATA);
		if (mbraink_priv.resume_power_data_size <= 0) {
			pr_notice("mbraink_get_power_info return failed, err %d\n",
				mbraink_priv.resume_power_data_size);
		}
	}
	mutex_unlock(&power_lock);

	return ret;
}

static const struct dev_pm_ops mbraink_class_dev_pm_ops = {
	.suspend	= mbraink_suspend,
	.resume	= mbraink_resume,
};

#define MBRAINK_CLASS_DEV_PM_OPS (&mbraink_class_dev_pm_ops)
#else
#define MBRAINK_CLASS_DEV_PM_OPS NULL
#endif /*end of CONFIG_PM_SLEEP*/

static void class_create_release(struct class *cls)
{
	/*do nothing because the mbraink class is not from malloc*/
}

static struct class mbraink_class = {
	.name		= "mbraink_host",
	.owner		= THIS_MODULE,
	.class_release	= class_create_release,
	.pm			= MBRAINK_CLASS_DEV_PM_OPS,
};

static void device_create_release(struct device *dev)
{
	/*do nothing because the mbraink device is not from malloc*/
}

static struct device mbraink_device = {
	.init_name	= "mbraink",
	.release		= device_create_release,
	.parent		= NULL,
	.driver_data	= NULL,
	.class		= NULL,
	.devt		= 0,
};

static ssize_t mbraink_info_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	mbraink_show_process_info();

	return sprintf(buf, "show the process information...\n");
}

static ssize_t mbraink_info_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf,
								size_t count)
{
	mbraink_netlink_send_msg(buf);

	return count;
}
static DEVICE_ATTR_RW(mbraink_info);

static ssize_t mbraink_gpu_show(struct device *dev,
								struct device_attribute *attr,
								char *buf)
{
	ssize_t size = 0;

	size = getTimeoutCouterReport(buf);
	return size;
}

static ssize_t mbraink_gpu_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf,
								size_t count)
{
	unsigned int command;
	unsigned long long value;
	int retSize = 0;

	retSize = sscanf(buf, "%d %llu", &command, &value);
	if (retSize == -1)
		return 0;

	pr_info("%s: Get Command (%d), Value (%llu) size(%d)\n",
			__func__,
			command,
			value,
			retSize);

	if (command == 1)
		mbraink_gpu_setQ2QTimeoutInNS(value);

	return count;
}
static DEVICE_ATTR_RW(mbraink_gpu);


static int mbraink_dev_init(void)
{
	dev_t mbraink_dev_no = 0;
	int attr_ret = 0;

	mbraink_priv.power_buffer[0] = '\0';
	mbraink_priv.suspend_power_buffer[0] = '\0';
	mbraink_priv.suspend_power_data_size = 0;
	mbraink_priv.resume_power_buffer[0] = '\0';
	mbraink_priv.resume_power_data_size = 0;
	mbraink_priv.suspend_power_info_en[0] = '0';

	/*Allocating Major number*/
	if ((alloc_chrdev_region(&mbraink_dev_no, 0, 1, CHRDEV_NAME)) < 0) {
		pr_notice("Cannot allocate major number %u\n",
				mbraink_dev_no);
		return -EBADF;
	}
	pr_info("[MBK_INFO] %s: Major = %u Minor = %u\n",
			__func__, MAJOR(mbraink_dev_no),
			MINOR(mbraink_dev_no));

	/*Initialize cdev structure*/
	cdev_init(&mbraink_priv.mbraink_cdev, &mbraink_fops);

	/*Adding character device to the system*/
	if ((cdev_add(&mbraink_priv.mbraink_cdev, mbraink_dev_no, 1)) < 0) {
		pr_notice("Cannot add the device to the system\n");
		goto r_class;
	}

	/*Register mbraink class*/
	if (class_register(&mbraink_class)) {
		pr_notice("Cannot register the mbraink class %s\n",
			mbraink_class.name);
		goto r_class;
	}

	/*add mbraink device into mbraink_class host,
	 *and assign the character device id to mbraink device
	 */

	mbraink_device.devt = mbraink_dev_no;
	mbraink_device.class = &mbraink_class;

	/*Register mbraink device*/
	if (device_register(&mbraink_device)) {
		pr_notice("Cannot register the Device %s\n",
			mbraink_device.init_name);
		goto r_device;
	}
	pr_info("[MBK_INFO] %s: Mbraink device init done.\n", __func__);

	attr_ret = device_create_file(&mbraink_device, &dev_attr_mbraink_info);
	pr_info("[MBK_INFO] %s: device create file mbraink info ret = %d\n", __func__, attr_ret);

	attr_ret = device_create_file(&mbraink_device, &dev_attr_mbraink_gpu);
	pr_info("[MBK_INFO] %s: device create file mbraink gpu ret = %d\n", __func__, attr_ret);

	return 0;

r_device:
	class_unregister(&mbraink_class);
r_class:
	unregister_chrdev_region(mbraink_dev_no, 1);

	return -EPERM;
}

int mbraink_netlink_send_msg(const char *msg)
{
	struct nlmsghdr *nlhead;
	struct sk_buff *skb_out = NULL;
	int ret = 0, msg_size = 0;

	if (mbraink_priv.client_pid != -1) {
		msg_size = strlen(msg);

		/*Allocate a new netlink message: skb_out*/
		skb_out = nlmsg_new(msg_size, 0);
		if (!skb_out) {
			pr_notice("Failed to allocate new skb\n");
			return -ENOMEM;
		}

		/*Add a new netlink message to an skb*/
		nlhead = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);

		NETLINK_CB(skb_out).dst_group = 0;

		strncpy(nlmsg_data(nlhead), msg, msg_size);

		ret = nlmsg_unicast(mbraink_priv.mbraink_sock, skb_out, mbraink_priv.client_pid);
		if (ret < 0)
			pr_notice("Error while sending back to user, ret = %d\n",
					ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mbraink_netlink_send_msg);

static void mbraink_netlink_recv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlhead;

	nlhead = (struct nlmsghdr *)skb->data;

	mbraink_priv.client_pid = nlhead->nlmsg_pid;

	pr_info("[MBK_INFO] %s: receive the connected client pid %d\n",
			__func__,
			mbraink_priv.client_pid);
}

static int mbraink_netlink_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.input = mbraink_netlink_recv_msg,
	};

	mbraink_priv.mbraink_sock = NULL;
	mbraink_priv.client_pid = -1;

	/*netlink_kernel_create() returns a pointer, should be checked with == NULL */
	mbraink_priv.mbraink_sock = netlink_kernel_create(&init_net, MBRAINK_NETLINK, &cfg);
	pr_info("[MBK_INFO] Entering: %s, protocol family = %d\n",
			__func__,
			MBRAINK_NETLINK);

	if (!mbraink_priv.mbraink_sock) {
		pr_notice("Error creating socket.\n");
		return -ENOMEM;
	}

	return 0;
}

static int mbraink_init(void)
{
	int ret = 0;

	ret = mbraink_dev_init();
	if (ret)
		pr_notice("mbraink device init failed.\n");

	ret = mbraink_netlink_init();
	if (ret)
		pr_notice("mbraink netlink init failed.\n");

	ret = mbraink_gpu_init();
	if (ret)
		pr_notice("mbraink gpu init failed.\n");

	ret = mbraink_process_tracer_init();
	if (ret)
		pr_notice("mbraink tracer init failed.\n");

	ret = mbraink_audio_init();
	if (ret)
		pr_notice("mbraink audio init failed.\n");

	ret =  mbraink_cpufreq_notify_init();
	if (ret)
		pr_notice("mbraink cpufreq tracer init failed.\n");

	mbraink_suspend_info_list_init();

	return ret;
}

static void mbraink_dev_exit(void)
{
	device_remove_file(&mbraink_device, &dev_attr_mbraink_info);

	device_unregister(&mbraink_device);
	mbraink_device.class = NULL;

	class_unregister(&mbraink_class);
	cdev_del(&mbraink_priv.mbraink_cdev);
	unregister_chrdev_region(mbraink_device.devt, 1);

	pr_info("[MBK_INFO] %s: MBraink device exit done, major:minor %u:%u\n",
			__func__,
			MAJOR(mbraink_device.devt),
			MINOR(mbraink_device.devt));
}

static void mbraink_netlink_exit(void)
{
	if (mbraink_priv.mbraink_sock)
		netlink_kernel_release(mbraink_priv.mbraink_sock);

	pr_info("[MBK_INFO] mbraink_netlink exit done.\n");
}

static void mbraink_exit(void)
{
	mbraink_dev_exit();
	mbraink_netlink_exit();
	mbraink_process_tracer_exit();
	mbraink_gpu_deinit();
	mbraink_audio_deinit();
	mbraink_cpufreq_notify_exit();
}

module_init(mbraink_init);
module_exit(mbraink_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<Wei-chin.Tsai@mediatek.com>");
MODULE_DESCRIPTION("MBrainK Linux Device Driver");
MODULE_VERSION("1.0");
