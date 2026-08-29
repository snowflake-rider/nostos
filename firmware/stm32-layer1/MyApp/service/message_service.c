#include "message_service.h"

#include "alert.h"
#include "audio_service.h"
#include "buzzer.h"

static message_service_status_t service_status = {
    .audio_status = VS1003B_STATUS_INVALID_ARGUMENT,
    .audio_playing = false,
    .audio_position = 0U,
    .buzzer_active = false,
    .buzzer_pattern = BUZZER_PATTERN_NONE,
    .alert_state = ALERT_STATE_OFF,
    .alert_led_on = false,
};
static message_type_t rear_state = MSG_NONE;
static bool emergency_latched = false;

static void message_service_play_audio(message_type_t message)
{
    if (service_status.audio_status == VS1003B_STATUS_OK)
    {
        service_status.audio_status = audio_service_play(message);
    }
}

void message_service_init(vs1003b_status_t initial_audio_status)
{
    alert_init();
    buzzer_init();
    rear_state = MSG_NONE;
    emergency_latched = false;

    service_status.audio_status = initial_audio_status;
    service_status.audio_playing = audio_service_is_playing();
    service_status.audio_position = audio_service_position();
    service_status.buzzer_active = buzzer_is_active();
    service_status.buzzer_pattern = buzzer_get_pattern();
    service_status.alert_state = alert_get_state();
    service_status.alert_led_on = alert_is_led_on();
}

void message_service_handle(message_type_t message)
{
    if (message == MSG_NONE)
    {
        return;
    }

    if ((message == MSG_FALL_DETECTED) || (message == MSG_SOS))
    {
        if (!emergency_latched)
        {
            emergency_latched = true;
            alert_show(message);
            buzzer_play_pattern(BUZZER_PATTERN_EMERGENCY);
            message_service_play_audio(message);
        }

        return;
    }

    /* 긴급 상태는 보드가 리셋되기 전까지 후방 상태로 덮어쓰지 않습니다. */
    if ((message == MSG_REAR_SAFE) || (message == MSG_REAR_WARNING))
    {
        if (emergency_latched)
        {
            return;
        }

        if (message == MSG_REAR_SAFE)
        {
            rear_state = MSG_REAR_SAFE;
            alert_show(message);

            if (buzzer_get_pattern() == BUZZER_PATTERN_REAR_WARNING)
            {
                buzzer_stop();
            }
        }
        else if (rear_state != MSG_REAR_WARNING)
        {
            rear_state = MSG_REAR_WARNING;
            alert_show(message);
            buzzer_play_pattern(BUZZER_PATTERN_REAR_WARNING);
            message_service_play_audio(message);
        }

        return;
    }

    /* 버튼 요청은 LED와 부저를 변경하지 않고 음성만 재생합니다. */
    message_service_play_audio(message);
}

void message_service_process(void)
{
    if (service_status.audio_status == VS1003B_STATUS_OK)
    {
        service_status.audio_status = audio_service_process();
    }

    alert_process();
    buzzer_process();

    service_status.audio_playing = audio_service_is_playing();
    service_status.audio_position = audio_service_position();
    service_status.buzzer_active = buzzer_is_active();
    service_status.buzzer_pattern = buzzer_get_pattern();
    service_status.alert_state = alert_get_state();
    service_status.alert_led_on = alert_is_led_on();
}

const message_service_status_t *message_service_get_status(void)
{
    return &service_status;
}
