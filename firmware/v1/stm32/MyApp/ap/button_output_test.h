#ifndef BUTTON_OUTPUT_TEST_H
#define BUTTON_OUTPUT_TEST_H

#include "message_type.h"
#include "stm32f4xx_hal.h"
#include "vs1003b.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    message_type_t last_message;
    uint32_t press_count;
    vs1003b_status_t audio_status;
    bool audio_playing;
    uint32_t audio_position;
    bool rgb_active;
} button_output_test_status_t;

/* app_init() 뒤에 호출합니다. BUTTON_OUTPUT_TEST 전용이며 제품 규칙이 아닙니다. */
void button_output_test_init(void);
void button_output_test_process(void);
const button_output_test_status_t *button_output_test_get_status(void);

#endif /* BUTTON_OUTPUT_TEST_H */
