#ifndef MESH_NODE_H
#define MESH_NODE_H

#include <stdbool.h>

#include "esp_err.h"

esp_err_t mesh_node_init(void);

esp_err_t mesh_node_send_onoff(bool onoff, bool acknowledged);

esp_err_t mesh_node_set_low_tx_power(bool enabled);

void mesh_node_log_status(void);

esp_err_t mesh_node_factory_reset(void);

#endif
