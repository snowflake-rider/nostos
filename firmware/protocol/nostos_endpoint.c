#include "nostos_endpoint.h"
nostos_result_t nostos_endpoint_init(nostos_endpoint_t *e, uint8_t source, uint32_t session, const nostos_endpoint_io_t *io)
{
    if (!e || !io || !io->uart_send || !io->outputs || !io->audio_ready || !io->audio_play)
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
    if(!e->outputs_initialized || o.led!=e->last_outputs.led || o.buzzer!=e->last_outputs.buzzer) {
        e->io.outputs(e->io.context,o); e->last_outputs=o; e->outputs_initialized=true;
    }
    /* Expire stale queued requests even while audio is busy; no late replay. */
    while(e->receiver.request_count) {
        size_t head=e->receiver.request_head;
        bool expired=(uint32_t)(now-e->receiver.request_received_ms[head])>NOSTOS_REQUEST_MAX_AGE_MS;
        if(!expired && !e->io.audio_ready(e->io.context)) break;
        nostos_message_t m;
        if(nostos_receiver_pop_request(&e->receiver,&m)!=NOSTOS_OK) break;
        if(expired) { ++e->expired_requests; continue; }
        if(!e->io.audio_play(e->io.context,m.type)) ++e->audio_failures;
        break; /* Bounded one new audio start per app iteration. */
    }
}
