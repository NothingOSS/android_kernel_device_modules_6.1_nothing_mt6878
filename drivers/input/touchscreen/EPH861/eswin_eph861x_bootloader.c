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
#include <linux/types.h>



#include <uapi/asm-generic/errno-base.h>
#include <linux/sysfs.h>

#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/kernel.h>
#include <uapi/linux/stat.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include "eswin_eph861x_project_config.h"
#include "eswin_eph861x_tlv.h"
#include "eswin_eph861x_types.h"
#include "eswin_eph861x_comms.h"
#include "eswin_eph861x_eswin.h"
#include "eswin_eph861x_bootloader.h"
#include "eswin_eph861x_tlv_command.h"
#include "eswin_debug.h"

/* Bootloader Component defintions */
#define BOOTLOADER_COMPONENT_ID             (0xFEFEu)

/* Start Command definitions */ 
#define START_COMMAND_SEQUENCE              (0xDCAAu)
#define START_COMMAND_SEQUENCE_0            (0xDCu)
#define START_COMMAND_SEQUENCE_1            (0xAAu)


#define BOOTLOADER_FRAME_CRC_SIZE           (2u)


typedef enum bootloader_state_tag
{
   bootloader_state_INVALID               = 0,
   bootloader_state_APP_CRC_FAILED        = 1,
   bootloader_state_WAITING_START_COMMAND = 2,
   bootloader_state_WAITING_FRAME_DATA    = 3,
   bootloader_state_PROCESS_FRAME         = 4
} bootloader_state_e;


static int eph_proc_bootloader_status_report(struct eph_data *ephdata, u8 *message, bootloader_state_e *status_byte);
static int eph_write_bootloader_frame(struct eph_data *ephdata, u16 payload_length, u8 *buf, bool last_frame);
static int eph_write_bootloader_start(struct eph_data *ephdata);



/**
 * \brief Send all the river firmware data from the firmware file.
 * \pre The file handle in the firmware context structure must already be open and
 *      be "pointing" to the beginning of the file.
 */
int eph_send_frames(struct eph_data *ephdata)
{
    struct device *dev = &ephdata->commsdevice->dev;
    struct eph_flash *ephflash = ephdata->ephflash;
    int ret_val;
    bool last_frame =  false;

    EPH_DBG(dev, "%s >\n", __func__);


    if (NULL == ephflash)
    {
        EPH_ERR(dev, "%s eph_flash = NULL - no fw image found\n", __func__);
        /* boot fail - restore chg function */
        (void)eph_bootloader_release_chg(ephdata);
        return EINVAL;
    }

    ret_val = eph_check_bootloader(ephdata, bootloader_state_WAITING_START_COMMAND);
    if (0 == ret_val)
    {
        EPH_INFO(dev, "Unlocking bootloader\n");
        ret_val = eph_write_bootloader_start(ephdata);
        (void)eph_bootloader_release_chg(ephdata);

        if (0 != ret_val)
        {
            EPH_ERR(dev, "Failured to unlock bootloader\n");
            return ret_val;
        }
        EPH_INFO(dev, "Bootloader unlocked\n");
    }
    else if (bootloader_state_WAITING_FRAME_DATA == ret_val)
    {
        EPH_INFO(dev, "Bootloader already unlocked\n");
    }
    else
    {
        EPH_ERR(dev, "Unexpected state\n");
        return -EINVAL;
    }

    EPH_INFO(dev, "Flashing device...\n");

    ephflash->frame_count = 1;
    ephflash->fw_pos = 0;
    while ((ephflash->fw_pos+BOOTLOADER_FRAME_CRC_SIZE) < (loff_t)ephflash->fw->size)
    {

        ephflash->pframe = (u8*)(ephflash->fw->data + ephflash->fw_pos);
        ephflash->frame_size = *ephflash->pframe << 8 | *(ephflash->pframe + 1);
        EPH_DBG(dev, "Frame %d: size %ld\n", ephflash->frame_count, ephflash->frame_size);

        /* Allow for CRC bytes at end of frame */
        ephflash->frame_size += BOOTLOADER_FRAME_CRC_SIZE;

        if (0 != eph_check_bootloader(ephdata, bootloader_state_WAITING_FRAME_DATA))
        {
            EPH_ERR(dev, "Unexpected bootloader state\n");
            return -EINVAL;
        }

        if((ephflash->fw_pos + BOOTLOADER_FRAME_CRC_SIZE + ephflash->frame_size) >= (loff_t)ephflash->fw->size)
        {
            /* we have reached the end of the firmware file after this frame */
            last_frame = true;
        }


        /* Write one frame to device */
        ret_val = eph_write_bootloader_frame(ephdata, ephflash->frame_size, ephflash->pframe, last_frame);
        if (0 != ret_val)
        {
            return ret_val;
        }

        EPH_DBG(dev, "frame: %d sent \n", ephflash->frame_count);
        ephflash->frame_count++;
        ephflash->fw_pos += ephflash->frame_size;

    }
    EPH_INFO(dev, "Sent %d frames, %lld bytes\n", ephflash->frame_count, ephflash->fw_pos);

    return 0;
}

