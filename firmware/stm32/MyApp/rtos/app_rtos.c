#include "app_rtos.h"

#include "app_runtime.h"
#include "watchdog.h"
#include "stm32f4xx_hal.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#define APP_INPUT_PERIOD_MS 5U
#define APP_SERVICE_PERIOD_MS 1U
#define APP_EVENT_MAX_AGE_MS 1000U
#define APP_URGENT_BURST 4U
#define APP_URGENT_QUEUE_LENGTH 4U
#define APP_NORMAL_QUEUE_LENGTH 12U
#define APP_RESET_NOTIFICATION (1UL << 0)
#define APP_CAL_BUTTON_PRESSED_NOTIFICATION (1UL << 1)
#define APP_CAL_BUTTON_RELEASED_NOTIFICATION (1UL << 2)

#define APP_INPUT_STACK_WORDS 256U
#define APP_SERVICE_STACK_WORDS 1024U
#define APP_WATCHDOG_STACK_WORDS 160U

typedef enum
{
    APP_EVENT_LOCAL_MESSAGE,
    APP_EVENT_REMOTE_MESSAGE,
} app_event_kind_t;

typedef struct
{
    uint32_t received_ms;
    message_type_t message;
    app_event_kind_t kind;
} app_event_t;

static StaticQueue_t urgent_queue_control;
static StaticQueue_t normal_queue_control;
static uint8_t urgent_queue_storage[APP_URGENT_QUEUE_LENGTH * sizeof(app_event_t)];
static uint8_t normal_queue_storage[APP_NORMAL_QUEUE_LENGTH * sizeof(app_event_t)];
static QueueHandle_t urgent_queue;
static QueueHandle_t normal_queue;

static StaticTask_t input_task_control;
static StaticTask_t service_task_control;
static StaticTask_t watchdog_task_control;
static StackType_t input_task_stack[APP_INPUT_STACK_WORDS];
static StackType_t service_task_stack[APP_SERVICE_STACK_WORDS];
static StackType_t watchdog_task_stack[APP_WATCHDOG_STACK_WORDS];
static TaskHandle_t service_task_handle;

static StaticTask_t idle_task_control;
static StackType_t idle_task_stack[configMINIMAL_STACK_SIZE];

static volatile uint32_t input_heartbeat;
static volatile uint32_t service_heartbeat;
static app_rtos_stats_t stats;

static bool event_is_urgent(const app_event_t *event)
{
    return (event != NULL) &&
        (event->message == MSG_FALL_DETECTED);
}

static void enqueue_event(app_event_kind_t kind, message_type_t message)
{
    if ((message == MSG_NONE) || (message == MSG_UNKNOWN))
    {
        return;
    }

    const app_event_t event = {
        .received_ms = HAL_GetTick(),
        .message = message,
        .kind = kind,
    };
    QueueHandle_t queue = event_is_urgent(&event) ? urgent_queue : normal_queue;
    if (xQueueSend(queue, &event, 0U) == pdPASS)
    {
        ++stats.queued;
    }
    else
    {
        ++stats.queue_full;
    }
}

static void input_task(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    bool calibration_button_state_known = false;
    bool calibration_button_last_pressed = false;

    for (;;)
    {
        bool reset_requested = false;
        bool calibration_button_pressed = false;
        message_type_t message = app_runtime_poll_button(
            &reset_requested,
            &calibration_button_pressed);
        if (!calibration_button_state_known)
        {
            calibration_button_state_known = true;
            calibration_button_last_pressed = calibration_button_pressed;
        }
        else if (calibration_button_pressed !=
                 calibration_button_last_pressed)
        {
            calibration_button_last_pressed = calibration_button_pressed;
            uint32_t button_notification = calibration_button_pressed ?
                APP_CAL_BUTTON_PRESSED_NOTIFICATION :
                APP_CAL_BUTTON_RELEASED_NOTIFICATION;
            (void)xTaskNotify(
                service_task_handle,
                button_notification,
                eSetBits);
        }
        if (reset_requested)
        {
            (void)xTaskNotify(service_task_handle, APP_RESET_NOTIFICATION, eSetBits);
        }
        else
        {
            enqueue_event(APP_EVENT_LOCAL_MESSAGE, message);
        }

#if !NOSTOS_PROTOCOL_V2
        message_type_t remote_message = MSG_NONE;
        if (app_runtime_poll_remote(&remote_message))
        {
            enqueue_event(APP_EVENT_REMOTE_MESSAGE, remote_message);
        }
#endif

        ++input_heartbeat;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_INPUT_PERIOD_MS));
    }
}

