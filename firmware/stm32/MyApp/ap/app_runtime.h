#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H

#include "message_type.h"

#include <stdbool.h>

/* Internal ownership boundary used by the bare loop and the RTOS tasks. */
message_type_t app_runtime_poll_button(
    bool *reset_requested,
    bool *calibration_button_pressed);
bool app_runtime_poll_remote(message_type_t *message);
void app_runtime_set_calibration_button_pressed(bool pressed);
void app_runtime_dispatch_local(message_type_t message);
void app_runtime_dispatch_remote(message_type_t message);
void app_runtime_reset(void);
void app_runtime_process_services(void);

#endif /* APP_RUNTIME_H */