/**
 * \brief Read bootloader status and check it is as expected.
 *
 * The boot loader returns a 1-byte status via 0x50 type TLV.
 * There are three status of the bootloader: application CRC fail, Waiting for a start bootloader command and 
 * Waiting for next frame of encrypted firwmare data.
 *
 * | Status | Meaning
 * | 0x01   | Application failed its CRC check. After the status is read, the device will then enter WAITING_START_COMMAND state,.
 * | 0x02   | Waiting for a start bootloader command.
 * | 0x03   | Waiting for next frame of encrypted firwmare data.
 *
 */
int eph_check_bootloader(struct eph_data *ephdata, bootloader_state_e expected_next_state)
{
    struct device *dev = &ephdata->commsdevice->dev;
    bootloader_state_e status_byte;
    int ret_val;

recheck:
    /* Wait for the /CHG line to be asserted by the bootloader to tell us that
     * it is ready to output data. The /CHG line is being used as a flow control
     * mechanism. Once the bootloader has data to output it will assert the /CHG. */
    if (0 != eph_wait_for_chg(ephdata))
    {
        EPH_ERR(dev, "Bootloader Timeout on CHG expected_next_state=%d\n", expected_next_state);
    }

    ret_val = eph_read_report(ephdata, ephdata->report_buf);
    if (0 != ret_val)
    {
        EPH_ERR(dev, "Bootloader read FAIL\n");
        return ret_val;
    }
    /* Process report */
    ret_val = eph_proc_bootloader_status_report(ephdata, ephdata->report_buf, &status_byte);
    EPH_DBG(dev, "Bootloader status_byte %d\n", status_byte);
    if ( (bootloader_state_WAITING_FRAME_DATA == expected_next_state) ||
         (bootloader_state_WAITING_START_COMMAND == expected_next_state) )
    {
        if((bootloader_state_WAITING_START_COMMAND == expected_next_state) &&
            (bootloader_state_APP_CRC_FAILED == status_byte))
        {
            /* Device is telling us the app had a crc fail -> do another read to move bootloader
             * state machine to waiting command */

            /* We expected to be waiting for frame data or waiting bootloader start 
             * but we got CRC fail so app failed its crc - start again and we should
            * be in waiting bootloader start */
            goto recheck;
        }
    }
    else
    {
        return -EINVAL;
    }

    if (status_byte != expected_next_state)
    {
        EPH_INFO(dev, "Invalid bootloader mode state %02X", status_byte);
        if ( (bootloader_state_WAITING_START_COMMAND == expected_next_state) &&
             (bootloader_state_WAITING_FRAME_DATA == status_byte) )
        {
            return bootloader_state_WAITING_FRAME_DATA;
        }
        if (bootloader_state_WAITING_START_COMMAND == expected_next_state)
        {
            eph_reset_device(ephdata);
        }
        return -EINVAL;
    }

    return 0;
}


int eph_chg_force_bootloader(struct eph_data *ephdata)
{
    int ret_val;
    struct device *dev = &ephdata->commsdevice->dev;
    /* NOTE: interrupts on CHG should be disabled before calling this function */

    gpio_set_value(ephdata->ephplatform->gpio_reset, GPIO_RESET_YES_LOW);
    ret_val = gpio_direction_output(ephdata->ephplatform->gpio_chg_irq, CHG_HELD_LOW);
    /* wait some time for the reset to be seen */
    msleep(EPH_RESET_HOLD_TIME);
    gpio_set_value(ephdata->ephplatform->gpio_reset, GPIO_RESET_NO_HIGH);
    msleep((EPH_POWERON_DELAY));

    EPH_INFO(dev, "%s, Forced device into bootloader mode. ret_val: %d\n",  __func__, ret_val);

    return ret_val;
}


