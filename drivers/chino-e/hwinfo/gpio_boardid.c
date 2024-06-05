#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>


extern unsigned int platform_board_id;
struct boardid {
	unsigned int gpio;
	char gpio_name[32];
};

static const char * const boardid_gpios[] = {
	"gpio,boardid0",
	"gpio,boardid1",
	"gpio,boardid2",
	"gpio,boardid3",
        "gpio,boardid4",
};

struct gpio_data {
	struct platform_device *pdev;
	int value;
	struct mutex mutex;
	struct boardid *boardid_gpio;
	struct pinctrl *boardid_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
	bool manage_pin_ctrl;
};

static struct gpio_data *gdata;

static int boardid_parse_dt(struct device *dev, struct gpio_data *data)
{
	struct device_node *of_node = dev->of_node;
	int i = 0;

	data->boardid_gpio = devm_kzalloc(dev, sizeof(struct boardid) *
	                                  ARRAY_SIZE(boardid_gpios), GFP_KERNEL);

	if (!data->boardid_gpio)
	{
		dev_err(dev, "%s: devm_kzalloc error\n", __func__);
                printk(KERN_ERR "[BOARDID]devm_kzalloc error \n");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(boardid_gpios); i++) {
		data->boardid_gpio[i].gpio = of_get_named_gpio(of_node, boardid_gpios[i], 0);

		if (data->boardid_gpio[i].gpio < 0) {
			dev_err(dev, "%s: dts get gpio error\n", __func__);
			printk(KERN_ERR "[BOARDID]dts get gpio error \n");
			return -EINVAL;
		}

		strlcpy(data->boardid_gpio[i].gpio_name, boardid_gpios[i],
		        sizeof(data->boardid_gpio[i].gpio_name));
	}
	return 0;
}
/*
static int boardid_gpio_select(struct gpio_data *data, bool on)
{
	struct pinctrl_state *pins_state;
	int ret = 0;

	pins_state = on ? data->gpio_state_active : data->gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(data->boardid_pinctrl, pins_state);
		if (ret) {
			dev_err(&data->pdev->dev,
			        "%s:can not set %s pins\n", __func__,
			        on ? "boardid_active" : "boardid_suspend");
			return ret;
		}

	} else {
		dev_warn(&data->pdev->dev,
		         "%s:not a valid '%s' pinstate\n", __func__,
		         on ? "boardid_active" : "boardid_suspend");
		return ret;
	}

	return ret;
}

static int boardid_pinctrl_enable(struct gpio_data *ts, bool on)
{
	int rc = 0;

	if (!ts->manage_pin_ctrl) {
		pr_err("%s: pinctrl info is not available\n", __func__);
		return 0;
	}

	if (!on)
		goto err_pinctrl_enable;

	rc = boardid_gpio_select(ts, true);
	if (rc < 0)
	{
        pr_err("%s: boardid_gpio_select\n", __func__);
		return -EINVAL;
	}

	return rc;

err_pinctrl_enable:
	boardid_gpio_select(ts, false);
	return rc;
}
*/
static int boardid_pinctrl_init(struct gpio_data *data)
{
	const char *statename;
	int rc;
	int state_cnt, i;
	struct device_node *np = data->pdev->dev.of_node;
	bool pinctrl_state_act_found = false;
	bool pinctrl_state_sus_found = false;

	data->boardid_pinctrl = devm_pinctrl_get(&(data->pdev->dev));
	if (IS_ERR_OR_NULL(data->boardid_pinctrl)) {
		dev_err(&data->pdev->dev,
		        "%s:Target does not use pinctrl\n", __func__);
		printk(KERN_ERR "[BOARDID]Target does not use pinctrl\n");
		rc = PTR_ERR(data->boardid_pinctrl);
		data->boardid_pinctrl = NULL;
		return rc;
	}

	state_cnt = of_property_count_strings(np, "pinctrl-names");
	if (state_cnt < 2) {
		/*
		 *if pinctrl names are not available then,
		 *power_sync can't be enabled
		 */
		dev_info(&data->pdev->dev,
		         "%s:pinctrl names are not available\n", __func__);
		printk(KERN_ERR "[BOARDID]pinctrl names are not available\n");
		rc = -EINVAL;
		goto error;
	}

	for (i = 0; i < state_cnt; i++) {
		rc = of_property_read_string_index(np,
		                                   "pinctrl-names", i, &statename);
		if (rc) {
			dev_err(&data->pdev->dev,
			        "%s:failed to read pinctrl states by index\n", __func__);
			printk(KERN_ERR "[BOARDID]failed to read pinctrl states by index\n");
			goto error;
		}

		if (!strcmp(statename, "pmx_boardid_active")) {
			data->gpio_state_active
			    = pinctrl_lookup_state(data->boardid_pinctrl,
			                           statename);
			if (IS_ERR_OR_NULL(data->gpio_state_active)) {
				dev_err(&data->pdev->dev,
				        "%s:Can not get boardid default state\n", __func__);
				printk(KERN_ERR "[BOARDID]Can not get boardid default state\n");
				rc = PTR_ERR(data->gpio_state_active);
				goto error;
			}
			pinctrl_state_act_found = true;
		} else if (!strcmp(statename, "pmx_boardid_suspend")) {
			data->gpio_state_suspend
			    = pinctrl_lookup_state(data->boardid_pinctrl,
			                           statename);
			if (IS_ERR_OR_NULL(data->gpio_state_suspend)) {
				dev_err(&data->pdev->dev,
				        "%s:Can not get boardid sleep state\n", __func__);
				printk(KERN_ERR "[BOARDID]Can not get boardid sleep state\n");
				rc = PTR_ERR(data->gpio_state_suspend);
				goto error;
			}
			pinctrl_state_sus_found = true;
		}
	}

	if (!pinctrl_state_act_found || !pinctrl_state_sus_found) {
		dev_err(&data->pdev->dev,
		        "%s:missing required pinctrl states\n",__func__);
		printk(KERN_ERR "[BOARDID]missing required pinctrl states\n");
		rc = -EINVAL;
		goto error;
	}
	rc = pinctrl_select_state(data->boardid_pinctrl, data->gpio_state_active);
	if (rc) {
		dev_err(&data->pdev->dev,
		        "%s:can not set pins\n", __func__);
		printk(KERN_ERR "[BOARDID]can not set pins\n");
		return rc;
	}

	data->manage_pin_ctrl = true;
error:
	devm_pinctrl_put(data->boardid_pinctrl);
	data->boardid_pinctrl = NULL;
	return rc;
}

