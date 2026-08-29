#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* 1: HC-SR04 후방 감지 사용, 0: 관련 센서 접근과 메시지 생성 중지 */
#ifndef FEATURE_ULTRASONIC_SENSOR
#define FEATURE_ULTRASONIC_SENSOR 1
#endif

/* MPU6050 기반 낙차 감지는 공용 기준 기능으로 사용합니다. */
#ifndef FEATURE_FALL_DETECTION
#define FEATURE_FALL_DETECTION 1
#endif

/* 현재 실물 STM32는 primary 0x0005인 D6/source2의 UART에 연결됩니다. */
#ifndef NOSTOS_V2_LOCAL_SOURCE
#define NOSTOS_V2_LOCAL_SOURCE 2U
#endif

/* 모든 STM32가 함께 변경되는 명시적 배포 epoch입니다. 자동 승인 값이 아닙니다. */
#ifndef NOSTOS_V2_DEPLOYMENT_SESSION
#define NOSTOS_V2_DEPLOYMENT_SESSION 1U
#endif

#endif /* APP_CONFIG_H */
