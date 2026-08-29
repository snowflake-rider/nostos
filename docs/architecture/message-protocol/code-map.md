# 그림에서 실제 코드로

[목차](README.md) · [전체 흐름 그림](02-one-message.md)

**한 번에 파일 하나만 보세요.** 처음에는 [03의 코드 한 줄](03-message-id.md)부터 읽으면 됩니다.

## ① 버튼 → ② STM32 A

[button.c](../../../firmware/stm32/MyApp/hw/button.c)의 `button_get_message()`가 정지 버튼 눌림을 `MSG_STOP_REQUEST`로 바꿉니다.

현재 BTN4와 D10/PB6 테스트 버튼이 이 ID에 연결됩니다. 이름과 번호는 [message_type.h](../../../libs/protocol/message_type.h)에서 정합니다.

## ② STM32 A → ③ ESP32 A

[app.c](../../../firmware/stm32/MyApp/ap/app.c)에서 [message_router.c](../../../firmware/stm32/MyApp/service/message_router.c)의 `message_router_publish_local()`을 호출합니다.

그 안에서 [uart_service.c](../../../firmware/stm32/MyApp/service/uart_service.c)의 `uart_service_send_message()`를 호출해 ID 한 바이트를 보냅니다. A의 로컬 알림 처리도 수행합니다.

## ③ ESP32 A: 두 바이트 만들기

[event_protocol.c](../../../libs/protocol/event_protocol.c)의 `event_encode()`가 `[01 13]`을 만듭니다.

[bridge_runtime.c](../../../firmware/esp32/main/bridge_runtime.c)와 [mesh_node.c](../../../firmware/esp32/main/mesh_node.c)가 Mesh 전송을 연결합니다.

## ④ ESP32 B: 검사하고 ID 꺼내기

같은 [event_protocol.c](../../../libs/protocol/event_protocol.c)의 `event_decode()`가 길이·버전·ID를 검사합니다. ESP32의 bridge가 꺼낸 ID를 UART로 보냅니다.

## ⑤ STM32 B: 받은 요청 처리하기

UART 수신 → `app_process()` → `message_router_deliver_remote()` → [message_service.c](../../../firmware/stm32/MyApp/service/message_service.c)의 `message_service_handle()`로 이어집니다.

## ⑥ 저장된 음원 → 소리

[audio_service.c](../../../firmware/stm32/MyApp/service/audio_service.c)가 `stop_request_audio_data`를 선택합니다. [vs1003b.c](../../../firmware/stm32/MyApp/hw/vs1003b.c)가 SPI로 오디오 모듈에 음원 데이터를 보냅니다.

---

[목차](README.md) · [상세 규칙](reference.md) · [공통 API 예제](../../../libs/protocol/README.md)
