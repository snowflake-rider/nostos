#include "nostos_bridge.h"
#include "nostos_debug.h"
#include "nostos_endpoint.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_messages.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static const nostos_peer_t peers[3]={{0x101,1,1},{0x202,2,2},{0x303,3,3}};
static nostos_message_t message(uint8_t type, uint16_t seq)
{
    nostos_message_t m={.type=type,.source_id=2,.session_id=1,.sequence=seq};
    m.payload.incident=(nostos_incident_ref_t){1,1}; return m;
}
static nostos_receiver_t receiver(void)
{
    nostos_receiver_t r; CHECK(nostos_receiver_init(&r,3)==NOSTOS_OK);
    for (uint8_t i=1;i<=3;++i) CHECK(nostos_receiver_approve_session(&r,i,1,0)==NOSTOS_OK);
    return r;
}
static size_t encode(nostos_message_t m, uint8_t *out)
{
    size_t n=0; CHECK(nostos_message_encode(&m,out,NOSTOS_WIRE_MAX,&n)==NOSTOS_OK); return n;
}
static void codec(void)
{
    CHECK(COUNT(fixtures)==NOSTOS_TYPE_COUNT);
    for (size_t i=0;i<COUNT(fixtures);++i) {
        const fixture_t *f=&fixtures[i]; nostos_message_t m;
        CHECK(nostos_message_decode(f->wire,f->length,&m)==NOSTOS_OK);
        CHECK(m.type==f->type && m.source_id==f->source && m.session_id==f->session && m.sequence==f->sequence);
        if(m.type==NOSTOS_FALL || m.type==NOSTOS_FALL_CLEAR)
            CHECK(m.payload.incident.session_id==1 && m.payload.incident.incident_id==11);
        if(m.type==NOSTOS_SOS || m.type==NOSTOS_SOS_CLEAR)
            CHECK(m.payload.incident.session_id==1 && m.payload.incident.incident_id==12);
        if(m.type==NOSTOS_SPEED) CHECK(m.payload.speed.valid && m.payload.speed.kmh_x10==253);
        if(m.type==NOSTOS_ENVIRONMENT) CHECK(m.payload.environment.temperature_c_x10==362 && m.payload.environment.humidity_pct_x10==605);
        if(m.type==NOSTOS_HEARTBEAT) CHECK(m.payload.status==3);
        if(m.type==NOSTOS_ACK) CHECK(m.payload.ack.source_id==3 && m.payload.ack.session_id==1 &&
            m.payload.ack.sequence==7 && m.payload.ack.type==NOSTOS_STOP && m.payload.ack.result==0);
        const nostos_type_info_t *registered=nostos_type_info(nostos_types[i].type);
        CHECK(registered==&nostos_types[i] && registered->encode_payload && registered->decode_payload);
        bool covered=false;
        for(size_t j=0;j<COUNT(fixtures);++j) covered|=fixtures[j].type==nostos_types[i].type;
        CHECK(covered);
        uint8_t w[64]; size_t n=encode(m,w); CHECK(n==f->length && memcmp(w,f->wire,n)==0);
        printf("GOLDEN %-16s %zu bytes PASS\n",f->name,n);
        for (size_t cut=0;cut<f->length;++cut) {
            nostos_message_t before; memset(&m,0xa5,sizeof(m)); memcpy(&before,&m,sizeof(m));
            CHECK(nostos_message_decode(w,cut,&m)!=NOSTOS_OK); CHECK(memcmp(&before,&m,sizeof(m))==0);
        }
        CHECK(nostos_message_decode(w,n,&m)==NOSTOS_OK);
        uint8_t unchanged[64]; memset(w,0xa5,sizeof(w)); memcpy(unchanged,w,sizeof(w)); n=999;
        CHECK(nostos_message_encode(&m,w,f->length-1,&n)==NOSTOS_BAD_LENGTH);
        CHECK(n==999 && memcmp(w,unchanged,sizeof(w))==0);
    }
    CHECK(nostos_type_info(0x14)==NULL && nostos_type_info(0xff)==NULL);
    nostos_message_t e=message(NOSTOS_ENVIRONMENT,7), decoded;
    e.payload.environment=(nostos_environment_t){362,603,NOSTOS_VALID,NOSTOS_VALID};
    const uint8_t golden[]={2,0x41,2,1,0,0,0,7,0,0x89,0x79}; uint8_t w[64];
    CHECK(encode(e,w)==sizeof(golden) && !memcmp(w,golden,sizeof(golden)));
    for (int t=INT16_MIN;t<=INT16_MAX;++t) {
        e.payload.environment.temperature_c_x10=(int16_t)t;
        size_t n=encode(e,w); CHECK(nostos_message_decode(w,n,&decoded)==NOSTOS_OK);
        nostos_environment_t *d=&decoded.payload.environment;
        CHECK(d->temperature_quality==(t<225?NOSTOS_BELOW_RANGE:t>475?NOSTOS_ABOVE_RANGE:NOSTOS_VALID));
        if (t>=225 && t<=475) CHECK(d->temperature_c_x10==t);
    }
    e.payload.environment.temperature_c_x10=350;
    for (unsigned h=0;h<=UINT16_MAX;++h) {
        e.payload.environment.humidity_pct_x10=(uint16_t)h;
        size_t n=encode(e,w); CHECK(nostos_message_decode(w,n,&decoded)==NOSTOS_OK);
        if (h<=1000) {
            int error=(int)decoded.payload.environment.humidity_pct_x10-(int)h;
            CHECK(error>=-2 && error<=2);
        } else CHECK(decoded.payload.environment.humidity_quality==NOSTOS_ABOVE_RANGE);
    }
    memcpy(w,golden,sizeof(golden));
    for (unsigned t=0;t<256;++t) for (unsigned h=0;h<256;++h) {
        w[9]=(uint8_t)t; w[10]=(uint8_t)h;
        bool valid=t!=255 && (h<=200 || (h>=251 && h<=254));
        CHECK((nostos_message_decode(w,11,&decoded)==NOSTOS_OK)==valid);
    }
    CHECK(nostos_message_decode(NULL,0,&decoded)==NOSTOS_BAD_ARGUMENT);
    memcpy(w,golden,sizeof(golden)); w[0]=1;
    CHECK(nostos_message_decode(w,11,&decoded)==NOSTOS_UNSUPPORTED_VERSION);
    w[0]=2; w[1]=0x70; CHECK(nostos_message_decode(w,11,&decoded)==NOSTOS_UNSUPPORTED_TYPE);
    w[2]=4; CHECK(nostos_message_decode(w,11,&decoded)==NOSTOS_BAD_VALUE);
    nostos_sender_t s; CHECK(nostos_sender_init(&s,2,1)==NOSTOS_OK); s.next_sequence=65535;
    CHECK(nostos_sender_stamp(&s,&e)==NOSTOS_OK && e.sequence==65535);
    CHECK(nostos_sender_stamp(&s,&e)==NOSTOS_EXHAUSTED);
    puts("Integer domains, reserved codes, failed-output preservation, sequence exhaustion PASS");
}
static size_t feed_frame(nostos_uart_parser_t *p, const uint8_t *frame, size_t n, uint32_t tick, uint8_t *out)
{
    size_t frames=0, len=0;
    for(size_t i=0;i<n;++i) {
        nostos_result_t r=nostos_uart_feed(p,frame[i],tick,out,&len);
        CHECK(r==NOSTOS_EMPTY || r==NOSTOS_OK);
        if(r==NOSTOS_OK) ++frames;
    }
    CHECK(frames==1); return len;
}
static void uart(void)
{
    CHECK(nostos_crc16((const uint8_t *)"123456789",9)==0x29b1);
    /* Independently checked with Python binascii.crc_hqx(length+body,0xffff). */
    const uint8_t golden_body[]={2,0x41,2,1,0,0,0,7,0,0x89,0x79};
    const uint8_t golden_frame[]={0x7e,0x0b,2,0x41,2,1,0,0,0,7,0,0x89,0x79,0x05,0xb9,0x7e};
    uint8_t golden_encoded[NOSTOS_UART_FRAME_MAX]; size_t golden_n=0;
    CHECK(nostos_uart_encode(golden_body,sizeof(golden_body),golden_encoded,sizeof(golden_encoded),&golden_n)==NOSTOS_OK);
    CHECK(golden_n==sizeof(golden_frame) && !memcmp(golden_encoded,golden_frame,golden_n));
    for(size_t f=0;f<COUNT(fixtures);++f) {
        uint8_t frame[NOSTOS_UART_FRAME_MAX], out[64]; size_t n=0;
        CHECK(nostos_uart_encode(fixtures[f].wire,fixtures[f].length,frame,sizeof(frame),&n)==NOSTOS_OK);
        nostos_uart_parser_t p={0};
        CHECK(feed_frame(&p,frame,n,0,out)==fixtures[f].length);
        CHECK(!memcmp(out,fixtures[f].wire,fixtures[f].length));
        CHECK(feed_frame(&p,frame,n,1,out)==fixtures[f].length);
    }
    uint8_t w[64], f[NOSTOS_UART_FRAME_MAX], out[64]; memset(w,0x7e,sizeof(w));
    w[0]=2; w[1]=0x70; w[2]=2; w[9]=0x7d; size_t n=0,len=0;
    CHECK(nostos_uart_encode(w,64,f,sizeof(f),&n)==NOSTOS_OK && n>100);
    nostos_uart_parser_t p={0}; CHECK(feed_frame(&p,f,n,UINT32_MAX-10,out)==64); CHECK(!memcmp(w,out,64));
    nostos_uart_reset(&p);
    for(size_t i=0;i<5;++i) CHECK(nostos_uart_feed(&p,f[i],0,out,&len)==NOSTOS_EMPTY);
    CHECK(nostos_uart_feed(&p,1,101,out,&len)==NOSTOS_TIMEOUT);
    CHECK(feed_frame(&p,f,n,102,out)==64);
    /* Corrupt data without introducing a delimiter: CRC fails, next frame recovers. */
    uint8_t corrupt[NOSTOS_UART_FRAME_MAX]; memcpy(corrupt,f,n); corrupt[2]^=1;
    nostos_uart_reset(&p); nostos_result_t last=NOSTOS_EMPTY;
    for(size_t i=0;i<n;++i) last=nostos_uart_feed(&p,corrupt[i],0,out,&len);
    CHECK(last==NOSTOS_BAD_CRC); CHECK(feed_frame(&p,f,n,1,out)==64);
    nostos_uart_reset(&p);
    CHECK(nostos_uart_feed(&p,0x13,0,out,&len)==NOSTOS_EMPTY);
    CHECK(nostos_uart_feed(&p,0x7e,0,out,&len)==NOSTOS_EMPTY);
    CHECK(nostos_uart_feed(&p,65,0,out,&len)==NOSTOS_TOO_LARGE);
    CHECK(feed_frame(&p,f,n,1,out)==64);
    CHECK(nostos_uart_encode(w,65,f,sizeof(f),&n)==NOSTOS_TOO_LARGE);
    puts("CRC golden, 15 framed messages, escaping, full64B, partial/timeout/CRC recovery PASS");
}
static void state(void)
{
    nostos_receiver_t r=receiver(); nostos_message_t m=message(NOSTOS_ENVIRONMENT,10);
    m.payload.environment=(nostos_environment_t){362,603,NOSTOS_VALID,NOSTOS_VALID};
    CHECK(nostos_receiver_apply(&r,&m,100)==NOSTOS_OK);
    nostos_node_state_t *n=&r.shared_data.nodes[1];
    CHECK(n->environment.temperature_c_x10.value==362 && n->environment.humidity_pct_x10.value==605);
    CHECK(!r.shared_data.nodes[0].environment.report.seen);
    CHECK(nostos_receiver_apply(&r,&m,200)==NOSTOS_DUPLICATE);
    CHECK(n->environment.temperature_c_x10.value_received_ms==100);
    m.sequence=11; m.payload.environment.humidity_quality=NOSTOS_SENSOR_ERROR;
    CHECK(nostos_receiver_apply(&r,&m,300)==NOSTOS_OK);
    CHECK(n->environment.humidity_pct_x10.value==605 && n->environment.humidity_pct_x10.value_received_ms==100);
    CHECK(n->environment.humidity_pct_x10.quality==NOSTOS_SENSOR_ERROR);
    m=message(NOSTOS_HEARTBEAT,12); m.payload.status=0;
    CHECK(nostos_receiver_apply(&r,&m,400)==NOSTOS_OK);
    CHECK(n->environment.report.received_ms==300);
    CHECK(!nostos_report_fresh(&n->environment.report,4000,NOSTOS_FRESH_MS));
    m=message(NOSTOS_ENVIRONMENT,9); m.payload.environment=(nostos_environment_t){350,500,NOSTOS_VALID,NOSTOS_VALID};
    CHECK(nostos_receiver_apply(&r,&m,500)==NOSTOS_STALE);
    m=message(NOSTOS_SPEED,9); m.payload.speed=(nostos_speed_t){true,0};
    CHECK(nostos_receiver_apply(&r,&m,500)==NOSTOS_OK); /* Other stream may arrive out of order. */
    CHECK(n->speed.speed_kmh_x10.has_value && n->speed.speed_kmh_x10.value==0);
    m=message(NOSTOS_FALL_CLEAR,20); CHECK(nostos_receiver_apply(&r,&m,600)==NOSTOS_OK);
    m=message(NOSTOS_FALL,19); CHECK(nostos_receiver_apply(&r,&m,601)==NOSTOS_STALE);
    m=message(NOSTOS_FALL,21); m.payload.incident.incident_id=2;
    CHECK(nostos_receiver_apply(&r,&m,602)==NOSTOS_OK);
    CHECK(nostos_receiver_outputs(&r,602).buzzer==NOSTOS_BUZZER_EMERGENCY);
    CHECK(nostos_receiver_mute(&r,2,NOSTOS_FALL,m.payload.incident)==NOSTOS_OK);
    CHECK(nostos_receiver_outputs(&r,602).buzzer==NOSTOS_BUZZER_OFF);
    CHECK(nostos_receiver_outputs(&r,602).led==NOSTOS_LED_RED_BLINK);
    m=message(NOSTOS_SOS,22); CHECK(nostos_receiver_apply(&r,&m,603)==NOSTOS_OK);
    CHECK(nostos_receiver_outputs(&r,603).buzzer==NOSTOS_BUZZER_EMERGENCY);
    m=message(NOSTOS_FALL_CLEAR,23); m.payload.incident.incident_id=2;
    CHECK(nostos_receiver_apply(&r,&m,604)==NOSTOS_OK);
    CHECK(nostos_receiver_outputs(&r,604).led==NOSTOS_LED_RED_BLINK); /* SOS survives. */
    m=message(NOSTOS_SOS_CLEAR,24); m.source_id=1;
    CHECK(nostos_receiver_apply(&r,&m,605)==NOSTOS_OK); /* Closes source1 only, never source2. */
    CHECK(nostos_receiver_outputs(&r,605).led==NOSTOS_LED_RED_BLINK);
    CHECK(nostos_receiver_approve_session(&r,2,2,100)==NOSTOS_OK);
    CHECK(!r.shared_data.nodes[1].health.report.seen);
    CHECK(nostos_receiver_outputs(&r,50000).led==NOSTOS_LED_RED_BLINK);
    m=message(NOSTOS_SOS_CLEAR,25); CHECK(nostos_receiver_apply(&r,&m,606)==NOSTOS_SESSION_REQUIRED);
    m.session_id=2; m.sequence=99; CHECK(nostos_receiver_apply(&r,&m,606)==NOSTOS_STALE);
    m.sequence=100; CHECK(nostos_receiver_apply(&r,&m,606)==NOSTOS_OK);
    CHECK(nostos_receiver_outputs(&r,606).led==NOSTOS_LED_OFF);
    CHECK(nostos_receiver_approve_session(&r,2,1,0)==NOSTOS_STALE);
    r=receiver();
    for(uint16_t i=0;i<NOSTOS_REQUEST_CAPACITY;++i) { m=message(NOSTOS_STOP,i); CHECK(nostos_receiver_apply(&r,&m,0)==NOSTOS_OK); }
    m=message(NOSTOS_STOP,8); CHECK(nostos_receiver_apply(&r,&m,0)==NOSTOS_FULL);
    nostos_message_t request; CHECK(nostos_receiver_pop_request(&r,&request)==NOSTOS_OK && request.sequence==0);
    CHECK(nostos_receiver_apply(&r,&m,0)==NOSTOS_OK); /* Failed enqueue did not consume sequence. */
    r=receiver();
    for(uint16_t i=0;i<NOSTOS_INCIDENT_CAPACITY;++i) {
        m=message(NOSTOS_FALL_CLEAR,i); m.payload.incident.incident_id=(uint16_t)(i+1);
        CHECK(nostos_receiver_apply(&r,&m,0)==NOSTOS_OK);
    }
    m=message(NOSTOS_FALL,30); m.payload.incident.incident_id=100;
    CHECK(nostos_receiver_apply(&r,&m,0)==NOSTOS_FULL);
    CHECK(nostos_receiver_outputs(&r,0).led==NOSTOS_LED_OFF);
    puts("Freshness, quality, ordering, clear-before-fall, owner isolation, SOS, mute, session, bounded queues PASS");
}
static void bridge(void)
{
    nostos_bridge_t b; CHECK(nostos_bridge_init(&b,2,peers)==NOSTOS_OK);
    nostos_message_t m=message(NOSTOS_STOP,0); uint8_t w[64]; size_t n=encode(m,w);
    CHECK(nostos_bridge_accept(&b,NOSTOS_TO_MESH,w,n,0,0,false)==NOSTOS_NOT_READY);
    for(size_t i=0;i<NOSTOS_BRIDGE_NORMAL_CAPACITY;++i) CHECK(nostos_bridge_accept(&b,NOSTOS_TO_MESH,w,n,0,0,true)==NOSTOS_OK);
    CHECK(nostos_bridge_accept(&b,NOSTOS_TO_MESH,w,n,0,0,true)==NOSTOS_FULL);
    m=message(NOSTOS_FALL,1); n=encode(m,w);
    for(size_t i=0;i<NOSTOS_BRIDGE_URGENT_RESERVE;++i) CHECK(nostos_bridge_accept(&b,NOSTOS_TO_MESH,w,n,0,0,true)==NOSTOS_OK);
    CHECK(b.count==NOSTOS_BRIDGE_CAPACITY && b.normal_count==NOSTOS_BRIDGE_NORMAL_CAPACITY &&
        b.urgent_count==NOSTOS_BRIDGE_URGENT_RESERVE);
    CHECK(nostos_bridge_accept(&b,NOSTOS_TO_MESH,w,n,0,0,true)==NOSTOS_FULL);
    memset(w,0,sizeof(w)); nostos_job_t job;
    CHECK(nostos_bridge_next(&b,0,true,&job)==NOSTOS_OK && job.wire[1]==NOSTOS_FALL); /* Urgent first, owned copy. */
    for(size_t i=1;i<NOSTOS_BRIDGE_URGENT_RESERVE;++i)
        CHECK(nostos_bridge_next(&b,0,true,&job)==NOSTOS_OK && job.wire[1]==NOSTOS_FALL);
    CHECK(nostos_bridge_next(&b,2001,true,&job)==NOSTOS_EXPIRED && job.wire[1]==NOSTOS_STOP);
    while(nostos_bridge_next(&b,0,true,&job)==NOSTOS_OK) CHECK(job.wire[1]==NOSTOS_STOP);
    CHECK(nostos_bridge_next(&b,0,true,&job)==NOSTOS_EMPTY);

    CHECK(nostos_bridge_init(&b,2,peers)==NOSTOS_OK);
    m=message(NOSTOS_FALL,2); n=encode(m,w);
    for(size_t i=0;i<NOSTOS_BRIDGE_CAPACITY;++i)
        CHECK(nostos_bridge_accept(&b,NOSTOS_TO_MESH,w,n,0,0,true)==NOSTOS_OK);
    CHECK(nostos_bridge_accept(&b,NOSTOS_TO_MESH,w,n,0,0,true)==NOSTOS_FULL);
    for(size_t i=0;i<NOSTOS_BRIDGE_CAPACITY;++i)
        CHECK(nostos_bridge_next(&b,0,true,&job)==NOSTOS_OK && job.wire[1]==NOSTOS_FALL);
    CHECK(nostos_bridge_next(&b,0,true,&job)==NOSTOS_EMPTY);

    CHECK(nostos_bridge_init(&b,2,peers)==NOSTOS_OK);
    m=message(NOSTOS_ENVIRONMENT,10);
    m.payload.environment=(nostos_environment_t){350,500,NOSTOS_VALID,NOSTOS_VALID}; n=encode(m,w);
    CHECK(nostos_bridge_accept(&b,NOSTOS_TO_MESH,w,n,0,0,true)==NOSTOS_OK);
    const uint8_t urgent_types[5]={NOSTOS_FALL,NOSTOS_SOS,NOSTOS_FALL_CLEAR,NOSTOS_SOS_CLEAR,NOSTOS_FALL};
    for(size_t i=0;i<COUNT(urgent_types);++i) {
        m=message(urgent_types[i],(uint16_t)(11U+i)); n=encode(m,w);
        CHECK(nostos_bridge_accept(&b,NOSTOS_TO_MESH,w,n,0,0,true)==NOSTOS_OK);
    }
    for(size_t i=0;i<NOSTOS_BRIDGE_URGENT_BURST;++i)
        CHECK(nostos_bridge_next(&b,0,true,&job)==NOSTOS_OK && job.wire[1]==urgent_types[i] && job.wire[7]==11U+i);
    CHECK(nostos_bridge_next(&b,0,true,&job)==NOSTOS_OK && job.wire[1]==NOSTOS_ENVIRONMENT && job.wire[7]==10);
    CHECK(nostos_bridge_next(&b,0,true,&job)==NOSTOS_OK && job.wire[1]==NOSTOS_FALL && job.wire[7]==15);
    CHECK(nostos_bridge_init(&b,3,peers)==NOSTOS_OK);
    CHECK(nostos_bridge_accept(&b,NOSTOS_TO_UART,w,n,0x101,0,true)==NOSTOS_UNAUTHORIZED);
    CHECK(nostos_bridge_accept(&b,NOSTOS_TO_UART,w,n,0x202,0,true)==NOSTOS_OK);
    CHECK(nostos_bridge_next(&b,1,false,&job)==NOSTOS_OK && job.direction==NOSTOS_TO_UART);
    CHECK(nostos_bridge_next(&b,1,true,&job)==NOSTOS_EMPTY); /* No app mesh re-broadcast. */
    w[1]=0x70; w[9]=0x13; w[10]=0x7e;
    CHECK(nostos_bridge_accept(&b,NOSTOS_TO_UART,w,11,0x202,0,true)==NOSTOS_OK);
    CHECK(nostos_bridge_next(&b,1,true,&job)==NOSTOS_OK && job.length==11 && !memcmp(job.wire,w,11));
    nostos_receiver_t r=receiver();
    CHECK(nostos_receiver_wire(&r,job.wire,job.length,1)==NOSTOS_UNSUPPORTED_TYPE && !r.request_count);
    w[0]=1; CHECK(nostos_bridge_accept(&b,NOSTOS_TO_UART,w,11,0x202,0,true)==NOSTOS_UNSUPPORTED_VERSION);
    puts("Source binding, owned bytes, urgent reserve/FIFO/fairness, expiry, opaque v2 extension PASS");
}
typedef struct {
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t length;
    unsigned writes, output_changes, audio_starts;
    nostos_outputs_t outputs;
    uint8_t audio_type;
} endpoint_capture_t;
static bool capture_uart(void *context, const uint8_t *frame, size_t length)
{
    endpoint_capture_t *c=context;
    CHECK(length<=sizeof(c->frame));
    memcpy(c->frame,frame,length); c->length=length; ++c->writes;
    return true;
}
static void capture_outputs(void *context, nostos_outputs_t outputs)
{
    endpoint_capture_t *c=context;
    c->outputs=outputs; ++c->output_changes;
}
static bool capture_audio_ready(void *context) { (void)context; return true; }
static bool capture_audio_play(void *context, uint8_t type)
{
    endpoint_capture_t *c=context;
    c->audio_type=type; ++c->audio_starts; return true;
}
static nostos_endpoint_t captured_endpoint(uint8_t source, endpoint_capture_t *capture)
{
    nostos_endpoint_io_t io={capture,capture_uart,capture_outputs,capture_audio_ready,capture_audio_play};
    nostos_endpoint_t endpoint;
    CHECK(nostos_endpoint_init(&endpoint,source,1,&io)==NOSTOS_OK);
    return endpoint;
}
static void check_fixture_state(const fixture_t *f, const nostos_receiver_t *r)
{
    const nostos_node_state_t *n=&r->shared_data.nodes[f->source-1];
    CHECK(n->source_id==f->source && n->reachability.seen);
    switch(f->type) {
    case NOSTOS_SPEED:
        CHECK(n->speed.speed_kmh_x10.has_value && n->speed.speed_kmh_x10.quality==NOSTOS_VALID);
        CHECK(n->speed.speed_kmh_x10.value==253 && n->speed.report.sequence==f->sequence);
        break;
    case NOSTOS_ENVIRONMENT:
        CHECK(n->environment.temperature_c_x10.has_value && n->environment.humidity_pct_x10.has_value);
        CHECK(n->environment.temperature_c_x10.value==362 && n->environment.humidity_pct_x10.value==605);
        CHECK(n->environment.temperature_c_x10.quality==NOSTOS_VALID && n->environment.humidity_pct_x10.quality==NOSTOS_VALID);
        CHECK(n->environment.report.sequence==f->sequence);
        break;
    case NOSTOS_HEARTBEAT:
        CHECK(n->health.report.seen && n->health.report.sequence==f->sequence && n->health.status==3);
        CHECK(!n->environment.report.seen && !n->speed.report.seen);
        break;
    case NOSTOS_REAR_SAFE: case NOSTOS_REAR_WARNING: case NOSTOS_REAR_UNKNOWN:
        CHECK(n->rear.state==(f->type==NOSTOS_REAR_SAFE?NOSTOS_REAR_IS_SAFE:
              f->type==NOSTOS_REAR_WARNING?NOSTOS_REAR_IS_WARNING:NOSTOS_REAR_IS_UNKNOWN));
        CHECK(n->rear.quality==(f->type==NOSTOS_REAR_UNKNOWN?NOSTOS_SENSOR_ERROR:NOSTOS_VALID));
        CHECK(n->rear.report.sequence==f->sequence);
        break;
    case NOSTOS_FALL: case NOSTOS_FALL_CLEAR: case NOSTOS_SOS: case NOSTOS_SOS_CLEAR: {
        bool fall=f->type==NOSTOS_FALL || f->type==NOSTOS_FALL_CLEAR;
        bool clear=f->type==NOSTOS_FALL_CLEAR || f->type==NOSTOS_SOS_CLEAR;
        const nostos_incident_state_t *s=fall?&n->fall:&n->sos;
        CHECK(s->phase==(clear?NOSTOS_INCIDENT_CLOSED:NOSTOS_INCIDENT_ACTIVE));
        CHECK(s->incident.session_id==1 && s->incident.incident_id==(fall?11:12));
        break;
    }
    case NOSTOS_ACK:
        CHECK(!n->health.report.seen && !n->environment.report.seen && !n->speed.report.seen);
        CHECK(!r->request_count); /* ACK is not an actuator command. */
        break;
    case NOSTOS_SPEED_DOWN: case NOSTOS_SPEED_UP: case NOSTOS_SAFETY_REMINDER: case NOSTOS_STOP:
        break; /* The endpoint callback below checks the one-shot request. */
    default: CHECK(false); /* A new type needs an observable state assertion. */
    }
}
/* Mock network topology: source2 -- relay1 -- receiver3; no direct2->3 edge.
 * This models TTL/Relay behavior at the SDK boundary. It is NOT a BLE stack,
 * RF simulator, provisioning test, or proof of physical multi-hop delivery. */
