#ifndef NOSTOS_PROTOCOL_H
#define NOSTOS_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NOSTOS_VERSION 2U
#define NOSTOS_NODE_COUNT 3U
#define NOSTOS_HEADER_SIZE 9U
#define NOSTOS_WIRE_MAX 64U
#define NOSTOS_TYPE_COUNT 10U

typedef enum {
    NOSTOS_OK = 0, NOSTOS_EMPTY, NOSTOS_BAD_ARGUMENT, NOSTOS_BAD_LENGTH,
    NOSTOS_BAD_VALUE, NOSTOS_TOO_LARGE, NOSTOS_UNSUPPORTED_VERSION,
    NOSTOS_UNSUPPORTED_TYPE, NOSTOS_BAD_CRC, NOSTOS_TIMEOUT,
    NOSTOS_UNAUTHORIZED, NOSTOS_SESSION_REQUIRED, NOSTOS_STALE,
    NOSTOS_DUPLICATE, NOSTOS_FULL, NOSTOS_NOT_READY, NOSTOS_EXPIRED,
    NOSTOS_EXHAUSTED, NOSTOS_CONFLICT, NOSTOS_IO_ERROR
} nostos_result_t;

typedef enum {
    NOSTOS_SPEED_DOWN = 0x10, NOSTOS_SPEED_UP = 0x11,
    NOSTOS_STOP = 0x13,
    NOSTOS_FALL = 0x30,
    NOSTOS_ENVIRONMENT = 0x41, NOSTOS_FALL_CLEAR = 0x42,
    NOSTOS_RIDE = 0x44, NOSTOS_SHARED_DATA_REQUEST = 0x46,
    NOSTOS_HEARTBEAT = 0x50, NOSTOS_ACK = 0x51
} nostos_type_t;

enum {
    NOSTOS_SHARED_DATA_RIDE = 1U << 0,
    NOSTOS_SHARED_DATA_ENVIRONMENT = 1U << 1,
    NOSTOS_SHARED_DATA_MASK =
        NOSTOS_SHARED_DATA_RIDE | NOSTOS_SHARED_DATA_ENVIRONMENT
};

/* Zero-initialized state is unknown, never a successful measurement. */
typedef enum {
    NOSTOS_UNMEASURED = 0, NOSTOS_VALID, NOSTOS_BELOW_RANGE,
    NOSTOS_ABOVE_RANGE, NOSTOS_SENSOR_ERROR
} nostos_quality_t;
enum {
    NOSTOS_STATUS_SENSOR_FAULT = 1U << 0,
    NOSTOS_STATUS_OUTPUT_FAULT = 1U << 1,
    NOSTOS_STATUS_INPUT_OVERFLOW = 1U << 2,
    NOSTOS_STATUS_MASK = 7U
};
typedef struct { uint32_t session_id; uint16_t incident_id; } nostos_incident_ref_t;
typedef struct {
    int16_t temperature_c_x10;
    uint16_t humidity_pct_x10;
    nostos_quality_t temperature_quality, humidity_quality;
} nostos_environment_t;
typedef struct {
    bool valid;
    uint16_t kmh_x10;
    uint32_t distance_mm; /* Cumulative wheel travel distance. */
} nostos_ride_t;
typedef struct { uint8_t mask; } nostos_shared_data_request_t;
/* ACK confirms application acceptance, not audible output or network delivery.
 * It is never automatically ACKed and never causes an actuator change. */
typedef struct {
    uint8_t source_id, type;
    uint32_t session_id;
    uint16_t sequence;
    uint8_t result; /* 0=applied/queued, 1=duplicate, 2=unsupported, 3=rejected */
} nostos_ack_t;
typedef struct nostos_message {
    uint8_t type, source_id;
    uint32_t session_id;
    uint16_t sequence;
    union {
        nostos_environment_t environment;
        nostos_ride_t ride;
        nostos_shared_data_request_t shared_data_request;
        nostos_incident_ref_t incident;
        uint8_t status;
        nostos_ack_t ack;
    } payload;
} nostos_message_t;
typedef nostos_result_t (*nostos_payload_encode_fn)(const nostos_message_t *message,
    uint8_t *payload);
typedef nostos_result_t (*nostos_payload_decode_fn)(const uint8_t *payload,
    nostos_message_t *message);
typedef struct {
    uint8_t type, payload_size;
    const char *name;
    nostos_payload_encode_fn encode_payload;
    nostos_payload_decode_fn decode_payload;
} nostos_type_info_t;
extern const nostos_type_info_t nostos_types[NOSTOS_TYPE_COUNT];
const nostos_type_info_t *nostos_type_info(uint8_t type);
const char *nostos_result_name(nostos_result_t result);
nostos_result_t nostos_message_encode(const nostos_message_t *message,
    uint8_t *wire, size_t capacity, size_t *length);
nostos_result_t nostos_message_decode(const uint8_t *wire, size_t length,
    nostos_message_t *message);
/* Validate envelope even when type is new. Never infer a v2 header for v1. */
nostos_result_t nostos_envelope_validate(const uint8_t *wire, size_t length);
/* Explicit session/sequence ownership; no silent 16-bit wrap. */
typedef struct { uint8_t source_id; uint32_t session_id, next_sequence; } nostos_sender_t;
nostos_result_t nostos_sender_init(nostos_sender_t *sender, uint8_t source, uint32_t session);
nostos_result_t nostos_sender_stamp(nostos_sender_t *sender, nostos_message_t *message);
#endif
