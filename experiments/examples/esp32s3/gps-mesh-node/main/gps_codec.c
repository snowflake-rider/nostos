#include "gps_codec.h"
#include <stdbool.h>
#include <limits.h>

static uint32_t get32(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
static int32_t signed32(uint32_t u) {
    /* Avoid implementation-defined unsigned-to-signed overflow. */
    return u <= INT32_MAX ? (int32_t)u : -1 - (int32_t)(UINT32_MAX - u);
}
static void put32(uint8_t *b, uint32_t u) {
    for (unsigned i = 0; i < 4; ++i) b[i] = (uint8_t)(u >> (8 * i));
}
static bool valid(const gps_packet_t *p) {
    return p && p->flags <= 1 && p->accuracy_dm <= 500 &&
        p->session_id && p->sequence && p->measured_at &&
        p->latitude_e7 >= -900000000 && p->latitude_e7 <= 900000000 &&
        p->longitude_e7 >= -1800000000 && p->longitude_e7 <= 1800000000;
}
gps_result_t gps_decode(const uint8_t *b, size_t size, gps_packet_t *out) {
    if (size != GPS_PACKET_SIZE) return GPS_INVALID_LENGTH;
    if (!b || !out || b[0] != 1) return GPS_INVALID_FIELDS;
    gps_packet_t p = {
        .flags = b[1], .accuracy_dm = (uint16_t)(b[2] | ((uint16_t)b[3] << 8)),
        .session_id = get32(b + 4), .sequence = get32(b + 8), .measured_at = get32(b + 12),
        .latitude_e7 = signed32(get32(b + 16)), .longitude_e7 = signed32(get32(b + 20)),
    };
    if (!valid(&p)) return GPS_INVALID_FIELDS;
    *out = p; /* 실패한 입력은 마지막 정상 값을 덮어쓰지 않는다. */
    return GPS_OK;
}
gps_result_t gps_encode(const gps_packet_t *p, uint8_t *b, size_t size) {
    if (size != GPS_PACKET_SIZE) return GPS_INVALID_LENGTH;
    if (!b || !valid(p)) return GPS_INVALID_FIELDS;
    b[0] = 1; b[1] = p->flags;
    b[2] = (uint8_t)p->accuracy_dm; b[3] = (uint8_t)(p->accuracy_dm >> 8);
    put32(b + 4, p->session_id); put32(b + 8, p->sequence); put32(b + 12, p->measured_at);
    put32(b + 16, (uint32_t)p->latitude_e7); put32(b + 20, (uint32_t)p->longitude_e7);
    return GPS_OK;
}