static unsigned simulated_delivery(const fixture_t *f, bool relay_enabled, uint8_t ttl, bool duplicate)
{
    nostos_bridge_t tx, rx, middle;
    CHECK(nostos_bridge_init(&tx,2,peers)==NOSTOS_OK);
    CHECK(nostos_bridge_init(&middle,1,peers)==NOSTOS_OK);
    CHECK(nostos_bridge_init(&rx,3,peers)==NOSTOS_OK);
    endpoint_capture_t tx_capture={0}, rx_capture={0};
    nostos_endpoint_t sender=captured_endpoint(2,&tx_capture), destination=captured_endpoint(3,&rx_capture);
    CHECK(nostos_receiver_approve_session(&destination.receiver,2,1,0)==NOSTOS_OK);
    nostos_message_t outgoing;
    CHECK(nostos_message_decode(f->wire,f->length,&outgoing)==NOSTOS_OK);
    if(f->type==NOSTOS_FALL_CLEAR || f->type==NOSTOS_SOS_CLEAR) {
        /* CLEAR must end an actual active event, not merely create a tombstone. */
        nostos_message_t active=outgoing;
        active.type=f->type==NOSTOS_FALL_CLEAR?NOSTOS_FALL:NOSTOS_SOS;
        active.sequence=(uint16_t)(f->sequence-1);
        CHECK(nostos_receiver_apply(&sender.receiver,&active,0)==NOSTOS_OK);
        CHECK(nostos_receiver_apply(&destination.receiver,&active,0)==NOSTOS_OK);
        nostos_endpoint_process(&destination,0);
        CHECK(rx_capture.outputs.led==NOSTOS_LED_RED_BLINK);
    }
    sender.sender.next_sequence=f->sequence;
    outgoing.source_id=0; outgoing.session_id=0; outgoing.sequence=UINT16_MAX;
    if(f->type==NOSTOS_ENVIRONMENT) outgoing.payload.environment.humidity_pct_x10=603;
    CHECK(nostos_endpoint_publish(&sender,&outgoing,0)==NOSTOS_OK);
    CHECK(tx_capture.writes==1 && outgoing.source_id==2 && outgoing.session_id==1 && outgoing.sequence==f->sequence);
    if(f->type!=NOSTOS_ACK) check_fixture_state(f,&sender.receiver);
    uint8_t frame[NOSTOS_UART_FRAME_MAX], wire[64]; size_t frame_n=0;
    nostos_uart_parser_t p={0}; size_t n=feed_frame(&p,tx_capture.frame,tx_capture.length,0,wire);
    CHECK(n==f->length && !memcmp(wire,f->wire,n));
    CHECK(nostos_bridge_accept(&tx,NOSTOS_TO_MESH,wire,n,0,0,true)==NOSTOS_OK);
    nostos_job_t air;
    CHECK(nostos_bridge_next(&tx,1,true,&air)==NOSTOS_OK && air.direction==NOSTOS_TO_MESH);
    CHECK(!memcmp(air.wire,f->wire,f->length));
    CHECK(nostos_bridge_accept(&middle,NOSTOS_TO_UART,air.wire,air.length,0x202,2,true)==NOSTOS_OK);
    nostos_job_t local;
    CHECK(nostos_bridge_next(&middle,2,true,&local)==NOSTOS_OK && local.direction==NOSTOS_TO_UART);
    CHECK(nostos_bridge_next(&middle,2,true,&local)==NOSTOS_EMPTY);
    if(!relay_enabled || ttl<2) {
        CHECK(!destination.receiver.shared_data.nodes[1].reachability.seen ||
              f->type==NOSTOS_FALL_CLEAR || f->type==NOSTOS_SOS_CLEAR);
        if(f->type==NOSTOS_FALL_CLEAR || f->type==NOSTOS_SOS_CLEAR)
            CHECK(nostos_receiver_outputs(&destination.receiver,5).led==NOSTOS_LED_RED_BLINK);
        CHECK(rx_capture.audio_starts==0 && rx_capture.writes==0);
        return 0;
    }
    uint8_t received_ttl=(uint8_t)(ttl-1); CHECK(received_ttl>=1);
    unsigned applied=0, first_output_changes=0, first_audio_starts=0;
    for(unsigned repeat=0;repeat<(duplicate?2U:1U);++repeat) {
        /* Source is original0x202, NOT relay0x101. Bytes and app sequence unchanged. */
        CHECK(nostos_bridge_accept(&rx,NOSTOS_TO_UART,air.wire,air.length,0x202,3,true)==NOSTOS_OK);
        nostos_job_t delivered; CHECK(nostos_bridge_next(&rx,4,true,&delivered)==NOSTOS_OK);
        CHECK(delivered.direction==NOSTOS_TO_UART && !memcmp(delivered.wire,f->wire,f->length));
        CHECK(nostos_uart_encode(delivered.wire,delivered.length,frame,sizeof(frame),&frame_n)==NOSTOS_OK);
        nostos_result_t result=NOSTOS_EMPTY;
        for(size_t i=0;i<frame_n;++i) {
            result=nostos_endpoint_uart_byte(&destination,frame[i],5);
            if(i+1<frame_n) CHECK(result==NOSTOS_EMPTY);
        }
        CHECK(result==(repeat?NOSTOS_DUPLICATE:NOSTOS_OK)); if(result==NOSTOS_OK) ++applied;
        check_fixture_state(f,&destination.receiver);
        nostos_endpoint_process(&destination,5);
        if(!repeat) { first_output_changes=rx_capture.output_changes; first_audio_starts=rx_capture.audio_starts; }
        else CHECK(rx_capture.output_changes==first_output_changes && rx_capture.audio_starts==first_audio_starts);
    }
    nostos_outputs_t o=rx_capture.outputs;
    if(f->type==NOSTOS_FALL || f->type==NOSTOS_SOS) CHECK(o.led==NOSTOS_LED_RED_BLINK && o.buzzer==NOSTOS_BUZZER_EMERGENCY);
    if(f->type==NOSTOS_REAR_WARNING) CHECK(o.led==NOSTOS_LED_YELLOW_BLINK && o.buzzer==NOSTOS_BUZZER_REAR);
    if(f->type==NOSTOS_REAR_SAFE) CHECK(o.led==NOSTOS_LED_GREEN && o.buzzer==NOSTOS_BUZZER_OFF);
    if(f->type==NOSTOS_FALL_CLEAR || f->type==NOSTOS_SOS_CLEAR || f->type==NOSTOS_REAR_UNKNOWN)
        CHECK(o.led==NOSTOS_LED_OFF && o.buzzer==NOSTOS_BUZZER_OFF);
    if(f->type>=NOSTOS_SPEED_DOWN && f->type<=NOSTOS_STOP) {
        CHECK(rx_capture.audio_starts==1 && rx_capture.audio_type==f->type);
        CHECK(!destination.receiver.request_count);
    }
    CHECK(rx_capture.writes==0); /* Receiving never publishes the message again. */
    CHECK(!destination.receiver.shared_data.nodes[0].reachability.seen &&
          !destination.receiver.shared_data.nodes[2].reachability.seen);
    CHECK(nostos_bridge_next(&rx,5,true,&local)==NOSTOS_EMPTY);
    return applied;
}
static void relay(void)
{
    for(size_t i=0;i<COUNT(fixtures);++i) {
        CHECK(simulated_delivery(&fixtures[i],false,2,false)==0);
        CHECK(simulated_delivery(&fixtures[i],true,0,false)==0);
        CHECK(simulated_delivery(&fixtures[i],true,1,false)==0);
        CHECK(simulated_delivery(&fixtures[i],true,2,true)==1);
        CHECK(simulated_delivery(&fixtures[i],false,2,false)==0);
        printf("MOCK %-16s endpoint-TX->UART->ESP32->relay->ESP32->UART->endpoint-RX/state/output OFF=0 ON=1 OFF=0 TTL0/1=0 duplicate=once PASS\n",fixtures[i].name);
    }
    puts("SIMULATED_RELAY=PASS; REAL_BLE_RF=NOT_TESTED");
}
static uint32_t random_word(uint32_t *s) { *s=*s*UINT32_C(1664525)+UINT32_C(1013904223); return *s; }
static void fuzz(void)
{
    uint32_t seed=0x12345678; nostos_uart_parser_t parser={0}; uint8_t bytes[80],out[64]; size_t length=0;
    for(unsigned i=0;i<100000;++i) {
        size_t n=random_word(&seed)%81;
        for(size_t j=0;j<n;++j) bytes[j]=(uint8_t)(random_word(&seed)>>24);
        nostos_message_t m, before; memset(&m,0x5a,sizeof(m)); memcpy(&before,&m,sizeof(m));
        nostos_result_t result=nostos_message_decode(bytes,n,&m);
        if(result!=NOSTOS_OK) CHECK(!memcmp(&m,&before,sizeof(m)));
        for(size_t j=0;j<n;++j) (void)nostos_uart_feed(&parser,bytes[j],i,out,&length);
    }
    uint8_t frame[NOSTOS_UART_FRAME_MAX]; size_t n=0;
    CHECK(nostos_uart_encode(fixtures[0].wire,fixtures[0].length,frame,sizeof(frame),&n)==NOSTOS_OK);
    /* Arbitrary stream can leave an incomplete frame; first delimiter resyncs. */
    (void)nostos_uart_feed(&parser,NOSTOS_UART_FLAG,100001,out,&length);
    CHECK(feed_frame(&parser,frame,n,100001,out)==fixtures[0].length);
    puts("100000 deterministic malformed inputs and stream recovery PASS");
}
typedef struct { char lines[4][NOSTOS_HEXDUMP_LINE_CAPACITY]; size_t count; } debug_capture_t;
static bool capture_debug_line(void *context, const char *line)
{
    debug_capture_t *capture=context;
    CHECK(capture->count<COUNT(capture->lines));
    CHECK(strlen(line)<sizeof(capture->lines[0]));
    strcpy(capture->lines[capture->count++],line);
    return true;
}
static bool reject_debug_line(void *context, const char *line)
{
    (void)context; (void)line; return false;
}
static void debug(void)
{
    nostos_message_t environment=message(NOSTOS_ENVIRONMENT,7);
    environment.payload.environment=(nostos_environment_t){362,603,NOSTOS_VALID,NOSTOS_VALID};
    uint8_t wire[NOSTOS_WIRE_MAX]; size_t length=encode(environment,wire);
    debug_capture_t capture={0};
    CHECK(capture.count==0); /* Encode has no implicit logging side effect. */
    CHECK(nostos_debug_hexdump(wire,length,capture_debug_line,&capture)==NOSTOS_OK);
    CHECK(capture.count==1);
    CHECK(!strncmp(capture.lines[0],"0000  02 41 02 01 00 00 00 07 00 89 79 ",39));
    CHECK(strstr(capture.lines[0],".A........y")!=NULL);
    memset(wire,0x7e,sizeof(wire)); capture=(debug_capture_t){0};
    CHECK(nostos_debug_hexdump(wire,sizeof(wire),capture_debug_line,&capture)==NOSTOS_OK);
    CHECK(capture.count==4 && !strncmp(capture.lines[3],"0030  ",6));
    CHECK(nostos_debug_hexdump(wire,1,reject_debug_line,NULL)==NOSTOS_IO_ERROR);
    CHECK(nostos_debug_hexdump(NULL,1,capture_debug_line,&capture)==NOSTOS_BAD_ARGUMENT);
    CHECK(nostos_debug_hexdump(wire,0,capture_debug_line,&capture)==NOSTOS_BAD_LENGTH);
    CHECK(nostos_debug_hexdump(wire,NOSTOS_WIRE_MAX+1U,capture_debug_line,&capture)==NOSTOS_TOO_LARGE);
    CHECK(nostos_debug_hexdump(wire,1,NULL,&capture)==NOSTOS_BAD_ARGUMENT);
    puts("Opt-in bounded hexdump, no implicit logging, callback abort PASS");
}
int main(int argc, char **argv)
{
    if(argc!=2) return 2;
    if(!strcmp(argv[1],"codec")) codec();
    else if(!strcmp(argv[1],"uart")) uart();
    else if(!strcmp(argv[1],"state")) state();
    else if(!strcmp(argv[1],"bridge")) bridge();
    else if(!strcmp(argv[1],"relay")) relay();
    else if(!strcmp(argv[1],"fuzz")) fuzz();
    else if(!strcmp(argv[1],"debug")) debug();
    else return 2;
    return 0;
}
