---
status: accepted
---

# Application message는 네 종류로 제한한다

Nostos application message를 `STATE_UPDATE`, `PACE_REQUEST`, `STOP_REQUEST`, `STOP_ACK` 네 종류로 제한한다. Button 1과 Button 2는 별도 message type을 만들지 않고 하나의 PACE_REQUEST에 `ACCELERATE` 또는 `DECELERATE` action을 넣는다. STOP_ACK는 새 handshake type을 추가하지 않고 두 연결 구간에서 재사용한다. STM32↔paired ESP32에서는 local component가 Stop Request를 수락했음을, ESP32↔Mesh peer에서는 상대 STM32 application까지 수락했음을 같은 `request_id`로 확인한다. 어느 ACK도 audio·OLED 같은 물리 출력 완료를 뜻하지 않는다.
