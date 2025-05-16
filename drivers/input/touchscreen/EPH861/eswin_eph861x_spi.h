

#ifndef __LINUX_PLATFORM_DATA_ESWIN_EPH_SPI_H
#define __LINUX_PLATFORM_DATA_ESWIN_EPH_SPI_H

#include "eswin_eph861x_types.h"
#include "eswin_eph861x_tlv.h"


/* Set tx for bootloader frame size plus margin. Set Rx to largest
 * engineering output plus margin */

#define SPI_BOOTL_HEADER_LEN   2
#define SPI_APP_BUF_SIZE_WRITE  (8192)
#define SPI_APP_BUF_SIZE_READ   (3072)

extern int __eph_spi_read(struct eph_data *ephdata, u16 len, u8 *val);
extern int eph_comms_read(struct eph_data *ephdata, u16 len, u8 *buf);
extern int __eph_spi_write(struct eph_data *ephdata, u16 len, const u8 *val);
extern int eph_comms_write(struct eph_data *ephdata, u16 len, u8 *buf);

extern int eph_comms_specific_checks_spi(struct comms_device *commsdevice);
extern void eph_comms_driver_data_set_spi(struct comms_device *commsdevice, struct eph_data *ephdata);
extern struct eph_data* eph_comms_driver_data_get_spi(struct comms_device *commsdevice);
extern struct comms_device* eph_comms_device_get_spi(struct device *dev);
extern int eph_comms_specific_bootloader_checks_spi(struct eph_data *ephdata);

extern bool eph_is_report_null_report(u8 *buff);



#endif /* __LINUX_PLATFORM_DATA_ESWIN_EPH_SPI_ */
