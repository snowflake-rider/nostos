#ifndef NOSTOS_TEST_QUEUE_H
#define NOSTOS_TEST_QUEUE_H
#include "FreeRTOS.h"
typedef void *QueueHandle_t;
int xQueueReceive(QueueHandle_t queue,void *event,TickType_t wait);
int xQueueReset(QueueHandle_t queue);
#endif
