#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static unsigned int platform_board_id = 0;
static unsigned int board_id_version = 0;

enum {
    IDX_PreT0 = 0,
    IDX_T0_1,
    IDX_T0_2,
    IDX_EVT_1,
    IDX_EVT_2,
    IDX_DVT_1,
    IDX_DVT_2,
    IDX_PVT_1,
    IDX_PVT_2,
    IDX_MP,
    IDX_UNKNOW,
};
static const char * const hwid_type_text[] = {
    "PreT0",
    "T0",
    "T0_India",
    "EVT",
    "EVT_India",
    "DVT",
    "DVT_India",
    "PVT",
    "PVT_India",
    "MP",
    "Unknow",
};

static const char * const boardid_gpios[] = {
    "board_id_gpio0",
    "board_id_gpio1",
    "board_id_gpio2",
    "board_id_gpio3",
};

struct gpio_data {
    int value;
    int prjvalue;
    struct boardid *boardid_gpio;
};

struct boardid {
    unsigned int gpio;
    char gpio_name[32];
};

static int prize_pcb_version_show(struct seq_file *m, void *data)
{
    seq_printf(m, "version = %s    platform_board_id = %d.\n", hwid_type_text[board_id_version], platform_board_id);

    return 0;
}

static int prize_pcb_version_open(struct inode *node, struct file *file)
{
    return single_open(file, prize_pcb_version_show, pde_data(node));
}

static const struct proc_ops prize_pcb_version_fops = {
    //.owner = THIS_MODULE,
    .proc_open = prize_pcb_version_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
    .proc_write = NULL,
};

static int setup_board_id_proc_files(struct platform_device *pdev)
{
    struct proc_dir_entry *entry = NULL;

    entry = proc_create("hwid", 0644, NULL, &prize_pcb_version_fops);
    if (!entry) {
        return -ENODEV;
    }
    return 0;
}

#define GPIO_DEV_NAME  "gpio,board-id"
#ifdef CONFIG_OF
static const struct of_device_id board_id_of_match[] = {
    {.compatible = GPIO_DEV_NAME},
    {},
};
MODULE_DEVICE_TABLE(of, board_id_of_match);
#endif

static int parse_dt_get_gpios(struct device *dev, struct gpio_data *data)
{
    int i = 0;
    struct device_node *of_node = dev->of_node;
    struct boardid *tmp_gpios = NULL;
    int size = ARRAY_SIZE(boardid_gpios);

    tmp_gpios = devm_kzalloc(dev, sizeof(struct boardid) * size, GFP_KERNEL);
    if (!tmp_gpios)
        return -ENOMEM;

    for (i = 0; i < size; i++) {
        tmp_gpios[i].gpio = of_get_named_gpio(of_node, boardid_gpios[i], 0);
        if (tmp_gpios[i].gpio < 0) {
            devm_kfree(dev, tmp_gpios);
            pr_err(" dts get gpio error\n");
            return -EINVAL;
        }

        strlcpy(tmp_gpios[i].gpio_name, boardid_gpios[i], sizeof(tmp_gpios[i].gpio_name));
//        pr_err("%s:%d gpios[%d].gpio = %d, gpios[%d].gpio_name = %s", __func__, __LINE__,
//                i, tmp_gpios[i].gpio,
//                i, tmp_gpios[i].gpio_name);
    }

    data->boardid_gpio = tmp_gpios;
    return 0;
}

static int boardid_get_gpio_value(struct device *dev, struct boardid *gpios, int size, int *value)
{
    int i = 0;
    int err = 0;
    int tmp = 0;

    for (i = 0; i < size; i++) {
        if (gpio_is_valid(gpios[i].gpio)) {
            err = gpio_request(gpios[i].gpio, gpios[i].gpio_name);
            if (err) {
                dev_err(dev, "request %d gpio failed, err = %d\n", gpios[i].gpio, err);
                goto err_gpio_request;
            }

            err = gpio_direction_input(gpios[i].gpio);
            if (err) {
                dev_err(dev, "gpio %d set input failed\n", gpios[i].gpio);
                goto err_gpio_request;
            }
            pr_err("%s:%d gpios[%d].gpio = %d, gpios[%d].%s.gpio_value = %d", __func__, __LINE__,
                    i, gpios[i].gpio,
                    i, gpios[i].gpio_name,
                    gpio_get_value(gpios[i].gpio));
            tmp |= ((gpio_get_value(gpios[i].gpio)) ? (1 << i) : 0);
        }
    }

    *value = tmp;
    return 0;

err_gpio_request:
    for (; i >= 0; i--) {
        if (gpio_is_valid(gpios[i].gpio)) {
            gpio_free(gpios[i].gpio);
        }
    }
    return err;
}

