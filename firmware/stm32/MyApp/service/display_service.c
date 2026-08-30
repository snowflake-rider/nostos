/* Compact NOSTOS dashboard; data sources are connected independently. */
#include "display_service.h"

#include "app_config.h"
#include "message_type.h"
#include "sensor_view_service.h"
#include "ssd1306.h"

#include <stdint.h>

#define DISPLAY_PERIOD_MS 200U
#define DISPLAY_RETRY_MS 2000U
#define DISPLAY_WIDTH 128U
#define DISPLAY_CELL_WIDTH 64U
#define DISPLAY_SPEED_LEVEL_FIRST_X 58U
#define DISPLAY_SPEED_LEVEL_STEP_X 7U
#define DISPLAY_SPEED_LEVEL_WIDTH 5U
#define DISPLAY_SPEED_LEVEL_HEIGHT 5U
#define DISPLAY_SPEED_LEVEL_COUNT 10U
#define DISPLAY_SPEED_TRANSITION_COUNT 9U
#define DISPLAY_SPEED_LEVEL_Y 2U
#define DISPLAY_SPEED_PULSE_MS 400U
#define DISPLAY_FIRST_ROW_Y 12U
#define DISPLAY_SECOND_ROW_Y 23U
#define DISPLAY_TICKER_Y 39U
#define DISPLAY_MESSAGE_Y 55U
#define DISPLAY_TICKER_STEP_PX 2U
#define DISPLAY_TICKER_GAP_PX 18U
#define DISPLAY_FALL_BLINK_MS 400U
#define DISPLAY_FALL_TEXT_Y 50U
#define DISPLAY_PROGRESS_X 4U
#define DISPLAY_PROGRESS_Y 42U
#define DISPLAY_PROGRESS_WIDTH 120U
#define DISPLAY_PROGRESS_HEIGHT 10U
#define DISPLAY_PROGRESS_INNER_WIDTH 116U
#define DISPLAY_PROGRESS_MAX 1000U
#define DISPLAY_HOLD_MAX_MS 3000U
#define DISPLAY_CALIBRATION_STABLE_TENTHS 20U

static const char ticker_text[] =
    "Have an amazing ride! Remember, safety first.";

/* FHWA adult free-flow observations (N=443): mean 20.62, SD 5.49 km/h.
 * Transitions run from mean-2SD to mean+2SD in 0.5SD steps, rounded to 0.1 km/h.
 * The approximately mean+4SD value, 42.6 km/h, is context rather than a cap. */
static const uint16_t speed_level_thresholds[DISPLAY_SPEED_TRANSITION_COUNT] = {
    96U, 124U, 151U, 179U, 206U, 234U, 261U, 289U, 316U
};

static I2C_HandleTypeDef *display_i2c;
static bool display_ready;
static uint32_t display_tick;
static uint8_t last_button_sender;
static message_type_t last_button_type;
static bool fall_active;
static bool link_ready;
static uint16_t ticker_scroll_px;
static display_calibration_state_t calibration_state;
static uint16_t calibration_progress_permille;
static uint32_t calibration_hold_elapsed_ms;

uint8_t speed_level_from_kmh_x10(bool valid, uint16_t kmh_x10)
{
    if (!valid || kmh_x10 == 0U) return 0U;

    uint8_t level = 1U;
    while (level <= DISPLAY_SPEED_TRANSITION_COUNT &&
        kmh_x10 >= speed_level_thresholds[level - 1U]) {
        ++level;
    }
    return level;
}

#if FEATURE_SSD1306_DISPLAY
static char *append_text(char *out, const char *text)
{
    while (*text != '\0') *out++ = *text++;
    return out;
}

static char *append_u32(char *out, uint32_t value)
{
    char reversed[10];
    uint8_t count = 0U;
    do
    {
        reversed[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(reversed)));
    while (count != 0U) *out++ = reversed[--count];
    return out;
}

static char *append_u32_x10(char *out, uint32_t value)
{
    out = append_u32(out, value / 10U);
    *out++ = '.';
    *out++ = (char)('0' + (value % 10U));
    return out;
}

static char *append_u32_x1000(char *out, uint32_t value)
{
    out = append_u32(out, value / 1000U);
    *out++ = '.';
    value %= 1000U;
    *out++ = (char)('0' + (value / 100U));
    *out++ = (char)('0' + ((value / 10U) % 10U));
    *out++ = (char)('0' + (value % 10U));
    return out;
}

static char *append_u16_x10(char *out, uint16_t value)
{
    return append_u32_x10(out, value);
}

static char *append_i16_x10(char *out, int16_t value)
{
    int32_t wide = value;
    if (wide < 0)
    {
        *out++ = '-';
        wide = -wide;
    }
    return append_u16_x10(out, (uint16_t)wide);
}

