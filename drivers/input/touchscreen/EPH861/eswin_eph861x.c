/*
 * ESWIN EPH861X series Touchscreen driver
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

// uncomment to enable the EPH_DBG prints to dmesg
#define DEBUG
// uncomment to test with input forced open
//#define INPUT_DEVICE_ALWAYS_OPEN
#include <linux/types.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/sysfs.h>

#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <uapi/linux/input-event-codes.h>
#include <uapi/linux/stat.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hardirq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#if defined(CONFIG_DRM_MEDIATEK_V2)
#include "mtk_disp_notify.h"
#elif IS_ENABLED(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#include "eswin_eph861x_project_config.h"
#include "eswin_eph861x_types.h"
#include "eswin_eph861x_comms.h"
#include "eswin_eph861x_eswin.h"
#include "eswin_eph861x_bootloader.h"
#include "eswin_eph861x_tlv_command.h"
#include "eswin_eph861x_tlv_report.h"
#include "eswin_eph861x.h"
#include "eswin_debug.h"

/* device settings file format version expected*/
#define EPH_DEVICE_SETTINGS_FORMAT       "<product>EPH8610</product>"

/* Touchscreen absolute values */
#define EPH_MAX_HEIGHT_WIDTH      255u

extern char touch_version[32];
extern char panel_name_find[128];
static struct proc_dir_entry *proc_tp_test;

static int eph_sysfs_mem_access_init(struct eph_data *ephdata);
static void eph_sysfs_mem_access_remove(struct eph_data *ephdata);
static int eph_configure_components(struct eph_data *ephdata, const struct firmware *device_settings);

static int eph_check_mem_access_params(struct eph_data *ephdata,
                                       loff_t off,
                                       size_t *count)
{
    if (off >= PAGE_SIZE)
    {
        return -EIO;
    }

    if (off + *count > PAGE_SIZE)
    {
        return -EIO;
    }

    return 0;
}

static int eph_probe_bootloader(struct eph_data *ephdata)
{
    struct device *dev = &ephdata->commsdevice->dev;
    int ret_val;

    EPH_DBG(dev, "%s >\n", __func__);

    ret_val = eph_comms_specific_bootloader_checks(ephdata);
    if (ret_val)
    {
        return ret_val;
    }

    /* Check bootloader status and version information */
    ret_val = eph_read_bootloader_information(ephdata);

    EPH_INFO(dev, "%s, product_id = %x, variant_id = %x, bootloader_version = %x, error = %d\n",
            __func__,
            ephdata->ephdeviceinfo.product_id,
            ephdata->ephdeviceinfo.variant_id,
            ephdata->ephdeviceinfo.bootloader_version,
            ret_val);

    if (ret_val)
    {
        /* Force device into bootloader mode using chg line held low while toggle reset */
        ret_val = eph_chg_force_bootloader(ephdata);

        /* now forced into bootloader check whether we get a valid read */
        ret_val = eph_read_bootloader_information(ephdata);

        EPH_INFO(dev, "%s, product_id = %x, variant_id = %x, bootloader_version = %x, error = %d\n",
                __func__,
                ephdata->ephdeviceinfo.product_id,
                ephdata->ephdeviceinfo.variant_id,
                ephdata->ephdeviceinfo.bootloader_version,
                ret_val);
        
        /* Release CHG line as no more bootloader reads required - Can return to app (reset to app not strictly required */
        (void)eph_bootloader_release_chg(ephdata);

        if (ret_val)
        {
            return ret_val;
        }
    }

    return 0;
}

static int eph_read_and_process_messages(struct eph_data *ephdata)
{
    struct device *dev = &ephdata->commsdevice->dev;
    int ret_val;

    EPH_DBG(&ephdata->commsdevice->dev, "%s >\n", __func__);

    mutex_lock(&ephdata->comms_mutex);
    /* Read report */
    ret_val = eph_read_report(ephdata, ephdata->report_buf);
    mutex_unlock(&ephdata->comms_mutex);

    if (0 > ret_val)
    {
        EPH_ERR(dev, "Failed to read message (%d)\n", ret_val);
        return ret_val;
    }

    ret_val = eph_handle_report(ephdata, ephdata->report_buf);

    memset(ephdata->report_buf, 0, COMMS_BUF_SIZE);
    /* return 0 is success */
    return ret_val;
}

static irqreturn_t eph_interrupt(int irq, void *dev_id)
{
    struct eph_data *ephdata = (struct eph_data *)dev_id;
    struct device *dev = &ephdata->commsdevice->dev;

    if (dev == NULL)
        return IRQ_HANDLED;

    pm_stay_awake(dev);

    eph_read_and_process_messages(ephdata);

    /* will not unblock any other threads until message has been read */
    complete(&ephdata->chg_completion);

    pm_relax(dev);

    return IRQ_HANDLED;
}

static int eph_trigger_baseline(struct eph_data *ephdata)
{
    int ret_val;
    u8 cfg[8] =    {TLV_CONTROL_DATA_WRITE, 0x05, 0x00, 0xF0, 0x00, 0x00, 0x00,0x01};
    u16 length = (sizeof(cfg)/sizeof(cfg[0]));
    mutex_lock(&ephdata->comms_mutex);
    ret_val = eph_write_control_config(ephdata, length, &cfg[0]);
    mutex_unlock(&ephdata->comms_mutex);

    return ret_val;

}

static void eph_trigger_baseline_work(struct work_struct *work)
{
    int ret_val = 0;
    struct eph_data *ephdata = container_of(work, struct eph_data, force_baseline_work);
    struct backlight_device *bd = ephdata->bl;
    int brightness = 0;

    printk("%s\n", __func__);

    if (ephdata->last_brightness == 0) {
        do {
            if (bd->ops && bd->ops->get_brightness)
                brightness = bd->ops->get_brightness(bd);
            else
                brightness = bd->props.brightness;

            if (brightness)
                break;

            EPH_INFO(&ephdata->commsdevice->dev, "eph wait 5 msec\n");
            msleep(5);

        } while (brightness == 0);

        ret_val = eph_trigger_baseline(ephdata);

        if (ret_val)
            EPH_ERR(&ephdata->commsdevice->dev, "eph set baseline fail %d\n", ret_val);

        ephdata->last_brightness = brightness;

        EPH_INFO(&ephdata->commsdevice->dev, "brightness %d\n", brightness);
    } else {
        EPH_INFO(&ephdata->commsdevice->dev, "no need force baseline\n");
    }
}



static int eph_trigger_backup_to_nvm(struct eph_data *ephdata)
{
    int ret_val;
    u8 cfg[8] =    {TLV_CONTROL_DATA_WRITE, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00,0x02};
    u16 length = (sizeof(cfg)/sizeof(cfg[0]));
    mutex_lock(&ephdata->comms_mutex);
    ret_val = eph_write_control_config(ephdata, length, &cfg[0]);
    mutex_unlock(&ephdata->comms_mutex);

    return ret_val;

}



/* eph_update_device_settings - download device settings to chip
 * The file consists of repeating patterns of the following:
 *   <TYPE> - 1-byte type
 *   <LENGTH> - 2-byte length
 *   <DATA> - length-bytes of data
 */
static int eph_update_device_settings(struct eph_data *ephdata, const struct firmware *device_settings_image)
{
    struct device *dev = &ephdata->commsdevice->dev;
    struct eph_device_settings device_settings;
    int ret_val = 0;
    u8* cfg_ptr;
    u16 component;
    u16 offset;
    u16 msg_count = 0u;
    u8 retry = 0u;
    u16 pos = 0u;
    struct tlv_header tlvheader;

    EPH_DBG(dev, "%s >\n", __func__);

    /* Allocate space for zero terminated copy of the device settings file */
    device_settings.raw = (u8 *)kzalloc(device_settings_image->size + 1, GFP_KERNEL);
    if (!device_settings.raw)
    {
        EPH_ERR(dev, "ESWIN Couldnt allocate memory for device settings file %d.\n", -ENOMEM);
        return -ENOMEM;
    }

    /* Copy from the firmware image into local buffer */
    memcpy(device_settings.raw, device_settings_image->data, device_settings_image->size);
    /* Pad last entry as a zero. We are about to loop through all config and control tlvs */
    /* The 0 will not match an expected T and the configuration loading will terminate */
    device_settings.raw[device_settings_image->size] = 0;
    device_settings.raw_size = device_settings_image->size;
    cfg_ptr = &device_settings.raw[0];

    //TODO add format checking - Currently no version information in settings file.
    while (pos < device_settings_image->size)
    {
        tlvheader = eph_get_tl_header_info(ephdata, cfg_ptr);
        /* if item next in the data stream is not configuration or control break out */
        if((TLV_CONFIG_DATA_WRITE == tlvheader.type) || (TLV_CONTROL_DATA_WRITE == tlvheader.type))
        {
            if (pos > device_settings_image->size)
            {
                /* Have reached the end of the file but did not find the terminating character */
                ret_val = -ENOMEM;
                EPH_ERR(dev, "ESWIN went out of bounds of device settings file %d.\n", ret_val);
                /* Ensure memory released before returning */
                goto fail_release;
            }
            component = *(cfg_ptr + TLV_HEADER_SIZE + TLV_WRITE_COMPONENT_FIELD) | (*(cfg_ptr + TLV_HEADER_SIZE + TLV_WRITE_COMPONENT_FIELD + 1) << 8);
            offset = *(cfg_ptr + TLV_HEADER_SIZE + TLV_WRITE_OFFSET_FIELD) | (*(cfg_ptr + TLV_HEADER_SIZE + TLV_WRITE_OFFSET_FIELD + 1) << 8);
            EPH_DBG(dev, "TYPE: %d LEGNTH: %d COMPONENT_ID: %d OFFSET: %d\n", tlvheader.type, tlvheader.length, component, offset);
            msg_count++;
            EPH_DBG(dev, "Configuration/control write: BLOCK NUMBER: %d.\n", msg_count);

            mutex_lock(&ephdata->comms_mutex);
            ret_val = eph_write_control_config(ephdata, tlvheader.length, cfg_ptr);
            mutex_unlock(&ephdata->comms_mutex);

            retry = 0u;
            while (ret_val)
            {
                EPH_INFO(&ephdata->commsdevice->dev,
                         "Retry control config write, It failed for some reason. msg_count: %u",
                         msg_count);

                mutex_lock(&ephdata->comms_mutex);
                ret_val = eph_write_control_config(ephdata, tlvheader.length, cfg_ptr);
                mutex_unlock(&ephdata->comms_mutex);

                retry++;
                if (10 < retry)
                {
                    ret_val = -EIO;
                    EPH_ERR(dev, "ESWIN failed to send settings to TIC %d.\n", ret_val);
                    /* Ensure memory released before returning */
                    goto fail_release;

                }
            }

            cfg_ptr = cfg_ptr + tlvheader.length;
            pos = pos + tlvheader.length;

            if(ret_val)
            {
                EPH_INFO(dev, "Configuration/Contol write failure on: BLOCK NUMBER: %d.\n", msg_count);
                /* Ensure memory released before returning */
                goto fail_release;
            }

        }
        else
        {
            break;
        }
    }

    
    ret_val = eph_trigger_backup_to_nvm(ephdata);
    if(ret_val)
    {
        EPH_ERR(dev, "Failed to trigger_backup_to_nvm %d.\n", ret_val);
    }
    ret_val = eph_trigger_baseline(ephdata);
    if(ret_val)
    {
        EPH_ERR(dev, "Failed to trigger_baseline %d.\n", ret_val);
    }

    EPH_INFO(dev, "Config successfully updated\n");

fail_release:
    kfree(device_settings.raw);

    return ret_val;
}

