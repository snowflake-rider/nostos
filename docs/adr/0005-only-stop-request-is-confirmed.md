---
status: accepted
---

# Stop Request만 peer별 수락을 확인한다

STOP_REQUEST만 Confirmed Message로 두어 모든 의도된 peer의 Peer Acceptance와 Partial Delivery를 추적하고 미수락 peer에 재시도한다. peer ESP32는 Stop Request를 paired STM32에 전달하고 STM32의 local STOP_ACK를 받은 뒤 해당 `request_id`의 Mesh STOP_ACK를 발신자에게 보낸다. PACE_REQUEST와 RIDE·ENVIRONMENT Latest State는 peer별 application ACK나 retry가 없는 best-effort group message로 전달한다. 최대 10개 Rider Node에서 평상시 ACK traffic과 delivery state를 제한하기 위한 선택이다.
