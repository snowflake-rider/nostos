#include "message_protocol_service.h"
#include "alert.h"
#include "audio_service.h"
#include "buzzer.h"
#include "display_service.h"
#include "sensor_link.h"
#include "sensor_store.h"
#include "sensor_view_service.h"

static nostos_endpoint_t endpoint;
static UART_HandleTypeDef *data_uart;
static vs1003b_status_t audio_status;
static volatile bool initialized;
#define RX_CAPACITY 512U
static volatile uint8_t rx_bytes[RX_CAPACITY];
static volatile uint32_t rx_times[RX_CAPACITY];
static volatile unsigned rx_head, rx_tail;
static volatile bool rx_overflow;
static uint32_t next_incident=1;
static message_protocol_stats_t stats;
static sensor_link_parser_t sensor_parser;
typedef enum { RX_ROUTE_IDLE, RX_ROUTE_NOSTOS, RX_ROUTE_SENSOR } rx_route_t;
static rx_route_t rx_route;
static uint32_t rx_route_last_ms;
static uint32_t rx_route_started_ms;
static size_t rx_route_bytes;
static bool boot_ready;
static uint32_t last_hello_ms;
static bool identity_ack_pending;
static sensor_link_identity_t identity_ack;
static uint32_t last_identity_ack_ms;

#define HELLO_PERIOD_MS 1000U
#define IDENTITY_ACK_RETRY_MS 250U

static bool fall_is_active(void)
{
    for (size_t index = 0U; index < NOSTOS_INCIDENT_CAPACITY; ++index)
    {
        const nostos_incident_record_t *incident =
            &endpoint.receiver.incidents[index];
        if (incident->used && !incident->closed &&
            incident->kind == NOSTOS_FALL)
        {
            return true;
        }
    }
    return false;
}

static void reset_route(void)
{
    sensor_link_reset(&sensor_parser);
    rx_route = RX_ROUTE_IDLE;
    rx_route_last_ms = 0U;
    rx_route_started_ms = 0U;
    rx_route_bytes = 0U;
}

static bool transmit_local(const uint8_t *frame, size_t length)
{
    if (data_uart == NULL || frame == NULL || length == 0U ||
        length > SENSOR_LINK_FRAME_SIZE) {
        return false;
    }
    return HAL_UART_Transmit(data_uart, (uint8_t *)frame, (uint16_t)length,
        20U) == HAL_OK;
}

static nostos_result_t send_hello(uint32_t now_ms)
{
    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_result_t encoded = sensor_link_encode_hello(frame, &length);
    last_hello_ms = now_ms;
    if (encoded != SENSOR_LINK_OK || !transmit_local(frame, length)) {
        ++stats.hello_failures;
        stats.last_result = NOSTOS_IO_ERROR;
        return NOSTOS_IO_ERROR;
    }
    ++stats.hello_sent;
    return NOSTOS_OK;
}

static bool send_identity_ack(uint32_t now_ms, bool immediate)
{
    if (!identity_ack_pending ||
        (!immediate &&
         (uint32_t)(now_ms - last_identity_ack_ms) < IDENTITY_ACK_RETRY_MS)) {
        return !identity_ack_pending;
    }

    uint8_t frame[SENSOR_LINK_FRAME_SIZE];
    size_t length = 0U;
    sensor_link_result_t encoded = sensor_link_encode_identity_ack(
        identity_ack.source_id, identity_ack.session_id, frame, &length);
    last_identity_ack_ms = now_ms;
    if (encoded != SENSOR_LINK_OK || !transmit_local(frame, length)) {
        stats.last_result = NOSTOS_IO_ERROR;
        return false;
    }
    identity_ack_pending = false;
    ++stats.identity_acks;
    return true;
}

nostos_result_t message_protocol_service_boot(
    UART_HandleTypeDef *uart,
    vs1003b_status_t status)
{
    if (uart == NULL) {
        return NOSTOS_BAD_ARGUMENT;
    }

    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    rx_head = 0U;
    rx_tail = 0U;
    rx_overflow = false;
    __set_PRIMASK(mask);

    endpoint = (nostos_endpoint_t){0};
    initialized = false;
    data_uart = uart;
    audio_status = status;
    boot_ready = true;
    next_incident = 1U;
    stats = (message_protocol_stats_t){0};
    identity_ack_pending = false;
    identity_ack = (sensor_link_identity_t){0};
    last_identity_ack_ms = 0U;
    reset_route();
    sensor_view_service_bind_network(NULL, 0U);
    alert_init();
    buzzer_init();
    display_service_set_fall(false);
    return send_hello(HAL_GetTick());
}

