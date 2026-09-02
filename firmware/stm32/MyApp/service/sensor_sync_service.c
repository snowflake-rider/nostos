#include "sensor_sync_service.h"

#include "sensor_store.h"

#include "message_protocol_service.h"

#define SENSOR_SYNC_RETRY_MS 200U
#define SENSOR_SYNC_PUBLISH_MS 2000U

static uint32_t ride_revision;
static uint32_t environment_revision;
static uint32_t ride_next_publish_ms;
static uint32_t environment_next_publish_ms;
static bool ride_retry_pending;
static bool environment_retry_pending;

static bool due(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

void sensor_sync_service_init(void)
{
    ride_revision = 0U;
    environment_revision = 0U;
    ride_next_publish_ms = 0U;
    environment_next_publish_ms = 0U;
    ride_retry_pending = false;
    environment_retry_pending = false;
}

void sensor_sync_service_process(void)
{
    if (!message_protocol_service_is_ready()) return;

    uint32_t now = HAL_GetTick();
    sensor_snapshot_t snapshot;
    if (!sensor_store_snapshot(now, &snapshot)) return;

    bool ride_due = snapshot.ride.revision != 0U &&
        (!ride_retry_pending || due(now, ride_next_publish_ms)) &&
        (ride_retry_pending || snapshot.ride.revision != ride_revision ||
         due(now, ride_next_publish_ms));
    if (ride_due) {
        bool valid = snapshot.ride.quality == SENSOR_QUALITY_VALID;
        message_protocol_result_t result = message_protocol_service_publish_ride(
            valid,
            valid ? snapshot.ride.kmh_x10 : 0U,
            valid ? snapshot.ride.distance_mm / 1000U : 0U);
        if (result != MESSAGE_PROTOCOL_OK) {
            ride_next_publish_ms = now + SENSOR_SYNC_RETRY_MS;
            ride_retry_pending = true;
            return;
        }
        ride_revision = snapshot.ride.revision;
        ride_next_publish_ms = now + SENSOR_SYNC_PUBLISH_MS;
        ride_retry_pending = false;
    }

    bool environment_due = snapshot.environment.revision != 0U &&
        (!environment_retry_pending || due(now, environment_next_publish_ms)) &&
        (environment_retry_pending ||
         snapshot.environment.revision != environment_revision ||
         due(now, environment_next_publish_ms));
    if (environment_due) {
        bool valid = snapshot.environment.quality == SENSOR_QUALITY_VALID;
        message_protocol_result_t result =
            message_protocol_service_publish_environment(
                valid,
                valid ? snapshot.environment.temperature_c_x10 : 0,
                valid ? snapshot.environment.humidity_pct_x10 : 0U);
        if (result != MESSAGE_PROTOCOL_OK) {
            environment_next_publish_ms = now + SENSOR_SYNC_RETRY_MS;
            environment_retry_pending = true;
            return;
        }
        environment_revision = snapshot.environment.revision;
        environment_next_publish_ms = now + SENSOR_SYNC_PUBLISH_MS;
        environment_retry_pending = false;
    }
}
