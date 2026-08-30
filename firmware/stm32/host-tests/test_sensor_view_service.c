#include "sensor_store.h"
#include "sensor_view_service.h"

#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static void initialize_nodes(nostos_network_state_t *network)
{
    *network = (nostos_network_state_t){0};
    for (uint8_t index = 0U; index < NOSTOS_NODE_COUNT; ++index) {
        network->nodes[index].source_id = (uint8_t)(index + 1U);
    }
}

static void set_ride(
    nostos_node_state_t *node,
    uint32_t received_ms,
    uint16_t kmh_x10,
    uint32_t distance_mm)
{
    node->ride.report = (nostos_report_t){
        .received_ms = received_ms,
        .seen = true,
    };
    node->ride.speed_kmh_x10 = (nostos_u16_value_t){
        .value = kmh_x10,
        .quality = NOSTOS_VALID,
        .has_value = true,
        .value_received_ms = received_ms,
    };
    node->ride.distance_mm = (nostos_u32_value_t){
        .value = distance_mm,
        .quality = NOSTOS_VALID,
        .has_value = true,
        .value_received_ms = received_ms,
    };
}

static void set_environment(
    nostos_node_state_t *node,
    uint32_t received_ms,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10)
{
    node->environment.report = (nostos_report_t){
        .received_ms = received_ms,
        .seen = true,
    };
    node->environment.temperature_c_x10 = (nostos_i16_value_t){
        .value = temperature_c_x10,
        .quality = NOSTOS_VALID,
        .has_value = true,
        .value_received_ms = received_ms,
    };
    node->environment.humidity_pct_x10 = (nostos_u16_value_t){
        .value = humidity_pct_x10,
        .quality = NOSTOS_VALID,
        .has_value = true,
        .value_received_ms = received_ms,
    };
}

int main(void)
{
    sensor_store_init();
    sensor_view_service_init();
    CHECK(!sensor_view_service_snapshot(0U, NULL));
    CHECK(sensor_store_update_ride(true, 125U, 1500U, 100U));
    CHECK(sensor_store_update_environment(true, 253, 610U, 100U));

    nostos_network_state_t network;
    initialize_nodes(&network);
    set_ride(&network.nodes[0], 200U, 200U, 2000U);
    set_environment(&network.nodes[0], 200U, 260, 620U);
    sensor_view_service_bind_network(&network, 2U);

    sensor_view_snapshot_t view;
    CHECK(sensor_view_service_snapshot(250U, &view));
    CHECK(view.sensors.ride.kmh_x10 == 125U);
    CHECK(view.sensors.ride.distance_mm == 1500U);
    CHECK(view.ride_source_id == 2U);
    CHECK(view.sensors.environment.temperature_c_x10 == 253);
    CHECK(view.sensors.environment.humidity_pct_x10 == 610U);
    CHECK(view.environment_source_id == 2U);

    /* Once local data is stale, choose the newest complete report as a pair. */
    set_ride(&network.nodes[0], 3900U, 220U, 3900U);
    set_ride(&network.nodes[2], 4100U, 310U, 4100U);
    set_environment(&network.nodes[0], 4000U, 270, 650U);
    set_environment(&network.nodes[2], 4050U, 280, 660U);
    CHECK(sensor_view_service_snapshot(4200U, &view));
    CHECK(view.sensors.ride.quality == SENSOR_QUALITY_VALID);
    CHECK(view.sensors.ride.kmh_x10 == 310U);
    CHECK(view.sensors.ride.distance_mm == 4100U);
    CHECK(view.ride_source_id == 3U);
    CHECK(view.sensors.environment.quality == SENSOR_QUALITY_VALID);
    CHECK(view.sensors.environment.temperature_c_x10 == 280);
    CHECK(view.sensors.environment.humidity_pct_x10 == 660U);
    CHECK(view.environment_source_id == 3U);

    /* The read-only view must never make remote values publishable locally. */
    sensor_snapshot_t local;
    CHECK(sensor_store_snapshot(4200U, &local));
    CHECK(local.ride.kmh_x10 == 125U);
    CHECK(local.ride.distance_mm == 1500U);
    CHECK(local.environment.temperature_c_x10 == 253);
    CHECK(local.environment.humidity_pct_x10 == 610U);

    /* Reports are no longer displayable at the exact freshness boundary. */
    CHECK(sensor_view_service_snapshot(7100U, &view));
    CHECK(view.sensors.ride.quality == SENSOR_QUALITY_STALE);
    CHECK(view.sensors.environment.quality == SENSOR_QUALITY_STALE);
    CHECK(view.ride_source_id == 0U);
    CHECK(view.environment_source_id == 0U);

    puts("sensor_view_service local/network merge tests passed");
    return 0;
}
