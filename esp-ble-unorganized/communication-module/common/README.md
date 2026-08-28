# Common Message Definitions

Event-driven과 Periodic이 함께 사용하는 데이터 정의를 두는 위치다. [comm_message.h](comm_message.h)에 버튼 이벤트와 Head의 평균 속도 메시지를 정의한다.

## 현재 구조

- `comm_message_t.type`: `COMM_MESSAGE_EVENT` 또는 `COMM_MESSAGE_SPEED`.
- `comm_message_t.data.event.code`: 버튼 메시지 1, 2, 3.
- `comm_button_message_t`: 이름으로 사용할 버튼 코드 상수.
- `comm_message_t.data.speed.average_cm_s`: 최근 5개 속도의 평균, cm/s 단위.
- `comm_message_t.data.speed.valid`: false이면 데이터 없음/만료. true일 때만 평균을 사용한다. true와 평균 0은 측정된 정지다.

`data`는 union이다. 먼저 type을 확인한 뒤 그 종류의 필드만 읽는다. 이벤트 큐에는 EVENT만 넣으며 SPEED를 넣으면 거절한다. Periodic은 정상 평균이 준비됐을 때만 valid=true인 SPEED를 전송 계층에 넘긴다. 초기 결측이나 만료를 알리는 무선 메시지 및 수신 타임아웃은 아직 구현하지 않았다.

이 헤더는 HAL, ESP-IDF, FreeRTOS에 의존하지 않는다. 이벤트 큐와 Periodic이 이 헤더를 참조하며, common은 그 구현을 참조하지 않는다.

## 메시지와 실제 패킷의 차이

여기서 정의하는 것은 프로그램 메모리 안의 애플리케이션 메시지다. UART나 Bluetooth로 보낼 바이트 배열은 아니다.

- C enum의 크기와 struct padding에 의존하여 `sizeof(comm_message_t)` 바이트를 그대로 전송하지 않는다.
- UART 프레임의 경계, 길이, 버전, CRC, 바이트 순서, 잘못된 입력 복구는 다음 패킷 인코더/디코더 단계에서 정한다.
- 노드 주소, 그룹, 이벤트 식별자, ACK 정책도 실제 통신 연결 전에 설계한다.
- UART 프레임과 Mesh Model 메시지는 같은 바이트 배열일 필요가 없다.
- 표준 Mesh의 네트워크 헤더와 Relay 규칙을 이 구조에 중복 구현하지 않는다.

현재 헤더는 초기 개발용 API이며 팀 배포용 wire protocol이 확정된 상태는 아니다. 공통 구조가 변경되면 이를 사용하는 코드도 함께 빌드·검증한다.

[상위 개발 범위](../README.md)
