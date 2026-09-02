---
status: accepted
---

# Source 0은 STM32에서 paired ESP32로 가는 로컬 UART에서만 사용한다

STM32는 모든 장비가 공유하는 binary이므로 provisioned Rider Node ID를 소유하지 않는다. STM32→paired ESP32 UART message는 `source_node_id=0`을 local sentinel로 사용한다. 요청은 ESP32가 자신의 검증된 Rider Node ID `1..10`으로 교체한 뒤 Mesh message로 encode한다. 수신 STM32가 보내는 local `STOP_ACK(source=0)`은 paired ESP32가 받은 원격 STOP을 application에서 수락했다는 확인이며 Mesh로 그대로 전달하지 않는다. 일반 application codec과 Mesh RX는 source 0을 거부하고 ESP32→STM32 요청은 실제 source `1..10`을 사용한다. 이를 위해 local-UART API와 Mesh-strict API를 분리하며 별도 identity handshake나 새 message type은 추가하지 않는다.
