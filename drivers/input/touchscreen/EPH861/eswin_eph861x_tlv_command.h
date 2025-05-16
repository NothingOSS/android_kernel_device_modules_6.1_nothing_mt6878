#ifndef __LINUX_PLATFORM_DATA_ESWIN_EPH_TLV_COMMAND_H
#define __LINUX_PLATFORM_DATA_ESWIN_EPH_TLV_COMMAND_H


#include "eswin_eph861x_types.h"
#include "eswin_eph861x_tlv.h"


extern int eph_comms_two_stage_read(struct eph_data *ephdata, u8 *buf);
extern int eph_read_report(struct eph_data *ephdata, u8 *buf);
extern int eph_comms_write(struct eph_data *ephdata, u16 len, u8 *buf);
extern int eph_read_device_information(struct eph_data *ephdata);
extern int eph_write_control_config(struct eph_data *ephdata, u16 len, u8 *buf);
extern int eph_read_control_config(struct eph_data *ephdata, struct eph_device_control_config_read_command *command_request, u8 *buf);
extern int eph_write_control_config_no_response(struct eph_data *ephdata, u16 len, u8 *buf);
extern int eph_read_bootloader_information(struct eph_data *ephdata);
extern int eph_write_engineering_data(struct eph_data *ephdata, u16 len, u8 *buf);



#endif /* __LINUX_PLATFORM_DATA_ESWIN_EPH_TLV_COMMAND_H */
