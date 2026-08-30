#ifndef NOSTOS_STATE_H
#define NOSTOS_STATE_H
#include "nostos_protocol.h"
#define NOSTOS_INCIDENT_CAPACITY 24U
#define NOSTOS_FRESH_MS 3000U
typedef struct { uint32_t session_id; uint16_t sequence; uint32_t received_ms; bool seen; } nostos_report_t;
typedef struct { int16_t value; nostos_quality_t quality; bool has_value; uint32_t value_received_ms; } nostos_i16_value_t;
typedef struct { uint16_t value; nostos_quality_t quality; bool has_value; uint32_t value_received_ms; } nostos_u16_value_t;
typedef struct { uint32_t value; nostos_quality_t quality; bool has_value; uint32_t value_received_ms; } nostos_u32_value_t;
typedef struct { nostos_report_t report; nostos_i16_value_t temperature_c_x10; nostos_u16_value_t humidity_pct_x10; } nostos_environment_state_t;
typedef struct {
    nostos_report_t report;
    nostos_u16_value_t speed_kmh_x10;
    nostos_u32_value_t distance_mm;
} nostos_ride_state_t;
typedef enum { NOSTOS_INCIDENT_UNSEEN=0, NOSTOS_INCIDENT_ACTIVE, NOSTOS_INCIDENT_CLOSED } nostos_incident_phase_t;
typedef struct { nostos_incident_ref_t incident; nostos_report_t last_report; nostos_incident_phase_t phase; } nostos_incident_state_t;
typedef struct { nostos_report_t report; uint8_t status; } nostos_health_state_t;
typedef struct { uint32_t last_valid_rx_ms; bool seen; } nostos_reachability_t;
typedef struct {
    uint8_t source_id;
    nostos_environment_state_t environment;
    nostos_ride_state_t ride;
    nostos_incident_state_t fall;
    nostos_health_state_t health;
    nostos_reachability_t reachability;
} nostos_node_state_t;
typedef struct { nostos_node_state_t nodes[NOSTOS_NODE_COUNT]; } nostos_network_state_t;
typedef struct {
    uint32_t session_id;
    uint16_t floor, highest;
    uint64_t seen;
    bool approved, started;
} nostos_rx_window_t;
typedef struct {
    uint8_t source_id, kind;
    nostos_incident_ref_t ref;
    bool used, closed, muted;
} nostos_incident_record_t;
typedef struct {
    nostos_message_t message;
    bool pending;
} nostos_request_slot_t;
typedef struct {
    nostos_network_state_t shared_data;
    nostos_rx_window_t windows[NOSTOS_NODE_COUNT];
    nostos_incident_record_t incidents[NOSTOS_INCIDENT_CAPACITY];
    nostos_request_slot_t pending_stop;
    nostos_request_slot_t pending_button;
    uint8_t local_source;
} nostos_receiver_t;
typedef enum { NOSTOS_LED_OFF=0, NOSTOS_LED_RED_BLINK } nostos_led_t;
typedef enum { NOSTOS_BUZZER_OFF=0, NOSTOS_BUZZER_EMERGENCY } nostos_buzzer_t;
typedef struct { nostos_led_t led; nostos_buzzer_t buzzer; } nostos_outputs_t;
nostos_result_t nostos_receiver_init(nostos_receiver_t *receiver, uint8_t local_source);
/* Trusted configuration only. No packet can approve its own session. Caller
 * restores approved session + sequence floor from durable state on reboot.
 * Same/lower session replacement is rejected; existing incidents survive. */
nostos_result_t nostos_receiver_approve_session(nostos_receiver_t *receiver,
    uint8_t source, uint32_t session, uint16_t sequence_floor);
nostos_result_t nostos_receiver_apply(nostos_receiver_t *receiver,
    const nostos_message_t *message, uint32_t now_ms);
nostos_result_t nostos_receiver_wire(nostos_receiver_t *receiver,
    const uint8_t *wire, size_t length, uint32_t now_ms);
nostos_result_t nostos_receiver_take_stop(
    nostos_receiver_t *receiver,
    nostos_message_t *message);
nostos_result_t nostos_receiver_take_button(
    nostos_receiver_t *receiver,
    nostos_message_t *message);
void nostos_receiver_clear_requests(nostos_receiver_t *receiver);
nostos_result_t nostos_receiver_mute(nostos_receiver_t *receiver, uint8_t source,
    uint8_t kind, nostos_incident_ref_t incident);
nostos_outputs_t nostos_receiver_outputs(const nostos_receiver_t *receiver, uint32_t now_ms);
bool nostos_report_fresh(const nostos_report_t *report, uint32_t now_ms, uint32_t maximum_age_ms);
#endif