int eph_bootloader_release_chg(struct eph_data *ephdata)
{
   int ret_val;
   ret_val = gpio_direction_input(ephdata->ephplatform->gpio_chg_irq);
   return ret_val;
}


int eph_app_force_bootloader_mode(struct eph_data *ephdata)
{
    int ret_val;
    u8 cfg[8] =    {TLV_CONTROL_DATA_WRITE,                                                      /* Type */
                    0x05, 0x00,                                                                  /* Length */
                    (0x00 & 0xFF), ((0x00>>8) & 0xFF),                                           /* Component ID */
                    0x00,0x00,                                                                   /* Offset */
                    0x01};                                                                       /* Value */
    u16 length = (sizeof(cfg)/sizeof(cfg[0]));
    mutex_lock(&ephdata->comms_mutex);
    ret_val = eph_write_control_config_no_response(ephdata, length, &cfg[0]);
    mutex_unlock(&ephdata->comms_mutex);

    return ret_val;

}

static int eph_write_bootloader_start(struct eph_data *ephdata)
{

    int ret_val; 
    u8 cfg[9] =    {TLV_CONTROL_DATA_WRITE,                                                      /* Type */
                    0x06, 0x00,                                                                  /* Length */
                    (BOOTLOADER_COMPONENT_ID & 0xFF), ((BOOTLOADER_COMPONENT_ID>>8) & 0xFF),     /* Component ID */
                    0x00,0x00,                                                                   /* Offset */
                    START_COMMAND_SEQUENCE_0,START_COMMAND_SEQUENCE_1};                          /* Value */
    u16 length = (sizeof(cfg)/sizeof(cfg[0]));

    mutex_lock(&ephdata->comms_mutex);
    ret_val = eph_write_control_config(ephdata, length, &cfg[0]);
    mutex_unlock(&ephdata->comms_mutex);

    return ret_val;
}



static int eph_write_bootloader_frame(struct eph_data *ephdata, u16 payload_length, u8 *buf, bool last_frame)
{
    int ret_val = 0;
    struct eph_device_eng_data_write_command eng_command;
    u8 data_id = 0;
    u16 count;


    if(last_frame)
    {
        /* we use 0x80 engineer data input type to package the firmware data. Data_id is used to represent last frame */
        data_id = 1;
    }

    count = payload_length + TLV_HEADER_SIZE + TLV_ENG_DEBUG_HEADER_SIZE;
    eng_command.header.type = TLV_ENG_DEBUG_DATA_WRITE;
    eng_command.header.length =  cpu_to_le16(payload_length + TLV_ENG_DEBUG_HEADER_SIZE);
    eng_command.component_id = cpu_to_le16(BOOTLOADER_COMPONENT_ID);
    eng_command.data_id = data_id;

    /* CRC left clear for bootloading */
    eng_command.crc = 0x00;


    /* Copy accross fixed contents and concatanate with bootloader frame data */
    memcpy((u8*)&ephdata->comms_send_crc_buf[0], &eng_command, sizeof(struct eph_device_eng_data_write_command));
    memcpy((u8*)&ephdata->comms_send_crc_buf[sizeof(struct eph_device_eng_data_write_command)], buf, payload_length);

    ret_val = eph_write_engineering_data(ephdata, count, ephdata->comms_send_crc_buf);


    if(ret_val)
    {   
        /* Failed to write engineering data frame. Retry to send the frame once more */
        ret_val = eph_write_engineering_data(ephdata, count, ephdata->comms_send_crc_buf);
    }


    return ret_val;
}


static int eph_proc_bootloader_status_report(struct eph_data *ephdata, u8 *message, bootloader_state_e *status_byte)
{
    int ret_val = 0;
    struct tlv_header tlvheader;

    /* Get the bootloader status type */
    /* Should only get here if a valid report was received from the device */
    tlvheader = eph_get_tl_header_info(ephdata, message);

    if (TLV_BOOTLOADER_STATUS_DATA == tlvheader.type)
    {
        *status_byte = (bootloader_state_e)message[TLV_HEADER_SIZE];
        EPH_DBG(&ephdata->commsdevice->dev,
        "TYPE:%d LENGTH:%d STATUS_FLAG:%d\n",
        tlvheader.type, tlvheader.length, *status_byte);
    }
    else
    {

        EPH_ERR(&ephdata->commsdevice->dev, "Unexpected Report type\n");
        *status_byte = bootloader_state_INVALID;
    }

    return ret_val;
}
