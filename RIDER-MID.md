# RIDER-MID

[공통 요구사항](REQUIREMENT.md)과 [공통 장치·배선](PINS.md)을 따르는 B/Mid 노드입니다.

| 차이 | 설정·역할 |
| --- | --- |
| 센서 | DHT11 PA1 장착; 온도·습도 publish |
| STM32 옵션 | `DHT11_SENSOR=ON`, `MPU6050_SENSOR=OFF` 필요 |
| 현재 차이 | 기본 build는 DHT11을 끄므로 노드별 profile과 실물 확인이 필요 |