static int eph_acquire_irq(struct eph_data *ephdata)
{
    int ret_val;
    char *commsdevice_name;

    EPH_DBG(&ephdata->commsdevice->dev, "%s >\n", __func__);

    if (!ephdata->chg_irq)
    {
        ephdata->chg_irq = gpio_to_irq(ephdata->ephplatform->gpio_chg_irq);

        commsdevice_name = eph_comms_devicename_get(ephdata);

        /* configure a thread that is triggered on the interrupt */
        /* This specific IRQ remains disabled while handler/thread is active */
        /* Thread happens outside of interrupt handler and so frees up the general interupt hardware */
        ret_val = request_threaded_irq(ephdata->chg_irq,
                                       NULL,
                                       eph_interrupt,
                                       IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                       commsdevice_name,
                                       ephdata);
        if (ret_val)
        {
            EPH_ERR(&ephdata->commsdevice->dev, "request_threaded_irq (%d)\n", ret_val);
            return ret_val;
        }

        /* Presence of ephdata->chg_irq means IRQ initialised */
        EPH_INFO(&ephdata->commsdevice->dev, "gpio_to_irq %lu -> %d\n", ephdata->ephplatform->gpio_chg_irq, ephdata->chg_irq);
    }
    else
    {
        enable_irq(ephdata->chg_irq);
    }


    EPH_INFO(&ephdata->commsdevice->dev, "%s <\n", __func__);

    return 0;
}


static int eph_input_open(struct input_dev *inputdev);
static void eph_input_close(struct input_dev *inputdev);

static void eph_unregister_input_device(struct eph_data *ephdata)
{
    EPH_DBG(&ephdata->commsdevice->dev, "%s >\n", __func__);

    if (ephdata->inputdev)
    {
#ifdef INPUT_DEVICE_ALWAYS_OPEN
        eph_input_close(ephdata->inputdev);
#endif //INPUT_DEVICE_ALWAYS_OPEN

        input_unregister_device(ephdata->inputdev);
    }
}


static int eph_probe_regulators(struct eph_data *ephdata)
{
    struct device *dev = &ephdata->commsdevice->dev;
    int ret_val = 0;
    int ret= 0;

    EPH_DBG(dev, "%s >\n", __func__);

    /* Must have reset GPIO to use regulator support */
    if (!gpio_is_valid(ephdata->ephplatform->gpio_reset))
    {
        ret_val = -EINVAL;
        goto fail;
    }
    if (!gpio_is_valid(ephdata->ephplatform->gpio_chg_irq))
    {
        ret_val = -EINVAL;
        goto fail;
    }
    if (!gpio_is_valid(ephdata->ephplatform->gpio_dvdd))
    {
        ret_val = -EINVAL;
	EPH_ERR(dev, "dvdd gpio is valid %s >\n", __func__);
        goto fail;
    }

    ret = gpio_request(ephdata->ephplatform->gpio_dvdd, "dvdd_gpio");
    if (ret) {
        ret_val = -EINVAL;
        EPH_ERR(dev,"[GPIO]dvdd gpio request failed\n");
        goto fail;
    }

    ephdata->reg_avdd = regulator_get(dev,"vdd");
    if (IS_ERR(ephdata->reg_avdd)) {
        ret = PTR_ERR(ephdata->reg_avdd);
        EPH_ERR(dev,"[regulator]get_direction for avdd regulator failed\n");
        goto fail;
    }else{
    	EPH_ERR(dev,"[regulator]get_direction for avdd regulator success\n");
    }
    if (regulator_count_voltages(ephdata->reg_avdd) > 0) {
        ret = regulator_set_voltage(ephdata->reg_avdd, 3000000,
                                    3300000);
        if (ret) {
            EPH_ERR(dev,"[regulator] avdd regulator set_vtg failed\n");
            goto fail_release;
        }else{
	    EPH_ERR(dev,"[regulator] avdd regulator set_vtg success\n");
	}
    }


    eph_regulator_enable(ephdata);

    EPH_DBG(dev, "Initialised regulators end\n");
    return 0;

fail_release:
    regulator_put(ephdata->reg_vdd);
    regulator_put(ephdata->reg_avdd);

fail:
    ephdata->reg_vdd = NULL;
    ephdata->reg_avdd = NULL;
    gpio_free(ephdata->ephplatform->gpio_reset);
    gpio_free(ephdata->ephplatform->gpio_chg_irq);
    gpio_free(ephdata->ephplatform->gpio_dvdd);
    return ret_val;
}


static int eph_input_device_initialize(struct eph_data *ephdata)
{
    const struct eph_platform_data *ephplatform = ephdata->ephplatform;
    struct device *dev = &ephdata->commsdevice->dev;
    int ret_val;
    unsigned int mt_flags = 0;
    u16 max_resoultion;

    EPH_DBG(dev, "%s >\n", __func__);


    EPH_INFO(dev, "Touchscreen resolution X%uY%u\n", ephplatform->panel_max_x, ephplatform->panel_max_y);


    EPH_INFO(dev, "ESWIN device not allocated, allocate device %d\n", 0);
    /* allocate memory for new managed input device 
     * (no need to free as is freed when last reference to the device is dropped) 
     * use input_unregister_device() and memory will be freed (after last reference) */
    ephdata->inputdev = devm_input_allocate_device(dev);
    if (!ephdata->inputdev)
    {
        EPH_ERR(dev, "allocate device failed %d\n", -ENOMEM);
        return -ENOMEM;
    }

    if (ephplatform->input_name)
    {
        ephdata->inputdev->name = ephplatform->input_name;
    }
    else
    {
        ephdata->inputdev->name = "ESWIN EPH861X Touchscreen";
    }

    ephdata->inputdev->phys = ephdata->phys;
    ephdata->inputdev->id.bustype = EPH_COMMS_BUS_TYPE;
    ephdata->inputdev->dev.parent = dev;

#ifndef INPUT_DEVICE_ALWAYS_OPEN
    ephdata->inputdev->open = eph_input_open;
    ephdata->inputdev->close = eph_input_close;
#endif //INPUT_DEVICE_ALWAYS_OPEN


    /* this will confuse android into thinking we support a feature we dont - IDEALLY should no be set */
    /* Cannot remove EV_KEY from get event so appears to be required for pixel at least */
    input_set_capability(ephdata->inputdev, EV_KEY/*event type*/, BTN_TOUCH/*event code*/);


    input_set_capability(ephdata->inputdev, EV_ABS, ABS_MT_POSITION_X);
    input_set_capability(ephdata->inputdev, EV_ABS, ABS_MT_POSITION_Y);


    /* direct device, e.g. touchscreen */
    mt_flags |= INPUT_MT_DIRECT;

    /* multi touch */
    ret_val = input_mt_init_slots(ephdata->inputdev, CONFIG_SUPPORTED_TOUCHES, mt_flags);
    if (ret_val)
    {
        EPH_ERR(dev, "Error %d initialising slots\n", ret_val);
        return ret_val;
    }

    /* reports co-ordinates of the tool */
    /* Height appears to always be Y */
    input_set_abs_params(ephdata->inputdev, ABS_MT_POSITION_X, 0, ephplatform->panel_max_x, 0, 0);
    input_set_abs_params(ephdata->inputdev, ABS_MT_POSITION_Y, 0, ephplatform->panel_max_y, 0, 0);

    max_resoultion = (ephplatform->panel_max_y > ephplatform->panel_max_x) ? ephplatform->panel_max_y :
        ephplatform->panel_max_x;

    /* The minor and major has no particular orientation just the longest/shortest axis */
    input_set_abs_params(ephdata->inputdev, ABS_MT_TOUCH_MAJOR, 0, (EPH_MAX_HEIGHT_WIDTH*max_resoultion), 0, 0);
    input_set_abs_params(ephdata->inputdev, ABS_MT_TOUCH_MINOR, 0, (EPH_MAX_HEIGHT_WIDTH*max_resoultion), 0, 0);
    input_set_abs_params(ephdata->inputdev, ABS_MT_PRESSURE, 0, 255, 0, 0);


    input_set_drvdata(ephdata->inputdev, ephdata);

    EPH_DBG(dev, "input_register_device\n");

    input_set_capability(ephdata->inputdev, EV_KEY, KEY_WAKEUP);

    ret_val = input_register_device(ephdata->inputdev);
    if (ret_val)
    {
        EPH_ERR(dev, "Error %d registering input device\n", ret_val);
        return ret_val;
    }

#ifdef INPUT_DEVICE_ALWAYS_OPEN
    eph_input_open(ephdata->inputdev);
#endif //INPUT_DEVICE_ALWAYS_OPEN

    EPH_INFO(dev, "%s <\n", __func__);

    return 0;
}

