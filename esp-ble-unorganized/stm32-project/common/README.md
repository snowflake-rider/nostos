# Common

팀 전체가 따르는 메시지와 배선 규칙을 관리합니다.

- 공용 펌웨어: `integration/stm32`
- 메시지 정의: `common/protocol/message_type.h`
- 기능 설정: `integration/stm32/MyApp/common/app_config.h`
- USART 핀과 배선: `common/PIN_ASSIGNMENT.md`

공통 메시지 ID나 핀을 변경할 때는 세 보드의 펌웨어와 관련 문서를 함께 수정합니다. 같은 코드를 별도 위치에 복사하지 않고 `common/protocol`의 메시지 계약을 유일한 기준으로 사용합니다.
