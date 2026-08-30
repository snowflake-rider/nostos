#include "sensor_view_service.h"

#include <stddef.h>

static const nostos_network_state_t *bound_network;
static uint8_t bound_local_source_id;

static bool timestamp_after(uint32_t candidate, uint32_t selected)
{
    return (int32_t)(candidate - selected) > 0;
}

static bool report_is_fresh(
    const nostos_report_t *report,
    uint32_t now_ms)
{
    return report->seen &&
        (uint32_t)(now_ms - report->received_ms) < NOSTOS_FRESH_MS;
}

void sensor_view_service_init(void)
{
    bound_network = NULL;
    bound_local_source_id = 0U;
}

void sensor_view_service_bind_network(
    const nostos_network_state_t *network,
    uint8_t local_source_id)
{
    bound_network = network;
    bound_local_source_id =
        local_source_id >= 1U && local_source_id <= NOSTOS_NODE_COUNT
            ? local_source_id
            : 0U;
}

bool sensor_view_service_snapshot(
    uint32_t now_ms,
    sensor_view_snapshot_t *snapshot)
{
    if (snapshot == NULL ||
        !sensor_store_snapshot(now_ms, &snapshot->sensors)) {
        return false;
    }

    snapshot->ride_source_id =
        snapshot->sensors.ride.quality == SENSOR_QUALITY_VALID
            ? bound_local_source_id
            : 0U;
    snapshot->environment_source_id =
        snapshot->sensors.environment.quality == SENSOR_QUALITY_VALID
            ? bound_local_source_id
            : 0U;

    if (bound_network == NULL) {
        return true;
    }

    if (snapshot->sensors.ride.quality != SENSOR_QUALITY_VALID) {
        bool selected = false;
        uint32_t selected_ms = 0U;
        for (size_t index = 0U; index < NOSTOS_NODE_COUNT; ++index) {
            const nostos_node_state_t *node = &bound_network->nodes[index];
            const nostos_ride_state_t *ride = &node->ride;
            if (!report_is_fresh(&ride->report, now_ms) ||
                ride->speed_kmh_x10.quality != NOSTOS_VALID ||
                !ride->speed_kmh_x10.has_value ||
                ride->distance_mm.quality != NOSTOS_VALID ||
                !ride->distance_mm.has_value ||
                (selected &&
                 !timestamp_after(ride->report.received_ms, selected_ms))) {
                continue;
            }
            selected = true;
            selected_ms = ride->report.received_ms;
            snapshot->sensors.ride = (sensor_ride_sample_t){
                .quality = SENSOR_QUALITY_VALID,
                .kmh_x10 = ride->speed_kmh_x10.value,
                .distance_mm = ride->distance_mm.value,
                .updated_ms = ride->report.received_ms,
                .revision = 0U,
            };
            snapshot->ride_source_id = node->source_id;
        }
    }

    if (snapshot->sensors.environment.quality != SENSOR_QUALITY_VALID) {
        bool selected = false;
        uint32_t selected_ms = 0U;
        for (size_t index = 0U; index < NOSTOS_NODE_COUNT; ++index) {
            const nostos_node_state_t *node = &bound_network->nodes[index];
            const nostos_environment_state_t *environment =
                &node->environment;
            if (!report_is_fresh(&environment->report, now_ms) ||
                environment->temperature_c_x10.quality != NOSTOS_VALID ||
                !environment->temperature_c_x10.has_value ||
                environment->humidity_pct_x10.quality != NOSTOS_VALID ||
                !environment->humidity_pct_x10.has_value ||
                (selected &&
                 !timestamp_after(environment->report.received_ms,
                     selected_ms))) {
                continue;
            }
            selected = true;
            selected_ms = environment->report.received_ms;
            snapshot->sensors.environment = (sensor_environment_sample_t){
                .quality = SENSOR_QUALITY_VALID,
                .temperature_c_x10 =
                    environment->temperature_c_x10.value,
                .humidity_pct_x10 = environment->humidity_pct_x10.value,
                .updated_ms = environment->report.received_ms,
                .revision = 0U,
            };
            snapshot->environment_source_id = node->source_id;
        }
    }

    return true;
}
