#include "app_config.h"

#if FEATURE_BUTTON_OUTPUT_TEST
#include "output_test.h"

#include "audio_service.h"
#include "button.h"
#include "buzzer.h"
#include "message_router.h"
#include "message_service.h"
#include "rgb_led.h"
#include "uart_service.h"

#include <stdarg.h>
#include <stdio.h>

static UART_HandleTypeDef *log_uart;
static SPI_HandleTypeDef *test_spi;
static uint32_t led_started;
static bool led_active;
static uint32_t status_at;
static bool was_playing;
static uint32_t previous_position;
static uint32_t progress_at;
static bool stalled_reported;
static message_type_t playing_message;
static bool sine_active;
static uint32_t sine_at;
static uint32_t next_probe_position;
static bool message_test_armed;
static uint32_t message_test_armed_at;
static uint32_t message_test_seq;

static const char *audio_status_name(vs1003b_status_t status)
{
    switch (status)
    {
        case VS1003B_STATUS_OK: return "OK";
        case VS1003B_STATUS_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case VS1003B_STATUS_DREQ_TIMEOUT: return "DREQ_TIMEOUT";
        case VS1003B_STATUS_SPI_ERROR: return "SPI_ERROR";
        case VS1003B_STATUS_MODE_MISMATCH: return "MODE_MISMATCH";
        case VS1003B_STATUS_REGISTER_MISMATCH: return "REGISTER_MISMATCH";
        case VS1003B_STATUS_BUSY: return "BUSY";
        default: return "UNKNOWN";
    }
}

/* 진단 실패는 실제 UART 메시지의 재전송이나 상태 변경으로 이어지지 않습니다. */
static void report(const char *format, ...)
{
    if (log_uart == NULL) return;
    char line[192];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length <= 0) return;
    if ((size_t)length >= sizeof(line)) length = (int)sizeof(line) - 1;
    (void)HAL_UART_Transmit(log_uart, (uint8_t *)line, (uint16_t)length, 50U);
}

static void report_status(void)
{
    const message_service_status_t *s = message_service_get_status();
    report("STATUS audio=%s playing=%u pos=%lu dreq=%u led=%u buzzer=%u\r\n",
           audio_status_name(s->audio_status), (unsigned)audio_service_is_playing(),
           (unsigned long)audio_service_position(), (unsigned)vs1003b_is_ready(),
           (unsigned)led_active, (unsigned)buzzer_is_active());
}

/* DREQ LOW 상태에서는 SCI도 보내지 않습니다. 실패한 값은 정상 측정값으로 출력하지 않습니다. */
static void report_codec(const char *phase)
{
    static const uint8_t addresses[] = {0x00U, 0x01U, 0x03U, 0x0BU, 0x05U, 0x08U, 0x09U, 0x04U};
    uint16_t values[8] = {0U};
    for (unsigned i = 0U; i < 8U; ++i)
    {
        if (!vs1003b_is_ready())
        {
            report("CODEC phase=%s unavailable=DREQ_LOW\r\n", phase);
            return;
        }
        vs1003b_status_t status = vs1003b_read_register(addresses[i], &values[i]);
        if (status != VS1003B_STATUS_OK)
        {
            report("CODEC phase=%s reg=0x%02X error=%s\r\n", phase,
                   (unsigned)addresses[i], audio_status_name(status));
            return;
        }
    }
    report("CODEC phase=%s mode=%04X status=%04X clock=%04X vol=%04X audata=%04X hdat0=%04X hdat1=%04X seconds=%u\r\n",
           phase, (unsigned)values[0], (unsigned)values[1], (unsigned)values[2],
           (unsigned)values[3], (unsigned)values[4], (unsigned)values[5],
           (unsigned)values[6], (unsigned)values[7]);
}

static vs1003b_status_t reset_codec(void)
{
    uint16_t mode = 0U, value = 0U;
    vs1003b_status_t status = vs1003b_init(test_spi, &mode);
    /* app_init과 같은 클럭/볼륨. 진단 중 자동 볼륨 증가는 하지 않습니다. */
    const uint8_t addresses[] = {0x03U, 0x0BU};
    const uint16_t values[] = {0x9800U, 0x5050U};
    for (unsigned i = 0U; i < 2U && status == VS1003B_STATUS_OK; ++i)
    {
        status = vs1003b_write_register(addresses[i], values[i]);
        if (status == VS1003B_STATUS_OK)
            status = vs1003b_read_register(addresses[i], &value);
        if (status == VS1003B_STATUS_OK && value != values[i])
            status = VS1003B_STATUS_REGISTER_MISMATCH;
    }
    message_service_init(status);
    rgb_led_off();
    buzzer_stop();
    sine_active = led_active = was_playing = stalled_reported = false;
    previous_position = 0U;
    progress_at = HAL_GetTick();
    playing_message = MSG_NONE;
    next_probe_position = 512U;
    report("CODEC_INIT status=%s mode=%04X\r\n", audio_status_name(status), (unsigned)mode);
    if (status == VS1003B_STATUS_OK) report_codec("reset");
    return status;
}

