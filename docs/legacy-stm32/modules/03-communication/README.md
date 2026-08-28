> 이관 원문: `stm32-project/modules/03-communication/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Module 03 - Communication Experiments

USART와 향후 통신 장치를 독립적으로 시험하는 공간입니다. 현재 공용 펌웨어는 STM32 USART1의 1바이트 메시지 송수신을 지원하며, 구현은 `integration/stm32/MyApp/service`에 있습니다.

현재 단계에서는 STM32 두 대를 USART1로 직접 연결해 시험합니다. 자세한 배선은 [`common/PIN_ASSIGNMENT.md`](../../../common/PIN_ASSIGNMENT.md)를 확인합니다.