void message_protocol_service_rx_isr(uint8_t byte, uint32_t received_ms)
{
    unsigned next=(rx_head+1U)%RX_CAPACITY;
    if(next==rx_tail) { rx_overflow=true; return; }
    rx_bytes[rx_head]=byte; rx_times[rx_head]=received_ms; rx_head=next;
}
void message_protocol_service_rx_error_isr(void) { rx_overflow=true; }

void message_protocol_service_clear_pending(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    rx_tail = rx_head;
    rx_overflow = false;
    __set_PRIMASK(mask);
    reset_route();

    if (!initialized)
    {
        return;
    }

    nostos_uart_reset(&endpoint.uart);
    nostos_receiver_clear_requests(&endpoint.receiver);

    /* 현재 안전 상태를 기억해 리셋 직후 같은 출력이 자동 재적용되지 않게 합니다. */
    endpoint.last_outputs = nostos_receiver_outputs(
        &endpoint.receiver,
        HAL_GetTick()
    );
    endpoint.outputs_initialized = true;
}

static bool transmit(void *context, const uint8_t *frame, size_t length)
{
    (void)context;
    if (!data_uart || length>NOSTOS_UART_FRAME_MAX) return false;
    /* Max136B is <12ms at115200/8N1. Main-loop only, never an ISR. */
    return HAL_UART_Transmit(data_uart,(uint8_t *)frame,(uint16_t)length,20)==HAL_OK;
}
static void apply_outputs(void *context, nostos_outputs_t outputs)
{
    (void)context;
    bool active = fall_is_active();
    if (active || outputs.led == NOSTOS_LED_RED_BLINK) {
        alert_show(MSG_FALL_DETECTED);
        buzzer_play_pattern(BUZZER_PATTERN_EMERGENCY);
    } else {
        alert_reset();
        buzzer_stop();
    }
    display_service_set_fall(active);
}
static bool request_audio_playing(void *context)
{
    (void)context;
    return audio_service_is_playing();
}
static bool stop_request_audio(void *context)
{
    (void)context;
    audio_status = audio_service_stop();
    return audio_status == VS1003B_STATUS_OK;
}
static bool consume_request(
    void *context,
    const nostos_message_t *message)
{
    (void)context;
    if (message == NULL) return false;
    (void)display_service_show_button_message(
        message->source_id, message->type);
    alert_show_local_button((message_type_t)message->type);
    audio_status = audio_service_play((message_type_t)message->type);
    return audio_status == VS1003B_STATUS_OK;
}
nostos_result_t message_protocol_service_init(UART_HandleTypeDef *uart,
    uint8_t source, uint32_t session, vs1003b_status_t status)
{
    if (!uart) return NOSTOS_BAD_ARGUMENT;
    nostos_endpoint_io_t io={.uart_send=transmit,.outputs=apply_outputs,
        .audio_playing=request_audio_playing,.audio_stop=stop_request_audio,
        .audio_play=consume_request};
    nostos_result_t r=nostos_endpoint_init(&endpoint,source,session,&io);
    if(r!=NOSTOS_OK) return r;
    data_uart=uart; audio_status=status; initialized=true; boot_ready=true;
    sensor_view_service_bind_network(&endpoint.receiver.shared_data, source);
    identity_ack_pending=false;
    uint32_t mask=__get_PRIMASK(); __disable_irq();
    rx_head=0; rx_tail=0; rx_overflow=false;
    __set_PRIMASK(mask);
    next_incident=1;
    reset_route();
    alert_init();
    buzzer_init();
    display_service_set_fall(false);
    return NOSTOS_OK;
}
nostos_endpoint_t *message_protocol_service_endpoint(void) { return initialized?&endpoint:NULL; }
const message_protocol_stats_t *message_protocol_service_stats(void) { return &stats; }

nostos_result_t message_protocol_service_receive(uint8_t byte, uint32_t received_ms)
{
    nostos_result_t r=initialized?nostos_endpoint_uart_byte(&endpoint,byte,received_ms):NOSTOS_NOT_READY;
    if (r == NOSTOS_OK) nostos_endpoint_process(&endpoint,received_ms);
    if(r!=NOSTOS_EMPTY) {
        stats.last_result=r;
        if(r==NOSTOS_OK) ++stats.received;
        else if(r==NOSTOS_DUPLICATE) ++stats.duplicates;
        else ++stats.rejected;
    }
    return r;
}

