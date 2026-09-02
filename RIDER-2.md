# RIDER-2

[공통 요구사항](REQUIREMENT.md)과 [공통 장치·배선](PINS.md)을 따르는 Rider Node 2의 현재 capability profile입니다.

| 차이 | 설정·역할 |
| --- | --- |
| 센서 | DHT11 PA1 장착; 온도·습도 publish |
| STM32 옵션 | `DHT11_SENSOR=ON`, `MPU6050_SENSOR=OFF` 필요 |
| Release variant | `node2-dht11` artifact가 별도로 생성됨; DHT11 실물 확인은 남아 있음 |