static void report_message_test(void)
{
    report("MESSAGE_TEST_READY protocol=1 ids=10,11,12,13,20,21,30,31 uart=USART1 seq=%lu build="
           __DATE__ "_" __TIME__ "\r\n", (unsigned long)message_test_seq);
}

static bool message_test_id_valid(uint8_t id)
{
    switch (id)
    {
        case MSG_SPEED_DOWN_REQUEST: case MSG_SPEED_UP_REQUEST:
        case MSG_SAFETY_REMINDER: case MSG_STOP_REQUEST:
        case MSG_REAR_SAFE: case MSG_REAR_WARNING:
        case MSG_FALL_DETECTED: case MSG_SOS: return true;
        default: return false;
    }
}

/* USART2 시험 입력. m 확인 응답 후 1초 안의 ID 한 바이트만 USART1로 보냅니다. */
static void process_command(void)
{
    if (message_test_armed && (uint32_t)(HAL_GetTick() - message_test_armed_at) >= 1000U)
    {
        message_test_armed = false;
        report("MESSAGE_TEST_REJECT reason=TIMEOUT\r\n");
    }
    uint8_t command;
    if (log_uart == NULL || HAL_UART_Receive(log_uart, &command, 1U, 0U) != HAL_OK) return;
    if (message_test_armed)
    {
        message_test_armed = false;
        if (!message_test_id_valid(command))
        {
            report("MESSAGE_TEST_REJECT reason=INVALID_ID\r\n");
            return;
        }
        /* 센서/오디오/LED를 실행하지 않고 실제 기존 UART 전송 경로만 검사합니다. */
        HAL_StatusTypeDef tx = uart_service_send_message((message_type_t)command);
        ++message_test_seq;
        report("MESSAGE_TEST_TX id=0x%02X uart=%s seq=%lu\r\n", (unsigned)command,
               tx == HAL_OK ? "OK" : "ERROR", (unsigned long)message_test_seq);
        return;
    }
    if (command == '?') report_message_test();
    else if (command == 'm')
    {
        message_test_armed = true;
        message_test_armed_at = HAL_GetTick();
        report("MESSAGE_TEST_ARMED timeout_ms=1000\r\n");
    }
    else if (command == 'd') report_codec("manual");
    else if (command == 'r') (void)reset_codec();
    else if (command == 't')
    {
        uint16_t echo = 0U;
        vs1003b_status_t status = reset_codec();
        if (status == VS1003B_STATUS_OK) status = vs1003b_sdi_test(&echo);
        report("SDI_TEST status=%s expected=0820 echo=%04X\r\n", audio_status_name(status), (unsigned)echo);
        (void)reset_codec();
    }
    else if (command == 's')
    {
        vs1003b_status_t status = reset_codec();
        if (status == VS1003B_STATUS_OK) status = vs1003b_sine_test_start();
        report("SINE_START status=%s duration_ms=1000 vol=5050\r\n", audio_status_name(status));
        if (status == VS1003B_STATUS_OK)
        {
            sine_active = true;
            sine_at = HAL_GetTick();
        }
        else (void)reset_codec(); /* 명령이 일부만 전송되어도 시험 모드를 해제합니다. */
    }
}

void output_test_init(UART_HandleTypeDef *debug_uart, SPI_HandleTypeDef *codec_spi)
{
    log_uart = debug_uart;
    test_spi = codec_spi;
    rgb_led_off();
    buzzer_stop();
    led_active = false;
    led_started = status_at = progress_at = HAL_GetTick();
    was_playing = false;
    previous_position = 0U;
    stalled_reported = false;
    playing_message = MSG_NONE;
    sine_active = false;
    next_probe_position = 512U;
    message_test_armed = false;
    message_test_seq = 0U;
    report("OUTPUT_TEST_READY sensors=OFF remote_output=IGNORED uart1=EVENT_BYTES\r\n");
    report_message_test();
    report("MAP 1=RED/UP 2=GREEN/DOWN 3=BLUE/SAFETY 4=WHITE/STOP led_ms=2000 buzzer=2x100ms\r\n");
    report_status();
    report("CODEC_COMMANDS d=registers r=reset t=SDI_test s=sine_1s\r\n");
    report_codec("boot");
}