static nostos_result_t activate_identity(const sensor_link_identity_t *identity)
{
    if (!boot_ready || data_uart == NULL) {
        return NOSTOS_NOT_READY;
    }
    if (identity == NULL || identity->source_id < 1U ||
        identity->source_id > NOSTOS_NODE_COUNT || identity->session_id == 0U) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (initialized) {
        if (endpoint.sender.source_id != identity->source_id ||
            endpoint.sender.session_id != identity->session_id) {
            return NOSTOS_UNAUTHORIZED;
        }
        return NOSTOS_OK;
    }

    nostos_endpoint_io_t io={.uart_send=transmit,.outputs=apply_outputs,
        .audio_playing=request_audio_playing,.audio_stop=stop_request_audio,
        .audio_play=consume_request};
    nostos_result_t result = nostos_endpoint_init(&endpoint,
        identity->source_id, identity->session_id, &io);
    if (result != NOSTOS_OK) {
        return result;
    }
    initialized = true;
    sensor_view_service_bind_network(
        &endpoint.receiver.shared_data,
        identity->source_id);
    next_incident = 1U;
    alert_init();
    buzzer_init();
    display_service_set_fall(false);
    return NOSTOS_OK;
}

static nostos_result_t approve_peer_session(
    const sensor_link_approve_session_t *approval)
{
    if (!initialized) {
        return NOSTOS_NOT_READY;
    }
    if (approval == NULL || approval->source_id < 1U ||
        approval->source_id > NOSTOS_NODE_COUNT || approval->session_id == 0U ||
        approval->source_id == endpoint.sender.source_id) {
        return NOSTOS_BAD_ARGUMENT;
    }

    nostos_rx_window_t *window =
        &endpoint.receiver.windows[approval->source_id - 1U];
    if (window->approved) {
        if (approval->session_id == window->session_id) {
            return NOSTOS_OK;
        }
        if (approval->session_id < window->session_id) {
            return NOSTOS_STALE;
        }
    }
    return nostos_receiver_approve_session(&endpoint.receiver,
        approval->source_id, approval->session_id, approval->sequence_floor);
}

static void handle_local_message(
    const sensor_link_message_t *message,
    uint32_t received_ms)
{
    nostos_result_t result = NOSTOS_BAD_VALUE;
    if (message->type == SENSOR_LINK_RIDE) {
        if (sensor_store_update_ride(message->ride.valid,
                message->ride.kmh_x10, message->ride.distance_mm,
                received_ms)) {
            ++stats.sensor_received;
            return;
        }
    } else if (message->type == SENSOR_LINK_IDENTITY) {
        ++stats.identities;
        result = activate_identity(&message->identity);
        if (result == NOSTOS_OK) {
            identity_ack = message->identity;
            identity_ack_pending = true;
            (void)send_identity_ack(HAL_GetTick(), true);
            return;
        }
    } else if (message->type == SENSOR_LINK_APPROVE_SESSION) {
        result = approve_peer_session(&message->approve_session);
        if (result == NOSTOS_OK) {
            ++stats.sessions_approved;
            return;
        }
    }

    ++stats.sensor_rejected;
    ++stats.control_rejected;
    stats.last_result = result;
}

