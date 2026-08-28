#include "uart_diag.h"
#include <inttypes.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "hal/gpio_ll.h"
#include "soc/uart_periph.h"

#define TAG "LAYER_8_UART"

/* 진단 실패는 로그로만 알린다. 핀 재설정, FIFO 비우기, 리셋은 하지 않는다. */
#define READ_OR_RETURN(operation) do { \
    esp_err_t read_error = (operation); \
    if (read_error != ESP_OK) { \
        ESP_LOGW(TAG, "UART_DIAG_ERROR op=%s err=%s", #operation, esp_err_to_name(read_error)); \
        return; \
    } \
} while (0)

void uart_diag_log_status(uart_port_t port, int tx_gpio, int expected_rx_gpio)
{
    uint32_t baud = 0;
    uart_word_length_t data_bits;
    uart_parity_t parity;
    uart_stop_bits_t stop;
    uart_hw_flowcontrol_t flow;
    size_t buffered = 0;
    gpio_io_config_t tx, expected_rx;
    READ_OR_RETURN(uart_get_baudrate(port, &baud));
    READ_OR_RETURN(uart_get_word_length(port, &data_bits));
    READ_OR_RETURN(uart_get_parity(port, &parity));
    READ_OR_RETURN(uart_get_stop_bits(port, &stop));
    READ_OR_RETURN(uart_get_hw_flow_ctrl(port, &flow));
    READ_OR_RETURN(uart_get_buffered_data_len(port, &buffered));
    READ_OR_RETURN(gpio_get_io_config(tx_gpio, &tx));
    READ_OR_RETURN(gpio_get_io_config(expected_rx_gpio, &expected_rx));

    const uart_periph_sig_t *rx_pin = &uart_periph_signal[port].pins[SOC_UART_RX_PIN_IDX];
    const uart_periph_sig_t *tx_pin = &uart_periph_signal[port].pins[SOC_UART_TX_PIN_IDX];
    int matrix_gpio = gpio_ll_get_in_signal_connected_io(&GPIO, rx_pin->signal);
    int mapped_gpio = matrix_gpio < 0 ? rx_pin->default_gpio : matrix_gpio;
    bool rx_path_enabled = false;
    int mapped_level = -1;
    if (GPIO_IS_VALID_GPIO(mapped_gpio)) {
        gpio_io_config_t mapped;
        READ_OR_RETURN(gpio_get_io_config(mapped_gpio, &mapped));
        rx_path_enabled = mapped.ie && mapped.fun_sel ==
            (uint32_t)(matrix_gpio < 0 ? rx_pin->iomux_func : PIN_FUNC_GPIO);
        mapped_level = mapped.ie ? gpio_get_level(mapped_gpio) : -1;
    }
    const char *tx_route = "other";
    if (tx_gpio == tx_pin->default_gpio && tx.fun_sel == (uint32_t)tx_pin->iomux_func)
        tx_route = "iomux";
    else if (tx.fun_sel == PIN_FUNC_GPIO && tx.sig_out == tx_pin->signal)
        tx_route = "matrix";

    ESP_LOGI(TAG, "UART_DIAG version=1 port=%d baud=%" PRIu32
             " data=%d parity=%s stop=%s flow=%d buffered=%u",
             port, baud, (int)data_bits + 5,
             parity == UART_PARITY_DISABLE ? "none" : parity == UART_PARITY_EVEN ? "even" : "odd",
             stop == UART_STOP_BITS_1 ? "1" : stop == UART_STOP_BITS_1_5 ? "1.5" : "2",
             (int)flow, (unsigned)buffered);
    /* matrix_gpio=-1은 오류가 아니라 native IOMUX 경로일 수 있다. */
    ESP_LOGI(TAG, "UART_DIAG_RX route=%s matrix_gpio=%d mapped_gpio=%d path_enabled=%u level=%d",
             matrix_gpio < 0 ? "iomux" : "matrix", matrix_gpio, mapped_gpio,
             (unsigned)rx_path_enabled, mapped_level);
    ESP_LOGI(TAG, "UART_DIAG_TX gpio=%d route=%s iomux=%" PRIu32
             " output=%u signal=%" PRIu32, tx_gpio, tx_route, tx.fun_sel,
             (unsigned)tx.oe, tx.sig_out);
    /* level은 순간값이다. UART 파형, 전압, 접지 연속성의 증거가 아니다. */
    ESP_LOGI(TAG, "UART_DIAG_PIN gpio=%d level=%d input=%u output=%u pullup=%u pulldown=%u iomux=%" PRIu32,
             expected_rx_gpio, expected_rx.ie ? gpio_get_level(expected_rx_gpio) : -1,
             (unsigned)expected_rx.ie, (unsigned)expected_rx.oe,
             (unsigned)expected_rx.pu, (unsigned)expected_rx.pd, expected_rx.fun_sel);
}
