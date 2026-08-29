#include "speed_sensor_local.h"

enum {
    LOCAL_MAGIC_0 = 0xA5U,
    LOCAL_MAGIC_1 = 0x5AU,
    LOCAL_VERSION = 1U,
    LOCAL_TYPE_SPEED = 1U,
    LOCAL_FLAG_VALID = 1U << 0
};

bool speed_sensor_local_encode(const xoss_speed_sample_t *sample,
                               uint8_t *payload,
                               size_t capacity)
{
    if (sample == NULL || payload == NULL ||
        capacity < SPEED_SENSOR_LOCAL_PAYLOAD_SIZE ||
        (!sample->valid && sample->kmh_x10 != 0U)) {
        return false;
    }

    payload[0] = LOCAL_MAGIC_0;
    payload[1] = LOCAL_MAGIC_1;
    payload[2] = LOCAL_VERSION;
    payload[3] = LOCAL_TYPE_SPEED;
    payload[4] = sample->valid ? LOCAL_FLAG_VALID : 0U;
    payload[5] = (uint8_t)sample->kmh_x10;
    payload[6] = (uint8_t)(sample->kmh_x10 >> 8);
    payload[7] = 0U;
    payload[8] = 0U;
    return true;
}

bool speed_sensor_local_decode(const uint8_t *payload,
                               size_t length,
                               xoss_speed_sample_t *sample)
{
    if (payload == NULL || sample == NULL ||
        length != SPEED_SENSOR_LOCAL_PAYLOAD_SIZE ||
        payload[0] != LOCAL_MAGIC_0 || payload[1] != LOCAL_MAGIC_1 ||
        payload[2] != LOCAL_VERSION || payload[3] != LOCAL_TYPE_SPEED ||
        (payload[4] & (uint8_t)~LOCAL_FLAG_VALID) != 0U ||
        payload[7] != 0U || payload[8] != 0U) {
        return false;
    }

    const uint16_t kmh_x10 = (uint16_t)((uint16_t)payload[5] |
                                        ((uint16_t)payload[6] << 8));
    const bool valid = (payload[4] & LOCAL_FLAG_VALID) != 0U;
    if (!valid && kmh_x10 != 0U) {
        return false;
    }

    sample->valid = valid;
    sample->kmh_x10 = kmh_x10;
    return true;
}
