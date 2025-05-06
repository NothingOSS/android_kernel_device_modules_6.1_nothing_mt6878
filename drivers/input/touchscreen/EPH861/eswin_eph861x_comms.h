#ifndef __LINUX_PLATFORM_DATA_ESWIN_EPH_COMMS_H
#define __LINUX_PLATFORM_DATA_ESWIN_EPH_COMMS_H


#include "eswin_eph861x_types.h"
#include "eswin_eph861x_tlv.h"

#if (ESWIN_EPH861X_SPI)
#include "eswin_eph861x_spi.h"
#define COMMS_BUF_SIZE   SPI_APP_BUF_SIZE_READ
#define COMMS_BUF_WRITE_SIZE   SPI_APP_BUF_SIZE_WRITE
#endif

#if (ESWIN_EPH861X_I2C)
#include "eswin_eph861x_i2c.h"
#define COMMS_BUF_SIZE   I2C_APP_BUF_SIZE_READ
#define COMMS_BUF_WRITE_SIZE   I2C_APP_BUF_SIZE_WRITE
#endif

#define COMMS_READ_RETRY_NUM   3

extern struct dma_pool *pool_rx;
extern struct dma_pool *pool_tx;


extern struct tlv_header eph_get_tl_header_info(struct eph_data *ephdata, u8 *message);

extern int eph_comms_two_stage_read(struct eph_data *ephdata, u8 *buf);
extern int eph_read_report(struct eph_data *ephdata, u8 *buf);
extern int eph_comms_write(struct eph_data *ephdata, u16 len, u8 *buf);

extern int eph_wait_for_chg(struct eph_data *ephdata);
extern u8 eph_read_chg(struct eph_data *ephdata);

extern int eph_comms_specific_checks(struct comms_device *commsdevice);
extern void eph_comms_driver_data_set(struct comms_device *commsdevice, struct eph_data *ephdata);
extern struct eph_data* eph_comms_driver_data_get(struct comms_device *commsdevice);
extern struct comms_device* eph_comms_device_get(struct device *dev);
extern char* eph_comms_devicename_get(struct eph_data *ephdata);
extern int eph_comms_specific_bootloader_checks(struct eph_data *ephdata);


extern u8 get_crc8_iter(u8 crc, u8 data);
extern u8 eph_get_data_crc(uint8_t* p_msg, size_t size);

extern int eph_allocate_comms_memory(struct comms_device *commsdevice, struct eph_data *ephdata);

#endif /* __LINUX_PLATFORM_DATA_ESWIN_EPH_COMMS_ */
