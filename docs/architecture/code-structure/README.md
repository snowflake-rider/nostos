# 코드 구조

**어느 폴더의 코드가 무엇을 담당하고, 서로 어떻게 연결되는지** 정리합니다.

[아키텍처 목록](../README.md) · [메시지 프로토콜 공부](../message-protocol/README.md)

## 전체 연결

```text
STM32 입력·판정·출력 ← UART → ESP32 UART/Mesh bridge
       │                              │
       └────── libs/protocol ──────────┘
                                      ↕ Bluetooth Mesh
                                  다른 ESP32 노드
```

## 코드의 위치와 역할

- [firmware/stm32](../../../firmware/stm32/README.md): 장치 제어, 센서 판정, 이벤트 생성·수신 알림. `Core/Drivers/MyApp` 경계를 유지합니다.
- [firmware/esp32](../../../firmware/esp32/README.md): Layer 8 기반 UART1 ↔ Mesh C001 bridge와 USB 콘솔. 독립 ESP-IDF 프로젝트입니다.
- [libs/protocol](../../../libs/protocol/README.md): 양쪽 펌웨어가 사용하는 공통 메시지 계약. 폴더별 복사본을 만들지 않습니다.
- [apps/mesh-console](../../../apps/mesh-console/README.md): PC에서 ESP32 USB 콘솔을 관찰·제어합니다. Mesh 네트워크를 운영하는 서버가 아닙니다.
- [iPhone GPS 앱](../../../apps/ios-gps-mesh/README.md)과 [GPS 참고 노드](../../../experiments/examples/esp32s3/gps-mesh-node/README.md): 별도 GPS 경로입니다. 팀 펌웨어에 통합됐다는 뜻이 아닙니다.

## 코드 사이의 경계

펌웨어의 재사용 코드는 `libs/`로 분리하고, 제품별 구현·설정·단위 테스트는 해당 프로젝트에 둡니다. 여러 구성 요소를 연결하는 시험은 `tests/integration/`에 둡니다.

별도 Communication Module과 SharedState를 UART/Mesh wire 규칙으로 새로 통합한 작업은 기존 이관에 포함되지 않습니다. 이번 문서 정리에서도 구현은 변경하지 않았습니다.

폴더 구조의 선택 이유는 [설계 결정 0001](../../decisions/0001-repository-layout.md), 공유 상태 설명은 [SharedState](../shared-state.md)에서 이어 봅니다.
