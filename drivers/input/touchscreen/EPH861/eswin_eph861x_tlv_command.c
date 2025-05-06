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
#include "eswin_eph861x_tlv_report.h"
#include "eswin_eph861x_tlv_command.h"
#include "eswin_debug.h"



int eph_read_device_information(struct eph_data *ephdata)
{
    int ret_val = 0;
    u8 retry = 0;
    u8 type_match = false;
    const struct tlv_header tlvheader = {1, 0};


    ret_val = eph_comms_write(ephdata,
                              TLV_HEADER_SIZE,
                              (u8*)&tlvheader);

    EPH_INFO(&ephdata->commsdevice->dev,
             "ESWIN - Device information request::eph_comms_write type: %d, length: %d, ret_val %d ",
             tlvheader.type, tlvheader.length, ret_val);
    if (ret_val)
    {
        return ret_val;
    }

    /* wait a delay for TIC to be ready */
    udelay(100);

    while (false ==  type_match)
    {
        memset(ephdata->comms_receive_buf, 0, COMMS_BUF_SIZE);
        memset(&ephdata->ephdeviceinfo, 0 , sizeof(struct eph_device_info));

        ret_val = eph_comms_two_stage_read(ephdata, ephdata->comms_receive_buf);

        if (!ret_val)
        {
            u8 calc_crc;

            type_match = (TLV_DEVICE_INFO_DATA ==  ephdata->comms_receive_buf[0]);

            if(true == type_match)
            {
                /* Only copy the amount for the expected message type and its storage */
                /* Copy across regardless of CRC failure - rely on retry mechanism */
                memcpy((u8*)&ephdata->ephdeviceinfo, ephdata->comms_receive_buf, sizeof(struct eph_device_info));
            }

            /* CRC of payload */
            calc_crc = eph_get_data_crc((u8*)&ephdata->ephdeviceinfo.product_id, (sizeof(struct eph_device_info) - (TLV_HEADER_SIZE + sizeof(u8))));
            if (ephdata->ephdeviceinfo.crc != calc_crc)
            {
                EPH_ERR(&ephdata->commsdevice->dev, "CRC mismatch - expected: %d, got: %d", calc_crc, ephdata->ephdeviceinfo.crc);
                type_match= false;
            }

            if (false == type_match) {
                ret_val = eph_handle_report(ephdata, ephdata->comms_receive_buf);
                EPH_INFO(&ephdata->commsdevice->dev, "Process unexpected response %d\n", ret_val);
            }
        }

        if(ret_val || ((retry > COMMS_READ_RETRY_NUM) && (false ==  type_match)))
        {   
            /* have already retried and failed to get the correct response - break out the loop */
            EPH_ERR(&ephdata->commsdevice->dev, "Failed to read expected response on second retry");
            return -EAGAIN;
        }
        else
        {
            /* increment retry each attempt */
            retry++;
        }
    }


    return ret_val;
}


