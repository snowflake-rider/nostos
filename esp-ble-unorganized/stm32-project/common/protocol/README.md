# API를 짧은 예제로 이해하기

[전체 흐름부터 보기](../../integration/esp32-s3/README.md)

**API는 다른 코드가 가져다 쓰도록 만들어 둔 함수입니다.**
예를 들어 “정지 요청을 전송할 바이트로 바꿔 줘”라는 일을 `event_encode()`에 맡길 수 있습니다.

## 1. 가장 작은 예제: 정지 요청을 포장하고 다시 읽기

이 예제는 **컴퓨터에서 바이트 변환만** 합니다. 보드나 Bluetooth는 필요 없습니다.

```c
#include <stdio.h>
#include "event_protocol.h"

int main(void)
{
    uint8_t packet[EVENT_WIRE_SIZE];

    // 정지 요청을 Mesh에서 사용할 2바이트로 바꿉니다.
    if (!event_encode(MSG_STOP_REQUEST, packet, sizeof(packet))) {
        return 1;
    }
    printf("Mesh payload: %02X %02X\n",
           (unsigned)packet[0], (unsigned)packet[1]);

    // 받은 바이트에서 원래 메시지 번호를 꺼냅니다.
    uint8_t received;
    if (!event_decode(packet, sizeof(packet), &received)) {
        return 1;
    }
    printf("STM32 ID: %02X\n", (unsigned)received);
    return 0;
}
```

출력:

```text
Mesh payload: 01 13
STM32 ID: 13
```

- `MSG_STOP_REQUEST`: 정지 요청의 이름입니다. 실제 번호는 `0x13`입니다.
- `event_encode()`: 번호 앞에 버전 `01`을 붙여 줍니다.
- `event_decode()`: 버전·길이·번호가 맞는지 확인하고 원래 번호를 꺼냅니다.
- 변환할 수 없으면 함수가 `false`를 반환합니다.

Mac에서 실행하려면 위 코드를 저장소 루트의 `event_example.c`로 저장하고 다음을 실행합니다. C 컴파일러가 필요합니다.

```sh
cc -std=c11 -Icommon/protocol \
  event_example.c common/protocol/event_protocol.c -o event_example
./event_example
```

여기서 성공해도 실제 UART나 무선으로 보낸 것은 아닙니다. `printf`는 화면에 결과를 보여 줄 뿐입니다.

## 2. 큐 API는 언제 쓰나요?

메시지가 바로 처리되지 못할 때 잠깐 보관하는 데 씁니다. 현재 ESP32 코드가 아래 과정을 이미 수행합니다.

| 함수 | 쉽게 말하면 |
| --- | --- |
| `event_bridge_uart()` | STM32가 보낸 메시지를 줄에 넣기 |
| `event_bridge_mesh()` | 다른 라이더가 보낸 메시지를 줄에 넣기 |
| `event_bridge_next()` | 앞에 있는 메시지 하나 꺼내기 |
| `event_job_send()` | 정해진 방향으로 전송 함수 호출하기 |
| `event_bridge_complete()` | 전송 함수의 성공·실패 기록하기 |

예를 들어 STM32에서 정지 요청이 들어오면:

```text
event_bridge_uart() → 큐에서 대기 → event_bridge_next() → event_job_send()
```

`event_bridge_next()`는 꺼낸 항목을 큐에서 제거합니다. 전송 함수가 실패해도 현재 코드는 다시 넣거나 자동으로 재전송하지 않습니다.

## 3. 팀원이 꼭 지킬 약속

- 센서 읽기·판정은 **STM32 쪽**에서 합니다. ESP32는 메시지를 전달합니다.
- STM32의 UART에는 메시지 번호 **1바이트만** 보냅니다. `"13"`이라는 글자 두 개나 줄바꿈을 보내면 안 됩니다.
- 메시지 번호는 공통 [message_type.h](message_type.h)를 함께 씁니다. 각자 다른 번호를 만들지 않습니다.
- 컴퓨터 예제와 달리 여러 Task가 같은 큐를 쓰면 보호가 필요합니다. 실제 ESP32에서는 이미 [bridge_runtime.c](../../integration/esp32-s3/main/bridge_runtime.c)가 그 일을 합니다.
- 전송 함수의 성공은 **상대가 받았다는 뜻이 아닙니다**. 실제 시험에서는 상대 STM32의 수신도 확인해야 합니다.

8종 메시지 번호, 큐의 만료 규칙, callback 작성법은 [상세 참고](../../integration/esp32-s3/DETAILS.md#공통-api-상세)에 남겨 두었습니다. 처음에는 위 예제만 이해하면 됩니다.
