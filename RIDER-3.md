# RIDER-3

[공통 요구사항](REQUIREMENT.md)과 [공통 장치·배선](PINS.md)을 따르는 Rider Node 3의 현재 capability profile입니다.

| 차이 | 설정·역할 |
| --- | --- |
| 센서 | MPU6050 I2C1 PB8/PB9 장착; SSD1306과 버스 공유, 낙상 전이에 `STOP_REQUEST(reason=FALL)` 한 번 생성 |
| STM32 옵션 | `MPU6050_SENSOR=ON`, `DHT11_SENSOR=OFF` 필요 |
| Release variant | `node3-mpu6050` artifact가 별도로 생성됨 |
| 현재 상태 | 원시 accel/gyro와 지속적인 낙상 상태는 로컬 판정 전용; 단일 STOP_REQUEST 구현 완료, 실물 확인 필요 |
