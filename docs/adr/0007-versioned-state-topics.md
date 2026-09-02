---
status: superseded by ADR-0018
---

# 센서 확장은 Versioned State Topic으로 한다

Latest State는 `Rider Node ID + Topic ID`별로 관리한다. 각 Topic ID는 명시된 단위와 고정 binary payload를 가지며 첫 Schema Revision은 `1`이다. 새 센서는 공통 message envelope를 바꾸지 않고 새 Topic ID로 추가하고, 기존 topic의 구조나 단위가 호환되지 않게 바뀔 때만 해당 Schema Revision을 올린다. 수신자가 모르는 Topic ID나 Schema Revision은 안전하게 무시한다. Schema Revision은 전체 protocol version이나 firmware version과 별개다.

Bluetooth Mesh source address는 application payload의 영구 정체성이 아니라 transport metadata다. 수신 ESP32는 provisioning의 Mesh Address Binding으로 실제 source address를 Rider Node ID에 대응시키고, binding이 일치하는 message만 STM32와 application state에 전달한다.
