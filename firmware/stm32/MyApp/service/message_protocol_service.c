#include "message_protocol_service.h"
#include "alert.h"
#include "audio_service.h"
#include "buzzer.h"
#include <string.h>

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

__attribute__((weak)) nostos_result_t message_protocol_service_boot(UART_HandleTypeDef *uart, vs1003b_status_t status)
{ (void)uart; (void)status; return NOSTOS_NOT_READY; }
__attribute__((weak)) nostos_result_t message_protocol_service_checkpoint_commit(
    const message_protocol_checkpoint_t *checkpoint)
{
    /* Host-only service tests may initialize the endpoint directly. Production
     * v2 builds replace this hook with the flash-backed implementation below;
     * the weak boot hook still refuses a target boot without that provider. */
    (void)checkpoint;
    return NOSTOS_OK;
}

void message_protocol_service_rx_isr(uint8_t byte, uint32_t received_ms)
{
    if(!initialized) return;
    unsigned next=(rx_head+1U)%RX_CAPACITY;
    if(next==rx_tail) { rx_overflow=true; return; }
    rx_bytes[rx_head]=byte; rx_times[rx_head]=received_ms; rx_head=next;
}
void message_protocol_service_rx_error_isr(void) { rx_overflow=true; }

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
    if (outputs.led==NOSTOS_LED_OFF) alert_init();
    else if (outputs.led==NOSTOS_LED_GREEN) alert_show(MSG_REAR_SAFE);
    else if (outputs.led==NOSTOS_LED_YELLOW_BLINK) alert_show(MSG_REAR_WARNING);
    else alert_show(MSG_FALL_DETECTED);
    if (outputs.buzzer==NOSTOS_BUZZER_OFF) buzzer_stop();
    else buzzer_play_pattern(outputs.buzzer==NOSTOS_BUZZER_EMERGENCY?
        BUZZER_PATTERN_EMERGENCY:BUZZER_PATTERN_REAR_WARNING);
}
static bool audio_ready(void *context)
{ (void)context; return audio_status==VS1003B_STATUS_OK && !audio_service_is_playing(); }
static bool play_audio(void *context, uint8_t type)
{
    (void)context;
    audio_status=audio_service_play((message_type_t)type);
    return audio_status==VS1003B_STATUS_OK;
}
nostos_result_t message_protocol_service_init(UART_HandleTypeDef *uart,
    uint8_t source, uint32_t session, vs1003b_status_t status)
{
    if (!uart) return NOSTOS_BAD_ARGUMENT;
    nostos_endpoint_io_t io={.uart_send=transmit,.outputs=apply_outputs,
        .audio_ready=audio_ready,.audio_play=play_audio};
    nostos_result_t r=nostos_endpoint_init(&endpoint,source,session,&io);
    if(r!=NOSTOS_OK) return r;
    data_uart=uart; audio_status=status; initialized=true;
    uint32_t mask=__get_PRIMASK(); __disable_irq();
    rx_head=0; rx_tail=0; rx_overflow=false;
    __set_PRIMASK(mask);
    next_incident=1;
    stats=(message_protocol_stats_t){0};
    alert_init(); buzzer_init(); return NOSTOS_OK;
}
nostos_endpoint_t *message_protocol_service_endpoint(void) { return initialized?&endpoint:NULL; }
const message_protocol_stats_t *message_protocol_service_stats(void) { return &stats; }
void message_protocol_service_shutdown(void)
{
    uint32_t mask=__get_PRIMASK(); __disable_irq();
    initialized=false; rx_head=0; rx_tail=0; rx_overflow=false;
    __set_PRIMASK(mask);
}
nostos_result_t message_protocol_service_checkpoint(message_protocol_checkpoint_t *checkpoint)
{
    if(!initialized || !checkpoint) return NOSTOS_NOT_READY;
    *checkpoint=(message_protocol_checkpoint_t){
        .source_id=endpoint.sender.source_id,
        .session_id=endpoint.sender.session_id,
        .next_sequence=endpoint.sender.next_sequence,
        .next_incident=next_incident,
    };
    memcpy(checkpoint->windows,endpoint.receiver.windows,sizeof(checkpoint->windows));
    memcpy(checkpoint->incidents,endpoint.receiver.incidents,sizeof(checkpoint->incidents));
    return NOSTOS_OK;
}
static void reconstruct_incidents(nostos_receiver_t *receiver)
{
    for(size_t i=0;i<NOSTOS_INCIDENT_CAPACITY;++i) {
        const nostos_incident_record_t *record=&receiver->incidents[i];
        if(!record->used) continue;
        nostos_incident_state_t *state=record->kind==NOSTOS_FALL?
            &receiver->shared_data.nodes[record->source_id-1U].fall:
            &receiver->shared_data.nodes[record->source_id-1U].sos;
        if(state->phase==NOSTOS_INCIDENT_UNSEEN || record->ref.session_id>state->incident.session_id ||
            (record->ref.session_id==state->incident.session_id &&
             record->ref.incident_id>=state->incident.incident_id)) {
            state->incident=record->ref;
            state->phase=record->closed?NOSTOS_INCIDENT_CLOSED:NOSTOS_INCIDENT_ACTIVE;
        }
    }
}
nostos_result_t message_protocol_service_restore(UART_HandleTypeDef *uart,
    vs1003b_status_t status, const message_protocol_checkpoint_t *checkpoint)
{
    if(!message_protocol_checkpoint_valid(checkpoint)) return NOSTOS_BAD_VALUE;
    nostos_result_t result=message_protocol_service_init(
        uart,checkpoint->source_id,checkpoint->session_id,status);
    if(result!=NOSTOS_OK) return result;
    endpoint.sender.next_sequence=checkpoint->next_sequence;
    memcpy(endpoint.receiver.windows,checkpoint->windows,sizeof(checkpoint->windows));
    memcpy(endpoint.receiver.incidents,checkpoint->incidents,sizeof(checkpoint->incidents));
    next_incident=checkpoint->next_incident;
    reconstruct_incidents(&endpoint.receiver);
    return NOSTOS_OK;
}
static nostos_result_t persist_current(void)
{
    message_protocol_checkpoint_t checkpoint;
    nostos_result_t result=message_protocol_service_checkpoint(&checkpoint);
    return result==NOSTOS_OK?message_protocol_service_checkpoint_commit(&checkpoint):result;
}
nostos_result_t message_protocol_service_receive(uint8_t byte, uint32_t received_ms)
{
    nostos_result_t r=initialized?nostos_endpoint_uart_byte(&endpoint,byte,received_ms):NOSTOS_NOT_READY;
    if(r!=NOSTOS_EMPTY) {
        if(r==NOSTOS_OK) {
            ++stats.received;
            nostos_result_t saved=persist_current();
            if(saved!=NOSTOS_OK) { message_protocol_service_shutdown(); r=NOSTOS_IO_ERROR; }
        }
        stats.last_result=r;
        if(r==NOSTOS_DUPLICATE) ++stats.duplicates;
        else if(r!=NOSTOS_OK) ++stats.rejected;
    }
    return r;
}
void message_protocol_service_process(void)
{
    if (!initialized) return;
    for(unsigned budget=0;budget<64;++budget) {
        uint32_t mask=__get_PRIMASK(); __disable_irq();
        if(rx_overflow) {
            rx_tail=rx_head; rx_overflow=false;
            __set_PRIMASK(mask);
            nostos_uart_reset(&endpoint.uart);
            ++stats.overflows; stats.last_result=NOSTOS_FULL;
            break;
        }
        if(rx_tail==rx_head) { __set_PRIMASK(mask); break; }
        uint8_t byte=rx_bytes[rx_tail]; uint32_t received_ms=rx_times[rx_tail];
        rx_tail=(rx_tail+1U)%RX_CAPACITY;
        __set_PRIMASK(mask);
        (void)message_protocol_service_receive(byte,received_ms);
        if(!initialized) break;
    }
    if(!initialized) return;
    nostos_endpoint_process(&endpoint,HAL_GetTick());
    if(audio_status==VS1003B_STATUS_OK) audio_status=audio_service_process();
    alert_process(); buzzer_process();
}

