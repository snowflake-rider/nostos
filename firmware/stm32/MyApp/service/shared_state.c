#include "shared_state.h"

#include <math.h>
#include <stddef.h>

static bool valid_slot(const shared_state_t *state, shared_node_t node,
                       shared_sensor_t sensor)
{
    return (state != NULL) && (state->stale_after_ms != 0U) &&
           ((unsigned)node < SHARED_NODE_COUNT) &&
           ((unsigned)sensor < SHARED_SENSOR_COUNT);
}

bool shared_state_init(shared_state_t *state, uint32_t stale_after_ms)
{
    if ((state == NULL) || (stale_after_ms == 0U))
    {
        return false;
    }
    *state = (shared_state_t){.stale_after_ms = stale_after_ms};
    return true;
}

bool shared_state_update(
    shared_state_t *state,
    shared_node_t node,
    shared_sensor_t sensor,
    float value,
    uint32_t now_ms
)
{
    if (!valid_slot(state, node, sensor) || !isfinite(value))
    {
        return false;
    }
    state->values[node][sensor] = (shared_sensor_value_t){
        .value = value,
        .has_value = true,
        .updated_ms = now_ms,
    };
    return true;
}

shared_value_status_t shared_state_get(
    const shared_state_t *state,
    shared_node_t node,
    shared_sensor_t sensor,
    uint32_t now_ms,
    shared_sensor_value_t *out_value
)
{
    if (!valid_slot(state, node, sensor) || (out_value == NULL))
    {
        return SHARED_VALUE_INVALID_ARGUMENT;
    }
    *out_value = state->values[node][sensor];
    if (!out_value->has_value)
    {
        return SHARED_VALUE_MISSING;
    }
    /* HAL_GetTick()의 32비트 한 번 넘침도 unsigned 뺄셈으로 처리합니다. */
    uint32_t age_ms = (uint32_t)(now_ms - out_value->updated_ms);
    return (age_ms >= state->stale_after_ms) ? SHARED_VALUE_STALE : SHARED_VALUE_FRESH;
}
