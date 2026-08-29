#ifndef ENVIRONMENT_SERVICE_H
#define ENVIRONMENT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

void environment_service_init(void);
void environment_service_process(void);
bool environment_service_data_valid(void);
bool environment_service_get(int16_t *temperature_x10, uint16_t *humidity_x10);
uint32_t environment_service_failure_count(void);

#endif /* ENVIRONMENT_SERVICE_H */
