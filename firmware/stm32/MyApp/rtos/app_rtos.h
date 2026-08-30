#ifndef APP_RTOS_H
#define APP_RTOS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t queued;
    uint32_t queue_full;
    uint32_t expired;
    uint32_t dispatched;
    uint32_t resets;
} app_rtos_stats_t;

bool app_rtos_start(void);
const app_rtos_stats_t *app_rtos_get_stats(void);

#endif /* APP_RTOS_H */
