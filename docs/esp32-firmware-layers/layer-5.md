# Layer 5 — 사용자 패킷

BLE Advertising에 20-byte 사용자 패킷을 넣어 보낸다.

- CRC로 패킷 오류를 확인한다.
- Sequence로 중복 패킷을 걸러낸다.
