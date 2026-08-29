# NOSTOS 공통 메시지 protocol

STM32와 ESP32가 함께 빌드하는 메시지 계약입니다. v1 기본 경로는 STM32 UART에서 메시지 ID 1바이트를 사용하고, ESP32 Mesh payload에서는 `[버전, 메시지 ID]` 2바이트를 사용합니다.

## 가장 작은 예제

```c
#include "event_protocol.h"

uint8_t packet[EVENT_WIRE_SIZE];
uint8_t received;

if (!event_encode(MSG_STOP_REQUEST, packet, sizeof(packet))) {
    return 1;
}
if (!event_decode(packet, sizeof(packet), &received)) {
    return 1;
}
```

`MSG_STOP_REQUEST`는 `0x13`이고, 위 packet은 `01 13`입니다. `event_decode()`는 길이·버전·메시지 ID를 검사합니다.

## 구성

| 파일 | 역할 |
| --- | --- |
| `message_type.h` | 공통 메시지 ID |
| `event_protocol.*` | v1 Mesh payload encode/decode |
| `event_bridge.*` | UART↔Mesh 큐와 전송 방향 |
| `nostos_*` | 기본 OFF인 선택형 v2 호환 경로 |

센서 읽기와 사건 판정은 STM32가 담당하고 ESP32는 메시지를 전달합니다. 전송 callback의 성공은 상대 노드 수신을 뜻하지 않으므로 실제 수신 측까지 별도로 확인해야 합니다.

## 검사

```sh
cmake -S firmware/protocol -B /tmp/nostos-protocol -DENABLE_SANITIZERS=ON
cmake --build /tmp/nostos-protocol --parallel
ctest --test-dir /tmp/nostos-protocol --output-on-failure
```

저장소 전체 호스트 검사는 루트에서 `bash firmware/tools/fw test`를 사용합니다.

v2 호환 코드는 현재 v1 기본 설정에서 비활성화되어 있습니다. 계약은 [V2.md](V2.md)에 남아 있으며, 활성화와 정식 배포는 별도 기능 변경·검증 후 새 bundle release로 진행합니다.
