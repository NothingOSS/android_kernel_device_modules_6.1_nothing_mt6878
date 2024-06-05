#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/kobject.h>
#include <linux/soc/qcom/smem.h>
//#include <soc/qcom/socinfo.h>
//#include <soc/qcom/boot_stats.h>
#include <asm-generic/bug.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/of.h>

#include <asm/system_misc.h>

//#include <linux/qpnp/qpnp-adc.h>
#include "board_id_adc.h"

#define BUF_SIZE 64

#if 0
//read borad ID from adc
#define BOARD_ID_MAX 16
struct qpnp_vadc_result chip_adc_result;

typedef struct adc_voltage {
	int min_voltage;
	int max_voltage;
} adc_boardid_match;

static adc_boardid_match boardid_table[] = {
	{.min_voltage = 150, .max_voltage = 250}, //10k
	{.min_voltage = 250, .max_voltage = 350}, //20k
	{.min_voltage = 350, .max_voltage = 480}, //27k
	{.min_voltage = 480, .max_voltage = 550}, //39k
	{.min_voltage = 550, .max_voltage = 630}, //47k
	{.min_voltage = 630, .max_voltage = 710}, //56k
	{.min_voltage = 710, .max_voltage = 790}, //68k
	{.min_voltage = 790, .max_voltage = 870}, //82k
	{.min_voltage = 870, .max_voltage = 970}, //100k
	{.min_voltage = 970, .max_voltage = 1070}, //120k
	{.min_voltage = 1070, .max_voltage = 1170}, //150k
};
#endif



typedef struct board_id {
	int index;
	const char *hw_version;
	const char *qcn_type;
	const char *model;
} boardid_match_t;


typedef struct mid_match {
	int index;
	const char *name;
} mid_match_t;



#define MAX_HWINFO_SIZE 64
#include "hwinfo.h"
typedef struct {
	char *hwinfo_name;
	char hwinfo_buf[MAX_HWINFO_SIZE];
} hwinfo_t;

#define KEYWORD(_name) \
	[_name] = {.hwinfo_name = __stringify(_name), \
		   .hwinfo_buf = {0}},

static hwinfo_t hwinfo[HWINFO_MAX] =
{
#include "hwinfo.h"
};
#undef KEYWORD

/************
static int hwinfo_write_file(char *file_name, const char buf[], int buf_size)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos = 0;
	ssize_t len = 0;

	if (file_name == NULL || buf == NULL || buf_size < 1)
		return -1;

	fp = filp_open(file_name, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		printk(KERN_CRIT "[BOARDID]file not found/n");
		return -1;

	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = fp->f_pos;
	len = vfs_write(fp, buf, buf_size, &pos);
	fp->f_pos = pos;
	printk(KERN_INFO "[BOARDID]buf = %s,size = %ld \n", buf, len);
	filp_close(fp, NULL);
	set_fs(fs);
	return 0;
}

#define BATTARY_IN_SUSPEND_FILE "/sys/class/power_supply/battery/input_suspend"
static int put_battery_input_suspend(const char * buf, int n)
{
	int ret = 0;

	ret = hwinfo_write_file(BATTARY_IN_SUSPEND_FILE, buf, 1);
	if (ret != 0)
	{
		printk(KERN_CRIT "[BOARDID]input_suspend_value failed.");
		return -1;
	}

	return 0;
}

#define BATTARY_CHARGING_EN_FILE "/sys/class/power_supply/battery/battery_charging_enabled"
static int put_battery_charging_enabled(const char * buf, int n)
{
	int ret = 0;

	ret = hwinfo_write_file(BATTARY_CHARGING_EN_FILE, buf, 1);
	if (ret != 0)
	{
		printk(KERN_CRIT "[BOARDID]battery_charging_enabled failed.");
		return -1;
	}

	return 0;
}
************/


unsigned int platform_board_id = 0;
EXPORT_SYMBOL(platform_board_id);
static int get_version_id(void)
{
#if 1
	int id = platform_board_id;
	return sprintf(hwinfo[board_id].hwinfo_buf, "%04d", id);
#else
	int rc = 0;
	int voltage = 0;
	int board_id_num = 0;
	int id = 0;

	if (NULL == chip_adc) {
		pr_err("[BOARDID]ontim: %s: chip_adc is NULL\n", __func__);
		return -ENOMEM;
	}

	rc = qpnp_vadc_read(chip_adc->vadc_dev, chip_adc->vadc_mux, &chip_adc_result);
	if (rc) {
		pr_err("[BOARDID]ontim: %s: qpnp_vadc_read failed(%d)\n",
		       __func__, rc);
	} else {
		voltage = (int)chip_adc_result.physical / 1000;
	}
	for (board_id_num = 0; board_id_num < sizeof(boardid_table) / sizeof(adc_boardid_match); board_id_num++) {
		if ( voltage > boardid_table[board_id_num].min_voltage &&
		        voltage < boardid_table[board_id_num].max_voltage ) {
			id = board_id_num;
		}
	}
	printk(KERN_INFO "[BOARDID]hwinfo id=%04d,board_id_num=%d,chip_adc->voltage=%d\n", id, board_id_num, voltage);
	return sprintf(hwinfo[board_id].hwinfo_buf, "%d", id);
#endif
}

