#ifndef MESSAGE_TYPE_H
#define MESSAGE_TYPE_H

#include <stdint.h>

/* STM32-local presentation IDs used by display, audio, and button services.
 * These values are not NOSTOS application wire message types. */
typedef enum {
    MSG_NONE = 0x00,
    MSG_SPEED_DOWN_REQUEST = 0x10,
    MSG_SPEED_UP_REQUEST = 0x11,
    MSG_STOP_REQUEST = 0x13,
    MSG_FALL_DETECTED = 0x30,
    MSG_UNKNOWN = 0xFF
} message_type_t;

#endif
