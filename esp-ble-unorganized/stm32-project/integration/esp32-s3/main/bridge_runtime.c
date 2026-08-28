#include "bridge_runtime.h"
#include <inttypes.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "event_bridge.h"
#include "mesh_node.h"

#define TAG "EVENT_BRIDGE"
#define DATA_UART UART_NUM_1
#define UART_TX_GPIO 17
#define UART_RX_GPIO 18

static event_bridge_t bridge;
static portMUX_TYPE bridge_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t worker;
static QueueHandle_t uart_events;
static uint32_t mesh_async_ok, mesh_async_failed, uart_errors;

static uint64_t now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000U; }

static bool send_mesh(void *context, const uint8_t *bytes, size_t length)
{
    (void)context;
    return mesh_node_send_event(bytes, length) == ESP_OK;
}

static bool send_uart(void *context, const uint8_t *bytes, size_t length)
{
    (void)context;
    if (length != 1) return false;
    /* Sole UART1 TX owner: the driver's internal TX mutex is uncontended.
     * No software TX ring; FIFO copy does not wait for room. Do not add another
     * writer (including logs) or that mutex could introduce unbounded waiting. */
    if (uart_tx_chars(DATA_UART, (const char *)bytes, 1) != 1) return false;
    return uart_wait_tx_done(DATA_UART, pdMS_TO_TICKS(10)) == ESP_OK;
    /* Timeout does not cancel a byte already in hardware. Never retry it. */
}

static void worker_task(void *argument)
{
    (void)argument;
    const event_transport_t transport = {.mesh = send_mesh, .uart = send_uart};
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (;;) {
            event_job_t job;
            bool ready = mesh_node_ready();
            uint64_t now = now_ms();
            portENTER_CRITICAL(&bridge_lock);
            event_result_t result = event_bridge_next(&bridge, now, ready, &job);
            portEXIT_CRITICAL(&bridge_lock);
            if (result == EVENT_EMPTY) break;
            if (result != EVENT_OK) {
                ESP_LOGW(TAG, "DROP direction=%u id=0x%02x reason=%u", job.direction, job.id, result);
                vTaskDelay(1);
                continue;
            }
            /* Queue lock is released before any driver or Mesh call. */
            bool accepted = event_job_send(&job, &transport);
            portENTER_CRITICAL(&bridge_lock);
            event_bridge_complete(&bridge, job.direction, accepted);
            portEXIT_CRITICAL(&bridge_lock);
            ESP_LOGI(TAG, "TX direction=%s id=0x%02x source=0x%04x api=%s age_ms=%" PRIu64,
                     job.direction == EVENT_TO_MESH ? "mesh" : "uart", job.id, job.source,
                     accepted ? "accepted" : "failed", now - job.received_ms);
            /* Give lower-priority work/idle a turn even under continuous overload. */
            vTaskDelay(1);
        }
    }
}

static void uart_rx_task(void *argument)
{
    (void)argument;
    uart_event_t event;
    for (;;) {
        if (xQueueReceive(uart_events, &event, portMAX_DELAY) != pdTRUE) continue;
        if (event.type == UART_DATA) {
            for (size_t i = 0; i < event.size; ++i) {
                uint8_t byte;
                if (uart_read_bytes(DATA_UART, &byte, 1, 0) != 1) break;
                bool ready = mesh_node_ready();
                uint64_t now = now_ms();
                portENTER_CRITICAL(&bridge_lock);
                event_result_t result = event_bridge_uart(&bridge, byte, now, ready);
                portEXIT_CRITICAL(&bridge_lock);
                if (result == EVENT_OK) xTaskNotifyGive(worker);
                /* Per-input rejection is counted; avoid flooding the console here. */
            }
        } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL ||
                   event.type == UART_PARITY_ERR || event.type == UART_FRAME_ERR) {
            portENTER_CRITICAL(&bridge_lock);
            uart_errors++;
            portEXIT_CRITICAL(&bridge_lock);
            uart_flush_input(DATA_UART);
            xQueueReset(uart_events);
        }
        vTaskDelay(1); /* sustained input must not starve idle/watchdog tasks */
    }
}

void bridge_runtime_mesh_rx(const uint8_t *wire, size_t length, uint16_t source,
                            uint16_t own_address)
{
    uint64_t now = now_ms();
    portENTER_CRITICAL(&bridge_lock);
    event_result_t result = event_bridge_mesh(&bridge, wire, length, source, own_address, now);
    portEXIT_CRITICAL(&bridge_lock);
    if (result == EVENT_OK) xTaskNotifyGive(worker);
}

void bridge_runtime_mesh_complete(int error)
{
    portENTER_CRITICAL(&bridge_lock);
    if (error == 0) mesh_async_ok++;
    else mesh_async_failed++;
    portEXIT_CRITICAL(&bridge_lock);
}

void bridge_runtime_log_status(void)
{
    portENTER_CRITICAL(&bridge_lock);
    event_stats_t s = event_bridge_stats(&bridge);
    size_t pending = bridge.count;
    uint32_t async_ok = mesh_async_ok, async_failed = mesh_async_failed, errors = uart_errors;
    portEXIT_CRITICAL(&bridge_lock);
    ESP_LOGI(TAG, "QUEUE pending=%u capacity=32; uart_rx valid=%" PRIu32 " noop=%" PRIu32
             " invalid=%" PRIu32 " hw_errors=%" PRIu32, (unsigned)pending,
             s.uart_valid, s.uart_noop, s.uart_invalid, errors);
    ESP_LOGI(TAG, "MESH_RX valid=%" PRIu32 " invalid=%" PRIu32 " self=%" PRIu32
             " not_ready=%" PRIu32, s.mesh_valid, s.mesh_invalid, s.mesh_self, s.not_ready);
    for (unsigned d = 0; d < EVENT_DIRECTION_COUNT; ++d) {
        ESP_LOGI(TAG, "%s accepted=%" PRIu32 " failed=%" PRIu32 " full=%" PRIu32
                 " expired=%" PRIu32, d == EVENT_TO_MESH ? "MESH_TX" : "UART_TX",
                 s.accepted[d], s.failed[d], s.full[d], s.expired[d]);
    }
    ESP_LOGI(TAG, "MESH_STACK complete_ok=%" PRIu32 " failed=%" PRIu32 " peer_ACK=none",
             async_ok, async_failed);
}

esp_err_t bridge_runtime_init(void)
{
    event_bridge_init(&bridge);
    const uart_config_t config = {
        .baud_rate = 115200, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_param_config(DATA_UART, &config);
    if (err != ESP_OK) return err;
    err = uart_set_pin(DATA_UART, UART_TX_GPIO, UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    err = uart_driver_install(DATA_UART, 256, 0, 16, &uart_events, 0);
    if (err != ESP_OK) return err;
    if (xTaskCreate(worker_task, "event_bridge", 4096, NULL, 5, &worker) != pdPASS) {
        uart_driver_delete(DATA_UART);
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(uart_rx_task, "stm32_uart_rx", 3072, NULL, 4, NULL) != pdPASS) {
        vTaskDelete(worker);
        worker = NULL;
        uart_driver_delete(DATA_UART);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "UART1_READY TX=GPIO17 RX=GPIO18 115200/8N1; no periodic task");
    return ESP_OK;
}
