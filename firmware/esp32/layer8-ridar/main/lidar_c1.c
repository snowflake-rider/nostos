#include "lidar_c1.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lidar_c1_protocol.h"

#define LIDAR_UART UART_NUM_2
#define LIDAR_TX_GPIO 14
#define LIDAR_RX_GPIO 13
#define LIDAR_BAUD_RATE 460800
#define LIDAR_RESPONSE_TIMEOUT_US 1000000LL
#define LIDAR_RX_TEST_SAMPLES 8U
#define LIDAR_RX_TEST_SETTLE_US 50U

static const char *TAG = "LAYER8_RIDAR_LIDAR";

typedef enum {
    REQUEST_NONE = 0,
    REQUEST_INFO,
    REQUEST_HEALTH,
} pending_request_t;

typedef struct {
    bool initialized;
    pending_request_t pending;
    int64_t request_started_us;
    uint32_t requests;
    uint32_t responses;
    uint32_t rx_bytes;
    uint32_t rx_events;
    uint32_t timeouts;
    uint32_t parse_errors;
    uint32_t unexpected_responses;
    bool have_info;
    bool have_health;
    lidar_c1_info_t info;
    lidar_c1_health_t health;
} lidar_state_t;

static QueueHandle_t s_uart_events;
static lidar_state_t s_state;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;

static const char *request_name(pending_request_t request)
{
    switch (request) {
    case REQUEST_INFO: return "GET_INFO";
    case REQUEST_HEALTH: return "GET_HEALTH";
    case REQUEST_NONE:
    default: return "none";
    }
}

static const char *health_name(uint8_t status)
{
    switch (status) {
    case 0U: return "good";
    case 1U: return "warning";
    case 2U: return "error";
    default: return "unknown";
    }
}

static void process_response(const lidar_c1_response_t *response)
{
    lidar_c1_info_t info;
    lidar_c1_health_t health;
    const bool is_info = lidar_c1_decode_info(response, &info);
    const bool is_health = lidar_c1_decode_health(response, &health);
    pending_request_t pending;

    portENTER_CRITICAL(&s_state_lock);
    pending = s_state.pending;
    s_state.responses++;
    if (is_info) {
        s_state.info = info;
        s_state.have_info = true;
    } else if (is_health) {
        s_state.health = health;
        s_state.have_health = true;
    } else {
        s_state.parse_errors++;
    }

    const bool expected =
        (pending == REQUEST_INFO && is_info) ||
        (pending == REQUEST_HEALTH && is_health);
    if (expected) {
        s_state.pending = REQUEST_NONE;
    } else {
        s_state.unexpected_responses++;
    }
    portEXIT_CRITICAL(&s_state_lock);

    if (is_info) {
        char serial[sizeof(info.serial_number) * 2U + 1U];
        for (size_t i = 0; i < sizeof(info.serial_number); i++) {
            (void)snprintf(&serial[i * 2U], 3U, "%02X", info.serial_number[i]);
        }
        ESP_LOGI(TAG, "INFO model=%u firmware=%u.%u hardware=%u serial=%s",
                 info.model, info.firmware_major, info.firmware_minor,
                 info.hardware, serial);
    } else if (is_health) {
        ESP_LOGI(TAG, "HEALTH status=%s(%u) error_code=%u",
                 health_name(health.status), health.status, health.error_code);
    } else {
        ESP_LOGW(TAG, "Unsupported response type=0x%02X mode=%u size=%" PRIu32,
                 response->type, response->send_mode, response->payload_size);
    }

    if (!expected) {
        ESP_LOGW(TAG, "Unexpected response while pending=%s", request_name(pending));
    }
}

static void check_timeout(void)
{
    pending_request_t timed_out = REQUEST_NONE;
    const int64_t now = esp_timer_get_time();

    portENTER_CRITICAL(&s_state_lock);
    if (s_state.pending != REQUEST_NONE &&
        now - s_state.request_started_us >= LIDAR_RESPONSE_TIMEOUT_US) {
        timed_out = s_state.pending;
        s_state.pending = REQUEST_NONE;
        s_state.timeouts++;
    }
    portEXIT_CRITICAL(&s_state_lock);

    if (timed_out != REQUEST_NONE) {
        ESP_LOGW(TAG, "%s timeout after 1000 ms; check 5V/GND/TX/RX and baud",
                 request_name(timed_out));
    }
}

