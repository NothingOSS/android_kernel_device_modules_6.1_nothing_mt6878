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
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/dma-mapping.h>

#include "eswin_eph861x_tlv.h"
#include "eswin_eph861x_types.h"
#if (ESWIN_EPH861X_SPI)
#include "eswin_eph861x_spi.h"
#include "eswin_debug.h"
#define TLV_RESERVED_INVALID_TYPE  0xFF



u8 spi_tx_dummy_buf[SPI_APP_BUF_SIZE_READ];




int __eph_spi_read(struct eph_data *ephdata, u16 len, u8 *val)
{
    int ret_val;
    struct spi_message  spimsg;
    struct spi_transfer spitr;

    /* Read header - T(ype) and L(ength) */
    spi_message_init(&spimsg);
    memset(&spitr, 0,  sizeof(struct spi_transfer));


    if ((SPI_APP_BUF_SIZE_READ) < len)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "Error reading from spi: length greater than buffer size(%d)", len);
        return -ENOMEM;
    }

    /* Ensure dummy tx configured */
    memcpy(ephdata->comms_send_buf, &spi_tx_dummy_buf[0], len);


    /* Output 0xFFs when doing a read */
    spitr.tx_buf = ephdata->comms_send_buf;
    spitr.rx_buf = ephdata->comms_receive_buf;
    spitr.len = len;
    spitr.tx_dma = ephdata->comms_dma_handle_tx;
    spitr.rx_dma = ephdata->comms_dma_handle_rx;
    spimsg.is_dma_mapped = 1;

    //TODO Kernel version 5.5 introduces support for CS to CLK delay which will be useful when implementing low power sleep modes
    spi_message_add_tail(&spitr, &spimsg);
    ret_val = spi_sync(ephdata->commsdevice, &spimsg);

    if (ret_val < 0)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "Error reading from spi (%d)", ret_val);
        return ret_val;
    }       


    /* include T and L */
    memcpy(val, ephdata->comms_receive_buf, len);
    return 0;
}


int __eph_spi_write(struct eph_data *ephdata, u16 len, const u8 *val)
{
    int ret_val;
    struct spi_message  spimsg;
    struct spi_transfer spitr;
 

    if ((SPI_APP_BUF_SIZE_WRITE) < len)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "Error writing to spi: length greater than buffer size(%d)", len);
        return -ENOMEM;
    }

    /* SPI_WRITE */
    memcpy(ephdata->comms_send_buf, val, len);
    spi_message_init(&spimsg);
    memset(&spitr, 0,  sizeof(struct spi_transfer));
    spitr.tx_buf = ephdata->comms_send_buf;
    spitr.rx_buf = ephdata->comms_receive_buf;
    spitr.len = len;
#if (ESWIN_EPH861X_SPI_USE_DMA)
    spitr.tx_dma = ephdata->comms_dma_handle_tx;
    spitr.rx_dma = ephdata->comms_dma_handle_rx;
    spimsg.is_dma_mapped = 1;
#endif
    spi_message_add_tail(&spitr, &spimsg);
    ret_val = spi_sync(ephdata->commsdevice, &spimsg);
    if (ret_val < 0)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "Error writing to spi (%d)", ret_val);
        return ret_val;
    }

    return 0;
}


int eph_comms_read(struct eph_data *ephdata, u16 len, u8 *buf)
{
    int ret_val = 0;


    if ((SPI_APP_BUF_SIZE_READ) < len)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "Error reading from spi: length greater than buffer size(%d)", len);
        return -ENOMEM;
    }

    ret_val = __eph_spi_read(ephdata,
                             len,
                             buf);

    if (ret_val)
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
        ret_val = __eph_spi_read(ephdata,
                                 len,
                                 buf);

        if (ret_val)
        {
            return ret_val;
        }

        EPH_INFO(&ephdata->commsdevice->dev,
                "ESWIN SPI READ RETRY------>type: %u length_LSB: %u length_MSB: %u data1: %u data2: %u data3: %u data4: %u ",
                buf[0], buf[1],buf[2],buf[3],buf[4],buf[5],buf[6]);

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


int eph_comms_write(struct eph_data *ephdata, u16 len, u8 *buf)
{
    int ret_val = 0;

    ret_val = __eph_spi_write(ephdata,
                                  len,
                                  buf);


    return ret_val;
}


int eph_comms_specific_checks_spi(struct comms_device *commsdevice)
{

    if ( (commsdevice->bits_per_word && commsdevice->bits_per_word != 8) ||
         !(commsdevice->mode & SPI_CPHA) ||
         !(commsdevice->mode & SPI_CPOL) )
    {
        EPH_ERR(&commsdevice->dev, "unexpected spi device setup: SPI mode(%d), bits_per_word(%d) \n", commsdevice->mode, commsdevice->bits_per_word);
       // return -EINVAL;
    }

    /* initialise SPI dummy buffers */
    memset(&spi_tx_dummy_buf[0], 0xFF, SPI_APP_BUF_SIZE_READ);

  return 0;
}

void eph_comms_driver_data_set_spi(struct comms_device *commsdevice, struct eph_data *ephdata)
{

    snprintf(ephdata->phys, sizeof(ephdata->phys), "spi-%d/input0", commsdevice->master->bus_num);
    EPH_INFO(&commsdevice->dev, "%s %s\n", __func__, ephdata->phys);

    spi_set_drvdata(commsdevice, ephdata);
    if (0xff != spi_tx_dummy_buf[0])
    {
        memset(spi_tx_dummy_buf, 0xff, SPI_APP_BUF_SIZE_READ);
    }

  return;
}


struct eph_data* eph_comms_driver_data_get_spi(struct comms_device *commsdevice)
{

    struct eph_data *ephdata = (struct eph_data *)spi_get_drvdata(commsdevice);

  return ephdata;
}


struct comms_device* eph_comms_device_get_spi(struct device *dev)
{

    struct comms_device *commsdevice = to_spi_device(dev);


  return commsdevice;
}

int eph_comms_specific_bootloader_checks_spi(struct eph_data *ephdata)
{
    (void)ephdata;
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


#endif /*   ESWIN_EPH861X_SPI  */










