

#ifndef __LINUX_PLATFORM_DATA_ESWIN_EPH_EVENT_REPORT_H
#define __LINUX_PLATFORM_DATA_ESWIN_EPH_EVENT_REPORT_H

#include "eswin_eph861x_types.h"

extern u8 sysfs_report_buf[PAGE_SIZE];
extern u8 sysfs_packetised_eng_buf[PAGE_SIZE];


extern const char* get_touch_type_str(u8 touch_type);


extern void eph_recv_event_report_contianer(struct eph_data* ephdata, u8* message);


extern void eph_clear_all_host_touch_slots(struct eph_data* ephdata);

extern bool eph_proc_report(struct eph_data *ephdata, u8 *message);

extern int eph_buffer_report(struct eph_data *ephdata, u8 *message);

extern int eph_handle_report(struct eph_data *ephdata, u8 *message);

#endif /* __LINUX_PLATFORM_DATA_ESWIN_EPH_EVENT_REPORT_ */
