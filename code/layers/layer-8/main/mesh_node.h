#ifndef BSG_MESH_NODE_H
#define BSG_MESH_NODE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define BSG_COMPANY_ID 0x02E5 /* Espressif example ID: closed prototype only */
#define BSG_MODEL_ID 0x0001
#define BSG_EVENT_GROUP 0xC001

esp_err_t mesh_node_init(void);
bool mesh_node_ready(void);
uint16_t mesh_node_primary(void);
esp_err_t mesh_node_send_event(const uint8_t *wire, size_t length);
esp_err_t mesh_node_send_onoff(bool onoff, bool acknowledged);
esp_err_t mesh_node_set_low_tx_power(bool enabled);
void mesh_node_log_status(void);
#endif
