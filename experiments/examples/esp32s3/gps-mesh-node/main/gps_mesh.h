#ifndef GPS_MESH_H
#define GPS_MESH_H
#include "esp_ble_mesh_defs.h"
#include "esp_err.h"
extern esp_ble_mesh_model_t gps_models[1];
esp_err_t gps_mesh_init(void);
esp_err_t gps_mesh_set_source(uint16_t source);
void gps_mesh_log_status(void);
#endif
