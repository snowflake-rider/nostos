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
#define SENSOR_LINK_EVENT 0x07U
#define SENSOR_LINK_ENVIRONMENT 0x08U
#define SENSOR_LINK_SHARED_DATA_REQUEST 0x09U
#define SENSOR_LINK_READY 0x0AU
#define SENSOR_LINK_OUTPUT_EVENT 0x0BU
#define SENSOR_LINK_OUTPUT_RIDE 0x0CU
#define SENSOR_LINK_OUTPUT_ENVIRONMENT 0x0DU
#define SENSOR_LINK_OUTPUT_RESULT 0x0EU

#define SENSOR_LINK_EVENT_SPEED_DOWN 0x10U
#define SENSOR_LINK_EVENT_SPEED_UP 0x11U
#define SENSOR_LINK_EVENT_STOP 0x13U
#define SENSOR_LINK_EVENT_FALL 0x30U
#define SENSOR_LINK_EVENT_FALL_CLEAR 0x42U

#define SENSOR_LINK_OUTPUT_ACCEPTED 0U
#define SENSOR_LINK_OUTPUT_DUPLICATE 1U
#define SENSOR_LINK_OUTPUT_REJECTED 2U
#define SENSOR_LINK_OUTPUT_HARDWARE_ERROR 3U

#define SENSOR_LINK_SOURCE_ID_MIN 1U
#define SENSOR_LINK_SOURCE_ID_MAX 3U
#define SENSOR_LINK_QUALITY_UNMEASURED 0U
#define SENSOR_LINK_QUALITY_VALID 1U
#define SENSOR_LINK_QUALITY_BELOW 2U
#define SENSOR_LINK_QUALITY_ABOVE 3U
#define SENSOR_LINK_QUALITY_SENSOR_ERROR 4U
#define SENSOR_LINK_QUALITY_MAX SENSOR_LINK_QUALITY_SENSOR_ERROR

#define SENSOR_LINK_HELLO_PAYLOAD_SIZE 0U
#define SENSOR_LINK_IDENTITY_PAYLOAD_SIZE 5U
#define SENSOR_LINK_APPROVE_SESSION_PAYLOAD_SIZE 7U
#define SENSOR_LINK_IDENTITY_ACK_PAYLOAD_SIZE SENSOR_LINK_IDENTITY_PAYLOAD_SIZE
#define SENSOR_LINK_RIDE_PAYLOAD_SIZE 7U
#define SENSOR_LINK_EVENT_PAYLOAD_SIZE 1U
#define SENSOR_LINK_ENVIRONMENT_PAYLOAD_SIZE 6U
#define SENSOR_LINK_SHARED_DATA_REQUEST_PAYLOAD_SIZE 1U
#define SENSOR_LINK_READY_PAYLOAD_SIZE 4U
#define SENSOR_LINK_OUTPUT_EVENT_PAYLOAD_SIZE 6U
#define SENSOR_LINK_OUTPUT_RIDE_PAYLOAD_SIZE 12U
#define SENSOR_LINK_OUTPUT_ENVIRONMENT_PAYLOAD_SIZE 11U
#define SENSOR_LINK_OUTPUT_RESULT_PAYLOAD_SIZE 5U

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
#define SENSOR_LINK_EVENT_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_EVENT_PAYLOAD_SIZE)
#define SENSOR_LINK_ENVIRONMENT_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_ENVIRONMENT_PAYLOAD_SIZE)
#define SENSOR_LINK_SHARED_DATA_REQUEST_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_SHARED_DATA_REQUEST_PAYLOAD_SIZE)
#define SENSOR_LINK_READY_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_READY_PAYLOAD_SIZE)
#define SENSOR_LINK_OUTPUT_EVENT_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_OUTPUT_EVENT_PAYLOAD_SIZE)
#define SENSOR_LINK_OUTPUT_RIDE_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_OUTPUT_RIDE_PAYLOAD_SIZE)
#define SENSOR_LINK_OUTPUT_ENVIRONMENT_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_OUTPUT_ENVIRONMENT_PAYLOAD_SIZE)
#define SENSOR_LINK_OUTPUT_RESULT_FRAME_SIZE \
    (SENSOR_LINK_FRAME_OVERHEAD_SIZE + SENSOR_LINK_OUTPUT_RESULT_PAYLOAD_SIZE)

/* Maximum encoded frame and parser capacity. Existing callers may keep using it. */
#define SENSOR_LINK_FRAME_SIZE SENSOR_LINK_OUTPUT_RIDE_FRAME_SIZE
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
    uint8_t type; /* One of SPEED_DOWN, SPEED_UP, STOP, FALL, FALL_CLEAR. */
} sensor_link_event_t;

typedef struct {
    int16_t temperature_c_x10;
    uint16_t humidity_pct_x10;
    uint8_t temperature_quality;
    uint8_t humidity_quality;
} sensor_link_environment_t;

typedef struct {
    uint8_t mask;
} sensor_link_shared_data_request_t;

typedef struct {
    uint32_t command_epoch;
} sensor_link_ready_t;

typedef struct {
    uint32_t command_id;
    uint8_t source_id;
    uint8_t event_type;
} sensor_link_output_event_t;

typedef struct {
    uint32_t command_id;
    uint8_t source_id;
    bool valid;
    uint16_t kmh_x10;
    uint32_t distance_mm;
} sensor_link_output_ride_t;

typedef struct {
    uint32_t command_id;
    uint8_t source_id;
    int16_t temperature_c_x10;
    uint16_t humidity_pct_x10;
    uint8_t temperature_quality;
    uint8_t humidity_quality;
} sensor_link_output_environment_t;

typedef struct {
    uint32_t command_id;
    uint8_t status;
} sensor_link_output_result_t;

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
        sensor_link_event_t event;
        sensor_link_environment_t environment;
        sensor_link_shared_data_request_t shared_data_request;
        sensor_link_ready_t ready;
        sensor_link_output_event_t output_event;
        sensor_link_output_ride_t output_ride;
        sensor_link_output_environment_t output_environment;
        sensor_link_output_result_t output_result;
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

sensor_link_result_t sensor_link_encode_event(
    uint8_t event_type,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

sensor_link_result_t sensor_link_encode_environment(
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint8_t temperature_quality,
    uint8_t humidity_quality,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

sensor_link_result_t sensor_link_encode_shared_data_request(
    uint8_t mask,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

sensor_link_result_t sensor_link_encode_ready(
    uint32_t command_epoch,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

sensor_link_result_t sensor_link_encode_output_event(
    uint32_t command_id,
    uint8_t source_id,
    uint8_t event_type,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

sensor_link_result_t sensor_link_encode_output_ride(
    uint32_t command_id,
    uint8_t source_id,
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

sensor_link_result_t sensor_link_encode_output_environment(
    uint32_t command_id,
    uint8_t source_id,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint8_t temperature_quality,
    uint8_t humidity_quality,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length);

sensor_link_result_t sensor_link_encode_output_result(
    uint32_t command_id,
    uint8_t status,
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

/* A paired STM32/ESP32 control or producer frame, never a Mesh payload.
 * Legacy IDENTITY/APPROVE frames remain decode-compatible only. OUTPUT source_id
 * identifies the render origin; official identity/session/sequence state stays
 * owned by the ESP32 runtime. */
sensor_link_result_t sensor_link_feed(
    sensor_link_parser_t *parser,
    uint8_t byte,
    uint32_t now_ms,
    sensor_link_message_t *message);

const char *sensor_link_result_name(sensor_link_result_t result);

#endif /* NOSTOS_SENSOR_LINK_H */