static void eph_request_fw_device_settings_nowait_cb(const struct firmware *device_settings, void *ctx)
{
    (void)eph_configure_components((struct eph_data *)ctx, device_settings);
    (void)eph_input_device_initialize((struct eph_data *)ctx);
    release_firmware(device_settings);
}

static int eph_initialize(struct eph_data *ephdata)
{
    struct comms_device *commsdevice = ephdata->commsdevice;
    int comms_attempts = 0;
    int ret_val;
    int device_info_read_retry = 0;


    EPH_DBG(&commsdevice->dev, "%s >\n", __func__);


    while (2 > comms_attempts)
    {

        mutex_lock(&ephdata->comms_mutex);
        ret_val = eph_read_device_information(ephdata);
        while(ret_val && (device_info_read_retry <= COMMS_READ_RETRY_NUM))
        {
            /* retry we didnt get a device information response */
            ret_val = eph_read_device_information(ephdata);
            device_info_read_retry++;

        }
        mutex_unlock(&ephdata->comms_mutex);

        EPH_INFO(&ephdata->commsdevice->dev,
                 "Product ID: %u Variant ID: %u Application_version_major %u Application_version_minor: %u Bootloader_version: %u Protocol_version: %u CRC:%u\n",
                 ephdata->ephdeviceinfo.product_id, ephdata->ephdeviceinfo.variant_id,
                 ephdata->ephdeviceinfo.application_version_major, ephdata->ephdeviceinfo.application_version_minor,
                 ephdata->ephdeviceinfo.bootloader_version, ephdata->ephdeviceinfo.protocol_version, ephdata->ephdeviceinfo.crc);

        if ((0 == ret_val) && ((ephdata->ephdeviceinfo.application_version_major != 0) || (ephdata->ephdeviceinfo.application_version_minor != 0)))
        {

            /* sucessfully read from device info and confirmed in application mode */
            ephdata->in_bootloader = false;
            break;
        }

        /* Failed to read the device information - try bootloader */
        ret_val = eph_chg_force_bootloader(ephdata);

        /* Check bootloader state */
        ret_val = eph_probe_bootloader(ephdata);

        /* release chg as bootloader actions complete  */
        (void)eph_bootloader_release_chg(ephdata);

        if (ret_val)
        {
            /* Chip is not in appmode or bootloader mode */
            return ret_val;
        }
        comms_attempts++;
        /* OK, we are in bootloader, see if we can recover */
        if (comms_attempts > 1)
        {
            EPH_ERR(&commsdevice->dev, "Could not recover from bootloader mode\n");
            /*
             * We can reflash from this state, so do not
             * abort initialization.
             */
            ephdata->in_bootloader = true;
            return 0;
        }

        /* Attempt to exit bootloader into app mode */
        eph_reset_device(ephdata);
        msleep(EPH_FW_RESET_TIME);
    }

    ret_val = eph_acquire_irq(ephdata);
    if (ret_val)
    {
        return ret_val;
    }

    ret_val = eph_sysfs_mem_access_init(ephdata);
    if (ret_val)
    {
        return ret_val;
    }

    EPH_DBG(&commsdevice->dev, "device_settings_name: %s\n", ephdata->device_settings_name);
    if (ephdata->device_settings_name)
    {
        ret_val = request_firmware_nowait(THIS_MODULE,
                                          true,
                                          ephdata->device_settings_name,
                                          &ephdata->commsdevice->dev,
                                          GFP_KERNEL,
                                          ephdata,
                                          eph_request_fw_device_settings_nowait_cb);
        if (ret_val)
        {
            EPH_ERR(&commsdevice->dev, "Failed to invoke firmware load: %d\n", ret_val);
            return ret_val;
        }
    }
    else
    {
        ret_val = eph_configure_components(ephdata, NULL);
        if (ret_val)
        {
            /* Do not return on configuration loading failure as will still want to initialise device */
            EPH_ERR(&commsdevice->dev, "Failed to eph_configure_components %d\n", ret_val);
        }

        ret_val = eph_input_device_initialize(ephdata);
        if (ret_val)
        {
            return ret_val;
        }
    }

    EPH_INFO(&commsdevice->dev, "%s <\n", __func__);

    return ret_val;
}


static int eph_configure_components(struct eph_data *ephdata, const struct firmware *device_settings)
{
    struct device *dev = &ephdata->commsdevice->dev;
    int ret_val = 0;

    EPH_DBG(dev, "%s %s >\n", __func__, device_settings ? "device_settings":"-");

    if (device_settings)
    {
        ret_val = eph_update_device_settings(ephdata, device_settings);
        if (ret_val)
        {
            EPH_WARN(dev, "Error %d updating device_settings\n", ret_val);
        }
    }
    

    return ret_val;
}

/* Firmware Version is reported as Major.Minor.bootloaderversion */
static ssize_t eph_devattr_fw_version_show(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    struct eph_data *ephdata = (struct eph_data*)dev_get_drvdata(dev);
    return scnprintf(buf, PAGE_SIZE, "%u.%u.%u\n",
                     ephdata->ephdeviceinfo.application_version_major, ephdata->ephdeviceinfo.application_version_minor,
                     ephdata->ephdeviceinfo.bootloader_version);
}

/* Hardware Version is reported as DevicefamilyID.DevicevariantID */
static ssize_t eph_devattr_hw_version_show(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    struct eph_data *ephdata = (struct eph_data*)dev_get_drvdata(dev);
    return scnprintf(buf, PAGE_SIZE, "%u.%u\n",
                     ephdata->ephdeviceinfo.product_id, ephdata->ephdeviceinfo.variant_id);
}


static int eph_enter_bootloader(struct eph_data *ephdata)
{
    int ret_val;

    EPH_DBG(&ephdata->commsdevice->dev, "%s >\n", __func__);


    if (!ephdata->in_bootloader)
    {
        if (ephdata->suspended)
        {
            if (ephdata->ephplatform->suspend_mode == EPH_SUSPEND_REGULATOR)
            {
                eph_regulator_enable(ephdata);
            }

            ephdata->suspended = false;
        }

        /* only disable interrupt and unregister if we have not done so before */
        disable_irq(ephdata->chg_irq);

    }

    /* force bootloader regardless whether we are in bootloader already - Always in defined TIC state */
    /* Force device into bootloader mode using chg line held low while toggle reset */
    ret_val = eph_chg_force_bootloader(ephdata);

    if (ret_val)
    {
        /* failed to enter bootloader correctly - restore CHG */
        (void)eph_bootloader_release_chg(ephdata);
        return ret_val;
    }


    ret_val = eph_comms_specific_bootloader_checks(ephdata);
    if (ret_val)
    {
        /* failed to enter bootloader correctly - restore CHG */
        (void)eph_bootloader_release_chg(ephdata);
        return ret_val;
    }

    if (!ephdata->in_bootloader)
    {
        ephdata->in_bootloader = true;
        /* Need in_bootloader to be true otherwise the regulators get disabled */
        eph_sysfs_mem_access_remove(ephdata);
        eph_unregister_input_device(ephdata);
    }

    EPH_INFO(&ephdata->commsdevice->dev, "Entered bootloader\n");

    return 0;
}

static int eph_load_fw(struct device *dev)
{
    struct eph_data *ephdata = (struct eph_data*)dev_get_drvdata(dev);
    int ret_val;

    EPH_DBG(&ephdata->commsdevice->dev, "%s >\n", __func__);

    ephdata->ephflash = (struct eph_flash*)devm_kzalloc(dev, sizeof(struct eph_flash), GFP_KERNEL);
    if (!ephdata->ephflash)
    {
        return -ENOMEM;
    }

    ephdata->ephflash->ephdata = ephdata;

    EPH_DBG(&ephdata->commsdevice->dev, "%s request_firmware name %s \n", __func__, ephdata->fw_name);
    
    /* Finds fw under the requested name */
    ret_val = request_firmware(&ephdata->ephflash->fw, ephdata->fw_name, dev);
    if (ret_val)
    {
        EPH_ERR(dev, "request_firmware %d %s\n", ret_val, ephdata->fw_name);
        goto free;
    }

    /* Check for incorrect enc file */
    ret_val = eph_check_firmware_format(dev, ephdata->ephflash->fw);
    if (ret_val)
    {
        goto release_firmware;
    }

    ret_val = eph_enter_bootloader(ephdata);
    if (ret_val)
    {
        goto release_firmware;
    }

    ret_val = eph_send_frames(ephdata);
    if (ret_val)
    {
        goto release_firmware;
    }

release_firmware:
    release_firmware(ephdata->ephflash->fw);
free:
    devm_kfree(dev, ephdata->ephflash);
    return ret_val;
}

