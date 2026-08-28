#include "gps_receiver.h"
#include <string.h>

void gps_receiver_set_source(gps_receiver_t *r, uint16_t source) {
    if (!r || source == 0 || source > 0x7fff) return;
    *r = (gps_receiver_t){.source = source};
}
gps_rx_result_t gps_receiver_accept(gps_receiver_t *r, uint16_t source,
    const uint8_t *data, size_t size, uint64_t now_ms) {
    if (!r) return GPS_RX_INVALID;
    if (!r->source) return GPS_RX_SOURCE_UNSET;
    if (source != r->source) return GPS_RX_OTHER_SOURCE;
    gps_packet_t p;
    if (gps_decode(data, size, &p) != GPS_OK) return GPS_RX_INVALID;
    if (r->has_sample) {
        if (now_ms < r->received_ms) return GPS_RX_INVALID;
        if (p.session_id == r->previous_session ||
            (p.session_id == r->latest.session_id && p.sequence <= r->latest.sequence))
            return GPS_RX_OLD;
        if (p.session_id != r->latest.session_id) r->previous_session = r->latest.session_id;
    }
    r->latest = p; r->received_ms = now_ms; r->has_sample = true; r->stale = false;
    return GPS_RX_ACCEPTED;
}
bool gps_receiver_poll_stale(gps_receiver_t *r, uint64_t now_ms) {
    if (!r || !r->has_sample || r->stale || now_ms < r->received_ms ||
        now_ms - r->received_ms < 10000) return false;
    r->stale = true;
    return true;
}
bool gps_parse_source_command(const char *line, uint16_t *source) {
    static const char prefix[] = "gps-source 0x";
    if (!line || !source || strlen(line) != sizeof(prefix) - 1 + 4 ||
        strncmp(line, prefix, sizeof(prefix) - 1) != 0) return false;
    uint16_t value = 0;
    for (size_t i = sizeof(prefix) - 1; line[i]; ++i) {
        unsigned digit;
        if (line[i] >= '0' && line[i] <= '9') digit = (unsigned)(line[i] - '0');
        else if (line[i] >= 'a' && line[i] <= 'f') digit = (unsigned)(line[i] - 'a' + 10);
        else if (line[i] >= 'A' && line[i] <= 'F') digit = (unsigned)(line[i] - 'A' + 10);
        else return false;
        value = (uint16_t)((value << 4) | digit);
    }
    if (!value || value > 0x7fff) return false;
    *source = value;
    return true;
}