static void draw_text_centered(
    uint8_t left,
    uint8_t width,
    uint8_t y,
    const char *text)
{
    uint16_t text_width = ssd1306_text_width(text);
    uint8_t x = left;
    if (text_width < width) {
        x = (uint8_t)(left + (width - text_width) / 2U);
    }
    ssd1306_draw_text(x, y, text);
}

static void render_button_message(void)
{
    if (last_button_sender == 0U) {
        if (link_ready) {
            ssd1306_draw_text(0U, DISPLAY_MESSAGE_Y, "ESP32 READY");
        }
        return;
    }

    const char *sender;
    if (last_button_sender == 1U) sender = "FRONT";
    else if (last_button_sender == 2U) sender = "REAR";
    else if (last_button_sender == 3U) sender = "CENTER";
    else return;

    const char *action;
    if (last_button_type == MSG_SPEED_UP_REQUEST) action = "ACCELERATE";
    else if (last_button_type == MSG_SPEED_DOWN_REQUEST) action = "DECEL";
    else if (last_button_type == MSG_STOP_REQUEST) action = "STOP";
    else return;

    char line[24];
    char *out = line;
    out = append_text(out, sender);
    out = append_text(out, ": ");
    out = append_text(out, action);
    *out = '\0';
    ssd1306_draw_text(0U, DISPLAY_MESSAGE_Y, line);
}

static void render_ticker(void)
{
    uint16_t text_width = ssd1306_text_width(ticker_text);
    uint16_t loop_width =
        (uint16_t)(DISPLAY_WIDTH + text_width + DISPLAY_TICKER_GAP_PX);
    int16_t x = (int16_t)DISPLAY_WIDTH - (int16_t)ticker_scroll_px;
    ssd1306_draw_text_scrolled(x, DISPLAY_TICKER_Y, ticker_text);
    ticker_scroll_px = (uint16_t)(ticker_scroll_px + DISPLAY_TICKER_STEP_PX);
    if (ticker_scroll_px >= loop_width) ticker_scroll_px = 0U;
}

static void render_dashboard(
    const sensor_snapshot_t *sensors,
    uint32_t now_ms)
{
    char line[24];
    bool environment_valid =
        sensors->environment.quality == SENSOR_QUALITY_VALID;
    bool ride_valid =
        sensors->ride.quality == SENSOR_QUALITY_VALID;

    ssd1306_clear();
    ssd1306_fill_rect(0U, 0U, DISPLAY_WIDTH, 10U);
    ssd1306_draw_text_inverted(0U, 1U, "NOSTOS");

    uint8_t speed_level = speed_level_from_kmh_x10(
        ride_valid, sensors->ride.kmh_x10);
    bool pulse_outline = speed_level != 0U &&
        ((now_ms / DISPLAY_SPEED_PULSE_MS) & 1U) != 0U;
    for (uint8_t index = 0U; index < DISPLAY_SPEED_LEVEL_COUNT; ++index)
    {
        uint8_t x = (uint8_t)(DISPLAY_SPEED_LEVEL_FIRST_X +
            index * DISPLAY_SPEED_LEVEL_STEP_X);
        bool active = index < speed_level;
        bool pulse_cell = active && index == (uint8_t)(speed_level - 1U) &&
            pulse_outline;
        if (active && !pulse_cell) {
            ssd1306_clear_rect(x, DISPLAY_SPEED_LEVEL_Y,
                DISPLAY_SPEED_LEVEL_WIDTH, DISPLAY_SPEED_LEVEL_HEIGHT);
        } else {
            ssd1306_draw_rect_inverted(x, DISPLAY_SPEED_LEVEL_Y,
                DISPLAY_SPEED_LEVEL_WIDTH, DISPLAY_SPEED_LEVEL_HEIGHT);
        }
    }
    ssd1306_draw_hline(0U, 10U, DISPLAY_WIDTH);
    ssd1306_draw_hline(0U, 21U, DISPLAY_WIDTH);
    ssd1306_draw_hline(0U, 32U, DISPLAY_WIDTH);
    ssd1306_fill_rect(63U, 11U, 1U, 21U);

    char *out;
    if (ride_valid)
    {
        out = line;
        out = append_u16_x10(out, sensors->ride.kmh_x10);
        out = append_text(out, "km/h");
        *out = '\0';
        draw_text_centered(
            0U,
            DISPLAY_CELL_WIDTH,
            DISPLAY_FIRST_ROW_Y,
            line);
    }

    if (ride_valid)
    {
        out = line;
        uint32_t distance_km_x1000 = sensors->ride.distance_mm / 1000U;
        out = append_u32_x1000(out, distance_km_x1000);
        out = append_text(out, "km");
        *out = '\0';
        draw_text_centered(
            DISPLAY_CELL_WIDTH,
            DISPLAY_CELL_WIDTH,
            DISPLAY_FIRST_ROW_Y,
            line);
    }

    if (environment_valid)
    {
        out = line;
        out = append_i16_x10(out, sensors->environment.temperature_c_x10);
        out = append_text(out, "\xC2\xB0" "C");
        *out = '\0';
        draw_text_centered(
            0U,
            DISPLAY_CELL_WIDTH,
            DISPLAY_SECOND_ROW_Y,
            line);
    }

    if (environment_valid)
    {
        out = line;
        out = append_u16_x10(out, sensors->environment.humidity_pct_x10);
        out = append_text(out, "%");
        *out = '\0';
        draw_text_centered(
            DISPLAY_CELL_WIDTH,
            DISPLAY_CELL_WIDTH,
            DISPLAY_SECOND_ROW_Y,
            line);
    }

    render_ticker();
    ssd1306_draw_hline(0U, 52U, DISPLAY_WIDTH);
    render_button_message();
}

