---
status: accepted
---

# Application Header는 두 바이트만 사용한다

모든 Nostos application message는 `type:u8 + source_node_id:u8` 두 바이트 Application Header 뒤에 type별 payload를 둔다. application header에는 boot epoch, session, application sequence, payload length, CRC와 Bluetooth address를 넣지 않는다. 길이와 CRC는 UART transport framing이 담당하고, 실제 Bluetooth Mesh source address는 수신 metadata에서 얻어 source_node_id의 Mesh Address Binding과 비교한다.
