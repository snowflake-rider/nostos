#include "safety_detector.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); } } while (0)

static bool near(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

static void add_stable_samples(
    float ax,
    float ay,
    float az,
    float gx,
    float gy,
    float gz
)
{
    for (uint32_t sample = 0U;
         sample < SAFETY_CALIBRATION_REQUIRED_SAMPLES;
         ++sample)
    {
        bool completed = safety_detector_add_calibration_sample(
            ax, ay, az, gx, gy, gz);
        CHECK(completed ==
              (sample == (SAFETY_CALIBRATION_REQUIRED_SAMPLES - 1U)));
    }
}

static void calibration_uses_mounted_gravity_direction(void)
{
    const float ax = 0.2f;
    const float ay = -0.3f;
    const float az = 0.9327379f;
    const float gx = 0.2f;
    const float gy = -0.1f;
    const float gz = 0.05f;

    safety_detector_init();
    const safety_detector_status_t *status = safety_detector_get_status();
    CHECK(status->calibration_state == SAFETY_CALIBRATION_UNCALIBRATED);
    CHECK(!status->calibration_valid);

    /* 캘리브레이션 전에는 큰 값도 낙상 상태를 시작하지 않습니다. */
    CHECK(safety_detector_check(0U, false, 0.0f, true,
                                2.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 0.0f) == SAFETY_EVENT_NONE);
    CHECK(safety_detector_get_status()->fall_state == FALL_STATE_IDLE);

    CHECK(safety_detector_start_calibration());
    for (uint32_t sample = 0U; sample < 10U; ++sample)
    {
        CHECK(!safety_detector_add_calibration_sample(
            ax, ay, az, gx, gy, gz));
    }
    CHECK(safety_detector_get_status()->calibration_sample_count == 10U);

    /* 움직임 한 번은 연속 샘플을 폐기하지만 캘리브레이션을 막지는 않습니다. */
    CHECK(!safety_detector_add_calibration_sample(
        ax, ay, az, 6.0f, 0.0f, 0.0f));
    CHECK(safety_detector_get_status()->calibration_sample_count == 0U);

    add_stable_samples(ax, ay, az, gx, gy, gz);
    status = safety_detector_get_status();
    CHECK(status->calibration_state == SAFETY_CALIBRATION_READY);
    CHECK(status->calibration_valid);
    CHECK(near(status->baseline_accel_x, ax, 0.001f));
    CHECK(near(status->baseline_accel_y, ay, 0.001f));
    CHECK(near(status->baseline_accel_z, az, 0.001f));
    CHECK(near(status->gyro_offset_x, gx, 0.001f));
    CHECK(near(status->gyro_offset_y, gy, 0.001f));
    CHECK(near(status->gyro_offset_z, gz, 0.001f));

    CHECK(safety_detector_check(100U, false, 0.0f, true,
                                ax, ay, az, gx, gy, gz) ==
          SAFETY_EVENT_NONE);
    status = safety_detector_get_status();
    CHECK(status->fall_state == FALL_STATE_IDLE);
    CHECK(near(status->total_acceleration_g, 1.0f, 0.001f));
    CHECK(near(status->tilt_cosine, 1.0f, 0.001f));

    /* 기준 방향의 1.6배 충격 뒤, 기준과 직각인 자세에서 카운트다운합니다. */
    CHECK(safety_detector_check(200U, false, 0.0f, true,
                                ax * 1.6f, ay * 1.6f, az * 1.6f,
                                gx, gy, gz) == SAFETY_EVENT_NONE);
    CHECK(safety_detector_get_status()->fall_state ==
          FALL_STATE_IMPACT_DETECTED);
    CHECK(safety_detector_check(1200U, false, 0.0f, true,
                                0.8320503f, 0.5547002f, 0.0f,
                                gx, gy, gz) == SAFETY_EVENT_FALL_COUNTDOWN);
    CHECK(safety_detector_get_status()->fall_state == FALL_STATE_COUNTDOWN);
    CHECK(!safety_detector_start_calibration());
}

static void failed_recalibration_keeps_last_good_baseline(void)
{
    safety_detector_init();
    CHECK(safety_detector_start_calibration());
    add_stable_samples(0.0f, 0.0f, 1.0f, 0.1f, 0.0f, 0.0f);
    CHECK(safety_detector_get_status()->calibration_valid);

    CHECK(safety_detector_start_calibration());
    CHECK(!safety_detector_add_calibration_sample(
        NAN, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f));
    CHECK(safety_detector_get_status()->calibration_sample_count == 0U);
    safety_detector_fail_calibration(SAFETY_CALIBRATION_FAILED_UNSTABLE);
    const safety_detector_status_t *status = safety_detector_get_status();
    CHECK(status->calibration_state == SAFETY_CALIBRATION_FAILED_UNSTABLE);
    CHECK(status->calibration_valid);
    CHECK(near(status->baseline_accel_z, 1.0f, 0.001f));
}

int main(void)
{
    calibration_uses_mounted_gravity_direction();
    failed_recalibration_keeps_last_good_baseline();
    puts("PASS mounted gravity baseline, gyro offsets, stable 40-sample calibration");
    puts("PASS orientation-independent magnitude/tilt and failed-recalibration fallback");
    puts("HARDWARE_MPU6050_AND_BICYCLE=NOT_TESTED");
    return 0;
}