static int boardid_suspend(struct device *dev)
{
/*
	int rc;

	mutex_lock(&gdata->mutex);
	if (gdata->pdev) {

		rc = boardid_pinctrl_enable(gdata, false);
		if (rc) {
			pr_err("%s: failed to disable GPIO pins\n", __func__);
			goto err_pin_disable;
		}
	}
	mutex_unlock(&gdata->mutex);
	return 0;
err_pin_disable:
	mutex_unlock(&gdata->mutex);
	
*/
    return 0;
}
static int boardid_resume(struct device *dev)
{
/*
	int rc;

	mutex_lock(&gdata->mutex);
	if (gdata->pdev) {

		rc = boardid_pinctrl_enable(gdata, true);

		if (rc) {
			pr_err("%s: failed to enable pin\n", __func__);
			goto err_pin_enable;
		}
	}
	mutex_unlock(&gdata->mutex);
	return 0;

err_pin_enable:
	mutex_unlock(&gdata->mutex);
*/
	return 0;
}
static const struct dev_pm_ops boardid_pm_ops = {
	.suspend = boardid_suspend,
	.resume  = boardid_resume,
};

static int boardid_probe(struct platform_device *pdev)
{
	int error;
	int i;

	gdata->pdev = pdev;
        printk(KERN_ERR "[BOARDID]boardid_probe \n");
        printk(KERN_ERR "[BOARDID]ARRAY_SIZE(boardid_gpios)= %lu \n",ARRAY_SIZE(boardid_gpios));
	if (pdev->dev.of_node) {
		error = boardid_parse_dt(&pdev->dev, gdata);
		if (error) {
			pr_err("%s: parse dt failed, rc=%d\n", __func__, error);
			printk(KERN_ERR "[BOARDID]parse dt failed, rc=%d\n",error);
			return error;
		}
	}

	platform_set_drvdata(pdev, gdata);

	error = boardid_pinctrl_init(gdata);
	if (error) {
		pr_err("%s: pinctrl isn't available, rc=%d\n", __func__,
		        error);
		printk(KERN_ERR "[BOARDID]pinctrl isn't available, rc=%d\n",error);
	}

	for (i = 0; i < ARRAY_SIZE(boardid_gpios); i++) {
		if (gpio_is_valid(gdata->boardid_gpio[i].gpio)) {
			error = gpio_request(gdata->boardid_gpio[i].gpio, gdata->boardid_gpio[i].gpio_name);
			if (error) {
				dev_err(&pdev->dev, "%s:request %d gpio failed, err = %d\n",
					__func__, gdata->boardid_gpio[i].gpio, error);
				printk(KERN_ERR "[BOARDID]request %d gpio failed, err = %d\n",gdata->boardid_gpio[i].gpio, error);
				goto gpio_request_error;
			}

			error = gpio_direction_input(gdata->boardid_gpio[i].gpio);
			if (error) {
				dev_err(&pdev->dev, "%s:gpio %d set input failed\n",
				 __func__, gdata->boardid_gpio[i].gpio);
				printk(KERN_ERR "[BOARDID]gpio %d set input failed\n",gdata->boardid_gpio[i].gpio);
				goto gpio_request_error;
			}

			gdata->value |=
			  ((gpio_get_value(gdata->boardid_gpio[i].gpio)) ? 1 : 0) << i;
		}
	}

	platform_board_id = gdata->value;
	dev_dbg(&pdev->dev, "%s ok boardid= %04d,value = %04d\n", __func__,
		     platform_board_id, gdata->value);
	printk(KERN_ERR "[BOARDID]ok boardid= %04d,value = %04d\n",platform_board_id, gdata->value);

	return error;

gpio_request_error:
	for (i = 0; i < ARRAY_SIZE(boardid_gpios); i++) {
		if (gpio_is_valid(gdata->boardid_gpio[i].gpio)) {
			gpio_free(gdata->boardid_gpio[i].gpio);
		}
	}
	devm_kfree(&pdev->dev, gdata);
	return error;
}

