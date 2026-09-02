---
status: accepted
---

# Confirmed Message는 group-first로 전달한다

Confirmed Message는 최초에 Bluetooth Mesh group address로 한 번 전송한다. 각 의도된 peer는 paired STM32 application이 요청을 수락한 뒤 발신자에게 Peer Acceptance를 unicast로 반환한다. peer ESP32의 UART 전송 완료만으로는 수락으로 보지 않는다. 이후 전달 재시도는 아직 수락하지 않은 peer에만 같은 message identity로 unicast한다. 이 방식은 최초 broadcast의 효율을 유지하면서 peer별 Partial Delivery를 관찰하고, 이미 수락한 peer에 대한 불필요한 반복 전송을 줄인다.
