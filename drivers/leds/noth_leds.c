#include <linux/device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/leds.h>

#define VTG_MIN_UV                      3300000
#define VTG_MAX_UV                      3300000

#define BREATHING_LIGHTS_DEVICE                     "noth_leds"

static struct hrtimer g_hr_timer;
static ktime_t hr_timer_interval_ns;
static struct work_struct g_led_work;
static int on_off = 0;
static int perioid = 0;
static int timer_enabled = 0;

struct breathing_lights_data {
    struct regulator *vdd;
    struct led_classdev light_cdev;
};
static struct breathing_lights_data *lights_data;
void led_blink(int on_off)
{
    int ret;
    if (on_off && !regulator_is_enabled(lights_data->vdd)) {
        ret = regulator_enable(lights_data->vdd);
    } else if ((!on_off) && regulator_is_enabled(lights_data->vdd)) {
        ret = regulator_disable(lights_data->vdd);
    }
}
static void led_work_handler(struct work_struct *work)
{
    led_blink(on_off);
    on_off = !on_off;
}

static enum hrtimer_restart hr_timer_callback(struct hrtimer *timer)
{
    if (timer_enabled){
        schedule_work(&g_led_work);
        // Restart the timer if it's still enabled
        hrtimer_forward_now(&g_hr_timer, perioid * 1000 * 1000);
        return HRTIMER_RESTART;
    } else {
        return HRTIMER_NORESTART;
    }
}
static void hr_timer_init(void)
{
    // Initialize and set up hr_timer
    timer_enabled = 0;

    hrtimer_init(&g_hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    g_hr_timer.function = hr_timer_callback;
    hr_timer_interval_ns = ktime_set(0, 1000000 * 1000); // 1000 ms
}

static ssize_t state_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int ret;

    sscanf(buf, "%d", &perioid);
    printk(KERN_ERR "[breathing_lights] value=%d\n", perioid);
    if (perioid > 0 ) {
        if(perioid == 1)
        {
            timer_enabled = 0;
            if (!regulator_is_enabled(lights_data->vdd)){
                ret = regulator_enable(lights_data->vdd);
            }
        } else {
            timer_enabled = 1;
            hrtimer_start(&g_hr_timer, perioid * 1000 * 1000, HRTIMER_MODE_REL);
        }
    } else {
        timer_enabled = 0;
        if (regulator_is_enabled(lights_data->vdd)){
            regulator_disable(lights_data->vdd);
        }
    }

    return count;
}
static ssize_t state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t len = 0;

    len = snprintf(buf, PAGE_SIZE, "breath perioid = %d\n", perioid);

    return len;
}

static DEVICE_ATTR(state, S_IWUSR | S_IRUGO, state_show, state_store);

static struct attribute * g[] = {
	&dev_attr_state.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int breathing_lights_probe(struct platform_device *pdev)
{
    int ret = 0;

    lights_data = devm_kzalloc(&pdev->dev, sizeof(*lights_data), GFP_KERNEL);
    if (!lights_data)
        return -ENOMEM;

    lights_data->vdd = devm_regulator_get(&pdev->dev, "vdd");
    if (lights_data->vdd == NULL) {
         printk(KERN_ERR "[breathing_lights] get ldo fail");
         return -1;
    }
    printk(KERN_ERR "[breathing_lights] get ldo sucess");

    if (regulator_count_voltages(lights_data->vdd) > 0) {
        ret = regulator_set_voltage(lights_data->vdd, VTG_MIN_UV, VTG_MAX_UV);
        if (ret) {
            printk(KERN_ERR "[breathing_lights] vdd regulator set_vtg failed ret=%d\n", ret);
            regulator_put(lights_data->vdd);
            return ret;
        }
    }

    printk(KERN_ERR "[breathing_lights] set ldo sucess");

    lights_data->light_cdev.name = BREATHING_LIGHTS_DEVICE;
    ret = devm_led_classdev_register(&pdev->dev, &lights_data->light_cdev);
    if (ret < 0) {
        printk(KERN_ERR "[breathing_lights] led class register fail\n");
        return -1;
    }

    ret = sysfs_create_group(&lights_data->light_cdev.dev->kobj, &attr_group);
    if (ret < 0) {
        printk(KERN_ERR "[breathing_lights] error creating sysfs attr files\n");
        return -1;
    }

    hr_timer_init();
    INIT_WORK(&g_led_work, led_work_handler);

    printk("[breathing_lights] g_led_work sucess\n");

    return ret;
}

static int breathing_lights_remove(struct platform_device *pdev)
{
    int ret;

    ret = regulator_disable(lights_data->vdd);

    if (timer_enabled)
        hrtimer_cancel(&g_hr_timer);
    cancel_work_sync(&g_led_work);
    devm_led_classdev_unregister(&pdev->dev, &lights_data->light_cdev);

    return 0;
}
static const struct of_device_id breathing_lights_of_ids[] = {
	{.compatible = "breathing,lights",},
	{},
};

static struct platform_driver breathing_lights_driver = {
    .probe = breathing_lights_probe,
    .remove = breathing_lights_remove,

    .driver = {
        .name = BREATHING_LIGHTS_DEVICE,
        .owner = THIS_MODULE,
        .of_match_table = breathing_lights_of_ids,
    },
};

module_platform_driver(breathing_lights_driver);

MODULE_DESCRIPTION("Breathing Lights");
MODULE_LICENSE("GPL");
