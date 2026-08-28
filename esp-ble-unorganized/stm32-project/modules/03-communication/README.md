# Module 03 - Communication Experiments

USART와 향후 통신 장치를 독립적으로 시험하는 공간입니다. 현재 공용 펌웨어는 STM32 USART1의 1바이트 메시지 송수신을 지원하며, 구현은 `integration/stm32/MyApp/service`에 있습니다.

현재 단계에서는 STM32 두 대를 USART1로 직접 연결해 시험합니다. 자세한 배선은 [`common/PIN_ASSIGNMENT.md`](../../common/PIN_ASSIGNMENT.md)를 확인합니다.
