#include "sensor_link.h"
#include "sensor_view_service.h"

#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    sensor_store_init();
    sensor_view_service_init();
    CHECK(!sensor_view_service_snapshot(0U, NULL));

    CHECK(sensor_store_update_ride(true, 125U, 1500U, 10U));
    CHECK(sensor_store_update_environment(true, 240, 500U, 10U));

    sensor_view_snapshot_t view;
    CHECK(sensor_view_service_snapshot(0U, &view));
    /* Producer sensor_store data is invisible until ESP sends OUTPUT_*. */
    CHECK(view.sensors.ride.quality == SENSOR_QUALITY_UNMEASURED);
    CHECK(view.sensors.environment.quality == SENSOR_QUALITY_UNMEASURED);

    CHECK(!sensor_view_service_apply_output_ride(0U, true, 200U, 1000U, 10U));
    CHECK(!sensor_view_service_apply_output_ride(1U, false, 1U, 0U, 10U));
    CHECK(sensor_view_service_apply_output_ride(3U, true, 310U, 4100U, 100U));
    CHECK(sensor_view_service_snapshot(100U, &view));
    CHECK(view.sensors.ride.quality == SENSOR_QUALITY_VALID);
    CHECK(view.sensors.ride.kmh_x10 == 310U);
    CHECK(view.sensors.ride.distance_mm == 4100U);
    CHECK(view.ride_source_id == 3U);

    CHECK(!sensor_view_service_apply_output_environment(
        4U, 253, 610U, SENSOR_LINK_QUALITY_VALID,
        SENSOR_LINK_QUALITY_VALID, 200U));
    CHECK(sensor_view_service_apply_output_environment(
        1U, 253, 610U, SENSOR_LINK_QUALITY_VALID,
        SENSOR_LINK_QUALITY_VALID, 200U));
    CHECK(sensor_view_service_snapshot(200U, &view));
    CHECK(view.sensors.environment.quality == SENSOR_QUALITY_VALID);
    CHECK(view.sensors.environment.temperature_c_x10 == 253);
    CHECK(view.sensors.environment.humidity_pct_x10 == 610U);
    CHECK(view.environment_source_id == 1U);

    CHECK(sensor_view_service_snapshot(3100U, &view));
    CHECK(view.sensors.ride.quality == SENSOR_QUALITY_STALE);
    CHECK(view.ride_source_id == 0U);
    CHECK(view.sensors.environment.quality == SENSOR_QUALITY_VALID);
    CHECK(sensor_view_service_snapshot(4200U, &view));
    CHECK(view.sensors.environment.quality == SENSOR_QUALITY_STALE);
    CHECK(view.environment_source_id == 0U);

    CHECK(sensor_view_service_apply_output_ride(2U, false, 0U, 0U, 5000U));
    CHECK(sensor_view_service_apply_output_environment(
        2U, 0, 0U, SENSOR_LINK_QUALITY_UNMEASURED,
        SENSOR_LINK_QUALITY_UNMEASURED, 5000U));
    CHECK(sensor_view_service_snapshot(5000U, &view));
    CHECK(view.sensors.ride.quality == SENSOR_QUALITY_UNMEASURED);
    CHECK(view.sensors.environment.quality == SENSOR_QUALITY_UNMEASURED);

    sensor_view_service_clear_outputs();
    CHECK(sensor_view_service_snapshot(5001U, &view));
    CHECK(view.ride_source_id == 0U && view.environment_source_id == 0U);
    CHECK(view.sensors.ride.revision == 0U);
    CHECK(view.sensors.environment.revision == 0U);

    puts("PASS display sensor mirror changes only through OUTPUT_RIDE/ENVIRONMENT");
    return 0;
}