/* gesture_mode: BIT0 enable/disable
 *               BIT1 tap
 *               BIT2 doubel tap
 *               BIT4 swipe
 */
static int eph_gesture_mode_set(struct eph_data *ephdata, u8 gesture_mode)
{
    int ret_val;

    u8 type = TLV_CONFIG_DATA_WRITE;
    u8 prepayload_len = 4;
    // gesture extraction 450
    u8 comp_id_low = (u8)450;
    u8 comp_id_high = (u8)(450 > 8);
    // param config flags
    u8 offset = 24;
    // config length
    u8 data_len = 1;
    volatile u8 data[] = { gesture_mode & 0xf };
    volatile u8 tlv[] = {type, data_len + prepayload_len, 0x00, comp_id_low, comp_id_high, offset, 0x00, data[0]};
    u16 length = (sizeof(tlv)/sizeof(tlv[0]));

    if (gesture_mode & 0xf0) {
        EPH_ERR(&ephdata->commsdevice->dev, "Invaild gesture mode");
        return -EINVAL;
    }

    mutex_lock(&ephdata->comms_mutex);
    ret_val = eph_write_control_config(ephdata, length, (u8 *)&tlv[0]);
    mutex_unlock(&ephdata->comms_mutex);

    if (likely(!ret_val))
        ephdata->gesture_mode = gesture_mode;
    else
        EPH_ERR(&ephdata->commsdevice->dev, "Set/Clr gesture mode fail\n");

    return ret_val;
}

static ssize_t eph_devattr_update_fw_store(struct device *dev,
                                           struct device_attribute *attr,
                                           const char *buf,
                                           size_t count)
{
    struct eph_data *ephdata = (struct eph_data*)dev_get_drvdata(dev);
    int ret_val;

    /* Pass in file name and length of name and if accepted put into fw_name */
    ret_val = eph_update_file_name(dev, &ephdata->fw_name, buf, count);
    if (ret_val)
    {
        return ret_val;
    }

    ret_val = eph_load_fw(dev);
    if (ret_val)
    {
        EPH_ERR(dev, "The firmware update failed(%d)\n", ret_val);
        count = ret_val;
    }
    else
    {
        EPH_INFO(dev, "The firmware update succeeded\n");

        /* TODO re-check - initial bootloader appears to require an extended delay before moving to communicating with App */
        msleep(EPH_RESET_TIME*2);
        ephdata->suspended = false;

        ret_val = eph_initialize(ephdata);
        if (ret_val)
        {
            return ret_val;
        }
    }

    return count;
}

static ssize_t eph_devattr_update_device_settings_store(struct device *dev,
                                            struct device_attribute *attr,
                                            const char *buf,
                                            size_t count)
{
    struct eph_data *ephdata = (struct eph_data*)dev_get_drvdata(dev);
    const struct eph_platform_data *ephplatform = ephdata->ephplatform;
    const struct firmware *device_settings;
    int ret_val;

    ret_val = eph_update_file_name(dev, &ephdata->device_settings_name, buf, count);
    if (ret_val)
    {
        return ret_val;
    }

    /* find the device settings file under the following name */
    ret_val = request_firmware(&device_settings, ephdata->device_settings_name, dev);
    if (ret_val)
    {
        EPH_ERR(dev, "request_firmware %d %s\n", ret_val, ephdata->device_settings_name);
        goto out;
    }

    ephdata->updating_device_settings = true;


    if (ephdata->suspended)
    {
        EPH_INFO(dev, "ESWIN device was in suspend %d\n", ret_val);
        if (ephplatform->suspend_mode == EPH_SUSPEND_REGULATOR)
        {
            enable_irq(ephdata->chg_irq);
            eph_regulator_enable(ephdata);
        }
        else if (ephplatform->suspend_mode == EPH_SUSPEND_DEEP_SLEEP)
        {
            /* do nothing as TIC does not currently support sleep */
        }

        ephdata->suspended = false;
    }

    ret_val = eph_configure_components(ephdata, device_settings);
    if (!ret_val)
    {
        /* no error so return count */
        ret_val = count;
    }

    release_firmware(device_settings);
out:
    ephdata->updating_device_settings = false;
    return ret_val;
}


static ssize_t eph_devattr_comms_read(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    struct eph_data *ephdata;
    int ret_val;
    size_t count = TLV_HEADER_SIZE;

    ephdata = (struct eph_data*)dev_get_drvdata(dev);


    /* Wait for any pending IRQ handler to complete - Gives priority for interrupt handler */
    synchronize_irq(ephdata->chg_irq);

    mutex_lock(&ephdata->comms_mutex);
    ret_val = eph_comms_two_stage_read(ephdata, (u8*)buf);
    mutex_unlock(&ephdata->comms_mutex);


    if(ret_val)
    {
        /* Return NULL message if there is no message or an error occured */
        /* clear 3 bytes to mimic null read */
        memset(buf, 0, TLV_HEADER_SIZE); 
    }
    else
    {
        /* decode and return the legth of message */
        count = (size_t)((buf[TLV_LENGTH_FIELD] | ((u16)buf[TLV_LENGTH_FIELD + 1u] << 8u)) + TLV_HEADER_SIZE);
    }

    EPH_INFO(&ephdata->commsdevice->dev, "ESWIN sysfs read the following %d, %d , %d<\n", buf[0], buf[1], buf[2]);

    return ret_val == 0 ? count : ret_val;
}


static ssize_t eph_devattr_comms_write(struct device *dev,
                                           struct device_attribute *attr,
                                           const char *buf,
                                           size_t count)
{
    
    struct eph_data *ephdata = (struct eph_data*)dev_get_drvdata(dev);
    ssize_t ret_val;

    ret_val = eph_check_mem_access_params(ephdata, 0, &count);
    if (0 < ret_val)
    {
        return ret_val;
    }

    if (0 < count)
    {

        /* Wait for any pending IRQ handler to complete - Gives priority for interrupt handler */
        synchronize_irq(ephdata->chg_irq);

        mutex_lock(&ephdata->comms_mutex);
        ret_val = eph_comms_write(ephdata, count, (u8 *)buf);
        mutex_unlock(&ephdata->comms_mutex);

        EPH_INFO(&ephdata->commsdevice->dev, "ESWIN sysfs: %s. Write the following %d, %d , %d<\n", __func__, buf[0], buf[1], buf[2]);        
    }
    

    return ret_val == 0 ? count : ret_val;


  }


static ssize_t eph_devattr_device_report_read(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    struct eph_data *ephdata;
    struct tlv_header tlvheader;
    ephdata = (struct eph_data*)dev_get_drvdata(dev);



    mutex_lock(&ephdata->sysfs_report_buffer_lock);
    tlvheader = eph_get_tl_header_info(ephdata, sysfs_report_buf);


    if (TLV_HEADER_SIZE>tlvheader.length)
    {
        /* if length was 0 for any reason generate NULL message - Ensures null message copied to the buffer */
        tlvheader.length = TLV_HEADER_SIZE;
        memset(&sysfs_report_buf[0], 0, tlvheader.length); 
    }

    /* copy message into page buffer */
    memcpy(buf,&sysfs_report_buf[0],tlvheader.length);

    /* Flush buffer once consumed - to prevent same message being read twice */
    /* relies on message being smaller than a page and the tooling processing the header in that single read */
    memset(&sysfs_report_buf[0], 0, tlvheader.length); 

    /* Only generate a message if valid message is being returned. */
    if (0!=tlvheader.type)
    {
        EPH_DBG(&ephdata->commsdevice->dev, "ESWIN sysfs read buffer. Type: %d, length: %d", tlvheader.type, tlvheader.length);
    }
    mutex_unlock(&ephdata->sysfs_report_buffer_lock);


    return (size_t)tlvheader.length;

  }


static ssize_t eph_devattr_reset_device(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    struct eph_data *ephdata;
    int ret_val;
    ephdata = (struct eph_data*)dev_get_drvdata(dev);

    EPH_DBG(&ephdata->commsdevice->dev, "%s start gpio_reset \n > \n", __func__);
    eph_reset_device(ephdata);

    EPH_DBG(&ephdata->commsdevice->dev, "%s start trigger_baseline \n > \n", __func__);
    ret_val = eph_trigger_baseline(ephdata);
    eph_clear_all_host_touch_slots(ephdata);

    EPH_DBG(&ephdata->commsdevice->dev, "%s < \n", __func__);
    return ret_val;

  }

static ssize_t eph_devattr_test_self_test(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    int ret = 0;
    struct eph_data *ephdata = (struct eph_data*)dev_get_drvdata(dev);

    ret = eswin_product_self_test(ephdata);

    ret += scnprintf(buf, 20,
            "SELF TEST: %s\n", (ret < 0) ? "fail" : "pass");
    return ret;
}