static void boardid_free_gpio(struct boardid *gpios, int size)
{
    int i = 0;

    for (i = 0; i < size; i++) {
        if (gpio_is_valid(gpios[i].gpio)) {
            gpio_free(gpios[i].gpio);
        }
    }
}

static int board_id_probe(struct platform_device *pdev)
{
    struct gpio_data *data;
    int id_value = 0;
    int ret = 0;

    pr_err("%s:%d start\n",__func__,__LINE__);

    data = devm_kzalloc(&pdev->dev, sizeof(struct gpio_data), GFP_KERNEL);
    if (!data) {
        pr_err("[BOARD_ID]failed to allocate memory.\n");
        return -ENOMEM;
    }


    dev_set_drvdata(&pdev->dev, data);

    ret = parse_dt_get_gpios(&pdev->dev, data);
    if (ret < 0) {
        pr_err("[BOARD_ID]Failed to parse device tree:%d\n", ret);
        goto err_free;
    }

    ret = boardid_get_gpio_value(&pdev->dev, data->boardid_gpio,
                                ARRAY_SIZE(boardid_gpios), &id_value);
    if (ret < 0){
        pr_err("[BOARD_ID]Failed to get gpio value:%d\n", ret);
        goto err_free_all;
    }

    platform_board_id = id_value;

    switch(id_value)
    {
//        case (0) :
//             board_id_version = IDX_PreT0;
//             break;
        case (0) :
             board_id_version = IDX_T0_1;
             break;
        case (1) :
             board_id_version = IDX_T0_2;
             break;
        case (4) :
             board_id_version = IDX_EVT_1;
             break;
        case (5) :
             board_id_version = IDX_EVT_2;
             break;
        case (8) :
        case (10) :
             board_id_version = IDX_DVT_1;
             break;
        case (9) :
        case (11) :
             board_id_version = IDX_DVT_2;
             break;
        case (12) :
             board_id_version = IDX_PVT_1;
             break;
        case (13) :
             board_id_version = IDX_PVT_2;
             break;
        default:
             board_id_version = IDX_UNKNOW;
             break;
    }
    pr_err("%s:%d board_id_version= %d, id_value = %d\n",
            __func__, __LINE__, board_id_version, id_value);
    setup_board_id_proc_files(pdev);

err_free_all:
    boardid_free_gpio(data->boardid_gpio, ARRAY_SIZE(boardid_gpios));
err_free:
    devm_kfree(&pdev->dev, data);
    return ret;
}

static int board_id_remove(struct platform_device *pdev)
{
    struct gpio_data *data = dev_get_drvdata(&pdev->dev);

    boardid_free_gpio(data->boardid_gpio, ARRAY_SIZE(boardid_gpios));
    devm_kfree(&pdev->dev, data);
    platform_set_drvdata(pdev, NULL);
    return 0;
}

static struct platform_driver board_id_platform_driver = {
    .probe = board_id_probe,
    .remove = board_id_remove,
    .driver = {
        .name = GPIO_DEV_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = board_id_of_match,
#endif
    },
};

static int __init board_id_init(void)
{
    int ret;

    pr_err("%s-%d start\n",__func__,__LINE__);

    ret = platform_driver_register(&board_id_platform_driver);
    if (ret) {
        pr_err("Failed to register board_id platform driver\n");
        return ret;
    }

    pr_err("%s-%d end\n",__func__,__LINE__);

    return 0;
}

static void __exit board_id_exit(void)
{
    platform_driver_unregister(&board_id_platform_driver);
}

module_init(board_id_init);
module_exit(board_id_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("board_id Driver");