static int boardid_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id platform_match_table[] = {
	{ .compatible = "gpio,boardid",},
	{ },
};

static struct platform_driver boardid_driver = {
	.driver = {
		.name           = "gpio_boardid",
		.of_match_table	= platform_match_table,
		.pm             = &boardid_pm_ops,
		.owner	        = THIS_MODULE,
	},
	.probe      = boardid_probe,
	.remove     = boardid_remove,
};


static int __init boardid_init(void)
{
	int error = 0;
	printk(KERN_ERR "[BOARDID]boardid_init\n");

	gdata = kzalloc(sizeof(struct gpio_data), GFP_KERNEL);
	if (!gdata)
	{
		pr_err("%s:kzalloc error\n", __func__);
		printk(KERN_ERR "[BOARDID]kzalloc error \n");
		return -ENOMEM;
	}

	mutex_init(&gdata->mutex);

	error = platform_driver_register(&boardid_driver);
	printk(KERN_ERR "[BOARDID]platform_driver_register\n");
	if (error) {
		pr_err("%s:Failed to register platform driver: %d\n", __func__, error);
		printk(KERN_ERR "[BOARDID]Failed to register platform driver:%d\n",error);
		goto err;
	}
	return 0;
err:
	kfree(gdata);
	return error;
}
module_init(boardid_init);

static void __exit boardid_exit(void)
{
	platform_driver_unregister(&boardid_driver);
	kfree(gdata);
}
module_exit(boardid_exit);

MODULE_DESCRIPTION("gpio boardid driver");
MODULE_LICENSE("GPL");