int eph_write_control_config(struct eph_data *ephdata, u16 len, u8 *buf)
{
    int ret_val = 0;
    struct eph_device_control_config_write_response response;
    bool type_match = false;
    u8 retry = 0;
    /* calculate this before CRC adjustment */
    u16 payload_length = (len - (TLV_HEADER_SIZE + TLV_WRITE_HEADER_SIZE));

    /* wait a delay for TIC to be ready */
    udelay(50);

    memcpy(ephdata->comms_send_crc_buf, buf, len);
    /* Dynamically handle the protocol version of the TIC FW */
    if(ephdata->ephdeviceinfo.protocol_version > CONFIG_ESWIN_TOUCH_PROTOCOL_VERSION_2)
    {
        /* CRC last byte for protocol version 3 */
        u8 calc_crc;
        struct tlv_header* header;
        header = (struct tlv_header*)ephdata->comms_send_crc_buf;
        /* Protocol version supports CRC in cmd increase payload length and insert CRC */
        header->length = header->length + cpu_to_le16(1);
        calc_crc = eph_get_data_crc((u8*)ephdata->comms_send_crc_buf, len);
        memcpy(&ephdata->comms_send_crc_buf[len], &calc_crc, sizeof(u8));
        /* increase transaction length to include crc */
        len++;
    }


    ret_val = eph_comms_write(ephdata, len, ephdata->comms_send_crc_buf);

    EPH_INFO(&ephdata->commsdevice->dev,
             "ESWIN - Write configuration/control command. type: %d, length: %d",
             ephdata->comms_send_crc_buf[0], 
             (ephdata->comms_send_crc_buf[1] | ephdata->comms_send_crc_buf[2] << 8u));  

    /* wait a delay for TIC to be ready - Extended for large 1k components */
    udelay(200);
    
    if (ret_val)
    {
        return ret_val;
    }


    while (false ==  type_match)
    {
        memset(ephdata->comms_receive_buf, 0, COMMS_BUF_SIZE);

        /* Do a 2 stage read, even though we know the format to allow consistant error handling when incorrect response received */
        /* This way if its an incorrect message format the complete message can be read out and the response can be discarded */
        ret_val = eph_comms_two_stage_read(ephdata, ephdata->comms_receive_buf);


        if (!ret_val)
        {
            /* Only copy the amount for the expected message type and its storage */
            memcpy((u8*)&response, ephdata->comms_receive_buf, sizeof(struct eph_device_control_config_write_response));

            /* check bytes written matches what was sent */
            if (le16_to_cpu(response.bytes_written) != payload_length)
            {
                EPH_ERR(&ephdata->commsdevice->dev, "Error writing to control config bytes written did not match what was sent");
                EPH_ERR(&ephdata->commsdevice->dev, 
                        "Sent the following number of bytes (%d) but (%d) bytes where written", 
                        payload_length, le16_to_cpu(response.bytes_written));
            }

            type_match = (TLV_CONFIG_DATA_WRITE ==  response.header.type) || (TLV_CONTROL_DATA_WRITE ==  response.header.type);    

            if (type_match == false) {
                ret_val = eph_handle_report(ephdata, ephdata->comms_receive_buf);
                EPH_INFO(&ephdata->commsdevice->dev, "Process unexpected response %d\n", ret_val);
            }
        }

        

        if(ret_val || ((retry > COMMS_READ_RETRY_NUM) && (false ==  type_match)))
        {   
            /* have already retried and failed to get the correct response - break out the loop */
            EPH_ERR(&ephdata->commsdevice->dev, "Failed to read expected response on second retry");
            break;
        }
        else
        {
            /* increment retry each attempt */
            retry++;
        }
    }

        /* retry */
        if ((ret_val) || (false ==  type_match))
        {
            /* wait a delay for TIC to be ready */
            mdelay(10);

            EPH_ERR(&ephdata->commsdevice->dev, "Control or contfiguration write failure. Type Match: (%d)", type_match);
            /* error or incorrect type received */
            return -EIO;
        }

        /* wait a delay for TIC to be ready, TIC may have prepared touch report so
         * give it time to prepare before the interrupt handler responds  */
        udelay(200);

    return ret_val;
}


int eph_write_control_config_no_response(struct eph_data *ephdata, u16 len, u8 *buf)
{
    int ret_val = 0;

    /* wait a delay for TIC to be ready */
    udelay(50);

    memcpy(ephdata->comms_send_crc_buf, buf, len);
    /* Dynamically handle the protocol version of the TIC FW */
    if(ephdata->ephdeviceinfo.protocol_version > CONFIG_ESWIN_TOUCH_PROTOCOL_VERSION_2)
    {
        /* CRC last byte for protocol version 3 */
        u8 calc_crc;
        struct tlv_header* header;
        header = (struct tlv_header*)ephdata->comms_send_crc_buf;
        /* Protocol version supports CRC in cmd increase payload length and insert CRC */
        header->length = header->length + cpu_to_le16(1);
        calc_crc = eph_get_data_crc((u8*)ephdata->comms_send_crc_buf, len);
        memcpy(&ephdata->comms_send_crc_buf[len], &calc_crc, sizeof(u8));
        /* increase transaction length to include crc */
        len++;
    }

    ret_val = eph_comms_write(ephdata, len, buf);

    EPH_INFO(&ephdata->commsdevice->dev,
             "ESWIN - Write configuration/control command. type: %d, length: %d",
             ephdata->comms_send_crc_buf[0],
             (ephdata->comms_send_crc_buf[1] | ephdata->comms_send_crc_buf[2] << 8u));  

    /* wait a delay for TIC to be ready */
    udelay(100);
    
    if (ret_val)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "Control or contfiguration write failure.");
        return ret_val;
    }

    return ret_val;
}