static void render_fall_warning(uint32_t now_ms)
{
    ssd1306_clear();

    /* Two nested outlines make the warning triangle visible at a glance. */
    ssd1306_draw_line(64U, 1U, 2U, 62U);
    ssd1306_draw_line(64U, 1U, 125U, 62U);
    ssd1306_draw_line(2U, 62U, 125U, 62U);
    ssd1306_draw_line(64U, 4U, 6U, 60U);
    ssd1306_draw_line(64U, 4U, 121U, 60U);
    ssd1306_draw_line(6U, 60U, 121U, 60U);

    ssd1306_fill_rect(61U, 16U, 6U, 21U);
    ssd1306_fill_rect(61U, 40U, 6U, 6U);
    draw_text_centered(0U, DISPLAY_WIDTH, DISPLAY_FALL_TEXT_Y,
        "FALL DETECTED!");

    if (((now_ms / DISPLAY_FALL_BLINK_MS) & 1U) != 0U) {
        ssd1306_invert();
    }
}

static void render_progress_gauge(uint16_t progress_permille)
{
    ssd1306_draw_rect(
        DISPLAY_PROGRESS_X,
        DISPLAY_PROGRESS_Y,
        DISPLAY_PROGRESS_WIDTH,
        DISPLAY_PROGRESS_HEIGHT);

    uint8_t filled_width = (uint8_t)(
        ((uint32_t)DISPLAY_PROGRESS_INNER_WIDTH * progress_permille) /
        DISPLAY_PROGRESS_MAX);
    if (filled_width != 0U) {
        ssd1306_fill_rect(
            (uint8_t)(DISPLAY_PROGRESS_X + 2U),
            (uint8_t)(DISPLAY_PROGRESS_Y + 2U),
            filled_width,
            (uint8_t)(DISPLAY_PROGRESS_HEIGHT - 4U));
    }
}

static void render_calibration(void)
{
    char line[16];
    char *out;

    ssd1306_clear();
    switch (calibration_state)
    {
        case DISPLAY_CALIBRATION_INIT:
            draw_text_centered(0U, DISPLAY_WIDTH, 6U, "Calibration");
            draw_text_centered(0U, DISPLAY_WIDTH, 22U, "Initialization");
            draw_text_centered(0U, DISPLAY_WIDTH, 42U, "PREPARING SENSOR");
            break;

        case DISPLAY_CALIBRATION_RUNNING:
            draw_text_centered(0U, DISPLAY_WIDTH, 2U, "CALIBRATING");
            draw_text_centered(0U, DISPLAY_WIDTH, 15U, "KEEP BIKE UPRIGHT");
            draw_text_centered(0U, DISPLAY_WIDTH, 27U, "AND HOLD IT STILL");
            render_progress_gauge(calibration_progress_permille);
            out = line;
            uint32_t remaining_tenths =
                (((uint32_t)DISPLAY_PROGRESS_MAX -
                  calibration_progress_permille) *
                 DISPLAY_CALIBRATION_STABLE_TENTHS +
                 (DISPLAY_PROGRESS_MAX - 1U)) /
                DISPLAY_PROGRESS_MAX;
            out = append_u32_x10(out, remaining_tenths);
            out = append_text(out, "s LEFT");
            *out = '\0';
            draw_text_centered(0U, DISPLAY_WIDTH, 55U, line);
            break;

        case DISPLAY_CALIBRATION_SUCCESS:
            draw_text_centered(0U, DISPLAY_WIDTH, 8U, "CAL OK");
            draw_text_centered(0U, DISPLAY_WIDTH, 27U, "FALL DETECTION");
            draw_text_centered(0U, DISPLAY_WIDTH, 42U, "READY");
            break;

        case DISPLAY_CALIBRATION_REQUIRED:
            draw_text_centered(0U, DISPLAY_WIDTH, 2U, "CAL REQUIRED");
            draw_text_centered(0U, DISPLAY_WIDTH, 16U, "KEEP BIKE STILL");
            draw_text_centered(0U, DISPLAY_WIDTH, 30U, "HOLD BUTTON 1");
            draw_text_centered(0U, DISPLAY_WIDTH, 44U, "FOR 3 SECONDS");
            break;

        case DISPLAY_CALIBRATION_HOLDING:
            draw_text_centered(0U, DISPLAY_WIDTH, 4U, "CAL REQUIRED");
            draw_text_centered(0U, DISPLAY_WIDTH, 18U, "HOLD BUTTON 1");
            render_progress_gauge(calibration_progress_permille);
            out = line;
            out = append_u32_x10(out,
                calibration_hold_elapsed_ms / 100U);
            *out++ = 's';
            *out = '\0';
            draw_text_centered(0U, DISPLAY_WIDTH, 55U, line);
            break;

        case DISPLAY_CALIBRATION_READY:
        default:
            break;
    }
}

