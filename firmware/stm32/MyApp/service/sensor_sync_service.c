#include "sensor_sync_service.h"

#include "sensor_store.h"

#if NOSTOS_PROTOCOL_V2
#include "message_protocol_service.h"

#define SENSOR_SYNC_RETRY_MS 200U

static uint32_t ride_revision;
static uint32_t environment_revision;
static sensor_quality_t ride_quality;
static sensor_quality_t environment_quality;
static uint32_t next_attempt_ms;

static bool due(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static uint8_t environment_quality_to_wire(sensor_quality_t quality)
{
    if (quality == SENSOR_QUALITY_VALID) return SENSOR_LINK_QUALITY_VALID;
    if (quality == SENSOR_QUALITY_ERROR) return SENSOR_LINK_QUALITY_SENSOR_ERROR;
    return SENSOR_LINK_QUALITY_UNMEASURED;
}
#endif

void sensor_sync_service_init(void)
{
#if NOSTOS_PROTOCOL_V2
    ride_revision = 0U;
    environment_revision = 0U;
    ride_quality = SENSOR_QUALITY_UNMEASURED;
    environment_quality = SENSOR_QUALITY_UNMEASURED;
    next_attempt_ms = 0U;
#endif
}

void sensor_sync_service_process(void)
{
#if NOSTOS_PROTOCOL_V2
    if (!message_protocol_service_is_ready()) return;

    uint32_t now = HAL_GetTick();
    if (!due(now, next_attempt_ms)) return;

    sensor_snapshot_t snapshot;
    if (!sensor_store_snapshot(now, &snapshot)) return;

    bool ride_changed = snapshot.ride.revision != 0U &&
        (snapshot.ride.revision != ride_revision ||
         snapshot.ride.quality != ride_quality);
    if (ride_changed) {
        bool valid = snapshot.ride.quality == SENSOR_QUALITY_VALID;
        message_protocol_result_t result = message_protocol_service_publish_ride(
            valid,
            valid ? snapshot.ride.kmh_x10 : 0U,
            valid ? snapshot.ride.distance_mm : 0U);
        if (result != MESSAGE_PROTOCOL_OK) {
            next_attempt_ms = now + SENSOR_SYNC_RETRY_MS;
            return;
        }
        ride_revision = snapshot.ride.revision;
        ride_quality = snapshot.ride.quality;
    }

    bool environment_changed = snapshot.environment.revision != 0U &&
        (snapshot.environment.revision != environment_revision ||
         snapshot.environment.quality != environment_quality);
    if (environment_changed) {
        message_protocol_result_t result =
            message_protocol_service_publish_environment(
                snapshot.environment.temperature_c_x10,
                snapshot.environment.humidity_pct_x10,
                environment_quality_to_wire(snapshot.environment.quality));
        if (result != MESSAGE_PROTOCOL_OK) {
            next_attempt_ms = now + SENSOR_SYNC_RETRY_MS;
            return;
        }
        environment_revision = snapshot.environment.revision;
        environment_quality = snapshot.environment.quality;
    }

    next_attempt_ms = now;
#endif
}
