#ifndef NOSTOS_XOSS_BLE_H
#define NOSTOS_XOSS_BLE_H

#include "esp_err.h"

esp_err_t xoss_ble_init(void);
/* Clears only the STM-session ride pipeline; BLE and Mesh stay connected. */
esp_err_t xoss_ble_reset_runtime_session(void);
void xoss_ble_log_status(void);

#endif /* NOSTOS_XOSS_BLE_H */
