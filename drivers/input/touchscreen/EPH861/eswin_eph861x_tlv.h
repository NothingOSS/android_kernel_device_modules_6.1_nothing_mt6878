#include "eswin_eph861x_project_config.h"

#define TLV_HEADER_SIZE (uint16_t)(sizeof(uint8_t) + sizeof(uint16_t))

/* Generic TLV types */
#define TLV_CONTAINER            (0x10u)
#define TLV_GENERIC_READ         (0x00u)
#define TLV_INVALID_COMMAND      (0xFEu)

/* Command input TLV types */
#define TLV_NULL                          (0x00u)
#define TLV_DEVICE_INFO_READ              (0x01u)
#define TLV_EXTENDED_INFO_READ            (0x02u)
#define TLV_CONTROL_DATA_WRITE            (0x08u)
#define TLV_CONTROL_DATA_READ             (0x09u)
#define TLV_CONFIG_DATA_WRITE             (0x0Au)
#define TLV_CONFIG_DATA_READ              (0x0Bu)
#define TLV_BOOTLOADER_INFO_READ          (0x51u)
#define TLV_ENG_DEBUG_DATA_WRITE          (0x80u)
#define TLV_ALG_IN_LOOP_ENABLE            (0x82u)


/* Data output TLV types */
#define TLV_DEVICE_INFO_DATA     (0x01u)
#define TLV_EXTENDED_INFO_DATA   (0x02u)
#define TLV_CONTROL_DATA         (0x09u)
#define TLV_CONFIG_DATA          (0x0Bu)
#define TLV_PACKETISED_DATA      (0x21u)
#define TLV_BOOTLOADER_INFO_DATA (0x51u)
#define TLV_ENG_DEBUG_DATA       (0x81u)

/* Report output TLV types */
#define TLV_DEVICE_STATUS_DATA      (0x22u)
#define TLV_REPORT_DATA             (0x23u)
#define TLV_AQFE_REPORT_DATA        (0x24u)
#define TLV_BOOTLOADER_STATUS_DATA  (0x50u)

/* Generic field offsets */
#define TLV_TYPE_FIELD          (0u)
#define TLV_LENGTH_FIELD        (1u)
#define TLV_PAYLOAD_FIELD       TLV_HEADER_SIZE


/* Device status Fields and Masks */
#define TLV_DEVICE_STATUS_FLAGS_FIELD      (0u)
#define TLV_DEVICE_STATUS_RESET_MASK       (0x01u)
#define TLV_DEVICE_STATUS_CONTROL_MASK     (0x02u)
#define TLV_DEVICE_STATUS_CONFIG_MASK      (0x04u)
#define TLV_DEVICE_STATUS_COMPONENT_FIELD  (1u)

/* Read field offsets (within payload) */
#define TLV_READ_COMPONENT_FIELD (0u)
#define TLV_READ_OFFSET_FIELD    (2u)
#define TLV_READ_LENGTH_FIELD    (4u)
#if (CONFIG_ESWIN_TOUCH_PROTOCOL_VERSION >= CONFIG_ESWIN_TOUCH_PROTOCOL_VERSION_3)
#define TLV_READ_HEADER_SIZE     (7u)
#else
#define TLV_READ_HEADER_SIZE     (6u)
#endif

/* Write field offsets (within payload) */
#define TLV_WRITE_COMPONENT_FIELD (0u)
#define TLV_WRITE_OFFSET_FIELD    (2u)
#define TLV_WRITE_HEADER_SIZE     (4u)
#define TLV_WRITE_RESPONSE_SIZE   (2u)


/* Engineering debug field offsets (within payload) */
#define TLV_ENG_DEBUG_COMPONENT_FIELD (0u)
#define TLV_ENG_DEBUG_ID_FIELD        (2u)
#define TLV_ENG_DEBUG_CRC_FIELD       (3u)
#define TLV_ENG_DEBUG_HEADER_SIZE     (4u)

#define TLV_ENG_DEBUG_ID_MASK            (0x1Fu)
#define TLV_ENG_DEBUG_PRE_COMPONENT_MASK (0x80u)

/* Engineering Algorithm-in-the-loop enable command size */
#define TLV_ALG_IN_LOOP_ENABLE_SIZE   (1u)

/* Packetising field offsets (within payload) */
#define TLV_PACKETISING_SEQUENCE_FIELD    (0u)
#define TLV_PACKETISING_DATA_OFFSET_FIELD (1u)
#define TLV_PACKETISING_SEQUENCE_ID_MASK  (0x1Fu)
#define TLV_PACKETISING_MORE_DATA_MASK    (0x80u)
#define TLV_PACKETISING_HEADER_SIZE       (3u)

/**************** Event type report defines  *************************/
#define RESERVED_TYPE                (0u)
#define CONTACT_TYPE                 (1u)
#define RELEASE_TYPE                 (2u)
#define HOVER_TYPE                   (3u)
#define STYLUS_POSITION_TYPE         (4u)
#define GESTURE_TYPE                 (5u)
#define STYLUS_STANDARD_DATA_TYPE    (6u)
#define STYLUS_PERIODIC_DATA_TYPE    (7u)
#define STYLUS_CAPABILITY_DATA_TYPE  (8u)
#define STYLUS_RELEASE_TYPE          (9u)

#define EVENT_REPORT_TYPE_OFFSET 	(4u)
#define EVENT_REPORT_TYPE_MASK 		(0xF0)
#define EVENT_REPORT_LENGTH_MASK 	(0x0F)

#define MINI_TLV_SIZE         (sizeof(uint8_t))

/*                             ID                X location         Y location         Minor axis        Major Axis      */
#define TOUCH_PAYLOAD_SIZE    (sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t))
#define REPORT_SIZE           (MINI_TLV_SIZE + TOUCH_PAYLOAD_SIZE)

/*                             Type              X location         Y location         Size             */
#define GESTURE_PAYLOAD_SIZE  (sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t))
#define GESTURE_SIZE          (MINI_TLV_SIZE + GESTURE_PAYLOAD_SIZE)

#define REPORTING_BUFFER_SIZE (TLV_HEADER_SIZE + (CONFIG_SUPPORTED_TOUCHES * REPORT_SIZE))

/* gesture type */
#define GESTURE_UNUSED 0
#define GESTURE_TAP 1
#define GESTURE_DOUBLE_TAP 2
#define GESTURE_SWIPE_LEFT 3
#define GESTURE_SWIPE_RIGHT 4
#define GESTURE_SWIPE_UP 5
#define GESTURE_SWIPE_DOWN 6
#define GESTURE_PINCH 7
#define GESTURE_STRETCH 8

