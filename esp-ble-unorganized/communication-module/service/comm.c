#include "comm.h"

static comm_status_t from_periodic(comm_periodic_status_t status)
{
    switch (status) {
    case COMM_PERIODIC_OK: return COMM_OK;
    case COMM_PERIODIC_NOT_READY: return COMM_NOT_READY;
    case COMM_PERIODIC_NOT_DUE: return COMM_IDLE;
    case COMM_PERIODIC_STALE: return COMM_STALE;
    case COMM_PERIODIC_BUSY: return COMM_BUSY;
    case COMM_PERIODIC_SENT: return COMM_SPEED_ACCEPTED;
    case COMM_PERIODIC_INVALID_ARGUMENT: return COMM_INVALID_ARGUMENT;
    case COMM_PERIODIC_INVALID_SAMPLE: return COMM_INVALID_SAMPLE;
    case COMM_PERIODIC_INVALID_TIME: return COMM_INVALID_TIME;
    }
    return COMM_INVALID_ARGUMENT;
}

comm_status_t comm_init(comm_t *comm, const comm_config_t *config, uint64_t now_ms)
{
    if (comm == NULL || config == NULL || config->send == NULL ||
        config->speed_period_ms == 0 || config->speed_stale_after_ms == 0 ||
        config->max_event_burst == 0) {
        return COMM_INVALID_ARGUMENT;
    }
    *comm = (comm_t){
        .send = config->send,
        .send_context = config->send_context,
        .max_event_burst = config->max_event_burst
    };
    (void)comm_event_queue_init(&comm->events);
    (void)comm_periodic_init(&comm->speed, config->speed_period_ms,
                             config->speed_stale_after_ms, now_ms);
    return COMM_OK;
}

comm_status_t comm_post_button(comm_t *comm, comm_button_message_t code)
{
    if (comm == NULL) {
        return COMM_INVALID_ARGUMENT;
    }
    /* uint8_t 변환 전에 검사하여 257 같은 값이 1로 잘리는 것을 막는다. */
    if (code != COMM_BUTTON_MSG_1 && code != COMM_BUTTON_MSG_2 &&
        code != COMM_BUTTON_MSG_3) {
        return COMM_INVALID_MESSAGE;
    }
    const comm_message_t message = {
        .type = COMM_MESSAGE_EVENT,
        .data.event.code = (uint8_t)code
    };
    return comm_event_queue_push(&comm->events, &message) == COMM_QUEUE_FULL
        ? COMM_FULL : COMM_OK;
}

comm_status_t comm_update_speed(comm_t *comm, float speed_cm_s, uint64_t now_ms)
{
    if (comm == NULL) {
        return COMM_INVALID_ARGUMENT;
    }
    return from_periodic(comm_periodic_update(&comm->speed, speed_cm_s, now_ms));
}

comm_status_t comm_invalidate_speed(comm_t *comm, uint64_t now_ms)
{
    if (comm == NULL) {
        return COMM_INVALID_ARGUMENT;
    }
    return from_periodic(comm_periodic_invalidate(&comm->speed, now_ms));
}

comm_status_t comm_read_speed(comm_t *comm, uint64_t now_ms, comm_speed_data_t *speed)
{
    if (comm == NULL) {
        return COMM_INVALID_ARGUMENT;
    }
    return from_periodic(comm_periodic_read(&comm->speed, now_ms, speed));
}

comm_status_t comm_process(comm_t *comm, uint64_t now_ms)
{
    if (comm == NULL) {
        return COMM_INVALID_ARGUMENT;
    }

    /* 속도가 없어도 공통 시계를 먼저 검사한다. 시간 오류에서 이벤트를 보내지 않는다. */
    comm_speed_data_t speed;
    if (comm_periodic_read(&comm->speed, now_ms, &speed) == COMM_PERIODIC_INVALID_TIME) {
        return COMM_INVALID_TIME;
    }
    comm_message_t event;
    const bool has_event = comm_event_queue_peek(&comm->events, &event) == COMM_QUEUE_OK;

    /* 이벤트 한도를 채웠거나 이벤트가 없으면 속도에 먼저 전송 기회를 준다. */
    if (!has_event || comm->events_since_speed >= comm->max_event_burst) {
        const comm_status_t status = from_periodic(comm_periodic_poll(
            &comm->speed, now_ms, comm->send, comm->send_context));
        if (status == COMM_SPEED_ACCEPTED) {
            comm->events_since_speed = 0;
            return status;
        }
        if (status != COMM_IDLE && status != COMM_NOT_READY && status != COMM_STALE) {
            return status; /* BUSY를 포함: 같은 호출에서 다른 메시지를 보내지 않는다. */
        }
    }
    if (!has_event) {
        return COMM_IDLE;
    }
    if (!comm->send(&event, comm->send_context)) {
        return COMM_BUSY;
    }
    (void)comm_event_queue_pop(&comm->events, &event);
    if (comm->events_since_speed < comm->max_event_burst) {
        ++comm->events_since_speed;
    }
    return COMM_EVENT_ACCEPTED;
}
