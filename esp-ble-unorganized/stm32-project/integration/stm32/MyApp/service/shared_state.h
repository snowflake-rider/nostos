#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <stdbool.h>
#include <stdint.h>

/* 앱에서 정한 출처입니다. Mesh 주소나 UART 패킷의 바이트 값은 아닙니다. */
typedef enum
{
    SHARED_NODE_A = 0,
    SHARED_NODE_B,
    SHARED_NODE_C,
    SHARED_NODE_COUNT
} shared_node_t;

typedef enum
{
    SHARED_SENSOR_SPEED_KMH = 0,
    SHARED_SENSOR_TEMPERATURE_C,
    SHARED_SENSOR_TILT_DEG, /* STM32 센서 담당 코드가 계산한 기울기 */
    SHARED_SENSOR_COUNT
} shared_sensor_t;

typedef enum
{
    SHARED_VALUE_INVALID_ARGUMENT = -1,
    SHARED_VALUE_MISSING = 0,
    SHARED_VALUE_FRESH,
    SHARED_VALUE_STALE
} shared_value_status_t;

typedef struct
{
    float value;
    bool has_value;
    uint32_t updated_ms;
} shared_sensor_value_t;

/* 각 STM32가 한 사본을 소유합니다. 필드는 아래 API를 통해 접근합니다.
 * init 후 사용하며 app_process 같은 단일 실행 흐름에서만 접근합니다.
 * ISR/다른 Task에서 동시에 호출할 경우 외부 동기화가 필요합니다. */
typedef struct
{
    shared_sensor_value_t values[SHARED_NODE_COUNT][SHARED_SENSOR_COUNT];
    uint32_t stale_after_ms;
} shared_state_t;

/* 전 항목을 미수신으로 초기화합니다. 0ms 기준은 거부하며 기존 상태를 유지합니다. */
bool shared_state_init(shared_state_t *state, uint32_t stale_after_ms);

/* 지정한 출처·센서 한 칸만 갱신합니다. now_ms는 이 STM32의 수신/측정 시각입니다.
 * NaN/무한대와 잘못된 인수는 거부하고 기존 값·시각을 유지합니다.
 * 같은 값도 새 시각으로 갱신합니다. 센서별 물리 범위/수신 순서 검사는 호출자 책임입니다. */
bool shared_state_update(
    shared_state_t *state,
    shared_node_t node,
    shared_sensor_t sensor,
    float value,
    uint32_t now_ms
);

/* 값은 복사해서 반환합니다. age >= stale_after_ms이면 STALE입니다.
 * now_ms는 update와 같은 STM32의 32비트 ms tick을 사용합니다.
 * 두 시각의 실제 간격은 2^32ms 미만이어야 합니다(약 49.7일).
 * 잘못된 인수이면 out_value를 바꾸지 않습니다. */
shared_value_status_t shared_state_get(
    const shared_state_t *state,
    shared_node_t node,
    shared_sensor_t sensor,
    uint32_t now_ms,
    shared_sensor_value_t *out_value
);

#endif /* SHARED_STATE_H */
