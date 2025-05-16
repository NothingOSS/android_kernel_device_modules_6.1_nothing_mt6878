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
#include <linux/i2c.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>

#include <eswin_eph861x_types.h>

#if (ESWIN_EPH861X_I2C)
#include <eswin_eph861x_i2c.h>

#define TLV_RESERVED_INVALID_TYPE  0xFF

//TODO file untested

int __eph_read_reg(struct eph_data *ephdata, u16 len, u8 *val)
{
    int ret_val;
    
    if ((I2C_APP_BUF_SIZE_READ) < len)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "Error reading from i2c: length greater than buffer size(%d)", len);
        return -ENOMEM;
    }

    ret_val = i2c_master_recv(ephdata->commsdevice, val, len);
    
    if(ret_val != len)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "%s: i2c_master_recv ret_val %d\n", __func__, ret_val);
        return -1;
    }

    return 0;
}

int __eph_write_reg(struct eph_data *ephdata, u16 len, u8 *val)
{
    int ret_val, retry;

    if (!val)
    {
        return -ENOMEM;
    }
    if ((I2C_APP_BUF_SIZE_WRITE) < len)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "Error writing to i2c: length greater than buffer size(%d)", len);
        return -ENOMEM;
    }

    for (retry = 0; retry < 2; retry++)
    {
        if (retry > 0)
        {
            EPH_ERR(&ephdata->commsdevice->dev, "%s: retry %d\n", __func__, retry);
            msleep(EPH_WAKEUP_TIME);
        }
        ret_val = i2c_master_send(ephdata->commsdevice, val, len);
        if (ret_val == len)
        {
            ret_val = 0;
            break;
        }
        EPH_ERR(&ephdata->commsdevice->dev, "%s: i2c_master_send ret_val %d\n", __func__, ret_val);
        ret_val = ret_val < 0 ? ret_val : -EIO;
    }

    return ret_val;
}

int eph_comms_write(struct eph_data *ephdata, u16 len, u8 *buf)
{
    int ret_val = 0;

    ret_val = __eph_write_reg(ephdata,
                                len,
                                buf);

    return ret_val;
}

int eph_comms_read(struct eph_data *ephdata, u16 len, u8 *buf)
{
    int ret_val = 0;


    ret_val = __eph_read_reg(ephdata,
                             len,
                             buf);

    if (ret_val < 0)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "Error occured(%d)", ret_val);
        return ret_val;
    }

    /* check valid read - (not a null or reserved message) */
    if (true == eph_is_report_null_report(buf) || (TLV_RESERVED_INVALID_TYPE == buf[0]))
    {
        /* Device response was not ready, try again */
        /* wait a delay for TIC to be ready - Additional delay as device was not previously ready */
        udelay(200);

        /* do a read */
        ret_val = __eph_read_reg(ephdata,
                             len,
                             buf);


        if (ret_val < 0)
        {
            return ret_val;
        }

        //EPH_INFO(&ephdata->commsdevice->dev,
        //        "ESWIN SPI READ RETRY------>type: %x length_LSB: %x length_MSB: %x data1: %x data2: %x data3: %x data4: %x ",
         //       buf[0], buf[1],buf[2],buf[3],buf[4],buf[5],buf[6]);

        if (true == eph_is_report_null_report(buf) || (TLV_RESERVED_INVALID_TYPE == buf[0]))
        {
            EPH_ERR(&ephdata->commsdevice->dev,
                    "Failed to read message --- No response type: %u ",
                    buf[0]);
            return -EIO;
        }

    }  
    
    return ret_val;
}

int eph_comms_specific_checks_i2c(struct comms_device *commsdevice)
{
  (void)commsdevice;
  return 0;
}

void eph_comms_driver_data_set_i2c(struct comms_device *commsdevice, struct eph_data *ephdata)
{

    snprintf(ephdata->phys, sizeof(ephdata->phys), "i2c-%u-%04x/input0", commsdevice->adapter->nr, commsdevice->addr);
    EPH_INFO(&commsdevice->dev, "%s %s\n", __func__, ephdata->phys);

    i2c_set_clientdata(commsdevice, ephdata);


  return;
}


struct eph_data* eph_comms_driver_data_get_i2c(struct comms_device *commsdevice)
{

    struct eph_data *ephdata = (struct eph_data *)i2c_get_clientdata(commsdevice);


  return ephdata;
}


struct comms_device* eph_comms_device_get_i2c(struct device *dev)
{

    struct comms_device *commsdevice = to_i2c_client(dev);


  return commsdevice;
}



int eph_comms_specific_bootloader_checks_i2c(struct eph_data *ephdata)
{
#if 0
    u8 appmode = ephdata->commsdevice->addr;
    u8 family_id = ephdata->ephinfo ? ephdata->ephinfo->device_family : 0;

    ephdata->bootloader_addr = 0x26u;

    if ((appmode!=0x4D) || (4 == ephdata->ephinfo->device_family))
    {
        EPH_ERR(&ephdata->commsdevice->dev, "Unrecognised device for boot id:%02x\n",
                ephdata->ephinfo->device_family);
    }

    EPH_INFO(&ephdata->commsdevice->dev, "Using bootloader addr:%02x\n",
             ephdata->bootloader_addr);
#else
    (void)ephdata;
#endif
    return 0;

}


bool eph_is_report_null_report(u8 *buff)
{

    bool is_null_report = false;

    if (0 == buff[0])
    {
        if((0 == buff[1]) && (0 == buff[2]))
        {
            is_null_report = true;
        }

    }

    return is_null_report;

}

#endif /* ESWIN_EPH861X_I2C */