static bool take_next_event(app_event_t *event, uint32_t *urgent_streak)
{
    UBaseType_t urgent_waiting = uxQueueMessagesWaiting(urgent_queue);
    UBaseType_t normal_waiting = uxQueueMessagesWaiting(normal_queue);

    if ((urgent_waiting > 0U) &&
        ((normal_waiting == 0U) || (*urgent_streak < APP_URGENT_BURST)))
    {
        if (xQueueReceive(urgent_queue, event, 0U) == pdPASS)
        {
            ++(*urgent_streak);
            return true;
        }
    }
    if (xQueueReceive(normal_queue, event, 0U) == pdPASS)
    {
        *urgent_streak = 0U;
        return true;
    }
    if (xQueueReceive(urgent_queue, event, 0U) == pdPASS)
    {
        ++(*urgent_streak);
        return true;
    }
    return false;
}

static void reset_runtime_queues(uint32_t *urgent_streak)
{
    (void)xQueueReset(urgent_queue);
    (void)xQueueReset(normal_queue);
    *urgent_streak = 0U;
    app_runtime_reset();
    ++stats.resets;
}

static void service_task(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t urgent_streak = 0U;

    for (;;)
    {
        uint32_t notification = 0U;
        if (xTaskNotifyWait(
                0U, UINT32_MAX, &notification, 0U) == pdPASS)
        {
            if ((notification & APP_RESET_NOTIFICATION) != 0U)
            {
                reset_runtime_queues(&urgent_streak);
            }
            if ((notification & APP_CAL_BUTTON_RELEASED_NOTIFICATION) != 0U)
            {
                app_runtime_set_calibration_button_pressed(false);
            }
            else if ((notification &
                      APP_CAL_BUTTON_PRESSED_NOTIFICATION) != 0U)
            {
                app_runtime_set_calibration_button_pressed(true);
            }
        }

        app_event_t event;
        if (take_next_event(&event, &urgent_streak))
        {
            uint32_t age_ms = (uint32_t)(HAL_GetTick() - event.received_ms);
            if (age_ms >= APP_EVENT_MAX_AGE_MS)
            {
                ++stats.expired;
            }
            else
            {
                if (event.kind == APP_EVENT_LOCAL_MESSAGE)
                {
                    app_runtime_dispatch_local(event.message);
                }
                else
                {
                    app_runtime_dispatch_remote(event.message);
                }
                ++stats.dispatched;
            }
        }

        app_runtime_process_services();
        ++service_heartbeat;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_SERVICE_PERIOD_MS));
    }
}

static void watchdog_task(void *argument)
{
    (void)argument;
    uint32_t previous_input = input_heartbeat;
    uint32_t previous_service = service_heartbeat;

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(500U));
        uint32_t current_input = input_heartbeat;
        uint32_t current_service = service_heartbeat;
        if ((current_input != previous_input) &&
            (current_service != previous_service))
        {
            watchdog_refresh();
        }
        previous_input = current_input;
        previous_service = current_service;
    }
}

bool app_rtos_start(void)
{
    stats = (app_rtos_stats_t){0};
    input_heartbeat = 0U;
    service_heartbeat = 0U;

    urgent_queue = xQueueCreateStatic(
        APP_URGENT_QUEUE_LENGTH,
        sizeof(app_event_t),
        urgent_queue_storage,
        &urgent_queue_control);
    normal_queue = xQueueCreateStatic(
        APP_NORMAL_QUEUE_LENGTH,
        sizeof(app_event_t),
        normal_queue_storage,
        &normal_queue_control);
    if ((urgent_queue == NULL) || (normal_queue == NULL))
    {
        return false;
    }

    service_task_handle = xTaskCreateStatic(
        service_task,
        "app_owner",
        APP_SERVICE_STACK_WORDS,
        NULL,
        3U,
        service_task_stack,
        &service_task_control);
    TaskHandle_t input_handle = xTaskCreateStatic(
        input_task,
        "app_input",
        APP_INPUT_STACK_WORDS,
        NULL,
        4U,
        input_task_stack,
        &input_task_control);
    TaskHandle_t watchdog_handle = xTaskCreateStatic(
        watchdog_task,
        "app_watchdog",
        APP_WATCHDOG_STACK_WORDS,
        NULL,
        1U,
        watchdog_task_stack,
        &watchdog_task_control);
    if ((service_task_handle == NULL) || (input_handle == NULL) ||
        (watchdog_handle == NULL))
    {
        return false;
    }

    vQueueAddToRegistry(urgent_queue, "app_urgent");
    vQueueAddToRegistry(normal_queue, "app_normal");
    return watchdog_start();
}

const app_rtos_stats_t *app_rtos_get_stats(void)
{
    return &stats;
}

void vApplicationGetIdleTaskMemory(
    StaticTask_t **task_buffer,
    StackType_t **stack_buffer,
    configSTACK_DEPTH_TYPE *stack_size)
{
    *task_buffer = &idle_task_control;
    *stack_buffer = idle_task_stack;
    *stack_size = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;
    nostos_freertos_assert(__FILE__, (uint32_t)__LINE__);
}

void nostos_freertos_assert(const char *file, uint32_t line)
{
    (void)file;
    (void)line;
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