void message_protocol_service_process(void)
{
    for(unsigned budget=0;budget<64;++budget) {
        uint32_t mask=__get_PRIMASK(); __disable_irq();
        if(rx_overflow) {
            rx_tail=rx_head; rx_overflow=false;
            __set_PRIMASK(mask);
            reset_route();
            if(initialized) nostos_uart_reset(&endpoint.uart);
            ++stats.overflows; stats.last_result=NOSTOS_FULL;
            break;
        }
        if(rx_tail==rx_head) { __set_PRIMASK(mask); break; }
        uint8_t byte=rx_bytes[rx_tail]; uint32_t received_ms=rx_times[rx_tail];
        rx_tail=(rx_tail+1U)%RX_CAPACITY;
        __set_PRIMASK(mask);
        bool route_timed_out=rx_route!=RX_ROUTE_IDLE &&
            ((uint32_t)(received_ms-rx_route_last_ms)>SENSOR_LINK_TIMEOUT_MS ||
             (uint32_t)(received_ms-rx_route_started_ms)>SENSOR_LINK_TIMEOUT_MS);
        if(route_timed_out) {
            reset_route();
            if(initialized) nostos_uart_reset(&endpoint.uart);
        }
        if(rx_route==RX_ROUTE_IDLE) {
            if(byte==NOSTOS_UART_FLAG) {
                rx_route=RX_ROUTE_NOSTOS; rx_route_last_ms=received_ms;
                rx_route_started_ms=received_ms; rx_route_bytes=1U;
                if(initialized) (void)message_protocol_service_receive(byte,received_ms);
            } else if(byte==SENSOR_LINK_PREAMBLE_0) {
                sensor_link_message_t sensor_message;
                rx_route=RX_ROUTE_SENSOR; rx_route_last_ms=received_ms;
                rx_route_started_ms=received_ms; rx_route_bytes=1U;
                (void)sensor_link_feed(&sensor_parser,byte,received_ms,&sensor_message);
            }
        } else if(rx_route==RX_ROUTE_NOSTOS) {
            rx_route_last_ms=received_ms;
            ++rx_route_bytes;
            if(initialized) (void)message_protocol_service_receive(byte,received_ms);
            if(byte==NOSTOS_UART_FLAG || rx_route_bytes>=NOSTOS_UART_FRAME_MAX) {
                if(byte!=NOSTOS_UART_FLAG && initialized) nostos_uart_reset(&endpoint.uart);
                reset_route();
            }
        } else {
            sensor_link_message_t sensor_message;
            rx_route_last_ms=received_ms;
            ++rx_route_bytes;
            sensor_link_result_t sensor_result=sensor_link_feed(
                &sensor_parser,byte,received_ms,&sensor_message);
            if(sensor_result==SENSOR_LINK_OK) {
                handle_local_message(&sensor_message,received_ms);
                reset_route();
            } else if(sensor_result!=SENSOR_LINK_EMPTY) {
                ++stats.sensor_rejected; reset_route();
            }
        }
    }
    uint32_t now_ms = HAL_GetTick();
    if (!initialized) {
        if (boot_ready &&
            (uint32_t)(now_ms - last_hello_ms) >= HELLO_PERIOD_MS) {
            (void)send_hello(now_ms);
        }
        return;
    }
    (void)send_identity_ack(now_ms, false);
    nostos_endpoint_process(&endpoint,now_ms);
    if(audio_status==VS1003B_STATUS_OK) audio_status=audio_service_process();
    alert_process();
    buzzer_process();
    display_service_set_fall(fall_is_active());
}

nostos_result_t message_protocol_service_publish_event(uint8_t type)
{
    if(!initialized) return NOSTOS_NOT_READY;
    nostos_message_t m={.type=type};
    if(type==NOSTOS_FALL) {
        if(next_incident>UINT16_MAX) return NOSTOS_EXHAUSTED;
        m.payload.incident=(nostos_incident_ref_t){endpoint.sender.session_id,(uint16_t)next_incident++};
    } else if(type==NOSTOS_FALL_CLEAR) {
        const nostos_node_state_t *local=&endpoint.receiver.shared_data.nodes[endpoint.sender.source_id-1];
        const nostos_incident_state_t *incident=&local->fall;
        if(incident->phase!=NOSTOS_INCIDENT_ACTIVE) return NOSTOS_STALE;
        m.payload.incident=incident->incident;
    } else if(type!=NOSTOS_SPEED_DOWN && type!=NOSTOS_SPEED_UP &&
        type!=NOSTOS_STOP) return NOSTOS_BAD_VALUE;
    nostos_result_t result = nostos_endpoint_publish(&endpoint,&m,HAL_GetTick());
    if (result == NOSTOS_OK) {
        nostos_endpoint_process(&endpoint,HAL_GetTick());
    }
    return result;
}

nostos_result_t message_protocol_service_publish_ride(
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm)
{
    if(!initialized) return NOSTOS_NOT_READY;
    if(!valid && (kmh_x10!=0U || distance_mm!=0U)) return NOSTOS_BAD_VALUE;
    nostos_message_t message={
        .type=NOSTOS_RIDE,
        .payload.ride={
            .valid=valid,
            .kmh_x10=valid?kmh_x10:0U,
            .distance_mm=valid?distance_mm:0U,
        },
    };
    return nostos_endpoint_publish(&endpoint,&message,HAL_GetTick());
}

nostos_result_t message_protocol_service_publish_environment(
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    nostos_quality_t quality)
{
    if(!initialized) return NOSTOS_NOT_READY;
    nostos_message_t message={
        .type=NOSTOS_ENVIRONMENT,
        .payload.environment={
            .temperature_c_x10=temperature_c_x10,
            .humidity_pct_x10=humidity_pct_x10,
            .temperature_quality=quality,
            .humidity_quality=quality,
        },
    };
    return nostos_endpoint_publish(&endpoint,&message,HAL_GetTick());
}
