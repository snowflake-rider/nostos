#include "comm_periodic.h"

#include <math.h>

static bool observe_time(comm_periodic_t *state, uint64_t now_ms)
{
    if (now_ms < state->last_observed_ms) {
        return false;
    }
    state->last_observed_ms = now_ms;
    return true;
}

comm_periodic_status_t comm_periodic_init(comm_periodic_t *state,
                                         uint32_t period_ms,
                                         uint32_t stale_after_ms,
                                         uint64_t now_ms)
{
    if (state == NULL || period_ms == 0 || stale_after_ms == 0) {
        return COMM_PERIODIC_INVALID_ARGUMENT;
    }
    *state = (comm_periodic_t){
        .period_ms = period_ms,
        .stale_after_ms = stale_after_ms,
        .last_slot_ms = now_ms,
        .last_observed_ms = now_ms
    };
    return COMM_PERIODIC_OK;
}

comm_periodic_status_t comm_periodic_update(comm_periodic_t *state,
                                           float speed_cm_s, uint64_t now_ms)
{
    if (state == NULL) {
        return COMM_PERIODIC_INVALID_ARGUMENT;
    }
    if (!isfinite(speed_cm_s) || speed_cm_s < 0.0f) {
        return COMM_PERIODIC_INVALID_SAMPLE;
    }
    if (!observe_time(state, now_ms)) {
        return COMM_PERIODIC_INVALID_TIME;
    }

    /* 수신이 오래 끊긴 뒤에는 이전 주행의 측정값과 섞지 않는다. */
    if (state->has_sample && now_ms - state->last_sample_ms >= state->stale_after_ms) {
        (void)comm_moving_average_init(&state->window);
    }
    (void)comm_moving_average_update(&state->window, speed_cm_s);
    state->last_sample_ms = now_ms;
    state->has_sample = true;
    return COMM_PERIODIC_OK;
}

comm_periodic_status_t comm_periodic_invalidate(comm_periodic_t *state, uint64_t now_ms)
{
    if (state == NULL) {
        return COMM_PERIODIC_INVALID_ARGUMENT;
    }
    if (!observe_time(state, now_ms)) {
        return COMM_PERIODIC_INVALID_TIME;
    }
    (void)comm_moving_average_init(&state->window);
    state->has_sample = false;
    return COMM_PERIODIC_OK;
}

comm_periodic_status_t comm_periodic_read(comm_periodic_t *state, uint64_t now_ms,
                                         comm_speed_data_t *speed)
{
    if (state == NULL || speed == NULL) {
        return COMM_PERIODIC_INVALID_ARGUMENT;
    }
    if (!observe_time(state, now_ms)) {
        return COMM_PERIODIC_INVALID_TIME;
    }

    /* 숫자 0만으로 결측과 정지를 구분하지 않는다. valid를 함께 반환한다. */
    *speed = (comm_speed_data_t){0};
    if (!state->has_sample) {
        return COMM_PERIODIC_NOT_READY;
    }
    if (now_ms - state->last_sample_ms >= state->stale_after_ms) {
        return COMM_PERIODIC_STALE;
    }
    if (!comm_moving_average_get(&state->window, &speed->average_cm_s)) {
        return COMM_PERIODIC_NOT_READY;
    }
    speed->valid = true;
    return COMM_PERIODIC_OK;
}

comm_periodic_status_t comm_periodic_poll(comm_periodic_t *state, uint64_t now_ms,
                                         comm_periodic_send_fn send, void *context)
{
    if (state == NULL || send == NULL) {
        return COMM_PERIODIC_INVALID_ARGUMENT;
    }

    comm_speed_data_t speed;
    const comm_periodic_status_t status = comm_periodic_read(state, now_ms, &speed);
    if (status != COMM_PERIODIC_OK) {
        return status;
    }
    const uint64_t elapsed = now_ms - state->last_slot_ms;
    if (elapsed < state->period_ms) {
        return COMM_PERIODIC_NOT_DUE;
    }

    const comm_message_t message = {
        .type = COMM_MESSAGE_SPEED,
        .data.speed = speed
    };
    if (!send(&message, context)) {
        return COMM_PERIODIC_BUSY;
    }

    /* 초기 주기 기준을 유지하되, 놓친 주기는 한꺼번에 재전송하지 않는다. */
    state->last_slot_ms = now_ms - (elapsed % state->period_ms);
    return COMM_PERIODIC_SENT;
}
