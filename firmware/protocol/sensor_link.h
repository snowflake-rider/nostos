#ifndef NOSTOS_SENSOR_LINK_H
#define NOSTOS_SENSOR_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SENSOR_LINK_PREAMBLE_0 0xA5U
#define SENSOR_LINK_PREAMBLE_1 0x5AU
#define SENSOR_LINK_VERSION 0x01U

#define SENSOR_LINK_HELLO 0x02U
#define SENSOR_LINK_IDENTITY 0x03U
#define SENSOR_LINK_APPROVE_SESSION 0x04U
#define SENSOR_LINK_IDENTITY_ACK 0x05U
#define SENSOR_LINK_RIDE 0x06U

#define SENSOR_LINK_HELLO_PAYLOAD_SIZE 0U
#define SENSOR_LINK_IDENTITY_PAYLOAD_SIZE 5U
#define SENSOR_LINK_APPROVE_SESSION_PAYLOAD_SIZE 7U
#define SENSOR_LINK_IDENTITY_ACK_PAYLOAD_SIZE SENSOR_LINK_IDENTITY_PAYLOAD_SIZE
#define SENSOR_LINK_RIDE_PAYLOAD_SIZE 7U

#define SENSOR_LINK_FRAME_OVERHEAD_SIZE 7U
#define SENSOR_LINK_HELLO_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_HELLO_PAYLOAD_SIZE)
#define SENSOR_LINK_IDENTITY_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_IDENTITY_PAYLOAD_SIZE)
#define SENSOR_LINK_APPROVE_SESSION_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_APPROVE_SESSION_PAYLOAD_SIZE)
#define SENSOR_LINK_IDENTITY_ACK_FRAME_SIZE SENSOR_LINK_IDENTITY_FRAME_SIZE
#define SENSOR_LINK_RIDE_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_RIDE_PAYLOAD_SIZE)

/* Maximum encoded frame and parser capacity. Existing callers may keep using it. */
#define SENSOR_LINK_FRAME_SIZE SENSOR_LINK_APPROVE_SESSION_FRAME_SIZE
#define SENSOR_LINK_TIMEOUT_MS 100U

typedef enum {
    SENSOR_LINK_OK = 0,
    SENSOR_LINK_EMPTY,
    SENSOR_LINK_BAD_ARGUMENT,
    SENSOR_LINK_BAD_VERSION,
    SENSOR_LINK_BAD_TYPE,
    SENSOR_LINK_BAD_LENGTH,
    SENSOR_LINK_BAD_VALUE,
    SENSOR_LINK_BAD_CRC,
    SENSOR_LINK_TIMEOUT
} sensor_link_result_t;

typedef struct {
    bool valid;
    uint16_t kmh_x10;
    uint32_t distance_mm; /* Cumulative wheel travel distance. */
} sensor_link_ride_t;

typedef struct {
    uint8_t source_id;
    uint32_t session_id;
} sensor_link_identity_t;

typedef struct {
    uint8_t source_id;
    uint32_t session_id;
    uint16_t sequence_floor;
} sensor_link_approve_session_t;

typedef struct {
    uint8_t type;
    union {
        sensor_link_ride_t ride;
        sensor_link_identity_t identity;
        sensor_link_approve_session_t approve_session;
    };
} sensor_link_message_t;

typedef struct {
    uint8_t bytes[SENSOR_LINK_FRAME_SIZE];
    size_t used;
    uint32_t last_byte_ms;
} sensor_link_parser_t;

sensor_link_result_t sensor_link_encode_ride(
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

sensor_link_result_t sensor_link_encode_hello(
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

sensor_link_result_t sensor_link_encode_identity(
    uint8_t source_id,
    uint32_t session_id,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

sensor_link_result_t sensor_link_encode_approve_session(
    uint8_t source_id,
    uint32_t session_id,
    uint16_t sequence_floor,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

sensor_link_result_t sensor_link_encode_identity_ack(
    uint8_t source_id,
    uint32_t session_id,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

void sensor_link_reset(sensor_link_parser_t *parser);

/* A local ESP32-to-STM32 control/sensor frame. This is never a Mesh payload. */
sensor_link_result_t sensor_link_feed(
    sensor_link_parser_t *parser,
    uint8_t byte,
    uint32_t now_ms,
    sensor_link_message_t *message);

const char *sensor_link_result_name(sensor_link_result_t result);

#endif /* NOSTOS_SENSOR_LINK_H */
