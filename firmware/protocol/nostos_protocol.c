#include "nostos_protocol.h"
#include <string.h>

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
static nostos_result_t encode_empty(const nostos_message_t *m, uint8_t *p)
{
    (void)m; (void)p; return NOSTOS_OK;
}
static nostos_result_t decode_empty(const uint8_t *p, nostos_message_t *m)
{
    (void)p; (void)m; return NOSTOS_OK;
}
static nostos_result_t encode_incident(const nostos_message_t *m, uint8_t *p)
{
    put32(p,m->payload.incident.session_id); put16(p+4,m->payload.incident.incident_id);
    return NOSTOS_OK;
}
static nostos_result_t decode_incident(const uint8_t *p, nostos_message_t *m)
{
    m->payload.incident=(nostos_incident_ref_t){get32(p),get16(p+4)};
    return m->payload.incident.session_id && m->payload.incident.incident_id?NOSTOS_OK:NOSTOS_BAD_VALUE;
}
static nostos_result_t encode_ride(const nostos_message_t *m, uint8_t *p)
{
    const nostos_ride_t *ride=&m->payload.ride;
    if (!ride->valid && (ride->kmh_x10!=0U || ride->distance_mm!=0U))
        return NOSTOS_BAD_VALUE;
    p[0]=(uint8_t)ride->valid;
    put16(p+1,ride->kmh_x10);
    put32(p+3,ride->distance_mm);
    return NOSTOS_OK;
}
static nostos_result_t decode_ride(const uint8_t *p, nostos_message_t *m)
{
    uint16_t kmh_x10=get16(p+1);
    uint32_t distance_mm=get32(p+3);
    if (p[0]>1U || (p[0]==0U && (kmh_x10!=0U || distance_mm!=0U)))
        return NOSTOS_BAD_VALUE;
    m->payload.ride=(nostos_ride_t){p[0]!=0U,kmh_x10,distance_mm};
    return NOSTOS_OK;
}
static nostos_result_t encode_shared_data_request(
    const nostos_message_t *m,
    uint8_t *p)
{
    uint8_t mask=m->payload.shared_data_request.mask;
    if (!mask || (mask&(uint8_t)~NOSTOS_SHARED_DATA_MASK))
        return NOSTOS_BAD_VALUE;
    p[0]=mask;
    return NOSTOS_OK;
}
static nostos_result_t decode_shared_data_request(
    const uint8_t *p,
    nostos_message_t *m)
{
    if (!p[0] || (p[0]&(uint8_t)~NOSTOS_SHARED_DATA_MASK))
        return NOSTOS_BAD_VALUE;
    m->payload.shared_data_request.mask=p[0];
    return NOSTOS_OK;
}
static nostos_result_t encode_environment(const nostos_message_t *m, uint8_t *p)
{
    const nostos_environment_t *e=&m->payload.environment;
    if (e->temperature_quality==NOSTOS_VALID) {
        int temperature=e->temperature_c_x10;
        p[0]=temperature<225?251U:temperature>475?252U:(uint8_t)(temperature-225);
    } else if (quality_code(e->temperature_quality,p)!=NOSTOS_OK) return NOSTOS_BAD_VALUE;
    if (e->humidity_quality==NOSTOS_VALID) {
        unsigned humidity=e->humidity_pct_x10;
        p[1]=humidity>1000U?252U:(uint8_t)((humidity+2U)/5U);
    } else if (quality_code(e->humidity_quality,p+1)!=NOSTOS_OK) return NOSTOS_BAD_VALUE;
    return NOSTOS_OK;
}
static nostos_result_t decode_environment(const uint8_t *p, nostos_message_t *m)
{
    nostos_environment_t *e=&m->payload.environment;
    if (read_quality(p[0],250U,&e->temperature_quality)!=NOSTOS_OK ||
        read_quality(p[1],200U,&e->humidity_quality)!=NOSTOS_OK) return NOSTOS_BAD_VALUE;
    if (e->temperature_quality==NOSTOS_VALID) e->temperature_c_x10=(int16_t)(225U+p[0]);
    if (e->humidity_quality==NOSTOS_VALID) e->humidity_pct_x10=(uint16_t)((uint16_t)p[1]*5U);
    return NOSTOS_OK;
}
static nostos_result_t encode_heartbeat(const nostos_message_t *m, uint8_t *p)
{
    p[0]=m->payload.status; return NOSTOS_OK;
}
static nostos_result_t decode_heartbeat(const uint8_t *p, nostos_message_t *m)
{
    if (p[0]&(uint8_t)~NOSTOS_STATUS_MASK) return NOSTOS_BAD_VALUE;
    m->payload.status=p[0]; return NOSTOS_OK;
}
static nostos_result_t encode_ack(const nostos_message_t *m, uint8_t *p)
{
    const nostos_ack_t *ack=&m->payload.ack;
    p[0]=ack->source_id; put32(p+1,ack->session_id); put16(p+5,ack->sequence);
    p[7]=ack->type; p[8]=ack->result; return NOSTOS_OK;
}
static nostos_result_t decode_ack(const uint8_t *p, nostos_message_t *m)
{
    m->payload.ack=(nostos_ack_t){.source_id=p[0],.type=p[7],.session_id=get32(p+1),
        .sequence=get16(p+5),.result=p[8]};
    if (!source_valid(p[0]) || p[0]==m->source_id || !m->payload.ack.session_id ||
        p[7]==NOSTOS_ACK || p[8]>3U || (p[8]<2U && !nostos_type_info(p[7]))) return NOSTOS_BAD_VALUE;
    return NOSTOS_OK;
}

const nostos_type_info_t nostos_types[NOSTOS_TYPE_COUNT] = {
    {NOSTOS_SPEED_DOWN,0U,"SPEED_DOWN",encode_empty,decode_empty},
    {NOSTOS_SPEED_UP,0U,"SPEED_UP",encode_empty,decode_empty},
    {NOSTOS_STOP,0U,"STOP",encode_empty,decode_empty},
    {NOSTOS_FALL,6U,"FALL",encode_incident,decode_incident},
    {NOSTOS_ENVIRONMENT,2U,"ENVIRONMENT",encode_environment,decode_environment},
    {NOSTOS_FALL_CLEAR,6U,"FALL_CLEAR",encode_incident,decode_incident},
    {NOSTOS_RIDE,7U,"RIDE",encode_ride,decode_ride},
    {NOSTOS_SHARED_DATA_REQUEST,1U,"SHARED_DATA_REQUEST",
        encode_shared_data_request,decode_shared_data_request},
    {NOSTOS_HEARTBEAT,1U,"HEARTBEAT",encode_heartbeat,decode_heartbeat},
    {NOSTOS_ACK,9U,"ACK",encode_ack,decode_ack}
};
const nostos_type_info_t *nostos_type_info(uint8_t type)
{
    for (size_t i=0; i<NOSTOS_TYPE_COUNT; ++i)
        if (nostos_types[i].type==type) return &nostos_types[i];
    return NULL;
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
    r=info->decode_payload(w+NOSTOS_HEADER_SIZE,&m);
    if (r!=NOSTOS_OK) return r;
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
    nostos_result_t r=info->encode_payload(m,w+NOSTOS_HEADER_SIZE);
    if (r!=NOSTOS_OK) return r;
    nostos_message_t validated;
    r=nostos_message_decode(w,n,&validated);
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
