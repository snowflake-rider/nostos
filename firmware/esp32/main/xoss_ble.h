#ifndef NOSTOS_XOSS_BLE_H
#define NOSTOS_XOSS_BLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define XOSS_BLE_AD_TYPE_COMPLETE_NAME 0x09U

/* Advertising fields use a one-byte length followed by type and payload.
 * Only a complete local-name field is an acceptable connection identity:
 * shortened, missing, malformed, prefix, and suffix names are rejected. */
static inline bool xoss_ble_complete_name_matches(
    const uint8_t *advertising_data,
    size_t advertising_length,
    const char *configured_name)
{
    if (advertising_data == NULL || configured_name == NULL ||
        configured_name[0] == '\0') {
        return false;
    }

    const size_t configured_length = strlen(configured_name);
    size_t offset = 0U;
    while (offset < advertising_length) {
        const size_t field_length = advertising_data[offset++];
        if (field_length == 0U) return false;
        if (field_length > advertising_length - offset) return false;

        const uint8_t field_type = advertising_data[offset];
        if (field_type == XOSS_BLE_AD_TYPE_COMPLETE_NAME) {
            const size_t name_length = field_length - 1U;
            return name_length == configured_length &&
                memcmp(&advertising_data[offset + 1U], configured_name,
                    configured_length) == 0;
        }
        offset += field_length;
    }
    return false;
}

#ifdef ESP_PLATFORM
#include "esp_err.h"

esp_err_t xoss_ble_init(void);
void xoss_ble_log_status(void);
#endif

#endif /* NOSTOS_XOSS_BLE_H */