static void process_button(message_type_t message)
{
    unsigned number;
    bool red = false, green = false, blue = false;
    switch (message)
    {
        case MSG_SPEED_UP_REQUEST: number = 1U; red = true; break;
        case MSG_SPEED_DOWN_REQUEST: number = 2U; green = true; break;
        case MSG_SAFETY_REMINDER: number = 3U; blue = true; break;
        case MSG_STOP_REQUEST: number = 4U; red = green = blue = true; break;
        default: return;
    }

    bool busy = audio_service_is_playing();
    HAL_StatusTypeDef tx = message_router_publish_local(message);
    rgb_led_set(red, green, blue);
    buzzer_play_pattern(BUZZER_PATTERN_REAR_WARNING);
    led_active = true;
    led_started = HAL_GetTick();
    vs1003b_status_t audio = message_service_get_status()->audio_status;
    const char *request = "BLOCKED";
    if (audio == VS1003B_STATUS_OK)
    {
        request = busy ? "BUSY_SKIPPED" : "STARTED";
        if (!busy)
        {
            playing_message = message;
            was_playing = audio_service_is_playing();
            previous_position = audio_service_position();
            progress_at = HAL_GetTick();
            stalled_reported = false;
            next_probe_position = 512U;
        }
    }
    report("BUTTON n=%u id=0x%02X rgb=%u%u%u uart=%s audio=%s status=%s\r\n",
           number, (unsigned)message, (unsigned)red, (unsigned)green, (unsigned)blue,
           tx == HAL_OK ? "OK" : "ERROR", request, audio_status_name(audio));
}

void output_test_process(void)
{
    process_command();
    message_type_t message = button_get_message();
    if (message != MSG_NONE)
    {
        if (!sine_active) process_button(message);
        else report("BUTTON_IGNORED reason=SINE_TEST\r\n");
    }

    /* 원격/센서 이벤트가 로컬 출력 시험에 섞이지 않도록 이번 모드에서만 제외합니다. */
    if (uart_service_get_message(&message))
        report("REMOTE_IGNORED id=0x%02X\r\n", (unsigned)message);

    message_service_process();
    uint32_t now = HAL_GetTick();
    if (sine_active && (uint32_t)(now - sine_at) >= 1000U)
    {
        vs1003b_status_t status = vs1003b_sine_test_stop();
        report("SINE_STOP status=%s\r\n", audio_status_name(status));
        (void)reset_codec(); /* stop 전송 실패여도 XRST로 시험음을 끝냅니다. */
    }
    if (led_active && (uint32_t)(now - led_started) >= 2000U)
    {
        rgb_led_off();
        led_active = false;
        report("RGB_OFF\r\n");
    }

    bool playing = audio_service_is_playing();
    uint32_t position = audio_service_position();
    if (playing && position >= next_probe_position)
    {
        report_codec("stream");
        next_probe_position += 2048U;
    }
    if (position != previous_position)
    {
        previous_position = position;
        progress_at = now;
        stalled_reported = false;
    }
    if (was_playing && !playing)
    {
        vs1003b_status_t status = message_service_get_status()->audio_status;
        /* DATA_DONE은 SPI 데이터 공급 완료이며 실제 음성 청취 성공은 별도입니다. */
        report("AUDIO_%s id=0x%02X bytes=%lu status=%s\r\n",
               status == VS1003B_STATUS_OK ? "DATA_DONE" : "ERROR",
               (unsigned)playing_message, (unsigned long)position, audio_status_name(status));
        report_codec("end");
    }
    if (playing && !stalled_reported && (uint32_t)(now - progress_at) >= 2000U)
    {
        report("AUDIO_STALLED id=0x%02X pos=%lu dreq=%u\r\n",
               (unsigned)playing_message, (unsigned long)position, (unsigned)vs1003b_is_ready());
        stalled_reported = true;
    }
    was_playing = playing;
    if ((uint32_t)(now - status_at) >= 2000U)
    {
        status_at = now;
        report_status();
    }
}
#endif
