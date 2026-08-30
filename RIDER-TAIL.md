# RIDER-TAIL

[공통 요구사항](REQUIREMENT.md)과 [공통 장치·배선](PINS.md)을 따르는 C/Tail 노드입니다.

| 차이 | 설정·역할 |
| --- | --- |
| 센서 | MPU6050 I2C1 PB8/PB9 장착; SSD1306과 버스 공유, 낙상 Yes/No만 publish |
| STM32 옵션 | `MPU6050_SENSOR=ON`, `DHT11_SENSOR=OFF` 필요 |
| 현재 차이 | 원시 accel/gyro는 로컬 낙상 판정 전용이며 Mesh로 공유하지 않음; 실물 확인 필요 |
