#include "nostos_protocol.h"
#include <string.h>

const nostos_type_info_t nostos_types[NOSTOS_TYPE_COUNT] = {
    {0x10,0,"SPEED_DOWN"}, {0x11,0,"SPEED_UP"}, {0x12,0,"SAFETY_REMINDER"},
    {0x13,0,"STOP"}, {0x20,0,"REAR_SAFE"}, {0x21,0,"REAR_WARNING"},
    {0x22,0,"REAR_UNKNOWN"}, {0x30,6,"FALL"}, {0x31,6,"SOS"},
    {0x40,3,"SPEED"}, {0x41,2,"ENVIRONMENT"}, {0x42,6,"FALL_CLEAR"},
    {0x43,6,"SOS_CLEAR"}, {0x50,1,"HEARTBEAT"}, {0x51,9,"ACK"}
};
const nostos_type_info_t *nostos_type_info(uint8_t type)
{
    for (size_t i=0; i<NOSTOS_TYPE_COUNT; ++i)
        if (nostos_types[i].type == type) return &nostos_types[i];
    return NULL;
}
const char *nostos_result_name(nostos_result_t r)
{
    static const char *const names[] = {"OK","EMPTY","BAD_ARGUMENT","BAD_LENGTH",
        "BAD_VALUE","TOO_LARGE","UNSUPPORTED_VERSION","UNSUPPORTED_TYPE",
        "BAD_CRC","TIMEOUT","UNAUTHORIZED","SESSION_REQUIRED","STALE",
        "DUPLICATE","FULL","NOT_READY","EXPIRED","EXHAUSTED","CONFLICT","IO_ERROR"};
    return (unsigned)r < sizeof(names)/sizeof(names[0]) ? names[r] : "UNKNOWN";
}
static void put16(uint8_t *p, uint16_t n) { p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8); }
static void put32(uint8_t *p, uint32_t n) { put16(p,(uint16_t)n); put16(p+2,(uint16_t)(n>>16)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1]<<8)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)get16(p) | ((uint32_t)get16(p+2)<<16); }
static bool source_valid(uint8_t n) { return n>=1 && n<=NOSTOS_NODE_COUNT; }
static bool incident_type(uint8_t t) { return t==0x30 || t==0x31 || t==0x42 || t==0x43; }
static nostos_result_t quality_code(nostos_quality_t q, uint8_t *code)
{
    switch (q) {
    case NOSTOS_UNMEASURED: *code=254; break;
    case NOSTOS_BELOW_RANGE: *code=251; break;
    case NOSTOS_ABOVE_RANGE: *code=252; break;
    case NOSTOS_SENSOR_ERROR: *code=253; break;
    default: return NOSTOS_BAD_VALUE;
    }
    return NOSTOS_OK;
}
static nostos_result_t read_quality(uint8_t code, uint8_t maximum, nostos_quality_t *q)
{
    if (code<=maximum) { *q=NOSTOS_VALID; return NOSTOS_OK; }
    switch (code) {
    case 251: *q=NOSTOS_BELOW_RANGE; break;
    case 252: *q=NOSTOS_ABOVE_RANGE; break;
    case 253: *q=NOSTOS_SENSOR_ERROR; break;
    case 254: *q=NOSTOS_UNMEASURED; break;
    default: return NOSTOS_BAD_VALUE;
    }
    return NOSTOS_OK;
}
nostos_result_t nostos_envelope_validate(const uint8_t *w, size_t n)
{
    if (!w) return NOSTOS_BAD_ARGUMENT;
    if (n>NOSTOS_WIRE_MAX) return NOSTOS_TOO_LARGE;
    if (!n) return NOSTOS_BAD_LENGTH;
    if (w[0]!=NOSTOS_VERSION) return NOSTOS_UNSUPPORTED_VERSION;
    if (n<NOSTOS_HEADER_SIZE) return NOSTOS_BAD_LENGTH;
    if (!source_valid(w[2]) || !get32(w+3)) return NOSTOS_BAD_VALUE;
    return NOSTOS_OK;
}
nostos_result_t nostos_message_decode(const uint8_t *w, size_t n, nostos_message_t *out)
{
    if (!out) return NOSTOS_BAD_ARGUMENT;
    nostos_result_t r=nostos_envelope_validate(w,n);
    if (r!=NOSTOS_OK) return r;
    const nostos_type_info_t *info=nostos_type_info(w[1]);
    if (!info) return NOSTOS_UNSUPPORTED_TYPE;
    if (n!=NOSTOS_HEADER_SIZE+info->payload_size) return NOSTOS_BAD_LENGTH;
    nostos_message_t m={0};
    m.type=w[1]; m.source_id=w[2]; m.session_id=get32(w+3); m.sequence=get16(w+7);
    const uint8_t *p=w+NOSTOS_HEADER_SIZE;
    if (incident_type(m.type)) {
        m.payload.incident=(nostos_incident_ref_t){get32(p),get16(p+4)};
        if (!m.payload.incident.session_id || !m.payload.incident.incident_id) return NOSTOS_BAD_VALUE;
    } else if (m.type==NOSTOS_SPEED) {
        if (p[0]>1 || (!p[0] && get16(p+1))) return NOSTOS_BAD_VALUE;
        m.payload.speed=(nostos_speed_t){p[0]!=0,get16(p+1)};
    } else if (m.type==NOSTOS_ENVIRONMENT) {
        nostos_environment_t *e=&m.payload.environment;
        if (read_quality(p[0],250,&e->temperature_quality)!=NOSTOS_OK ||
            read_quality(p[1],200,&e->humidity_quality)!=NOSTOS_OK) return NOSTOS_BAD_VALUE;
        if (e->temperature_quality==NOSTOS_VALID) e->temperature_c_x10=(int16_t)(225+p[0]);
        if (e->humidity_quality==NOSTOS_VALID) e->humidity_pct_x10=(uint16_t)((uint16_t)p[1]*5U);
    } else if (m.type==NOSTOS_HEARTBEAT) {
        if (p[0] & (uint8_t)~NOSTOS_STATUS_MASK) return NOSTOS_BAD_VALUE;
        m.payload.status=p[0];
    } else if (m.type==NOSTOS_ACK) {
        m.payload.ack=(nostos_ack_t){.source_id=p[0],.type=p[7],.session_id=get32(p+1),
            .sequence=get16(p+5),.result=p[8]};
        if (!source_valid(p[0]) || p[0]==m.source_id || !m.payload.ack.session_id ||
            p[7]==NOSTOS_ACK || p[8]>3 || (p[8]<2 && !nostos_type_info(p[7]))) return NOSTOS_BAD_VALUE;
    }
    *out=m; /* No observable partial output on any error. */
    return NOSTOS_OK;
}
nostos_result_t nostos_message_encode(const nostos_message_t *m, uint8_t *out, size_t cap, size_t *length)
{
    if (!m || !out || !length) return NOSTOS_BAD_ARGUMENT;
    const nostos_type_info_t *info=nostos_type_info(m->type);
    if (!info) return NOSTOS_UNSUPPORTED_TYPE;
    size_t n=NOSTOS_HEADER_SIZE+info->payload_size;
    if (cap<n) return NOSTOS_BAD_LENGTH;
    uint8_t w[NOSTOS_WIRE_MAX]={NOSTOS_VERSION};
    w[1]=m->type; w[2]=m->source_id; put32(w+3,m->session_id); put16(w+7,m->sequence);
    uint8_t *p=w+NOSTOS_HEADER_SIZE;
    if (incident_type(m->type)) {
        put32(p,m->payload.incident.session_id); put16(p+4,m->payload.incident.incident_id);
    } else if (m->type==NOSTOS_SPEED) {
        p[0]=(uint8_t)m->payload.speed.valid; put16(p+1,m->payload.speed.kmh_x10);
    } else if (m->type==NOSTOS_ENVIRONMENT) {
        const nostos_environment_t *e=&m->payload.environment;
        if (e->temperature_quality==NOSTOS_VALID) {
            int t=e->temperature_c_x10;
            p[0]=t<225?251:t>475?252:(uint8_t)(t-225);
        } else if (quality_code(e->temperature_quality,p)!=NOSTOS_OK) return NOSTOS_BAD_VALUE;
        if (e->humidity_quality==NOSTOS_VALID) {
            unsigned h=e->humidity_pct_x10;
            p[1]=h>1000?252:(uint8_t)((h+2)/5);
        } else if (quality_code(e->humidity_quality,p+1)!=NOSTOS_OK) return NOSTOS_BAD_VALUE;
    } else if (m->type==NOSTOS_HEARTBEAT) p[0]=m->payload.status;
    else if (m->type==NOSTOS_ACK) {
        const nostos_ack_t *a=&m->payload.ack;
        p[0]=a->source_id; put32(p+1,a->session_id); put16(p+5,a->sequence); p[7]=a->type; p[8]=a->result;
    }
    nostos_message_t validated;
    nostos_result_t r=nostos_message_decode(w,n,&validated);
    if (r!=NOSTOS_OK) return r;
    memcpy(out,w,n); *length=n;
    return NOSTOS_OK;
}
nostos_result_t nostos_sender_init(nostos_sender_t *s, uint8_t source, uint32_t session)
{
    if (!s || !source_valid(source) || !session) return NOSTOS_BAD_ARGUMENT;
    *s=(nostos_sender_t){source,session,0}; return NOSTOS_OK;
}
nostos_result_t nostos_sender_stamp(nostos_sender_t *s, nostos_message_t *m)
{
    if (!s || !m || !source_valid(s->source_id) || !s->session_id) return NOSTOS_BAD_ARGUMENT;
    if (s->next_sequence>UINT16_MAX) return NOSTOS_EXHAUSTED;
    m->source_id=s->source_id; m->session_id=s->session_id; m->sequence=(uint16_t)s->next_sequence++;
    return NOSTOS_OK;
}