static ssize_t eph_devattr_test_self_test_read(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    int ret = 0;
    int count = 0;
    int i = 0;
    struct eph_data *ephdata = (struct eph_data*)dev_get_drvdata(dev);

    if (!ephdata->stored_pt_data) {
        dev_info(dev, "no data stored, please do self test\n");
        return ret;
    }

    count += scnprintf(buf, 12, "PT1 data:\n");
    for (i = 0; i < PAGE_SIZE; i++) {
        count += scnprintf(buf + count, 4, "%u ", ephdata->pt1_buffer[i]);
    }
    count += scnprintf(buf + count, 2, "\n");

    count += scnprintf(buf, 12, "PT2 data:\n");
    for (i = 0; i < PAGE_SIZE; i++) {
        count += scnprintf(buf + count, 4, "%u ", ephdata->pt2_buffer[i]);
    }
    count += scnprintf(buf + count, 2, "\n");

    count += scnprintf(buf, 12, "PT3 data:\n");
    for (i = 0; i < PAGE_SIZE; i++) {
        count += scnprintf(buf + count, 4, "%u ", ephdata->pt3_buffer[i]);
    }
    count += scnprintf(buf + count, 2, "\n");

    count += scnprintf(buf, 12, "PT4 data:\n");
    for (i = 0; i < PAGE_SIZE; i++) {
        count += scnprintf(buf + count, 4, "%u ", ephdata->pt4_buffer[i]);
    }
    count += scnprintf(buf + count, 2, "\n");

    ephdata->stored_pt_data = false;
    memset(ephdata->pt1_buffer, 0x0, PAGE_SIZE);
    memset(ephdata->pt2_buffer, 0x0, PAGE_SIZE);
    memset(ephdata->pt3_buffer, 0x0, PAGE_SIZE);
    memset(ephdata->pt4_buffer, 0x0, PAGE_SIZE);

    return count;
}

static ssize_t eph_devattr_gesture_wakeup_read(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf)
{
    struct eph_data *ephdata;
    ephdata = (struct eph_data*)dev_get_drvdata(dev);

    if (!ephdata)
        return -EIO;

    return sprintf(buf, "%s mode[%x]\n", ephdata->gesture_wakeup_enable ? "enable" : "disable",
            ephdata->gesture_mode);
}

static ssize_t eph_devattr_gesture_wakeup_store(struct device *dev,
                                           struct device_attribute *attr,
                                           const char *buf,
                                           size_t count)
{
    struct eph_data *ephdata;
    int ret_val = -1;
    int input = 0;
    u8 gesture_mode = 0;
    u8 set_mode = 0;
    ephdata = (struct eph_data*)dev_get_drvdata(dev);

    if (!ephdata)
        return -EIO;

    if (kstrtoint(buf, 10, &input))
        return -EINVAL;

    switch (input) {
    case 1:
        /* enable tap */
        if (ephdata->gesture_mode & BIT(1)) {
            EPH_INFO(&ephdata->commsdevice->dev, "tap already set\n");
            goto exit;
        }
        gesture_mode = BIT(1);
        break;
    case 2:
        /* enable double tap */
        if (ephdata->gesture_mode & BIT(2)) {
            EPH_INFO(&ephdata->commsdevice->dev, "double tap already set\n");
            goto exit;
        }
        gesture_mode = BIT(2);
        break;
    case 3:
        /* enable swipe */
        if (ephdata->gesture_mode & BIT(3)) {
            EPH_INFO(&ephdata->commsdevice->dev, "swipe already set\n");
            goto exit;
        }
        gesture_mode = BIT(3);
        break;
    case -1:
        /* disable tap */
        if ((ephdata->gesture_mode & BIT(1)) == 0) {
            EPH_INFO(&ephdata->commsdevice->dev, "tap not set\n");
            goto exit;
        }
        gesture_mode |= ~BIT(1);
        break;
    case -2:
        /* disable double tap */
        if ((ephdata->gesture_mode & BIT(2)) == 0) {
            EPH_INFO(&ephdata->commsdevice->dev, "double tap not set\n");
            goto exit;
        }
        gesture_mode |= ~BIT(2);
        break;
    case -3:
        /* disable swipe */
        if ((ephdata->gesture_mode & BIT(3)) == 0) {
            EPH_INFO(&ephdata->commsdevice->dev, "swipe not set\n");
            goto exit;
        }
        gesture_mode |= ~BIT(3);
        break;
    case 0:
    default:
        EPH_ERR(&ephdata->commsdevice->dev, "Invaild Para set\n");
        return -EINVAL;
    }

    set_mode = (input > 0) ? (ephdata->gesture_mode | gesture_mode) :
            (ephdata->gesture_mode & gesture_mode);

    ret_val = eph_gesture_mode_set(ephdata, set_mode);

    if (ret_val) {
        EPH_ERR(&ephdata->commsdevice->dev, "mode set/clr fail %x", ret_val);
        goto exit;
    }

    if (ephdata->gesture_mode & 0xE) {
        ephdata->gesture_wakeup_enable = true;
        EPH_INFO(&ephdata->commsdevice->dev, "mode %x enabled\n", ephdata->gesture_mode);
    } else {
        ephdata->gesture_wakeup_enable = false;
        EPH_INFO(&ephdata->commsdevice->dev, "gesture disabled\n");
    }

exit:
    return count;
}

static int eph_test_show(struct seq_file *s, void *unused)
{
    int ret = 0;
    struct eph_data *ephdata = s->private;

    ret = eswin_product_self_test(ephdata);
    seq_printf(s, "SELF TEST: %s\n", (ret < 0) ? "fail" : "pass");
    return 0;
}

static int eph_test_open(struct inode* inode, struct file* file){

    return single_open(file, eph_test_show, pde_data(inode));
}

static const struct proc_ops tp_test_fops = {
//    .owner  = THIS_MODULE,
    .proc_open   = eph_test_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int eph_proccsv_show(struct seq_file *s, void *unused)
{
    int ret = 0;
    int i = 0;
    struct eph_data *ephdata = s->private;
    int count = 0;

    if (!ephdata->stored_pt_data) {
        EPH_INFO(&ephdata->commsdevice->dev, "no data stored, please do self test\n");
        return ret;
    }


    seq_printf(s, "PT1 data:\n");
    for (i = 7; i < (18 * 40 *2 + 7); i += 2) {
        seq_printf(s, "%d,", (short)(ephdata->pt1_buffer[i] + (ephdata->pt1_buffer[i+1] << 8)));
        count++;
        if ((count % 18) == 0) {
            seq_printf(s, "\n");
        }
    }
    seq_printf(s, "\n");

    seq_printf(s, "PT2 data:\n");
    count = 0;
    for (i = 7; i < (18 * 40 *2 + 7); i += 2) {
        seq_printf(s, "%d,", (short)(ephdata->pt2_buffer[i] + (ephdata->pt2_buffer[i+1] << 8)));
        count++;
        if ((count % 18) == 0) {
            seq_printf(s, "\n");
        }
    }
    seq_printf(s, "\n");

    seq_printf(s, "PT3 data:\n");
    count = 0;
    for (i = 7; i < (18 * 40 * 2 + 7); i += 2) {
        seq_printf(s, "%d,", (short)(ephdata->pt3_buffer[i] + (ephdata->pt3_buffer[i+1] << 8)));
        count++;
        if ((count % 18) == 0) {
            seq_printf(s, "\n");
        }
    }
    seq_printf(s, "\n");

    seq_printf(s, "PT4 data:\n");
    count = 0;
    for (i = 7; i < (18 * 40 *2 + 7); i += 2) {
        seq_printf(s, "%d,", (short)(ephdata->pt4_buffer[i] + (ephdata->pt4_buffer[i+1] << 8)));
        count++;
        if ((count % 18) == 0) {
            seq_printf(s, "\n");
        }
    }
    seq_printf(s, "\n");

//    ephdata->stored_pt_data = false;

    return 0;
}

static int eph_proccsv_open(struct inode* inode, struct file* file){

    return single_open(file, eph_proccsv_show, pde_data(inode));
}

static const struct proc_ops tp_proccsv_fops = {
//    .owner  = THIS_MODULE,
    .proc_open   = eph_proccsv_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/* .attr.name, .attr.mode, .show, .store  */
static DEVICE_ATTR(update_fw, S_IWUSR, NULL, eph_devattr_update_fw_store);
static DEVICE_ATTR(self_test, S_IRUGO, eph_devattr_test_self_test, NULL);
static DEVICE_ATTR(self_test_data, S_IRUGO, eph_devattr_test_self_test_read, NULL);

/* S_IRUGO - read-only attributes  */
static DEVICE_ATTR(fw_version, S_IRUGO, eph_devattr_fw_version_show, NULL);
static DEVICE_ATTR(hw_version, S_IRUGO, eph_devattr_hw_version_show, NULL);
/* S_IWUSR - write access to root only */
static DEVICE_ATTR(update_device_settings, S_IWUSR, NULL, eph_devattr_update_device_settings_store);
static DEVICE_ATTR(write_device_message, S_IWUSR, NULL, eph_devattr_comms_write);
static DEVICE_ATTR(read_device_message, S_IRUGO, eph_devattr_comms_read, NULL);
static DEVICE_ATTR(read_device_report, S_IRUGO, eph_devattr_device_report_read, NULL);
static DEVICE_ATTR(reset_device, S_IRUGO, eph_devattr_reset_device, NULL);
static DEVICE_ATTR(gesture_wakeup, (S_IWUSR|S_IRUGO), eph_devattr_gesture_wakeup_read, eph_devattr_gesture_wakeup_store);


static struct attribute *eph_fw_attrs[] =
{
    /* update_fw */
    &dev_attr_update_fw.attr,
    NULL
};

static const struct attribute_group eph_fw_attr_group =
{
    .attrs = eph_fw_attrs,
};

static struct attribute *eph_test_pt_attrs[] =
{
//    &dev_attr_pt01.attr,
//    &dev_attr_pt02.attr,
//    &dev_attr_pt03.attr,
 //   &dev_attr_pt04.attr,
    &dev_attr_self_test.attr,
    &dev_attr_self_test_data.attr,
    NULL,
};

static const struct attribute_group eph_test_attr_group =
{
    .attrs = eph_test_pt_attrs,
};

static struct attribute *eph_attrs[] =
{
    /* fw_version */
    &dev_attr_fw_version.attr,
    /* hw_version */    
    &dev_attr_hw_version.attr,
    /* update_device_settings */
    &dev_attr_update_device_settings.attr,
    /* write message to device */    
    &dev_attr_write_device_message.attr,
    /* read message from device */    
    &dev_attr_read_device_message.attr,
    /* read buffered report from device */    
    &dev_attr_read_device_report.attr,

    &dev_attr_reset_device.attr,
	&dev_attr_gesture_wakeup.attr,
    NULL
};

static const struct attribute_group eph_attr_group =
{
    .attrs = eph_attrs,
};

static int eph_sysfs_mem_access_init(struct eph_data *ephdata)
{
    struct comms_device *commsdevice = ephdata->commsdevice;
    int ret_val;

    EPH_DBG(&ephdata->commsdevice->dev, "%s >\n", __func__);

    ret_val = sysfs_create_group(&commsdevice->dev.kobj, &eph_attr_group);
    if (ret_val)
    {
        EPH_ERR(&commsdevice->dev, "Failure %d creating sysfs group\n", ret_val);
        sysfs_remove_group(&commsdevice->dev.kobj, &eph_attr_group);
        return ret_val;
    }
    ret_val = sysfs_create_group(&commsdevice->dev.kobj, &eph_test_attr_group);

    return ret_val;
}

static void eph_sysfs_mem_access_remove(struct eph_data *ephdata)
{
    struct comms_device *commsdevice = ephdata->commsdevice;

    EPH_DBG(&ephdata->commsdevice->dev, "%s >\n", __func__);

    sysfs_remove_group(&commsdevice->dev.kobj, &eph_attr_group);
}


static int eph_start(struct eph_data *ephdata)
{
    struct comms_device *commsdevice = ephdata->commsdevice;

    EPH_INFO(&commsdevice->dev, "%s, suspend_mode %d >\n", __func__, ephdata->ephplatform->suspend_mode);

    if (!ephdata->suspended || ephdata->in_bootloader)
    {
        EPH_INFO(&commsdevice->dev, "%s, suspended %d, in_bootloader = %d <\n", __func__, ephdata->suspended, ephdata->in_bootloader);
        return 0;
    }

    switch (ephdata->ephplatform->suspend_mode)
    {
        case EPH_SUSPEND_REGULATOR:
            enable_irq(ephdata->chg_irq);
            #if (ESWIN_EPH861X_I2C)
            /* TODO Pixel4XL specific - Temporary comment out diabling/enable of regulator due to demo handset specific issues */
            eph_regulator_enable(ephdata);
            #endif
            break;

        case EPH_SUSPEND_DEEP_SLEEP:
        default:
            /* do nothing for the moment as no tic sleep */

            break;
    }

    ephdata->suspended = false;

    EPH_INFO(&commsdevice->dev, "%s <\n",__func__);

    return 0;
}

static int eph_stop(struct eph_data *ephdata)
{

    struct comms_device *commsdevice = ephdata->commsdevice;

    EPH_INFO(&commsdevice->dev, "%s, suspend mode %d >\n", __func__, ephdata->ephplatform->suspend_mode);

    if (ephdata->suspended || ephdata->in_bootloader || ephdata->updating_device_settings)
    {
        EPH_INFO(&commsdevice->dev, "%s, suspended %d, in_bootloader %d, updating_device_settings %d <\n",__func__, ephdata->suspended, ephdata->in_bootloader, ephdata->updating_device_settings);
        return 0;
    }

    switch (ephdata->ephplatform->suspend_mode)
    {
        case EPH_SUSPEND_REGULATOR:
            disable_irq(ephdata->chg_irq);
            #if (ESWIN_EPH861X_I2C)
            /* TODO Pixel4XL specific - Temporary comment out diabling of regulator due to demo handset specific issues */
            eph_regulator_disable(ephdata);
            #endif
            eph_clear_all_host_touch_slots(ephdata);
            break;

        case EPH_SUSPEND_DEEP_SLEEP:
        default:

            /* For now does nothing as sleep not implemented in TIC */

            /* Clear all the touch slots on UI */
            eph_clear_all_host_touch_slots(ephdata);

    }

    ephdata->suspended = true;

    EPH_INFO(&commsdevice->dev, "%s <\n",__func__);

    return 0;
}

static int eph_input_open(struct input_dev *inputdev)
{
    struct eph_data *ephdata = (struct eph_data *)input_get_drvdata(inputdev);
    int ret_val;

    EPH_DBG(&ephdata->commsdevice->dev, "%s >\n", __func__);

    ret_val = eph_start(ephdata);

    if (ret_val)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "%s failed rc=%d\n", __func__, ret_val);
    }

    return ret_val;
}

