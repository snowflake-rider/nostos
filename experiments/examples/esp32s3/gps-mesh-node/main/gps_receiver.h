#ifndef GPS_RECEIVER_H
#define GPS_RECEIVER_H
#include "gps_codec.h"
#include <stdbool.h>

typedef enum {
    GPS_RX_ACCEPTED, GPS_RX_SOURCE_UNSET, GPS_RX_OTHER_SOURCE,
    GPS_RX_INVALID, GPS_RX_OLD
} gps_rx_result_t;
typedef struct {
    uint16_t source;
    bool has_sample, stale;
    uint32_t previous_session;
    uint64_t received_ms;
    gps_packet_t latest;
} gps_receiver_t;
void gps_receiver_set_source(gps_receiver_t *receiver, uint16_t source);
gps_rx_result_t gps_receiver_accept(gps_receiver_t *receiver, uint16_t source,
    const uint8_t *data, size_t size, uint64_t now_ms);
bool gps_receiver_poll_stale(gps_receiver_t *receiver, uint64_t now_ms);
bool gps_parse_source_command(const char *line, uint16_t *source);
#endif
