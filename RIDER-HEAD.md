# RIDER-HEAD

[공통 요구사항](REQUIREMENT.md)과 [공통 장치·배선](PINS.md)을 따르는 A/Head 노드입니다.

| 차이 | 설정·역할 |
| --- | --- |
| 센서 | XOSS CSC 속도 센서; speed와 휠 회전 기반 session distance publish |
| 속도 표시 | 평균 20.62, 표준편차 5.49 km/h의 `-2σ/-1σ/+1σ/+2σ` 경계로 Level 1~5 원을 누적 점등 |
| STM32 옵션 | `MPU6050_SENSOR=OFF`, `DHT11_SENSOR=OFF` |
| 현재 구성 | XOSS BLE driver는 구현됐으나 실물 센서 연결 시험은 남아 있음 |
