#include "speed_sensor_local.h"
#include "xoss_csc.h"

#include <stdio.h>

static void print_bytes(const uint8_t *bytes, size_t length)
{
    for (size_t i = 0U; i < length; ++i) {
        printf("%s%02X", i == 0U ? "" : " ", bytes[i]);
    }
    putchar('\n');
}

int main(void)
{
    /* 16 -> 17 wheel revolutions, 0x0400 -> 0x0800 ticks. */
    static const uint8_t notifications[][7] = {
        {0x01U, 0x10U, 0x00U, 0x00U, 0x00U, 0x00U, 0x04U},
        {0x01U, 0x11U, 0x00U, 0x00U, 0x00U, 0x00U, 0x08U}
    };
    xoss_csc_speed_state_t state = {0};

    for (size_t i = 0U; i < 2U; ++i) {
        xoss_csc_measurement_t measurement;
        xoss_speed_sample_t sample;
        xoss_csc_result_t result = xoss_csc_decode(
            notifications[i], sizeof(notifications[i]), &measurement);
        if (result == XOSS_CSC_OK) {
            result = xoss_csc_speed_update(&state, &measurement, 2105U, &sample);
        }

        printf("notification %zu: %s", i + 1U, xoss_csc_result_name(result));
        if (result != XOSS_CSC_OK) {
            putchar('\n');
            continue;
        }

        printf(", speed=%u.%u km/h\n",
               (unsigned)(sample.kmh_x10 / 10U),
               (unsigned)(sample.kmh_x10 % 10U));

        uint8_t local_payload[SPEED_SENSOR_LOCAL_PAYLOAD_SIZE];
        if (!speed_sensor_local_encode(&sample, local_payload,
                                       sizeof(local_payload))) {
            fputs("local payload encode failed\n", stderr);
            return 1;
        }
        fputs("ESP32 -> STM32 local body: ", stdout);
        print_bytes(local_payload, sizeof(local_payload));
    }

    return 0;
}