static void eph_input_close(struct input_dev *inputdev)
{
    struct eph_data *ephdata = (struct eph_data *)input_get_drvdata(inputdev);
    int ret_val;

    EPH_DBG(&ephdata->commsdevice->dev, "%s >\n", __func__);

    ret_val = eph_stop(ephdata);

    if (ret_val)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "%s failed rc=%d\n", __func__, ret_val);
    }
}

static int eph_dev_enter_lp_mode(struct eph_data *ephdata);
static int eph_dev_enter_normal_mode(struct eph_data *ephdata);

#if defined(CONFIG_BOARD_FLORAL) //pixel4XL platform
static int eph_notifier_callback(struct notifier_block *nb, unsigned long event,
        void *data)
{
    struct eph_data *ephdata = container_of(nb, struct eph_data, notifier);
    struct msm_drm_notifier *evdata = data;
    struct comms_device *commsdevice;
    struct backlight_device *bd = ephdata->bl;
    unsigned int blank;
    int brightness = 0;

    if (!evdata || evdata->id != 0)
        return 0;

    commsdevice = ephdata->commsdevice;

    EPH_DBG(&commsdevice->dev, "%s >>>\n", __func__);
    EPH_DBG(&commsdevice->dev, "event %x\n", event);

    if (event != MSM_DRM_EVENT_BLANK)
        return 0;

    if (evdata->data && event == MSM_DRM_EVENT_BLANK && ephdata) {
        blank = *(int *)(evdata->data);

        switch (blank) {
        case MSM_DRM_BLANK_POWERDOWN:
            eph_dev_enter_lp_mode(ephdata);
            if (bd->ops && bd->ops->get_brightness)
                brightness = bd->ops->get_brightness(bd);
            else
                brightness = bd->props.brightness;
            ephdata->last_brightness = brightness;
            EPH_INFO(&commsdevice->dev, "brightness %d\n", brightness);
            break;
        case MSM_DRM_BLANK_UNBLANK:
            eph_dev_enter_normal_mode(ephdata);
            schedule_work(&ephdata->force_baseline_work);
            break;
        default:
            break;
        }
    }

    return NOTIFY_OK;
}
#elif defined(CONFIG_BOARD_CLOUDRIPPER) // pixel7Pro platform
struct drm_connector *eph_get_bridge_connector(struct drm_bridge *bridge)
{
    struct drm_connector *connector;
    struct drm_connector_list_iter conn_iter;

    drm_connector_list_iter_begin(bridge->dev, &conn_iter);
    drm_for_each_connector_iter(connector, &conn_iter) {
        if (connector->encoder == bridge->encoder)
            break;
    }
    drm_connector_list_iter_end(&conn_iter);
    return connector;
}

static bool eph_bridge_is_lp_mode(struct drm_connector *connector)
{
    if (connector && connector->state) {
        struct exynos_drm_connector_state *s =
            to_exynos_connector_state(connector->state);
        return s->exynos_mode.is_lp_mode;
    }
    return false;
}

static void eph_panel_bridge_enable(struct drm_bridge *bridge)
{
    struct eph_data *ephdata =
                container_of(bridge, struct eph_data, panel_bridge);
    struct device *dev = &ephdata->commsdevice->dev;

    EPH_INFO(dev, "%s\n", __func__);
    ephdata->is_panel_lp_mode = eph_bridge_is_lp_mode(ephdata->connector);
    if (!ephdata->is_panel_lp_mode) {
        eph_dev_enter_normal_mode(ephdata);
        schedule_work(&ephdata->force_baseline_work);
    }
}

static void eph_panel_bridge_disable(struct drm_bridge *bridge)
{
    struct eph_data *ephdata =
            container_of(bridge, struct eph_data, panel_bridge);
    struct device *dev = &ephdata->commsdevice->dev;

    if (bridge->encoder && bridge->encoder->crtc) {
        const struct drm_crtc_state *crtc_state = bridge->encoder->crtc->state;

        if (drm_atomic_crtc_effectively_active(crtc_state))
            return;
    }

    EPH_INFO(dev, "%s\n", __func__);

    eph_dev_enter_lp_mode(ephdata);
    ephdata->last_brightness = backlight_get_brightness(ephdata->bl);
}

static void eph_panel_bridge_mode_set(struct drm_bridge *bridge,
                        const struct drm_display_mode *mode,
                        const struct drm_display_mode *adjusted_mode)
{
    struct eph_data *ephdata = container_of(bridge, struct eph_data, panel_bridge);
    struct device *dev = &ephdata->commsdevice->dev;

    EPH_INFO(dev, "%s\n", __func__);

    if (!ephdata->connector || !ephdata->connector->state) {
        ephdata->connector = eph_get_bridge_connector(bridge);
        EPH_ERR(dev, "%s: Get bridge connector.\n", __func__);
    }

}

static const struct drm_bridge_funcs panel_bridge_funcs = {
    .enable = eph_panel_bridge_enable,
    .disable = eph_panel_bridge_disable,
    .mode_set = eph_panel_bridge_mode_set,
};

