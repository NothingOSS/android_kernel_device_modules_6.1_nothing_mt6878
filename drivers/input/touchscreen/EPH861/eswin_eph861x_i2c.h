#ifndef __LINUX_PLATFORM_DATA_ESWIN_EPH_I2C_H
#define __LINUX_PLATFORM_DATA_ESWIN_EPH_I2C_H

#include <eswin_eph861x_types.h>
#define I2C_APP_BUF_SIZE_WRITE  (8192)
#define I2C_APP_BUF_SIZE_READ   (3072)

extern int __eph_read_reg(struct eph_data *ephdata, u16 len, u8 *val); 
extern int __eph_write_reg(struct eph_data *ephdata, u16 len, u8 *val);
extern int eph_comms_write(struct eph_data *ephdata, u16 len, u8 *buf);
extern int eph_comms_read(struct eph_data *ephdata, u16 len, u8 *buf);

extern int eph_comms_specific_checks_i2c(struct comms_device *commsdevice);
extern void eph_comms_driver_data_set_i2c(struct comms_device *commsdevice, struct eph_data *ephdata);
extern struct eph_data* eph_comms_driver_data_get_i2c(struct comms_device *commsdevice);
extern struct comms_device* eph_comms_device_get_i2c(struct device *dev);
extern int eph_comms_specific_bootloader_checks_i2c(struct eph_data *ephdata);

extern bool eph_is_report_null_report(u8 *buff);

#endif /* __LINUX_PLATFORM_DATA_ESWIN_EPH_I2C_ */
