---
status: accepted
---

# UART framing은 한 가지 length-delimited 형식만 사용한다

STM32→ESP32와 ESP32→STM32는 `A5 5A | length:u8 | application_message | CRC16:u16LE` 형식만 사용한다. `A5 5A`는 UART byte stream에서 frame 시작점을 다시 찾는 표식이고 업무 의미는 없으며, 기존 구현 값을 재사용한다. CRC-16/CCITT-FALSE는 `length + application_message`를 검사한다. 별도 UART version, UART 전용 type, delimiter escaping과 두 번째 framing은 두지 않는다. parser는 길이 또는 CRC가 잘못되면 frame을 버리고 다음 start marker를 찾는다.