static int eph_register_panel_bridge(struct eph_data *ephdata)
{
    struct device *dev = &ephdata->commsdevice->dev;
#ifdef CONFIG_OF
    ephdata->panel_bridge.of_node = dev->of_node;
#endif
    ephdata->panel_bridge.funcs = &panel_bridge_funcs;
    drm_bridge_add(&ephdata->panel_bridge);

    EPH_INFO(dev, "%s\n", __func__);

    return 0;
}

static void eph_unregister_panel_bridge(struct eph_data *ephdata)
{
    struct drm_bridge *node;
    struct drm_bridge *bridge = &ephdata->panel_bridge;

    drm_bridge_remove(bridge);

    if (!bridge->dev) /* not attached */
        return;

    drm_modeset_lock(&bridge->dev->mode_config.connection_mutex, NULL);
    list_for_each_entry(node, &bridge->encoder->bridge_chain, chain_node)
        if (node == bridge) {
            if (bridge->funcs->detach)
                bridge->funcs->detach(bridge);
            list_del(&bridge->chain_node);
            break;
        }
    drm_modeset_unlock(&bridge->dev->mode_config.connection_mutex);
    bridge->dev = NULL;
}
#endif //USE_DRM_BRIDGE

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
static int eph_notifier_callback(struct notifier_block *self, unsigned long event, void *v)
{
    int *data = (int *)v;
    struct eph_data *ephdata = container_of(self, struct eph_data, notifier);
    struct comms_device *commsdevice;
    commsdevice = ephdata->commsdevice;
    if (ephdata && v) {
        if (event == MTK_DISP_EVENT_BLANK) {
            /* resume: touch power on is after display to avoid display disturb */
            EPH_INFO(&commsdevice->dev, "event=%lu, MTK_DISP_EVENT_BLANK=%d\n", event, MTK_DISP_EVENT_BLANK);
            if (*data == MTK_DISP_BLANK_UNBLANK) {
                eph_dev_enter_normal_mode(ephdata);
//                schedule_work(&ephdata->force_baseline_work);
	        EPH_INFO(&commsdevice->dev, "normal mode\n");
            }
        } else if (event == MTK_DISP_EARLY_EVENT_BLANK) {
            /**
            * suspend: touch power off is before display to avoid touch report event
            * after screen is off
            */
            EPH_INFO(&commsdevice->dev, "event=%lu, MTK_DISP_EARLY_EVENT_BLANK=%d\n", event, MTK_DISP_EARLY_EVENT_BLANK);
            if (*data == MTK_DISP_BLANK_POWERDOWN) {
                eph_dev_enter_lp_mode(ephdata);

            //if (bd->ops && bd->ops->get_brightness)
            //    brightness = bd->ops->get_brightness(bd);
            //else

            //    brightness = bd->props.brightness;
            //ephdata->last_brightness = brightness;
//            EPH_INFO(&commsdevice->dev, "brightness %d\n", brightness);
	        EPH_INFO(&commsdevice->dev, "lp mode\n");
            }
        }
    } else {
        EPH_INFO(&commsdevice->dev, "eph861 touch IC can not suspend or resume");
        return -EINVAL;
    }

    return NOTIFY_OK;

}
#endif

static int eph_pinctrl_configure(struct eph_data *ephdata, bool enable)
{
    struct pinctrl_state *state;

    if (IS_ERR_OR_NULL(ephdata->pinctrl)) {
        EPH_WARN(&ephdata->commsdevice->dev, "Invalid pinctrl\n");
        return -EINVAL;
    }

    EPH_DBG(&ephdata->commsdevice->dev, "%s enable %d >>>\n", __func__, enable);

    if (enable) {
        state = pinctrl_lookup_state(ephdata->pinctrl, "ts_active");
        if (IS_ERR(state))
            EPH_ERR(&ephdata->commsdevice->dev, "Could not get ts_active pinstate!\n");
    } else {
        state = pinctrl_lookup_state(ephdata->pinctrl, "ts_suspend");
        if (IS_ERR(state))
            EPH_ERR(&ephdata->commsdevice->dev, "Could not get ts_suspend pinstate!\n");
    }
    if (!IS_ERR_OR_NULL(state))
        return pinctrl_select_state(ephdata->pinctrl, state);

    return 0;
}

#if (ESWIN_EPH861X_SPI)
static int eph_probe(struct comms_device *commsdevice)
#endif
#if (ESWIN_EPH861X_I2C)
static int eph_probe(struct comms_device *commsdevice, const struct comms_device_id *id)
#endif
{
    struct eph_data *ephdata;
    const struct eph_platform_data *ephplatform;
    int ret_val=0;
    int ret = 0;
    struct spi_delay d;

    EPH_DBG(&commsdevice->dev, "%s >>>\n", __func__);
    
    if (!strstr(panel_name_find, "epd8820_boe")) {
        EPH_DBG(&commsdevice->dev, "lcd name: %s\n", panel_name_find);
        return -ENODEV;
    }
    
    d.value = 100;
    d.unit = SPI_DELAY_UNIT_USECS;
    commsdevice->cs_setup = d;
    commsdevice->mode = 3;
    ret_val = eph_comms_specific_checks(commsdevice);
    if (ret_val)
    {
        return -EINVAL;
    }

    ephplatform = eph_platform_data_get(commsdevice);
    if (IS_ERR(ephplatform))
    {
        return PTR_ERR(ephplatform);
    }

    ephdata = (struct eph_data *)kzalloc(sizeof(struct eph_data), GFP_KERNEL);
    if (!ephdata)
    {
        return -ENOMEM;
    }
/*
    ephdata->bl = backlight_device_get_by_type(BACKLIGHT_RAW);

    if (ephdata->bl) {
        EPH_INFO(&commsdevice->dev, "backlight brightness %x\n", ephdata->bl->props.brightness);
        EPH_INFO(&commsdevice->dev, "backlight max brightness %x\n", ephdata->bl->props.max_brightness);
    } else {
        EPH_INFO(&commsdevice->dev, "backlight not ready\n");
        ret_val = -EPROBE_DEFER;
        goto err_free_mem;luxi
    }
*/
    INIT_WORK(&ephdata->force_baseline_work, eph_trigger_baseline_work);

    ephdata->commsdevice = commsdevice;
    ephdata->ephplatform = ephplatform;
    eph_comms_driver_data_set(commsdevice, ephdata);

    ephdata->pinctrl = devm_pinctrl_get(&commsdevice->dev);
    if (IS_ERR_OR_NULL(ephdata->pinctrl)) {
        EPH_ERR(&commsdevice->dev, "Could not get pinctrl\n");
    } else {
        eph_pinctrl_configure(ephdata, true);
    }

    ephdata->spi_pinctrl = devm_pinctrl_get(&commsdevice->dev);
    if (IS_ERR_OR_NULL(ephdata->spi_pinctrl)) {
        EPH_ERR(&commsdevice->dev, "Failed to get spi pinctrl, please check dts");
        ephdata->spi_pinctrl = NULL;
        ephdata->pins_spi_default = NULL;
    }
    ephdata->pins_spi_default = pinctrl_lookup_state(ephdata->spi_pinctrl, "ts_spi_mode");
    if (IS_ERR_OR_NULL(ephdata->pins_spi_default)) {
        EPH_ERR(&commsdevice->dev, "Pin state[spi] not found");
        if (ephdata->spi_pinctrl) {
            devm_pinctrl_put(ephdata->spi_pinctrl);
        }
        ephdata->spi_pinctrl = NULL;
        ephdata->pins_spi_default = NULL;
    }

    if (ephdata->spi_pinctrl && ephdata->pins_spi_default) {
        ret = pinctrl_select_state(ephdata->spi_pinctrl, ephdata->pins_spi_default);
        if (ret < 0) {
            EPH_ERR(&commsdevice->dev, "Set spi pins to default state failed,ret=%d", ret);
        }
    }

    ret_val = eph_allocate_comms_memory(commsdevice, ephdata);


    if (ephdata->ephplatform->device_settings_name)
    {
        eph_update_file_name(&ephdata->commsdevice->dev,
                             &ephdata->device_settings_name,
                             ephdata->ephplatform->device_settings_name,
                             strlen(ephdata->ephplatform->device_settings_name));
    }

    if (ephdata->ephplatform->fw_name)
    {
        eph_update_file_name(&ephdata->commsdevice->dev,
                             &ephdata->fw_name,
                             ephdata->ephplatform->fw_name,
                             strlen(ephdata->ephplatform->fw_name));
    }

    EPH_INFO(&commsdevice->dev, "%s ephdata->fw_name: %s, ephdata->device_settings_name: %s \n", __func__, ephdata->fw_name, ephdata->device_settings_name);
    snprintf(touch_version, sizeof(touch_version),"EPH861 %s",ephdata->fw_name);
    init_completion(&ephdata->chg_completion);
    init_completion(&ephdata->reset_completion);


    mutex_init(&ephdata->comms_mutex);
    mutex_init(&ephdata->sysfs_report_buffer_lock);

    device_init_wakeup(&commsdevice->dev, true);

    ret_val = eph_gpio_setup(ephdata);
    if (ret_val)
    {
        goto err_free_irq;
    }

    ret_val = eph_acquire_irq(ephdata);
    if (ret_val)
    {
        goto err_free_mem;
    }

    ret_val = eph_probe_regulators(ephdata);
    if (ret_val)
    {
        goto err_free_irq;
    }

    /* Need to have IRQ disabled before calling eph_initialize() as it re-enables it */
    disable_irq(ephdata->chg_irq);

    ret_val = sysfs_create_group(&commsdevice->dev.kobj, &eph_fw_attr_group);
    if (ret_val)
    {
        EPH_ERR(&commsdevice->dev, "Failure %d creating fw sysfs group\n", ret_val);
        return ret_val;
    }

    proc_tp_test = proc_mkdir("touchscreen", NULL);
    if (!proc_tp_test) {
        EPH_ERR(&commsdevice->dev, "procfs(test) create fail");
	return -ENOMEM;
    } else {
        proc_create_data("fts_test", 0664, proc_tp_test, &tp_test_fops, ephdata);
        proc_create_data("fts_test_csv", 0664, proc_tp_test, &tp_proccsv_fops, ephdata);
        EPH_ERR(&commsdevice->dev, "procfs(test) create successfully");
    }

    ret_val = eph_initialize(ephdata);
    if (ret_val)
    {
        goto err_free_irq;
    }

#if defined(CONFIG_BOARD_FLORAL) // pixel4XL plat
    ephdata->notifier.notifier_call = eph_notifier_callback;
    ret_val = msm_drm_register_client(&ephdata->notifier);
    if (ret_val < 0)
        EPH_ERR(&commsdevice->dev, "Failure %d register msm drm client\n", ret_val);
    else
        EPH_INFO(&commsdevice->dev, "success register norifier\n");
#elif defined(CONFIG_BOARD_CLOUDRIPPER) // pixel7Pro plat
    ret_val = eph_register_panel_bridge(ephdata);
    if (ret_val < 0)
        EPH_ERR(&commsdevice->dev, "Failure %d panel bridge\n", ret_val);
    else
        EPH_INFO(&commsdevice->dev, "success register panel bridge\n");
#endif
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
    ephdata->notifier.notifier_call = eph_notifier_callback;
    ret = mtk_disp_notifier_register("eph_ts_notifier", & ephdata->notifier);
    if (ret < 0) {
        EPH_ERR(&commsdevice->dev, "mtk disp notifier callback fail\n");
    }else{
        EPH_INFO(&commsdevice->dev, "mtk disp notifier callback success\n");
    }
#elif IS_ENABLED(CONFIG_FB)
    ephdata->notifier.notifier_call = eph_notifier_callback;
    ret = fb_register_client(&ts_data->fb_notif);
    if (ret) {
        printk("EPH ERR [FB]Unable to register fb_notifier: %d", ret);
    }
#endif
    /* default disable gesture */
    ephdata->gesture_mode = 0x0;
    ephdata->gesture_wakeup_enable = false;

    ephdata->lp = false;
    ephdata->irq_wake = false;

    ephdata->pt_threshold_add = 8000;
    ephdata->pt_flatness_max = 8000;
    ephdata->pt_max_delta = 8000;
    ephdata->pt_max_diff_delta = 8000;

    EPH_INFO(&commsdevice->dev, "%s <\n", __func__);
    return 0;

err_free_irq:
    if (ephdata->chg_irq)
    {
        free_irq(ephdata->chg_irq, ephdata);
    }

    gpio_free(ephdata->ephplatform->gpio_reset);
    gpio_free(ephdata->ephplatform->gpio_chg_irq);
    if(ephdata->reg_vdd)
    {
        regulator_put(ephdata->reg_vdd);
    }
    if(ephdata->reg_avdd)
    {
        regulator_put(ephdata->reg_avdd);
    }
err_free_mem:
    kfree(ephdata);
    EPH_INFO(&commsdevice->dev, "%s error\n", __func__);
    return ret_val;
}

