#ifndef LIDAR_C1_H
#define LIDAR_C1_H

#include "esp_err.h"

esp_err_t lidar_c1_init(void);
esp_err_t lidar_c1_request_info(void);
esp_err_t lidar_c1_request_health(void);
esp_err_t lidar_c1_test_rx_line(void);
void lidar_c1_log_status(void);

#endif
