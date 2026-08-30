# NOSTOS 공통 메시지 protocol

STM32와 ESP32가 함께 빌드하는 v2 wire 계약입니다. 두 target은 같은 `nostos_protocol.*`,
`nostos_uart.*`, `nostos_bridge.*`, `nostos_state.*`, `sensor_link.*`를 사용합니다.

## 공유 항목

application에서 공유하는 데이터와 사건은 다음으로 제한합니다.

| 구분 | v2 type | 값 |
| --- | --- | --- |
| 버튼 1 | `10` SPEED_DOWN | payload 없음 |
| 버튼 2 | `11` SPEED_UP | payload 없음 |
| 버튼 3 | `13` STOP | payload 없음 |
| 환경 | `41` ENVIRONMENT | 온도, 습도 |
| 주행 | `44` RIDE | 속도, 누적 바퀴 이동거리 |
| 낙상 | `30/42` FALL/FALL_CLEAR | 사건 발생, 해제 |

`50` HEARTBEAT와 `51` ACK는 내부 상태/제어용입니다. 이 등록표에 없는 type은 codec과 bridge에서
거부합니다. 로컬 ESP32→STM32 sensor link는 `06` RIDE와 identity/session handshake만 전달합니다.

RIDE의 `distance_mm`는 바퀴 회전에서 누적한 이동거리입니다. valid가 false이면 속도와 거리의 wire 값은
모두 0이어야 하며, 수신 상태는 마지막 정상값과 정상값 시각을 보존합니다.

전체 바이트, session/order, queue, freshness 계약은 [V2.md](V2.md)를 따릅니다.

## 2바이트 event framing

`event_protocol.*`, `event_bridge.*`, `message_type.h`도 버튼 1/2/3과 FALL 네 event만 허용합니다.
현재 v2 sensor data나 session 상태를 표현하는 framing은 아닙니다.

## 검사

```sh
bash firmware/tools/fw check protocol
bash firmware/tools/fw test protocol
```

`check`는 strict compile의 빠른 회귀이고 `test`는 sanitizer 빌드입니다. 공통 wire 변경 뒤에는 양쪽
target build와 실물 검증도 필요합니다.
