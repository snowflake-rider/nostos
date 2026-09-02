---
status: accepted
---

# 재부팅은 application runtime을 전부 비운다

ESP32와 STM32는 재부팅할 때 sensor cache, 처리한 request 중복 기록, Stop Ack, 진행 중인 Stop retry, UART parser buffer와 application queue를 복원하지 않는다. 과거 application message를 flash에 보관하거나 다시 실행하지 않으며 dashboard sensor state는 `unknown`에서 새 STATE_UPDATE로 다시 채운다. Bluetooth Mesh key·provisioning 정보와 firmware의 Mesh Address Binding map만 영속하며, Rider Node ID는 재부팅 후 보존된 primary address와 map에서 다시 결정한다.
