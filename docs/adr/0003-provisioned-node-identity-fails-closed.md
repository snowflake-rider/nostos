---
status: accepted
---

# Provisioned primary address로 Rider Node ID를 결정하고 실패 시 차단한다

Bluetooth Mesh stack이 보존한 provisioned primary address와 firmware의 `source1..source10` address map을 함께 Rider Node ID의 권한 있는 기준으로 사용한다. `0`은 미할당이다. Application은 Rider Node ID를 별도 NVS 값으로 중복 저장하지 않고 부팅할 때 primary address를 map에 대입해 `1..10` ID를 다시 결정한다. Address가 누락·중복되거나 map에 없거나 active publisher와 연결되지 않으면 노드는 unbound 상태로 남아 진단과 provisioning만 허용하며 NOSTOS application message 송수신을 차단한다. 실제 장비와 primary address의 대응은 Git에서 제외한 local board inventory에도 기록한다.