int eph_read_control_config(struct eph_data *ephdata, struct eph_device_control_config_read_command *command_request, u8 *buf)
{
    int ret_val = 0;
    bool type_match = false;
    u8 retry = 0;
    struct tlv_header tlvheader;
    u16 len = sizeof(struct eph_device_control_config_read_command);


    memcpy(ephdata->comms_send_crc_buf, command_request, len);
    /* Dynamically handle the protocol version of the TIC FW */
    if(ephdata->ephdeviceinfo.protocol_version > CONFIG_ESWIN_TOUCH_PROTOCOL_VERSION_2)
    {
        /* CRC last byte for protocol version 3 */
        u8 calc_crc;
        struct tlv_header* header;
        header = (struct tlv_header*)ephdata->comms_send_crc_buf;
        /* Protocol version supports CRC in cmd increase payload length and insert CRC */
        header->length = header->length + cpu_to_le16(1);
        calc_crc = eph_get_data_crc((u8*)ephdata->comms_send_crc_buf, (sizeof(struct eph_device_control_config_read_command)));
        memcpy(&ephdata->comms_send_crc_buf[len], &calc_crc, sizeof(u8));
        /* increase transaction length to include crc */
        len++;
    }


    /* do a write */
    ret_val = eph_comms_write(ephdata, len, (u8*)ephdata->comms_send_crc_buf);


    EPH_INFO(&ephdata->commsdevice->dev,
             "ESWIN - Read configuration/control command. type: %d, length: %d",
             command_request->header.type, command_request->header.length);  

    /* wait a delay for TIC to be ready - Extended for large 1k components */
    udelay(200);
    
    if (ret_val)
    {
        return ret_val;
    }


    while (false ==  type_match)
    {
        memset(ephdata->comms_receive_buf, 0, COMMS_BUF_SIZE);

        /* Do a 2 stage read, even though we know the format to allow consistant error handling when incorrect response received */
        /* This way if its an incorrect message format the complete message can be read out and the response can be discarded */
        ret_val = eph_comms_two_stage_read(ephdata, ephdata->comms_receive_buf);


        if (!ret_val)
        {
            /* Only copy the amount for the expected message type and its storage */
            memcpy((u8*)&tlvheader, ephdata->comms_receive_buf, sizeof(struct tlv_header));

            type_match = (TLV_CONFIG_DATA_READ ==  tlvheader.type) || (TLV_CONTROL_DATA_READ ==  tlvheader.type);

            if (type_match == false) {
                ret_val = eph_handle_report(ephdata, ephdata->comms_receive_buf);
                EPH_INFO(&ephdata->commsdevice->dev, "Process unexpected response %d\n", ret_val);
            } else {
                /* no functionality required yet so just print some response bytes */
                EPH_INFO(&ephdata->commsdevice->dev, "type: %u length: %u ", tlvheader.type, tlvheader.length);

            /* print 4 bytes of payload */
                EPH_INFO(&ephdata->commsdevice->dev,
                        "payload byte 1: %d payload byte 2: %d payload byte 3: %d payload byte 4: %d",
                        ephdata->comms_receive_buf[4], ephdata->comms_receive_buf[5], ephdata->comms_receive_buf[6], ephdata->comms_receive_buf[7]);

                /* Copy read value into dedicated buffer if needed */
                //TODO this function not currently needed but has been left is as will likely be needed.
            }
        }

        if(ret_val || ((retry > COMMS_READ_RETRY_NUM) && (false ==  type_match)))
        {   
            /* have already retried and failed to get the correct response - break out the loop */
            EPH_ERR(&ephdata->commsdevice->dev, "Failed to read expected response on second retry");
            break;
        }
        else
        {
            /* increment retry each attempt */
            retry++;
        }
    }

    /* retry */
    if ((ret_val) || (false ==  type_match))
    {
        /* wait a delay for TIC to be ready */
        mdelay(10);

        EPH_ERR(&ephdata->commsdevice->dev, "Control or contfiguration write failure. Type Match: (%d)", type_match);
        /* error or incorrect type received */
        return -EIO;
    }

    return ret_val;
}


