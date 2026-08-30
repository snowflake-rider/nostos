#include "nostos_endpoint.h"
nostos_result_t nostos_endpoint_init(nostos_endpoint_t *e, uint8_t source, uint32_t session, const nostos_endpoint_io_t *io)
{
    if (!e || !io || !io->uart_send || !io->outputs ||
        !io->audio_playing || !io->audio_stop || !io->audio_play)
        return NOSTOS_BAD_ARGUMENT;
    nostos_sender_t s;
    nostos_result_t r=nostos_sender_init(&s,source,session);
    if (r!=NOSTOS_OK) return r;
    *e=(nostos_endpoint_t){.sender=s,.io=*io};
    r=nostos_receiver_init(&e->receiver,source);
    if(r!=NOSTOS_OK) return r;
    return nostos_receiver_approve_session(&e->receiver,source,session,0);
}
nostos_result_t nostos_endpoint_uart_byte(nostos_endpoint_t *e, uint8_t b, uint32_t now)
{
    if(!e) return NOSTOS_BAD_ARGUMENT;
    uint8_t wire[NOSTOS_WIRE_MAX]; size_t n=0;
    nostos_result_t r=nostos_uart_feed(&e->uart,b,now,wire,&n);
    return r==NOSTOS_OK?nostos_receiver_wire(&e->receiver,wire,n,now):r;
}
nostos_result_t nostos_endpoint_publish(nostos_endpoint_t *e, nostos_message_t *m, uint32_t now)
{
    if(!e || !m) return NOSTOS_BAD_ARGUMENT;
    nostos_result_t r=nostos_sender_stamp(&e->sender,m);
    if(r!=NOSTOS_OK) return r;
    uint8_t wire[NOSTOS_WIRE_MAX], frame[NOSTOS_UART_FRAME_MAX]; size_t n=0, f=0;
    r=nostos_message_encode(m,wire,sizeof(wire),&n);
    if(r!=NOSTOS_OK) return r;
    r=nostos_uart_encode(wire,n,frame,sizeof(frame),&f);
    if(r!=NOSTOS_OK) return r;
    /* An outgoing ACK describes a peer request, not a local ACK reception. */
    if(m->type!=NOSTOS_ACK) {
        r=nostos_receiver_wire(&e->receiver,wire,n,now);
        if(r!=NOSTOS_OK) return r;
    }
    return e->io.uart_send(e->io.context,frame,f)?NOSTOS_OK:NOSTOS_IO_ERROR;
}
void nostos_endpoint_process(nostos_endpoint_t *e, uint32_t now)
{
    if(!e) return;
    nostos_outputs_t o=nostos_receiver_outputs(&e->receiver,now);
    /* A muted FALL keeps the safety incident active even though its buzzer is off. */
    bool was_emergency=e->outputs_initialized &&
        e->last_outputs.led==NOSTOS_LED_RED_BLINK;
    if(!e->outputs_initialized || o.led!=e->last_outputs.led || o.buzzer!=e->last_outputs.buzzer) {
        e->io.outputs(e->io.context,o); e->last_outputs=o; e->outputs_initialized=true;
    }

    bool emergency=o.led==NOSTOS_LED_RED_BLINK;
    if(emergency) {
        bool pending=e->receiver.pending_stop.pending ||
            e->receiver.pending_button.pending;
        bool playing=e->io.audio_playing(e->io.context);
        nostos_receiver_clear_requests(&e->receiver);
        if(!was_emergency && (pending || playing)) ++e->fall_preemptions;
        if(playing && !e->io.audio_stop(e->io.context)) ++e->audio_failures;
        return;
    }

    nostos_message_t request;
    if(nostos_receiver_take_stop(&e->receiver,&request)==NOSTOS_OK) {
        bool displaced=e->receiver.pending_button.pending;
        bool playing=e->io.audio_playing(e->io.context);
        e->receiver.pending_button.pending=false;
        if(displaced || playing) ++e->stop_preemptions;
        if(playing && !e->io.audio_stop(e->io.context)) ++e->audio_failures;
        if(!e->io.audio_play(e->io.context,&request)) ++e->audio_failures;
        return;
    }

    if(nostos_receiver_take_button(&e->receiver,&request)==NOSTOS_OK) {
        if(e->io.audio_playing(e->io.context)) {
            ++e->dropped_busy_buttons;
            return;
        }
        if(!e->io.audio_play(e->io.context,&request)) ++e->audio_failures;
    }
}
