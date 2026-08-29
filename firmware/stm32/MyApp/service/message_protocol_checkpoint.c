#include "message_protocol_service.h"

bool message_protocol_checkpoint_valid(const message_protocol_checkpoint_t *checkpoint)
{
    if(!checkpoint || checkpoint->source_id<1 || checkpoint->source_id>NOSTOS_NODE_COUNT ||
        !checkpoint->session_id || checkpoint->next_sequence>(uint32_t)UINT16_MAX+1U ||
        !checkpoint->next_incident || checkpoint->next_incident>(uint32_t)UINT16_MAX+1U) return false;
    for(size_t i=0;i<NOSTOS_NODE_COUNT;++i) {
        const nostos_rx_window_t *window=&checkpoint->windows[i];
        if(!window->approved || !window->session_id ||
            (window->started && (!window->seen || window->highest<window->floor)) ||
            (!window->started && (window->seen || window->highest))) return false;
    }
    if(checkpoint->windows[checkpoint->source_id-1U].session_id!=checkpoint->session_id) return false;
    for(size_t i=0;i<NOSTOS_INCIDENT_CAPACITY;++i) {
        const nostos_incident_record_t *incident=&checkpoint->incidents[i];
        if(!incident->used) continue;
        if(incident->source_id<1 || incident->source_id>NOSTOS_NODE_COUNT ||
            (incident->kind!=NOSTOS_FALL && incident->kind!=NOSTOS_SOS) ||
            !incident->ref.session_id || !incident->ref.incident_id) return false;
        for(size_t j=0;j<i;++j) {
            const nostos_incident_record_t *other=&checkpoint->incidents[j];
            if(other->used && other->source_id==incident->source_id && other->kind==incident->kind &&
                other->ref.session_id==incident->ref.session_id &&
                other->ref.incident_id==incident->ref.incident_id) return false;
        }
    }
    return true;
}
