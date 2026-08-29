#ifndef NOSTOS_TEST_ESP_LOG_H
#define NOSTOS_TEST_ESP_LOG_H
#include <stdarg.h>
static inline void test_esp_log(const char *tag,const char *format,...)
{ (void)tag; (void)format; }
#define ESP_LOGI test_esp_log
#define ESP_LOGW test_esp_log
#define ESP_LOGE test_esp_log
#endif
