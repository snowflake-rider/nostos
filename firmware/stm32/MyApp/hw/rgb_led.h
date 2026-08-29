#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdbool.h>

/*
 * CubeMX의 MX_GPIO_Init() 실행 후 호출합니다.
 * RGB LED의 모든 채널을 OFF 상태로 만듭니다.
 */
void rgb_led_init(void);

/*
 * 각 채널의 ON/OFF 상태를 지정합니다.
 *
 * red   = true: Red ON
 * green = true: Green ON
 * blue  = true: Blue ON
 */
void rgb_led_set(bool red, bool green, bool blue);

/* 모든 채널을 끕니다. */
void rgb_led_off(void);

#endif /* RGB_LED_H */