static void lidar_uart_task(void *argument)
{
    (void)argument;
    lidar_c1_parser_t parser;
    lidar_c1_parser_init(&parser);

    for (;;) {
        uart_event_t event;
        if (xQueueReceive(s_uart_events, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (event.type == UART_DATA) {
                size_t remaining = event.size;
                uint8_t bytes[128];
                while (remaining > 0U) {
                    const size_t wanted = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
                    const int count = uart_read_bytes(LIDAR_UART, bytes, wanted, 0);
                    if (count <= 0) break;
                    remaining -= (size_t)count;
                    portENTER_CRITICAL(&s_state_lock);
                    s_state.rx_bytes += (uint32_t)count;
                    s_state.rx_events++;
                    portEXIT_CRITICAL(&s_state_lock);
                    for (int i = 0; i < count; i++) {
                        lidar_c1_response_t response;
                        const lidar_c1_parse_event_t parsed =
                            lidar_c1_parser_feed(&parser, bytes[i], &response);
                        if (parsed == LIDAR_C1_PARSE_RESPONSE) {
                            process_response(&response);
                        } else if (parsed == LIDAR_C1_PARSE_ERROR) {
                            portENTER_CRITICAL(&s_state_lock);
                            s_state.parse_errors++;
                            portEXIT_CRITICAL(&s_state_lock);
                            ESP_LOGW(TAG, "Descriptor rejected; parser resynchronized");
                        }
                    }
                }
            } else if (event.type == UART_FIFO_OVF ||
                       event.type == UART_BUFFER_FULL ||
                       event.type == UART_PARITY_ERR ||
                       event.type == UART_FRAME_ERR) {
                uart_flush_input(LIDAR_UART);
                xQueueReset(s_uart_events);
                lidar_c1_parser_init(&parser);
                portENTER_CRITICAL(&s_state_lock);
                s_state.parse_errors++;
                portEXIT_CRITICAL(&s_state_lock);
                ESP_LOGW(TAG, "UART error type=%d; input flushed", event.type);
            }
        }
        check_timeout();
    }
}

static esp_err_t send_request(pending_request_t request)
{
    uint8_t bytes[LIDAR_C1_REQUEST_SIZE];
    if (request == REQUEST_INFO) {
        lidar_c1_make_info_request(bytes);
    } else if (request == REQUEST_HEALTH) {
        lidar_c1_make_health_request(bytes);
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&s_state_lock);
    if (!s_state.initialized || s_state.pending != REQUEST_NONE) {
        portEXIT_CRITICAL(&s_state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_state.pending = request;
    s_state.request_started_us = esp_timer_get_time();
    s_state.requests++;
    portEXIT_CRITICAL(&s_state_lock);

    const int written = uart_write_bytes(LIDAR_UART, bytes, sizeof(bytes));
    esp_err_t err = written == (int)sizeof(bytes)
                        ? uart_wait_tx_done(LIDAR_UART, pdMS_TO_TICKS(100))
                        : ESP_FAIL;
    if (err != ESP_OK) {
        portENTER_CRITICAL(&s_state_lock);
        if (s_state.pending == request) s_state.pending = REQUEST_NONE;
        portEXIT_CRITICAL(&s_state_lock);
        ESP_LOGE(TAG, "%s transmit failed: %s", request_name(request),
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "%s sent; awaiting single response", request_name(request));
    return ESP_OK;
}

esp_err_t lidar_c1_init(void)
{
    portENTER_CRITICAL(&s_state_lock);
    const bool already_initialized = s_state.initialized;
    portEXIT_CRITICAL(&s_state_lock);
    if (already_initialized) return ESP_ERR_INVALID_STATE;

    const uart_config_t config = {
        .baud_rate = LIDAR_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_param_config(LIDAR_UART, &config);
    if (err != ESP_OK) return err;
    err = uart_set_pin(LIDAR_UART, LIDAR_TX_GPIO, LIDAR_RX_GPIO,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    /* UART idles high; the weak pull-up also makes an open C1 TX path observable. */
    err = gpio_set_pull_mode(LIDAR_RX_GPIO, GPIO_PULLUP_ONLY);
    if (err != ESP_OK) return err;
    err = uart_driver_install(LIDAR_UART, 2048, 0, 16, &s_uart_events, 0);
    if (err != ESP_OK) return err;

    if (xTaskCreate(lidar_uart_task, "lidar_c1_rx", 4096, NULL, 4, NULL) != pdPASS) {
        uart_driver_delete(LIDAR_UART);
        s_uart_events = NULL;
        return ESP_ERR_NO_MEM;
    }

    portENTER_CRITICAL(&s_state_lock);
    s_state.initialized = true;
    portEXIT_CRITICAL(&s_state_lock);
    ESP_LOGI(TAG, "READY UART2 TX=GPIO14 RX=GPIO13 460800/8N1; scan/motor disabled");
    return ESP_OK;
}

esp_err_t lidar_c1_request_info(void)
{
    return send_request(REQUEST_INFO);
}

esp_err_t lidar_c1_request_health(void)
{
    return send_request(REQUEST_HEALTH);
}

esp_err_t lidar_c1_test_rx_line(void)
{
    portENTER_CRITICAL(&s_state_lock);
    const bool ready = s_state.initialized && s_state.pending == REQUEST_NONE;
    portEXIT_CRITICAL(&s_state_lock);
    if (!ready) return ESP_ERR_INVALID_STATE;

    esp_err_t err = uart_wait_tx_done(LIDAR_UART, pdMS_TO_TICKS(100));
    if (err != ESP_OK) return err;
    err = uart_disable_rx_intr(LIDAR_UART);
    if (err != ESP_OK) return err;

    int pullup_level = 0;
    unsigned high_samples = 0U;
    err = uart_flush_input(LIDAR_UART);
    if (err != ESP_OK) goto restore_rx;
    (void)xQueueReset(s_uart_events);
    pullup_level = gpio_get_level(LIDAR_RX_GPIO);
    err = gpio_set_pull_mode(LIDAR_RX_GPIO, GPIO_PULLDOWN_ONLY);
    if (err != ESP_OK) goto restore_rx;

    for (unsigned sample = 0U; sample < LIDAR_RX_TEST_SAMPLES; sample++) {
        esp_rom_delay_us(LIDAR_RX_TEST_SETTLE_US);
        if (gpio_get_level(LIDAR_RX_GPIO) != 0) high_samples++;
    }

restore_rx:
    {
        const esp_err_t restore_err =
            gpio_set_pull_mode(LIDAR_RX_GPIO, GPIO_PULLUP_ONLY);
        const esp_err_t flush_err = uart_flush_input(LIDAR_UART);
        (void)xQueueReset(s_uart_events);
        const esp_err_t enable_err = uart_enable_rx_intr(LIDAR_UART);
        if (err == ESP_OK && restore_err != ESP_OK) err = restore_err;
        if (err == ESP_OK && flush_err != ESP_OK) err = flush_err;
        if (err == ESP_OK && enable_err != ESP_OK) err = enable_err;
    }
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG,
             "RX_LINE_TEST pullup_level=%d pulldown_high=%u/%u result=%s; "
             "input-only test, pull-up restored",
             pullup_level, high_samples, LIDAR_RX_TEST_SAMPLES,
             high_samples == LIDAR_RX_TEST_SAMPLES
                 ? "externally_driven_high"
                 : "open_or_unpowered");
    return ESP_OK;
}

void lidar_c1_log_status(void)
{
    lidar_state_t snapshot;
    size_t buffered_bytes = 0U;
    portENTER_CRITICAL(&s_state_lock);
    snapshot = s_state;
    portEXIT_CRITICAL(&s_state_lock);
    const esp_err_t buffered_err =
        uart_get_buffered_data_len(LIDAR_UART, &buffered_bytes);
    const int rx_level = gpio_get_level(LIDAR_RX_GPIO);

    ESP_LOGI(TAG,
             "STATUS initialized=%u uart=2 tx=14 rx=13 baud=460800 pending=%s "
             "requests=%" PRIu32 " responses=%" PRIu32
             " rx_bytes=%" PRIu32 " rx_events=%" PRIu32
             " buffered=%u buffered_ok=%u rx_level=%d timeouts=%" PRIu32
             " parse_errors=%" PRIu32 " unexpected=%" PRIu32,
             snapshot.initialized, request_name(snapshot.pending), snapshot.requests,
             snapshot.responses, snapshot.rx_bytes, snapshot.rx_events,
             (unsigned)buffered_bytes, buffered_err == ESP_OK, rx_level,
             snapshot.timeouts, snapshot.parse_errors, snapshot.unexpected_responses);
    if (snapshot.have_info) {
        ESP_LOGI(TAG, "LAST_INFO model=%u firmware=%u.%u hardware=%u",
                 snapshot.info.model, snapshot.info.firmware_major,
                 snapshot.info.firmware_minor, snapshot.info.hardware);
    }
    if (snapshot.have_health) {
        ESP_LOGI(TAG, "LAST_HEALTH status=%s(%u) error_code=%u",
                 health_name(snapshot.health.status), snapshot.health.status,
                 snapshot.health.error_code);
    }
}
