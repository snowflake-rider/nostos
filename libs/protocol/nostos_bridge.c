#include "nostos_bridge.h"
#include <string.h>
static bool urgent_type(uint8_t type)
{
    return type==NOSTOS_FALL || type==NOSTOS_SOS ||
        type==NOSTOS_FALL_CLEAR || type==NOSTOS_SOS_CLEAR;
}
nostos_result_t nostos_bridge_init(nostos_bridge_t *b, uint8_t local, const nostos_peer_t peers[NOSTOS_NODE_COUNT])
{
    if (!b || !peers || local<1 || local>NOSTOS_NODE_COUNT) return NOSTOS_BAD_ARGUMENT;
    unsigned mask=0;
    for (size_t i=0; i<NOSTOS_NODE_COUNT; ++i) {
        if (peers[i].source_id<1 || peers[i].source_id>NOSTOS_NODE_COUNT ||
            peers[i].role<1 || peers[i].role>3 || !peers[i].mesh_address || peers[i].mesh_address>0x7fff)
            return NOSTOS_BAD_VALUE;
        unsigned bit=1U<<peers[i].source_id;
        if (mask&bit) return NOSTOS_BAD_VALUE;
        mask|=bit;
        for (size_t j=0; j<i; ++j)
            if (peers[i].mesh_address==peers[j].mesh_address || peers[i].role==peers[j].role) return NOSTOS_BAD_VALUE;
    }
    *b=(nostos_bridge_t){.local_source=local,.free_count=NOSTOS_BRIDGE_CAPACITY};
    memcpy(b->peers,peers,sizeof(b->peers));
    for (size_t i=0; i<NOSTOS_BRIDGE_CAPACITY; ++i)
        b->free_slots[i]=(uint8_t)(NOSTOS_BRIDGE_CAPACITY-1U-i);
    return NOSTOS_OK;
}
nostos_result_t nostos_bridge_accept(nostos_bridge_t *b, nostos_direction_t d,
    const uint8_t *w, size_t n, uint16_t source, uint32_t now, bool ready)
{
    if (!b || (d!=NOSTOS_TO_MESH && d!=NOSTOS_TO_UART)) return NOSTOS_BAD_ARGUMENT;
    nostos_message_t m;
    nostos_result_t r=nostos_message_decode(w,n,&m);
    /* Same-v2 unknown types may be carried opaquely, but never interpreted. */
    if (r!=NOSTOS_OK && r!=NOSTOS_UNSUPPORTED_TYPE) return r;
    uint8_t claimed=w[2];
    if (d==NOSTOS_TO_MESH) {
        if (claimed!=b->local_source) return NOSTOS_UNAUTHORIZED;
        if (!ready) return NOSTOS_NOT_READY;
    } else {
        if (claimed==b->local_source) return NOSTOS_UNAUTHORIZED;
        bool match=false;
        for (size_t i=0; i<NOSTOS_NODE_COUNT; ++i)
            if (b->peers[i].source_id==claimed && b->peers[i].mesh_address==source) match=true;
        if (!match) return NOSTOS_UNAUTHORIZED;
    }
    bool urgent=r==NOSTOS_OK && urgent_type(m.type);
    if (b->count==NOSTOS_BRIDGE_CAPACITY ||
        (!urgent && b->normal_count==NOSTOS_BRIDGE_NORMAL_CAPACITY)) return NOSTOS_FULL;
    uint8_t slot=b->free_slots[--b->free_count];
    nostos_job_t *job=&b->jobs[slot];
    memcpy(job->wire,w,n); job->length=n; job->received_ms=now; job->direction=d;
    if (urgent) {
        size_t tail=(b->urgent_head+b->urgent_count)%NOSTOS_BRIDGE_CAPACITY;
        b->urgent_order[tail]=slot; ++b->urgent_count;
    } else {
        size_t tail=(b->normal_head+b->normal_count)%NOSTOS_BRIDGE_NORMAL_CAPACITY;
        b->normal_order[tail]=slot; ++b->normal_count;
    }
    ++b->count;
    return NOSTOS_OK;
}
nostos_result_t nostos_bridge_next(nostos_bridge_t *b, uint32_t now, bool ready, nostos_job_t *out)
{
    if (!b || !out) return NOSTOS_BAD_ARGUMENT;
    if (!b->count) return NOSTOS_EMPTY;
    uint8_t slot;
    bool take_urgent=b->urgent_count &&
        (!b->normal_count || b->urgent_streak<NOSTOS_BRIDGE_URGENT_BURST);
    if (take_urgent) {
        slot=b->urgent_order[b->urgent_head];
        b->urgent_head=(b->urgent_head+1U)%NOSTOS_BRIDGE_CAPACITY;
        --b->urgent_count;
        ++b->urgent_streak;
    } else {
        slot=b->normal_order[b->normal_head];
        b->normal_head=(b->normal_head+1U)%NOSTOS_BRIDGE_NORMAL_CAPACITY;
        --b->normal_count;
        b->urgent_streak=0;
    }
    *out=b->jobs[slot];
    b->free_slots[b->free_count++]=slot;
    --b->count;
    if (!b->count) b->urgent_streak=0;
    if ((uint32_t)(now-out->received_ms)>NOSTOS_BRIDGE_MAX_AGE_MS) return NOSTOS_EXPIRED;
    if (out->direction==NOSTOS_TO_MESH && !ready) return NOSTOS_NOT_READY;
    return NOSTOS_OK;
}
