#include "nostos_state.h"
#include <string.h>
static bool same_ref(nostos_incident_ref_t a, nostos_incident_ref_t b)
{ return a.session_id==b.session_id && a.incident_id==b.incident_id; }
static bool newer(const nostos_report_t *r, const nostos_message_t *m)
{ return !r->seen || m->session_id>r->session_id || (m->session_id==r->session_id && m->sequence>r->sequence); }
static void report(nostos_report_t *r, const nostos_message_t *m, uint32_t now)
{ *r=(nostos_report_t){m->session_id,m->sequence,now,true}; }
bool nostos_report_fresh(const nostos_report_t *r, uint32_t now, uint32_t age)
{ return r && r->seen && (uint32_t)(now-r->received_ms)<=age; }
nostos_result_t nostos_receiver_init(nostos_receiver_t *r, uint8_t local)
{
    if (!r || local<1 || local>NOSTOS_NODE_COUNT) return NOSTOS_BAD_ARGUMENT;
    *r=(nostos_receiver_t){.local_source=local};
    for (uint8_t i=0; i<NOSTOS_NODE_COUNT; ++i) r->shared_data.nodes[i].source_id=(uint8_t)(i+1);
    return NOSTOS_OK;
}
nostos_result_t nostos_receiver_approve_session(nostos_receiver_t *r, uint8_t source, uint32_t session, uint16_t floor)
{
    if (!r || source<1 || source>NOSTOS_NODE_COUNT || !session) return NOSTOS_BAD_ARGUMENT;
    nostos_rx_window_t *w=&r->windows[source-1];
    if (w->approved && session<=w->session_id) return NOSTOS_STALE;
    *w=(nostos_rx_window_t){.session_id=session,.floor=floor,.approved=true};
    /* A source session is a strict data epoch. Never expose values, incidents,
     * or queued requests that were created by the replaced process lifetime. */
    nostos_node_state_t *n=&r->shared_data.nodes[source-1];
    *n=(nostos_node_state_t){.source_id=source};
    if (r->pending_stop.message.source_id==source)
        r->pending_stop=(nostos_request_slot_t){0};
    if (r->pending_button.message.source_id==source)
        r->pending_button=(nostos_request_slot_t){0};
    for (size_t i=0; i<NOSTOS_INCIDENT_CAPACITY; ++i)
        if (r->incidents[i].source_id==source)
            r->incidents[i]=(nostos_incident_record_t){0};
    return NOSTOS_OK;
}
static nostos_result_t window_check(const nostos_rx_window_t *w, const nostos_message_t *m)
{
    if (!w->approved || w->session_id!=m->session_id) return NOSTOS_SESSION_REQUIRED;
    if (m->sequence<w->floor) return NOSTOS_STALE;
    if (w->started && m->sequence<=w->highest) {
        unsigned back=(unsigned)w->highest-m->sequence;
        if (back>=64) return NOSTOS_STALE;
        if (w->seen&(UINT64_C(1)<<back)) return NOSTOS_DUPLICATE;
    }
    return NOSTOS_OK;
}
static void window_mark(nostos_rx_window_t *w, uint16_t seq)
{
    if (!w->started) { w->highest=seq; w->seen=1; w->started=true; }
    else if (seq>w->highest) {
        unsigned step=(unsigned)seq-w->highest;
        w->seen=step>=64?1:(w->seen<<step)|1; w->highest=seq;
    } else w->seen|=UINT64_C(1)<<((unsigned)w->highest-seq);
}
static nostos_result_t apply_incident(nostos_receiver_t *r, nostos_node_state_t *node,
    const nostos_message_t *m, uint32_t now)
{
    bool clear=m->type==NOSTOS_FALL_CLEAR;
    uint8_t kind=NOSTOS_FALL;
    nostos_incident_ref_t ref=m->payload.incident;
    if ((!clear && ref.session_id!=m->session_id) || ref.session_id>m->session_id) return NOSTOS_UNAUTHORIZED;
    nostos_incident_record_t *slot=NULL, *free_slot=NULL;
    for (size_t i=0; i<NOSTOS_INCIDENT_CAPACITY; ++i) {
        nostos_incident_record_t *x=&r->incidents[i];
        if (!x->used) { if (!free_slot) free_slot=x; }
        else if (x->source_id==m->source_id && x->kind==kind && same_ref(x->ref,ref)) slot=x;
    }
    if (slot && slot->closed && !clear) return NOSTOS_STALE;
    if (!slot) {
        if (!free_slot) return NOSTOS_FULL; /* Never evict an active incident or tombstone silently. */
        slot=free_slot;
        *slot=(nostos_incident_record_t){.source_id=m->source_id,.kind=kind,.ref=ref,.used=true};
    }
    slot->closed|=clear;
    nostos_incident_state_t *s=&node->fall;
    if (clear && same_ref(s->incident,ref)) s->phase=NOSTOS_INCIDENT_CLOSED;
    /* An old CLEAR can close its own record without replacing a newer display. */
    if (newer(&s->last_report,m) && (!s->last_report.seen ||
        ref.session_id>s->incident.session_id ||
        (ref.session_id==s->incident.session_id && ref.incident_id>=s->incident.incident_id))) {
        s->incident=ref; s->phase=slot->closed?NOSTOS_INCIDENT_CLOSED:NOSTOS_INCIDENT_ACTIVE;
        report(&s->last_report,m,now);
    }
    return NOSTOS_OK;
}
nostos_result_t nostos_receiver_apply(nostos_receiver_t *r, const nostos_message_t *input, uint32_t now)
{
    if (!r || !input) return NOSTOS_BAD_ARGUMENT;
    /* Also validate callers that construct C objects directly. Canonicalize
     * humidity identically on local and remote displays. */
    uint8_t wire[NOSTOS_WIRE_MAX]; size_t length=0;
    nostos_result_t result=nostos_message_encode(input,wire,sizeof(wire),&length);
    if (result!=NOSTOS_OK) return result;
    nostos_message_t m;
    result=nostos_message_decode(wire,length,&m);
    if (result!=NOSTOS_OK) return result;
    nostos_rx_window_t *w=&r->windows[m.source_id-1];
    result=window_check(w,&m);
    if (result!=NOSTOS_OK) return result;
    nostos_node_state_t *n=&r->shared_data.nodes[m.source_id-1];
    nostos_report_t *ordering=NULL;
    switch (m.type) {
    case NOSTOS_ENVIRONMENT: ordering=&n->environment.report; break;
    case NOSTOS_RIDE: ordering=&n->ride.report; break;
    case NOSTOS_HEARTBEAT: ordering=&n->health.report; break;
    default: break;
    }
    if (ordering && !newer(ordering,&m)) return NOSTOS_STALE;
    if (m.type==NOSTOS_STOP) {
        r->pending_stop=(nostos_request_slot_t){.message=m,.pending=true};
    } else if (m.type==NOSTOS_SPEED_DOWN || m.type==NOSTOS_SPEED_UP) {
        if (r->pending_button.pending) {
            /* Intentionally reject a second button while the one-slot
             * application mailbox is occupied, but consume its sequence so
             * transport duplication cannot replay it later. */
            n->reachability=(nostos_reachability_t){now,true};
            window_mark(w,m.sequence);
            return NOSTOS_FULL;
        }
        r->pending_button=(nostos_request_slot_t){.message=m,.pending=true};
    } else if (m.type==NOSTOS_ENVIRONMENT) {
        nostos_environment_t *e=&m.payload.environment;
        nostos_i16_value_t *t=&n->environment.temperature_c_x10;
        nostos_u16_value_t *h=&n->environment.humidity_pct_x10;
        t->quality=e->temperature_quality; h->quality=e->humidity_quality;
        if (t->quality==NOSTOS_VALID) { t->value=e->temperature_c_x10; t->has_value=true; t->value_received_ms=now; }
        if (h->quality==NOSTOS_VALID) { h->value=e->humidity_pct_x10; h->has_value=true; h->value_received_ms=now; }
    } else if (m.type==NOSTOS_RIDE) {
        nostos_u16_value_t *speed=&n->ride.speed_kmh_x10;
        nostos_u32_value_t *distance=&n->ride.distance_mm;
        speed->quality=m.payload.ride.valid?NOSTOS_VALID:NOSTOS_UNMEASURED;
        distance->quality=speed->quality;
        if (m.payload.ride.valid) {
            speed->value=m.payload.ride.kmh_x10;
            speed->has_value=true; speed->value_received_ms=now;
            distance->value=m.payload.ride.distance_mm;
            distance->has_value=true; distance->value_received_ms=now;
        }
    } else if (m.type==NOSTOS_HEARTBEAT) n->health.status=m.payload.status;
    else if (m.type==NOSTOS_ACK) {
        /* ACK is a control observation for the caller, not shared sensor state.
         * Matching an outstanding transaction is the sender's responsibility. */
        if (m.payload.ack.source_id!=r->local_source) return NOSTOS_UNAUTHORIZED;
    } else {
        result=apply_incident(r,n,&m,now);
        if (result!=NOSTOS_OK) return result;
    }
    if (ordering) report(ordering,&m,now);
    n->reachability=(nostos_reachability_t){now,true};
    window_mark(w,m.sequence);
    return NOSTOS_OK;
}
nostos_result_t nostos_receiver_wire(nostos_receiver_t *r, const uint8_t *w, size_t n, uint32_t now)
{
    nostos_message_t m; nostos_result_t result=nostos_message_decode(w,n,&m);
    return result==NOSTOS_OK?nostos_receiver_apply(r,&m,now):result;
}
static nostos_result_t take_request(
    nostos_request_slot_t *slot,
    nostos_message_t *message)
{
    if (!slot || !message) return NOSTOS_BAD_ARGUMENT;
    if (!slot->pending) return NOSTOS_EMPTY;
    *message=slot->message;
    slot->pending=false;
    return NOSTOS_OK;
}
nostos_result_t nostos_receiver_take_stop(nostos_receiver_t *r, nostos_message_t *m)
{ return r?take_request(&r->pending_stop,m):NOSTOS_BAD_ARGUMENT; }
nostos_result_t nostos_receiver_take_button(nostos_receiver_t *r, nostos_message_t *m)
{ return r?take_request(&r->pending_button,m):NOSTOS_BAD_ARGUMENT; }
void nostos_receiver_clear_requests(nostos_receiver_t *r)
{
    if (!r) return;
    r->pending_stop.pending=false;
    r->pending_button.pending=false;
}
nostos_result_t nostos_receiver_mute(nostos_receiver_t *r, uint8_t source, uint8_t kind, nostos_incident_ref_t ref)
{
    if (!r) return NOSTOS_BAD_ARGUMENT;
    for (size_t i=0; i<NOSTOS_INCIDENT_CAPACITY; ++i) {
        nostos_incident_record_t *x=&r->incidents[i];
        if (x->used && !x->closed && x->source_id==source && x->kind==kind && same_ref(x->ref,ref)) {
            x->muted=true; return NOSTOS_OK;
        }
    }
    return NOSTOS_STALE;
}
nostos_outputs_t nostos_receiver_outputs(const nostos_receiver_t *r, uint32_t now)
{
    (void)now;
    nostos_outputs_t o={NOSTOS_LED_OFF,NOSTOS_BUZZER_OFF};
    if (!r) return o;
    bool emergency=false, audible=false;
    for (size_t i=0; i<NOSTOS_INCIDENT_CAPACITY; ++i) {
        const nostos_incident_record_t *x=&r->incidents[i];
        if (x->used && !x->closed) { emergency=true; audible|=!x->muted; }
    }
    o.led=emergency?NOSTOS_LED_RED_BLINK:NOSTOS_LED_OFF;
    o.buzzer=audible?NOSTOS_BUZZER_EMERGENCY:NOSTOS_BUZZER_OFF;
    return o;
}
