#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* 1: HC-SR04 후방 감지 사용, 0: 관련 센서 접근과 메시지 생성 중지 */
#ifndef FEATURE_ULTRASONIC_SENSOR
#define FEATURE_ULTRASONIC_SENSOR 1
#endif

/* 현재 연결 보드에는 MPU6050이 없습니다. 센서 장착 빌드에서만 1로 설정합니다. */
#ifndef FEATURE_FALL_DETECTION
#define FEATURE_FALL_DETECTION 0
#endif

/* 1: 버튼→RGB→오디오 코덱 독립 진단. 제품 빌드 기본값은 반드시 0입니다. */
#ifndef FEATURE_BUTTON_OUTPUT_TEST
#define FEATURE_BUTTON_OUTPUT_TEST 0
#endif

/* SSD1306 128x64 OLED를 I2C1(PB8/PB9)에서 갱신합니다. */
#ifndef FEATURE_SSD1306_DISPLAY
#define FEATURE_SSD1306_DISPLAY 1
#endif

/* 현재 연결 보드에는 DHT11이 없습니다. 센서 장착 빌드에서만 1로 설정합니다. */
#ifndef FEATURE_DHT11_SENSOR
#define FEATURE_DHT11_SENSOR 0
#endif

#ifndef SSD1306_I2C_ADDRESS
#define SSD1306_I2C_ADDRESS 0x3CU
#endif

#ifndef DHT11_DATA_PORT
#define DHT11_DATA_PORT GPIOA
#endif

#ifndef DHT11_DATA_PIN
#define DHT11_DATA_PIN GPIO_PIN_1
#endif

#endif /* APP_CONFIG_H */