int eph_read_bootloader_information(struct eph_data *ephdata)
{
    int ret_val = 0;
    u8 retry = 0;
    u8 type_match = false;
    const struct tlv_header tlvheader = {1, 0};


    ret_val = eph_comms_write(ephdata,
                              TLV_HEADER_SIZE,
                              (u8*)&tlvheader);

    EPH_INFO(&ephdata->commsdevice->dev,
             "ESWIN - Boot information request type: %d, length: %d",
             tlvheader.type, tlvheader.length);  

    if (ret_val)
    {
        return ret_val;
    }

    /* wait a delay for TIC to be ready */
    udelay(100);

    while (false ==  type_match)
    {

        ret_val = eph_comms_two_stage_read(ephdata, ephdata->comms_receive_buf);

        if (!ret_val)
        {
            u8 calc_crc;
            type_match = (TLV_DEVICE_INFO_DATA ==  ephdata->comms_receive_buf[0]);

            if(type_match)
            {
                /* Only copy the amount for the expected message type and its storage */
                /* Copy across regardless of CRC failure - rely on retry mechanism */
                memcpy((u8*)&ephdata->ephdeviceinfo, ephdata->comms_receive_buf, sizeof(struct eph_device_info));
            }

            /* CRC of payload */
            calc_crc = eph_get_data_crc((u8*)&ephdata->ephdeviceinfo.product_id, (sizeof(struct eph_device_info) - (TLV_HEADER_SIZE + sizeof(u8))));
            if (ephdata->ephdeviceinfo.crc != calc_crc)
            {
                EPH_ERR(&ephdata->commsdevice->dev, "CRC mismatch - expected: %d, got: %d", calc_crc, ephdata->ephdeviceinfo.crc);
                type_match= false;
            }
       
        }

        if(ret_val || ((retry > COMMS_READ_RETRY_NUM) && (false ==  type_match)))
        {   
            /* have already retried and failed to get the correct response - break out the loop */
            EPH_ERR(&ephdata->commsdevice->dev, "Failed to read expected response on second retry");
            break;
        }
        else
        {
            /* increment retry each attempt */
            retry++;
        }
    }


    return ret_val;
}


int eph_write_engineering_data(struct eph_data *ephdata, u16 len, u8 *buf)
{
    int ret_val = 0;
    struct eph_device_eng_data_write_response response;
    bool type_match = false;
    u8 retry = 0;
    /* calculate this before CRC adjustment */
    u16 payload_length = (len - sizeof(struct eph_device_eng_data_write_command));

    /* wait a delay for TIC to be ready */
    udelay(50);

    ret_val = eph_comms_write(ephdata, len, buf);

    EPH_INFO(&ephdata->commsdevice->dev,
             "ESWIN - Write engineering data. type: %d, length: %d",
             buf[0], (buf[1] | buf[2] << 8u));  

    /* wait a delay for TIC to be ready */
    udelay(100);
    
    if (ret_val)
    {
        EPH_ERR(&ephdata->commsdevice->dev, "Write engineering frame failure. ");
        return ret_val;
    }

    while (false ==  type_match)
    {
        memset(ephdata->comms_receive_buf, 0, COMMS_BUF_SIZE);

        /* Do a 2 stage read, even though we know the format to allow consistant error handling when incorrect response received */
        /* This way if its an incorrect message format the complete message can be read out and the response can be discarded */
        ret_val = eph_comms_two_stage_read(ephdata, ephdata->comms_receive_buf);

       if (!ret_val)
        {
            /* Only copy the amount for the expected message type and its storage */
            memcpy((u8*)&response, ephdata->comms_receive_buf, sizeof(struct eph_device_eng_data_write_response));

            /* check bytes written matches what was sent */
            if (le16_to_cpu(response.bytes_written) != payload_length)
            {
                EPH_ERR(&ephdata->commsdevice->dev, "Error writing to eng data: bytes written did not match what was sent");
                EPH_ERR(&ephdata->commsdevice->dev, "Sent the following number of bytes (%d) but (%d) bytes where written", payload_length, le16_to_cpu(response.bytes_written));
            }

            type_match = (TLV_ENG_DEBUG_DATA_WRITE ==  response.header.type);    
            if (type_match == false) {
                ret_val = eph_handle_report(ephdata, ephdata->comms_receive_buf);
                EPH_INFO(&ephdata->commsdevice->dev, "Process unexpected response %d\n", ret_val);
            }
        }

        /* Have 4 Retries at waiting for response */
        if(ret_val || ((retry > COMMS_READ_RETRY_NUM) && (false ==  type_match)))
        {   
            /* have already retried and failed to get the correct response - break out the loop */
            EPH_ERR(&ephdata->commsdevice->dev, "Failed to read expected response on retry");
            break;
        }
        else
        {
            /* increment retry each attempt */
            retry++;
        }
    }

        /* retry */
        if ((ret_val) || (false ==  type_match))
        {
            /* wait a delay for TIC to be ready */
            mdelay(10);

            EPH_ERR(&ephdata->commsdevice->dev, "Write engineering failure. Type Match: (%d)", type_match);
            /* error or incorrect type received */
            return -EIO;
        }


    return ret_val;
}
