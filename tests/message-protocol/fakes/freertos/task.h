#ifndef NOSTOS_TEST_TASK_H
#define NOSTOS_TEST_TASK_H
#include "FreeRTOS.h"
#include <stddef.h>
typedef void *TaskHandle_t;
int xTaskCreate(void (*run)(void *),const char *name,uint32_t stack,void *arg,unsigned priority,TaskHandle_t *handle);
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
uint32_t ulTaskNotifyTake(int clear,TickType_t wait);
void xTaskNotifyGive(TaskHandle_t task);
#endif
