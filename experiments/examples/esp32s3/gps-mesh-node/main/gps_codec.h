#ifndef GPS_CODEC_H
#define GPS_CODEC_H
#include <stddef.h>
#include <stdint.h>

#define GPS_PACKET_SIZE 24U
typedef struct {
    uint8_t flags;
    uint16_t accuracy_dm;
    uint32_t session_id, sequence, measured_at;
    int32_t latitude_e7, longitude_e7;
} gps_packet_t;
typedef enum { GPS_OK, GPS_INVALID_LENGTH, GPS_INVALID_FIELDS } gps_result_t;
gps_result_t gps_decode(const uint8_t *bytes, size_t size, gps_packet_t *out);
gps_result_t gps_encode(const gps_packet_t *packet, uint8_t *bytes, size_t size);
#endif
