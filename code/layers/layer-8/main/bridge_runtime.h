#ifndef BSG_BRIDGE_RUNTIME_H
#define BSG_BRIDGE_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t bridge_runtime_init(void);
/* Called in Mesh task context, not ISR. Copies data; never waits on UART. */
void bridge_runtime_mesh_rx(const uint8_t *wire, size_t length, uint16_t source,
                            uint16_t own_address);
void bridge_runtime_mesh_complete(int error);
void bridge_runtime_log_status(void);
#endif
