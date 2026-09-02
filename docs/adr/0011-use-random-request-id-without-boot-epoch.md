---
status: accepted
---

# Application boot epoch 대신 Request ID를 사용한다

새 application protocol은 NVS에서 증가시키는 boot epoch, persistent session과 application sequence를 사용하지 않는다. Mesh로 보이는 PACE_REQUEST와 STOP_REQUEST는 매 요청마다 ESP32가 임의의 non-zero 32-bit `request_id`를 생성한다. 공용 STM32가 paired ESP32에 보내는 local STOP_REQUEST는 boot-local non-zero counter를 사용하며, ESP32가 이를 새 Mesh ID에 매핑한다. STOP_ACK는 각 연결 구간에서 받은 Stop Request의 같은 ID를 반환한다. 수신자는 최근 Request ID와 local-to-Mesh 매핑을 RAM에서만 짧게 기억하며 재부팅하면 비운다. Bluetooth Mesh 자체의 sequence number와 replay protection은 Mesh stack에 맡긴다.