static void eph_remove(struct comms_device *commsdevice)
{
    struct eph_data *ephdata = eph_comms_driver_data_get(commsdevice);
    EPH_INFO(&commsdevice->dev, "%s >\n", __func__);

    if (proc_tp_test)
        remove_proc_subtree("touchscreen", NULL);

    sysfs_remove_group(&commsdevice->dev.kobj, &eph_fw_attr_group);
    eph_sysfs_mem_access_remove(ephdata);

    if (ephdata->spi_pinctrl) {
        devm_pinctrl_put(ephdata->spi_pinctrl);
        ephdata->spi_pinctrl = NULL;
    }

#if defined(CONFIG_BOARD_FLORAL)
    msm_drm_unregister_client(&ephdata->notifier);
#elif defined(CONFIG_BOARD_CLOUDRIPPER)
    eph_unregister_panel_bridge(ephdata);
#elif defined(CONFIG_DRM_MEDIATEK_V2)
    mtk_disp_notifier_unregister(&ephdata->notifier);
#endif

    if (ephdata->chg_irq)
    {
        free_irq(ephdata->chg_irq, ephdata);
    }

    gpio_free(ephdata->ephplatform->gpio_reset);
    gpio_free(ephdata->ephplatform->gpio_chg_irq);

    if(ephdata->reg_avdd)
    {
        regulator_put(ephdata->reg_avdd);
    }
    if(ephdata->reg_vdd)
    {
        regulator_put(ephdata->reg_vdd);
    }
    eph_unregister_input_device(ephdata);

#if (ESWIN_EPH861X_SPI && ESWIN_EPH861X_SPI_USE_DMA)
    dma_pool_free(pool_rx, ephdata->comms_receive_buf, ephdata->comms_dma_handle_rx);
    dma_pool_free(pool_tx, ephdata->comms_send_buf, ephdata->comms_dma_handle_tx);

    dma_pool_destroy(pool_rx);
    dma_pool_destroy(pool_tx);
#elif (ESWIN_EPH861X_SPI)
    kfree(ephdata->comms_receive_buf);
    kfree(ephdata->comms_send_buf);
#endif

    kfree(ephdata->comms_send_crc_buf);
    kfree(ephdata->report_buf);
    kfree(ephdata);
    
//    return 0;
}

static int eph_dev_enter_lp_mode(struct eph_data *ephdata)
{
    int ret_val = 0;
    struct device *dev = &ephdata->commsdevice->dev;

    if (ephdata->lp)
        return 0;

    if (!ephdata->lp) {
        if (!ephdata->irq_wake) {
            enable_irq_wake(ephdata->chg_irq);
            ephdata->irq_wake = true;
        }
        if (ephdata->gesture_wakeup_enable)
            ret_val = eph_gesture_mode_set(ephdata, ephdata->gesture_mode | BIT(0));

        ephdata->lp = true;
    }

    if (ret_val)
        EPH_ERR(dev, "Failed to enter lp mode (%d)\n", ret_val);

    return ret_val;
}

static int eph_dev_enter_normal_mode(struct eph_data *ephdata)
{
    int ret_val = 0;
    struct device *dev = &ephdata->commsdevice->dev;

    if (!ephdata->lp)
        return 0;

    if (ephdata->lp) {
        if (ephdata->irq_wake) {
            disable_irq_wake(ephdata->chg_irq);
            ephdata->irq_wake = false;
        }

        if (ephdata->gesture_wakeup_enable)
            ret_val = eph_gesture_mode_set(ephdata, ephdata->gesture_mode & (~BIT(0)));

        ephdata->lp = false;
    }

    if (ret_val)
        EPH_ERR(dev, "Failed to enter normal mode (%d)\n", ret_val);

    return ret_val;
}

static int __maybe_unused eph_suspend(struct device *dev)
{
    struct comms_device *commsdevice = eph_comms_device_get(dev);
    struct eph_data *ephdata = eph_comms_driver_data_get(commsdevice);

    EPH_DBG(&ephdata->commsdevice->dev, "%s >\n", __func__);

    if (!ephdata->inputdev)
    {
        return 0;
    }

    if (ephdata->suspended == true)
        return 0;

    cancel_work_sync(&ephdata->force_baseline_work);

    mutex_lock(&ephdata->inputdev->mutex);

    if (ephdata->inputdev->users)
    {
        (void)eph_stop(ephdata);
    }

    mutex_unlock(&ephdata->inputdev->mutex);

    eph_pinctrl_configure(ephdata, false);

    return 0;
}

static int __maybe_unused eph_resume(struct device *dev)
{
    struct comms_device *commsdevice = eph_comms_device_get(dev);
    struct eph_data *ephdata = eph_comms_driver_data_get(commsdevice);

    EPH_DBG(&ephdata->commsdevice->dev, "%s >\n", __func__);

    if (!ephdata->inputdev)
    {
        return 0;
    }

    if (ephdata->suspended == false)
        return 0;

    eph_pinctrl_configure(ephdata, true);

    mutex_lock(&ephdata->inputdev->mutex);

    if (ephdata->inputdev->users)
    {
        (void)eph_start(ephdata);
    }

    mutex_unlock(&ephdata->inputdev->mutex);

    /* TIC report gesture event need 150 ~ 170ms delay 200ms for irq
     * process gesture event
    */
    mdelay(200);

    {

        eph_clear_all_host_touch_slots(ephdata);
    }

    return 0;
}

static SIMPLE_DEV_PM_OPS(eph_pm_ops, eph_suspend, eph_resume);

#ifdef CONFIG_OF // Open Firmware (Device Tree)
static const struct of_device_id eph_of_match[] =
{
    { .compatible = "eswin,eph861x", },
    {},
};
MODULE_DEVICE_TABLE(of, eph_of_match);
#endif // CONFIG_OF

static const struct comms_device_id eph_id[] =
{
    { "eswin_eph861x", 0 },
    { }
};
MODULE_DEVICE_TABLE(comms_mode_type, eph_id);

static struct comms_driver eph_driver =
{
    .id_table   = eph_id,
    .probe      = eph_probe,
    .remove     = eph_remove,
    .driver = {
        .name   = "eswin_eph861x",
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(eph_of_match),
        .pm = &eph_pm_ops,
    },

};

module_comms_driver(eph_driver);

/* Module information */
MODULE_AUTHOR("chris.ollerenshaw@eswin.com>");
MODULE_DESCRIPTION("ESWIN EPH861 series Touchscreen driver");
MODULE_LICENSE("GPL");

