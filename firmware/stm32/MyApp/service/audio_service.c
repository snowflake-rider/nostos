#include "audio_service.h"

#include "cheer_up_audio.h"
#include "rear_warning_audio.h"
#include "speed_down_request_audio.h"
#include "speed_up_request_audio.h"
#include "stop_request_audio.h"

typedef struct
{
    const uint8_t *data;
    uint32_t size;
} audio_asset_t;

static audio_asset_t audio_service_find_asset(message_type_t message)
{
    audio_asset_t asset = {0};

    switch (message)
    {
        case MSG_SPEED_DOWN_REQUEST:
            asset.data = speed_down_request_audio_data;
            asset.size = speed_down_request_audio_size;
            break;

        case MSG_SPEED_UP_REQUEST:
            asset.data = speed_up_request_audio_data;
            asset.size = speed_up_request_audio_size;
            break;

        case MSG_SAFETY_REMINDER:
            asset.data = cheer_up_audio_data;
            asset.size = cheer_up_audio_size;
            break;

        case MSG_STOP_REQUEST:
            asset.data = stop_request_audio_data;
            asset.size = stop_request_audio_size;
            break;

        case MSG_REAR_WARNING:
            asset.data = rear_warning_audio_data;
            asset.size = rear_warning_audio_size;
            break;

        case MSG_NONE:
        case MSG_REAR_SAFE:
        case MSG_FALL_DETECTED:
        case MSG_SOS:
        case MSG_UNKNOWN:
        default:
            break;
    }

    return asset;
}

vs1003b_status_t audio_service_play(message_type_t message)
{
    audio_asset_t asset = audio_service_find_asset(message);

    /* 음원이 없는 메시지는 정상적으로 무음 처리합니다. */
    if ((asset.data == NULL) || (asset.size == 0U))
    {
        return VS1003B_STATUS_OK;
    }

    /* 현재 음원을 보내는 중이면 새 요청은 이번 버전에서 무시합니다. */
    if (vs1003b_is_playing())
    {
        return VS1003B_STATUS_OK;
    }

    return vs1003b_play_start(asset.data, asset.size);
}

vs1003b_status_t audio_service_process(void)
{
    return vs1003b_play_process();
}

bool audio_service_is_playing(void)
{
    return vs1003b_is_playing();
}

uint32_t audio_service_position(void)
{
    return vs1003b_play_position();
}
