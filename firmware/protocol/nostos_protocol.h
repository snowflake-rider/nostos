#ifndef NOSTOS_PROTOCOL_H
#define NOSTOS_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NOSTOS_LOCAL_SOURCE_NODE_ID 0U
#define NOSTOS_NODE_ID_MIN 1U
#define NOSTOS_NODE_ID_MAX 10U
#define NOSTOS_APPLICATION_HEADER_SIZE 2U
#define NOSTOS_APPLICATION_MIN_SIZE 6U
#define NOSTOS_APPLICATION_MAX_SIZE 11U
#define NOSTOS_STATE_SCHEMA_REV 1U

typedef enum {
    NOSTOS_OK = 0,
    NOSTOS_EMPTY,
    NOSTOS_BAD_ARGUMENT,
    NOSTOS_BAD_LENGTH,
    NOSTOS_BAD_VALUE,
    NOSTOS_TOO_LARGE,
    NOSTOS_UNSUPPORTED_TYPE,
    NOSTOS_BAD_CRC,
    NOSTOS_TIMEOUT
} nostos_result_t;

typedef enum {
    NOSTOS_MESSAGE_STATE_UPDATE = 0x01,
    NOSTOS_MESSAGE_PACE_REQUEST = 0x02,
    NOSTOS_MESSAGE_STOP_REQUEST = 0x03,
    NOSTOS_MESSAGE_STOP_ACK = 0x04
} nostos_message_type_t;

typedef enum {
    NOSTOS_TOPIC_RIDE = 0x01,
    NOSTOS_TOPIC_ENVIRONMENT = 0x02
} nostos_topic_t;

typedef enum {
    NOSTOS_PACE_ACCELERATE = 0x01,
    NOSTOS_PACE_DECELERATE = 0x02
} nostos_pace_action_t;

typedef enum {
    NOSTOS_STOP_REASON_BUTTON = 0x01,
    NOSTOS_STOP_REASON_FALL = 0x02
} nostos_stop_reason_t;

typedef struct {
    uint16_t speed_x10_kmh;
    uint32_t trip_distance_m;
} nostos_ride_state_t;

typedef struct {
    int16_t temperature_x10_c;
    uint16_t humidity_x10_pct;
} nostos_environment_state_t;

typedef struct {
    uint8_t topic_id;
    uint8_t schema_rev;
    bool sensor_valid;
    union {
        nostos_ride_state_t ride;
        nostos_environment_state_t environment;
    } value;
} nostos_state_update_t;

typedef struct { uint32_t request_id; uint8_t action; } nostos_pace_request_t;
typedef struct { uint32_t request_id; uint8_t reason; } nostos_stop_request_t;
typedef struct { uint32_t request_id; } nostos_stop_ack_t;

typedef struct {
    uint8_t type;
    uint8_t source_node_id;
    union {
        nostos_state_update_t state_update;
        nostos_pace_request_t pace_request;
        nostos_stop_request_t stop_request;
        nostos_stop_ack_t stop_ack;
    } payload;
} nostos_message_t;

const char *nostos_result_name(nostos_result_t result);
const char *nostos_message_type_name(uint8_t type);
bool nostos_node_id_valid(uint8_t source_node_id);
bool nostos_request_id_valid(uint32_t request_id);

/* Normal application/Mesh codec: source_node_id must be 1..10. */
nostos_result_t nostos_message_encode(const nostos_message_t *message,
    uint8_t *wire, size_t capacity, size_t *length);
nostos_result_t nostos_message_decode(const uint8_t *wire, size_t length,
    nostos_message_t *message);

/* Local UART codec: additionally accepts source_node_id=0 from the paired
 * STM32.  ESP32 stamps its provisioned 1..10 ID before sending requests over
 * Mesh.  A local STOP_ACK with source 0 means only that the paired STM32
 * accepted the request; it must never be forwarded unchanged over Mesh. */
nostos_result_t nostos_local_message_encode(const nostos_message_t *message,
    uint8_t *wire, size_t capacity, size_t *length);
nostos_result_t nostos_local_message_decode(const uint8_t *wire, size_t length,
    nostos_message_t *message);

/* Constructors validate type-specific values and accept local source 0 for the
 * shared STM32 image.  Normal encode remains the boundary that rejects 0 for
 * Mesh.  Invalid sensor values are normalized to numeric zero. */
nostos_result_t nostos_message_make_ride(nostos_message_t *message,
    uint8_t source_node_id, bool sensor_valid, uint16_t speed_x10_kmh,
    uint32_t trip_distance_m);
nostos_result_t nostos_message_make_environment(nostos_message_t *message,
    uint8_t source_node_id, bool sensor_valid, int16_t temperature_x10_c,
    uint16_t humidity_x10_pct);
nostos_result_t nostos_message_make_pace(nostos_message_t *message,
    uint8_t source_node_id, uint32_t request_id, uint8_t action);
nostos_result_t nostos_message_make_stop(nostos_message_t *message,
    uint8_t source_node_id, uint32_t request_id, uint8_t reason);
nostos_result_t nostos_message_make_stop_ack(nostos_message_t *message,
    uint8_t source_node_id, uint32_t request_id);

#endif
