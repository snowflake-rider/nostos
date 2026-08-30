#ifndef VS1003B_H
#define VS1003B_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    VS1003B_STATUS_OK = 0,
    VS1003B_STATUS_INVALID_ARGUMENT,
    VS1003B_STATUS_DREQ_TIMEOUT,
    VS1003B_STATUS_SPI_ERROR,
    VS1003B_STATUS_MODE_MISMATCH,
    VS1003B_STATUS_REGISTER_MISMATCH,
    VS1003B_STATUS_BUSY
} vs1003b_status_t;

/*
 * VS1003B를 하드웨어 리셋한 뒤 SCI_MODE 레지스터를 읽어 통신을 검증합니다.
 * 성공하면 mode_value에 SCI_MODE 값(리셋 기본값 0x0800)을 저장합니다.
 */
vs1003b_status_t vs1003b_init(SPI_HandleTypeDef *hspi, uint16_t *mode_value);

/* DREQ가 High인지 즉시 확인합니다. */
bool vs1003b_is_ready(void);

/* DREQ를 기다린 뒤 SCI 레지스터 하나를 읽습니다. */
vs1003b_status_t vs1003b_read_register(uint8_t address, uint16_t *value);

/* DREQ를 기다린 뒤 SCI 레지스터 하나에 16비트 값을 씁니다. */
vs1003b_status_t vs1003b_write_register(uint8_t address, uint16_t value);

/* VS1003B 내장 사인 테스트를 시작하거나 종료합니다. */
vs1003b_status_t vs1003b_sine_test_start(void);
vs1003b_status_t vs1003b_sine_test_stop(void);

/* Flash에 있는 오디오 데이터를 DREQ에 맞춰 비차단 방식으로 재생합니다. */
vs1003b_status_t vs1003b_play_start(const uint8_t *data, uint32_t size);
vs1003b_status_t vs1003b_play_process(void);
/* 현재 스트림을 폐기하고 코덱을 제품 클럭/볼륨으로 다시 준비합니다. */
vs1003b_status_t vs1003b_play_stop(void);
bool vs1003b_is_playing(void);
uint32_t vs1003b_play_position(void);

#endif /* VS1003B_H */
