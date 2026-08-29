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
    /* Previous values are retained for diagnostics but not advertised as fresh. */
    nostos_node_state_t *n=&r->shared_data.nodes[source-1];
    n->reachability.seen=false;
    n->health.report.seen=false;
    n->environment.temperature_c_x10.quality=NOSTOS_UNMEASURED;
    n->environment.humidity_pct_x10.quality=NOSTOS_UNMEASURED;
    n->speed.speed_kmh_x10.quality=NOSTOS_UNMEASURED;
    n->rear.state=NOSTOS_REAR_IS_UNKNOWN; n->rear.quality=NOSTOS_UNMEASURED;
    /* Drop queued requests from the replaced source epoch, preserve others. */
    size_t keep=0;
    for (size_t i=0; i<r->request_count; ++i) {
        size_t from=(r->request_head+i)%NOSTOS_REQUEST_CAPACITY;
        if (r->requests[from].source_id==source) continue;
        size_t to=(r->request_head+keep++)%NOSTOS_REQUEST_CAPACITY;
        r->requests[to]=r->requests[from];
        r->request_received_ms[to]=r->request_received_ms[from];
    }
    r->request_count=keep;
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
    bool clear=m->type==NOSTOS_FALL_CLEAR || m->type==NOSTOS_SOS_CLEAR;
    uint8_t kind=(m->type==NOSTOS_FALL || m->type==NOSTOS_FALL_CLEAR)?NOSTOS_FALL:NOSTOS_SOS;
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
    nostos_incident_state_t *s=kind==NOSTOS_FALL?&node->fall:&node->sos;
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
    case NOSTOS_SPEED: ordering=&n->speed.report; break;
    case NOSTOS_REAR_SAFE: case NOSTOS_REAR_WARNING: case NOSTOS_REAR_UNKNOWN: ordering=&n->rear.report; break;
    case NOSTOS_HEARTBEAT: ordering=&n->health.report; break;
    default: break;
    }
    if (ordering && !newer(ordering,&m)) return NOSTOS_STALE;
    if (m.type>=NOSTOS_SPEED_DOWN && m.type<=NOSTOS_STOP) {
        if (r->request_count==NOSTOS_REQUEST_CAPACITY) return NOSTOS_FULL;
        size_t slot=(r->request_head+r->request_count)%NOSTOS_REQUEST_CAPACITY;
        r->requests[slot]=m; r->request_received_ms[slot]=now; ++r->request_count;
    } else if (m.type==NOSTOS_ENVIRONMENT) {
        nostos_environment_t *e=&m.payload.environment;
        nostos_i16_value_t *t=&n->environment.temperature_c_x10;
        nostos_u16_value_t *h=&n->environment.humidity_pct_x10;
        t->quality=e->temperature_quality; h->quality=e->humidity_quality;
        if (t->quality==NOSTOS_VALID) { t->value=e->temperature_c_x10; t->has_value=true; t->value_received_ms=now; }
        if (h->quality==NOSTOS_VALID) { h->value=e->humidity_pct_x10; h->has_value=true; h->value_received_ms=now; }
    } else if (m.type==NOSTOS_SPEED) {
        nostos_u16_value_t *s=&n->speed.speed_kmh_x10;
        s->quality=m.payload.speed.valid?NOSTOS_VALID:NOSTOS_UNMEASURED;
        if (m.payload.speed.valid) { s->value=m.payload.speed.kmh_x10; s->has_value=true; s->value_received_ms=now; }
    } else if (m.type==NOSTOS_HEARTBEAT) n->health.status=m.payload.status;
    else if (m.type==NOSTOS_REAR_SAFE || m.type==NOSTOS_REAR_WARNING || m.type==NOSTOS_REAR_UNKNOWN) {
        n->rear.state=m.type==NOSTOS_REAR_SAFE?NOSTOS_REAR_IS_SAFE:
            m.type==NOSTOS_REAR_WARNING?NOSTOS_REAR_IS_WARNING:NOSTOS_REAR_IS_UNKNOWN;
        n->rear.quality=m.type==NOSTOS_REAR_UNKNOWN?NOSTOS_SENSOR_ERROR:NOSTOS_VALID;
    } else if (m.type==NOSTOS_ACK) {
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
nostos_result_t nostos_receiver_pop_request(nostos_receiver_t *r, nostos_message_t *m)
{
    if (!r || !m) return NOSTOS_BAD_ARGUMENT;
    if (!r->request_count) return NOSTOS_EMPTY;
    *m=r->requests[r->request_head]; r->request_head=(r->request_head+1)%NOSTOS_REQUEST_CAPACITY; --r->request_count;
    return NOSTOS_OK;
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
    nostos_outputs_t o={NOSTOS_LED_OFF,NOSTOS_BUZZER_OFF};
    if (!r) return o;
    bool emergency=false, audible=false, rear_warning=false, rear_safe=false;
    for (size_t i=0; i<NOSTOS_INCIDENT_CAPACITY; ++i) {
        const nostos_incident_record_t *x=&r->incidents[i];
        if (x->used && !x->closed) { emergency=true; audible|=!x->muted; }
    }
    for (size_t i=0; i<NOSTOS_NODE_COUNT; ++i) {
        const nostos_rear_state_t *s=&r->shared_data.nodes[i].rear;
        if (nostos_report_fresh(&s->report,now,NOSTOS_FRESH_MS) && s->quality==NOSTOS_VALID) {
            rear_warning|=s->state==NOSTOS_REAR_IS_WARNING;
            rear_safe|=s->state==NOSTOS_REAR_IS_SAFE;
        }
    }
    o.led=emergency?NOSTOS_LED_RED_BLINK:rear_warning?NOSTOS_LED_YELLOW_BLINK:rear_safe?NOSTOS_LED_GREEN:NOSTOS_LED_OFF;
    o.buzzer=audible?NOSTOS_BUZZER_EMERGENCY:(!emergency && rear_warning)?NOSTOS_BUZZER_REAR:NOSTOS_BUZZER_OFF;
    return o;
}
