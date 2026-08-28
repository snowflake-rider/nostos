#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* 별도 시험 빌드에서만 버튼별 RGB/부저/MP3와 USB 진단 로그를 사용합니다. */
#ifndef FEATURE_BUTTON_OUTPUT_TEST
#define FEATURE_BUTTON_OUTPUT_TEST 0
#endif

/* 1: HC-SR04 후방 감지 사용, 0: 관련 센서 접근과 메시지 생성 중지 */
#ifndef FEATURE_ULTRASONIC_SENSOR
#define FEATURE_ULTRASONIC_SENSOR 1
#endif

/* MPU6050 기반 낙차 감지는 공용 기준 기능으로 사용합니다. */
#ifndef FEATURE_FALL_DETECTION
#define FEATURE_FALL_DETECTION 1
#endif

#endif /* APP_CONFIG_H */