static int get_sku_id(void)
{
	unsigned int skuid_gpios =387;
	int pin_val = 0;

	pin_val = !!!gpio_get_value(skuid_gpios) + 1;

	printk(KERN_INFO "%s: skuid_gpio is %d ;\n",__func__, pin_val);
	return sprintf(hwinfo[hw_sku].hwinfo_buf, "%d", pin_val);
}



char NFC_BUF[MAX_HWINFO_SIZE] = {"Unknow"};
EXPORT_SYMBOL(NFC_BUF);

#if 0
static int set_serialno(char *src)
{
	if (src == NULL)
		return 0;
	sprintf(hwinfo[serialno].hwinfo_buf, "%s", src);
	return 1;
}
__setup("androidboot.serialno=", set_serialno);

//get emmc info
char pMeminfo[MAX_HWINFO_SIZE] = {'\0'};
static int set_memory_info(char *src)
{
	if (src == NULL)
		return 0;
	sprintf(pMeminfo, "%s", src);
	return 1;
}
__setup("memory_info=", set_memory_info);

int _atoi(char * str)
{
	int value = 0;
	int sign = 1;
	int radix;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X'))
	{
		radix = 16;
		str += 2;
	}
	else if (*str == '0')
	{
		radix = 8;
		str++;
	} else {
		radix = 10;
	}
	while (*str && *str != '\0')
	{
		if (radix == 16)
		{
			if (*str >= '0' && *str <= '9')
				value = value * radix + *str - '0';
			else if (*str >= 'A' && *str <= 'F')
				value = value * radix + *str - 'A' + 10;
			else if (*str >= 'a' && *str <= 'f')
				value = value * radix + *str - 'a' + 10;
		} else {
			value = value * radix + *str - '0';
		}
		str++;
	}
	return sign * value;
}
#endif

static ssize_t hwinfo_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	int i = 0;
	printk(KERN_INFO "[BOARDID]hwinfo sys node %s \n", attr->attr.name);

	for (; i < HWINFO_MAX && strcmp(hwinfo[i].hwinfo_name, attr->attr.name) && ++i;);

	switch (i)
	{
	case board_id:
		get_version_id();
		break;
	case hw_sku:
		get_sku_id();
		break;
	default:
		break;
	}
	return sprintf(buf, "%s=%s \n",  attr->attr.name, ((i >= HWINFO_MAX || hwinfo[i].hwinfo_buf[0] == '\0') ? "unknow" : hwinfo[i].hwinfo_buf));
}

static ssize_t hwinfo_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	int i = 0;
	printk(KERN_INFO "[BOARDID]hwinfo sys node %s \n", attr->attr.name);

	for (; i < HWINFO_MAX && strcmp(hwinfo[i].hwinfo_name, attr->attr.name) && ++i;);

	switch (i)
	{
	case battery_input_suspend:
		//put_battery_input_suspend(buf, n);
		break;
	case battery_charging_enabled:
		//put_battery_charging_enabled(buf, n);
		break;
	default:
		break;
	};
	return n;
}
#define KEYWORD(_name) \
    static struct kobj_attribute hwinfo##_name##_attr = {   \
                .attr   = {                             \
                        .name = __stringify(_name),     \
                        .mode = 0644,                   \
                },                                      \
            .show   = hwinfo_show,                 \
            .store  = hwinfo_store,                \
        };

#include "hwinfo.h"
#undef KEYWORD

#define KEYWORD(_name)\
    [_name] = &hwinfo##_name##_attr.attr,

static struct attribute * g[] = {
#include "hwinfo.h"
	NULL
};
#undef KEYWORD

static struct attribute_group attr_group = {
	.attrs = g,
};

#if 0
int ontim_hwinfo_register(enum HWINFO_E e_hwinfo, char *hwinfo_name)
{
	if ((e_hwinfo >= HWINFO_MAX) || (hwinfo_name == NULL))
		return -1;
	strncpy(hwinfo[e_hwinfo].hwinfo_buf, hwinfo_name, \
	        (strlen(hwinfo_name) >= 20 ? 19 : strlen(hwinfo_name)));
	return 0;
}
EXPORT_SYMBOL(ontim_hwinfo_register);
#endif

static int __init hwinfo_init(void)
{
	struct kobject *k_hwinfo = NULL;

	if ( (k_hwinfo = kobject_create_and_add("hwinfo", NULL)) == NULL ) {
		printk(KERN_ERR "%s:[BOARDID]hwinfo sys node create error \n", __func__);
	}

	if ( sysfs_create_group(k_hwinfo, &attr_group) ) {
		printk(KERN_ERR "%s: [BOARDID]sysfs_create_group failed\n", __func__);
	}
	//arch_read_hardware_id = msm_read_hardware_id;
	return 0;
}

static void __exit hwinfo_exit(void)
{
	return ;
}

late_initcall_sync(hwinfo_init);
module_exit(hwinfo_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Product Hardward Info Exposure");
