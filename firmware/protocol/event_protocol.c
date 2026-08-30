#include "event_protocol.h"

bool event_id_valid(uint8_t id)
{
    switch (id) {
    case MSG_SPEED_DOWN_REQUEST:
    case MSG_SPEED_UP_REQUEST:
    case MSG_STOP_REQUEST:
    case MSG_FALL_DETECTED:
        return true;
    default:
        return false;
    }
}

bool event_encode(uint8_t id, uint8_t *wire, size_t length)
{
    if (wire == NULL || length != EVENT_WIRE_SIZE || !event_id_valid(id)) {
        return false;
    }
    wire[0] = EVENT_WIRE_VERSION;
    wire[1] = id;
    return true;
}

bool event_decode(const uint8_t *wire, size_t length, uint8_t *id)
{
    if (wire == NULL || id == NULL || length != EVENT_WIRE_SIZE ||
        wire[0] != EVENT_WIRE_VERSION || !event_id_valid(wire[1])) {
        return false;
    }
    *id = wire[1];
    return true;
}