nostos_result_t message_protocol_service_publish_event(uint8_t type)
{
    if(!initialized) return NOSTOS_NOT_READY;
    nostos_message_t m={.type=type};
    if(type==NOSTOS_FALL || type==NOSTOS_SOS) {
        if(next_incident>UINT16_MAX) return NOSTOS_EXHAUSTED;
        m.payload.incident=(nostos_incident_ref_t){endpoint.sender.session_id,(uint16_t)next_incident++};
    } else if(type==NOSTOS_FALL_CLEAR || type==NOSTOS_SOS_CLEAR) {
        const nostos_node_state_t *local=&endpoint.receiver.shared_data.nodes[endpoint.sender.source_id-1];
        const nostos_incident_state_t *incident=type==NOSTOS_FALL_CLEAR?&local->fall:&local->sos;
        if(incident->phase!=NOSTOS_INCIDENT_ACTIVE) return NOSTOS_STALE;
        m.payload.incident=incident->incident;
    } else if(!((type>=NOSTOS_SPEED_DOWN && type<=NOSTOS_STOP) ||
        type==NOSTOS_REAR_SAFE || type==NOSTOS_REAR_WARNING || type==NOSTOS_REAR_UNKNOWN)) return NOSTOS_BAD_VALUE;
    if(endpoint.sender.next_sequence>UINT16_MAX) return NOSTOS_EXHAUSTED;
    /* Reserve the sequence durably before any physical UART write. A reset may
     * skip one sequence, but can never reuse an already transmitted value. */
    message_protocol_checkpoint_t reserved;
    nostos_result_t result=message_protocol_service_checkpoint(&reserved);
    if(result!=NOSTOS_OK) return result;
    ++reserved.next_sequence;
    result=message_protocol_service_checkpoint_commit(&reserved);
    if(result!=NOSTOS_OK) return result;
    result=nostos_endpoint_publish(&endpoint,&m,HAL_GetTick());
    nostos_result_t saved=persist_current();
    if(saved!=NOSTOS_OK) { message_protocol_service_shutdown(); return NOSTOS_IO_ERROR; }
    return result;
}
