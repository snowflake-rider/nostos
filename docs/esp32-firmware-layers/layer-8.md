# Layer 8 — STM32 UART ↔ Mesh

STM32가 보낸 이벤트를 ESP32가 Mesh로 전달한다.

- STM32와 ESP32는 UART1로 통신한다.
- 받은 Mesh 이벤트는 다시 상대 STM32로 보낸다.