static void render_screen(void)
{
    uint32_t now_ms = HAL_GetTick();
    if (fall_active) {
        render_fall_warning(now_ms);
        return;
    }
    if (calibration_state != DISPLAY_CALIBRATION_READY) {
        render_calibration();
        return;
    }

    sensor_view_snapshot_t view = {0};
    if (!sensor_view_service_snapshot(now_ms, &view)) {
        view = (sensor_view_snapshot_t){0};
    }
    render_dashboard(&view.sensors, now_ms);
}

static bool initialize_and_render(void)
{
    if (!ssd1306_init(display_i2c)) return false;
    render_screen();
    return ssd1306_update();
}
#endif

void display_service_init(I2C_HandleTypeDef *i2c)
{
    display_i2c = i2c;
    last_button_sender = 0U;
    last_button_type = MSG_NONE;
    fall_active = false;
    link_ready = false;
    ticker_scroll_px = 0U;
    /* app_init may select CAL INIT before the OLED's first rendered frame. */
#if FEATURE_SSD1306_DISPLAY
    display_ready = (i2c != NULL) && initialize_and_render();
#else
    display_ready = false;
#endif
    display_tick = HAL_GetTick();
}

void display_service_process(void)
{
#if FEATURE_SSD1306_DISPLAY
    uint32_t now = HAL_GetTick();
    uint32_t period = display_ready ? DISPLAY_PERIOD_MS : DISPLAY_RETRY_MS;
    if ((uint32_t)(now - display_tick) < period) return;
    display_tick = now;

    if (!display_ready)
    {
        display_ready = initialize_and_render();
        return;
    }

    render_screen();
    display_ready = ssd1306_update();
#endif
}

bool display_service_is_ready(void)
{
    return display_ready;
}

void display_service_set_link_ready(bool ready)
{
    link_ready = ready;
}

void display_service_reset_outputs(void)
{
    last_button_sender = 0U;
    last_button_type = MSG_NONE;
    fall_active = false;
}

void display_service_set_fall(bool active)
{
    fall_active = active;
}

void display_service_set_calibration(
    display_calibration_state_t state,
    uint16_t progress_permille,
    uint32_t hold_elapsed_ms)
{
    if (state > DISPLAY_CALIBRATION_HOLDING) {
        state = DISPLAY_CALIBRATION_READY;
    }
    if (progress_permille > DISPLAY_PROGRESS_MAX) {
        progress_permille = DISPLAY_PROGRESS_MAX;
    }
    if (hold_elapsed_ms > DISPLAY_HOLD_MAX_MS) {
        hold_elapsed_ms = DISPLAY_HOLD_MAX_MS;
    }

    calibration_state = state;
    calibration_progress_permille = progress_permille;
    calibration_hold_elapsed_ms = hold_elapsed_ms;
}

bool display_service_show_button_message(uint8_t sender_id, uint8_t type)
{
    if (sender_id < 1U || sender_id > 3U ||
        (type != (uint8_t)MSG_SPEED_UP_REQUEST &&
         type != (uint8_t)MSG_SPEED_DOWN_REQUEST &&
         type != (uint8_t)MSG_STOP_REQUEST)) {
        return false;
    }
    last_button_sender = sender_id;
    last_button_type = (message_type_t)type;
    return true;
}
