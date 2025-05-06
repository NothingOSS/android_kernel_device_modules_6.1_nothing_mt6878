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
#include <linux/kernel.h>
#include <uapi/linux/input-event-codes.h>

#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/gfp.h>
#include <linux/delay.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>

#include "eswin_eph861x_tlv.h"
#include "eswin_eph861x_types.h"
#include "eswin_eph861x_project_config.h"
#include "eswin_eph861x_comms.h"
#include "eswin_debug.h"

struct dma_pool *pool_rx;
struct dma_pool *pool_tx;


struct tlv_header eph_get_tl_header_info(struct eph_data *ephdata, u8 *message)
{
    struct tlv_header tlvheader;

    tlvheader.type = message[TLV_TYPE_FIELD];
    tlvheader.length = (u16)min((int)((int)(message[TLV_LENGTH_FIELD] | ((u16)message[(TLV_LENGTH_FIELD+1)] << 8u)) + TLV_HEADER_SIZE), 0xFFFF); // little endian 16

    return tlvheader;
}


int eph_comms_two_stage_read(struct eph_data *ephdata, u8 *buf)
{
    int ret_val = 0;
    struct tlv_header tlvheader = {0};
    u16 length;

    /* River will do TL then TLV so header length - Decode length etc */
    /* offset not used for header */
    // note: SPI has difference with i2c, SPI read success return 0,but i2c retutn read count // add by hh
    ret_val = eph_comms_read(ephdata, sizeof(struct tlv_header),(u8*)&tlvheader);
    EPH_DBG(&ephdata->commsdevice->dev,
             "ESWIN - eph_comms_two_stage_read first stage type: %d, length: %d, ret_val %d ",
             tlvheader.type, tlvheader.length, ret_val);

    if (!ret_val)
    {
        /* saturate to 16 bit type */
        /* if device isnt connected then this can overflow without the saturation */
        length = (u16)min((int)((int)le16_to_cpu(tlvheader.length) + TLV_HEADER_SIZE), 0xFFFF);

        /* wait a delay for TIC to be ready */
        udelay(50);

        EPH_INFO(&ephdata->commsdevice->dev,
                 "eph_comms_two_stage_read: type: %u length: %u ",
                 tlvheader.type, length);  

        /* do a read */
        /* Read control configuration response */
        ret_val = eph_comms_read(ephdata, length, buf);
        EPH_DBG(&ephdata->commsdevice->dev,
                 "eph_comms_two_stage_read: ESWIN - eph_comms_two_stage_read second stage ret_val %d, Type 0x%x, length_LSB 0x%x, length_MSB 0x%x, buff[3] 0x%x, buff[4] 0x%x, buff[5] 0x%x, buff[6] 0x%x, buff[7] 0x%x, buff[8] 0x%x, buff[9] 0x%x, buff[10] 0x%x, buff[11] 0x%x",
                 ret_val, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
        if (ret_val)
        {
            EPH_ERR(&ephdata->commsdevice->dev, "eph_comms_two_stage_read: Failed to read payload: (%d)", ret_val);
        }
    }
    else
    {
       EPH_ERR(&ephdata->commsdevice->dev, "eph_comms_two_stage_read: Failed to read header: (%d)", ret_val);
    }

    return ret_val;
}



int eph_read_report(struct eph_data *ephdata, u8 *buf)
{
    int ret_val = 0;

    /* Part of interrupt handling - No error handling. If message cant be decoded then it will be discsarded */
    /* no read offset required ----- always part of the command aspect */
    ret_val = eph_comms_two_stage_read(ephdata, buf);


    return ret_val;

}


u8 eph_read_chg(struct eph_data *ephdata)
{
    u8 ret_val = (u8)gpio_get_value(ephdata->ephplatform->gpio_chg_irq);
    return ret_val;
}

int eph_wait_for_chg(struct eph_data *ephdata)
{
    int timeout_counter = 25000; // 500 msec

    while (0 != eph_read_chg(ephdata) && timeout_counter > 0)
    {
        timeout_counter--;
        udelay(20);
    }

    return timeout_counter > 0 ? 0 : -1;
}



int eph_comms_specific_checks(struct comms_device *commsdevice)
{
#if (ESWIN_EPH861X_SPI)
    int ret_val = eph_comms_specific_checks_spi(commsdevice);
#endif
#if (ESWIN_EPH861X_I2C)
    int ret_val = eph_comms_specific_checks_i2c(commsdevice);
#endif
#if ((ESWIN_EPH861X_SPI) && (ESWIN_EPH861X_I2C))
    #error " Must select only SPI or I2C "
#endif
#if (!((ESWIN_EPH861X_SPI) || (ESWIN_EPH861X_I2C)))
    #error " Must select SPI or I2C "
#endif

  return ret_val;
}

void eph_comms_driver_data_set(struct comms_device *commsdevice, struct eph_data *ephdata)
{

#if (ESWIN_EPH861X_SPI)
    eph_comms_driver_data_set_spi(commsdevice, ephdata);
#endif
#if (ESWIN_EPH861X_I2C)
    eph_comms_driver_data_set_i2c(commsdevice, ephdata);
#endif

  return;
}


struct eph_data* eph_comms_driver_data_get(struct comms_device *commsdevice)
{

#if (ESWIN_EPH861X_SPI)
    struct eph_data* ephdata = eph_comms_driver_data_get_spi(commsdevice);
#endif
#if (ESWIN_EPH861X_I2C)
    struct eph_data* ephdata = eph_comms_driver_data_get_i2c(commsdevice);
#endif

  return ephdata;
}


struct comms_device* eph_comms_device_get(struct device *dev)
{

#if (ESWIN_EPH861X_SPI)
    struct comms_device *commsdevice = eph_comms_device_get_spi(dev);
#endif
#if (ESWIN_EPH861X_I2C)
    struct comms_device *commsdevice = eph_comms_device_get_i2c(dev);
#endif

  return commsdevice;
}

char* eph_comms_devicename_get(struct eph_data *ephdata)
{
  char * commsdevice_name;

#if (ESWIN_EPH861X_SPI)
  commsdevice_name = ephdata->commsdevice->modalias;
#endif
#if (ESWIN_EPH861X_I2C)
  commsdevice_name = ephdata->commsdevice->name;
#endif

  return commsdevice_name;
}

int eph_comms_specific_bootloader_checks(struct eph_data *ephdata)
{

#if (ESWIN_EPH861X_SPI)
    int ret_val = eph_comms_specific_bootloader_checks_spi(ephdata);
#endif
#if (ESWIN_EPH861X_I2C)
    int ret_val = eph_comms_specific_bootloader_checks_i2c(ephdata);
#endif

    return ret_val;
}


u8 eph_get_data_crc(uint8_t* p_msg, size_t size)
{
   u8 calc_crc = 0;
   int i;
   for (i = 0; i < size; i++)
   {
      calc_crc = get_crc8_iter(calc_crc, p_msg[i]);
   }
   return calc_crc;
}


u8 get_crc8_iter(u8 crc, u8 data)
{
    static const u8 crcpoly = 0x8c;
    u8 index = 8;
    u8 fb;
    do
    {
        fb = (crc ^ data) & 0x01;
        data >>= 1;
        crc >>= 1;
        if (fb)
        {
            crc ^= crcpoly;
        }
    } while (--index);
    return crc;
}

int eph_allocate_comms_memory(struct comms_device *commsdevice, struct eph_data *ephdata)
{
    int ret_val = 0;

    ephdata->comms_send_crc_buf = (u8*)kcalloc(COMMS_BUF_WRITE_SIZE, sizeof(ephdata->comms_send_crc_buf), GFP_KERNEL);
    if (NULL == ephdata->comms_send_crc_buf)
    {
        return -ENOMEM;
    }

    ephdata->report_buf = (u8*)kcalloc(COMMS_BUF_SIZE, sizeof(ephdata->report_buf), GFP_KERNEL);
    if (NULL == ephdata->report_buf)
    {
        return -ENOMEM;
    }

#if (ESWIN_EPH861X_SPI && ESWIN_EPH861X_SPI_USE_DMA)
    //TODO configuration for DMA should not be needed in generic driver and the following can be consolodated into single pool
    /* Configure DMA for SPI */
    arch_setup_dma_ops(&commsdevice->dev, 0, U64_MAX, NULL, true);

    ret_val = dma_coerce_mask_and_coherent(&commsdevice->dev,
                                            DMA_BIT_MASK(32));
    if (ret_val)
    {
        EPH_ERR(&commsdevice->dev, "%s. dma_coerce_mask_and_coherent failed\n", __func__);
        return -ENOMEM;
    }

    /* Create memory pool */
    pool_rx = dma_pool_create("create_comms_receive_buf", &ephdata->commsdevice->dev, COMMS_BUF_SIZE,                  
                              0 /* byte alignment */,
                              0 /* no page-crossing issues */);
    
    /* Allocate memory from pool */
    ephdata->comms_receive_buf = (u8*)dma_pool_alloc(pool_rx, GFP_KERNEL, &ephdata->comms_dma_handle_rx);
    if (NULL == ephdata->comms_receive_buf)
    {
        EPH_ERR(&commsdevice->dev, "%s comms_receive_buf memory allocation failed \n", __func__);
        return -ENOMEM;
    }

    /* Create memory pool */
    pool_tx = dma_pool_create("create_comms_send_buf", &ephdata->commsdevice->dev, COMMS_BUF_WRITE_SIZE,                  
                           0 /* byte alignment */,
                           0 /* no page-crossing issues */);

    /* Allocate memory from pool */
    ephdata->comms_send_buf = (u8*)dma_pool_alloc(pool_tx, GFP_KERNEL, &ephdata->comms_dma_handle_tx);
    if (NULL == ephdata->comms_send_buf)
    {
        EPH_ERR(&commsdevice->dev, "%s comms_send_buf memory allocation failed \n", __func__);
        return -ENOMEM;
    }
#elif (ESWIN_EPH861X_SPI)
    // only use spi not use dma
    ephdata->comms_receive_buf = (u8*)kcalloc(COMMS_BUF_SIZE, sizeof(ephdata->comms_receive_buf), GFP_KERNEL);
    ephdata->comms_send_buf = (u8*)kcalloc(COMMS_BUF_WRITE_SIZE, sizeof(ephdata->comms_receive_buf), GFP_KERNEL);
#endif

#if (ESWIN_EPH861X_I2C) // modified by hh just define spi here 
    ephdata->comms_receive_buf = (u8*)kcalloc(COMMS_BUF_SIZE, sizeof(ephdata->comms_receive_buf), GFP_KERNEL);
#endif
    return ret_val;

}
