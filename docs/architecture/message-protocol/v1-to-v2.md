# v1 → v2: ESP32·STM32 변경 요약

**ESP32도 바뀝니다.** ESP32만 또는 STM32만 v2로 바꾸면 UART 형식이 달라 통신하지 못하므로
연결된 STM32와 ESP32를 함께 전환해야 합니다.

## 한눈에 보기

| 장치 | v1 | v2에서 바뀌는 것 |
| --- | --- | --- |
| ESP32 | 1바이트 수신, 단일 FIFO | frame/CRC 해석, source·Mesh 주소 확인, FALL/SOS 긴급 큐 |
| STM32 | 메시지 ID 1바이트 송수신 | v2 frame 송수신, session·sequence, 중복·FALL/CLEAR 처리 |

## ESP32에서 바뀌는 것

v2 설정을 켜면 `bridge_runtime.c` 대신 `bridge_runtime_v2.c`가 빌드됩니다.

```ini
CONFIG_NOSTOS_PROTOCOL_V2=y

# 세 보드가 함께 사용하는 주소표
CONFIG_NOSTOS_SOURCE1_ADDRESS=0x0003  # 76
CONFIG_NOSTOS_SOURCE2_ADDRESS=0x0005  # D6
CONFIG_NOSTOS_SOURCE3_ADDRESS=0x0006  # B6
```

보드별 로컬 source도 설정합니다.

```text
76: source 1
D6: source 2
B6: source 3
```

주요 동작 변화:

- UART: raw 1바이트 → 길이·CRC가 있는 v2 frame
- Mesh payload: ID 2바이트 → source/session/sequence/payload가 있는 메시지
- 큐: 일반 12칸 + 긴급 예약 4칸, FALL/SOS 우선 처리
- 수신: payload source와 실제 Mesh 발신 주소가 다르면 거절
- Vendor opcode: v1 `0x20` → v2 `0x21`

GPIO17/18, 115200/8N1, TTL 7, 기존 Mesh 주소/AppKey/group은 그대로 사용할 수 있습니다.
NVS를 지우지 않는 일반 Flash라면 보통 재등록할 필요는 없지만, 세 ESP32 모두 v2 펌웨어여야 합니다.

## STM32에서 바뀌는 것

일반 펌웨어 빌드에 v2를 켭니다.

```sh
cmake -S firmware/stm32 -B build/stm32-v2 \
  -DNOSTOS_PROTOCOL_V2=ON
```

주요 동작 변화:

- 버튼/센서 이벤트를 1바이트가 아닌 v2 frame으로 송신
- UART ISR은 바이트를 버퍼에 넣고, main loop에서 frame/CRC를 검사
- source/session/sequence로 과거·중복 메시지 거절
- FALL과 FALL_CLEAR를 같은 사고 단위로 관리
- 중복 메시지 때문에 RGB·부저·음성이 반복 실행되는 것을 방지

아직 필요한 배포 작업:

```text
message_protocol_service_boot()
  → STM32의 local source 설정
  → 재부팅 후에도 안전한 session 생성·복구
  → 승인된 상대 session 복구
```

현재 기본 boot 함수는 `NOT_READY`를 반환하므로, 이것을 구현하지 않고 옵션만 ON으로 바꾸면
STM32 v2 통신은 시작되지 않습니다. 독립 `BUTTON_OUTPUT_TEST` 빌드는 현재 v2와 동시에 켤 수 없습니다.

## 전환 순서

```text
1. STM32 session boot 준비
2. STM32 v2 빌드
3. 76·D6·B6 보드별 ESP32 v2 빌드
4. 호스트 mock/build 테스트
5. STM32와 ESP32를 함께 Flash
6. UART → D6 → 76/B6 실물 확인
```

현재 상태는 **v2 공통 코드와 mock 테스트는 준비됨**, **실물 배포용 STM32 session boot와
보드별 ESP32 설정/Flash는 아직 적용 전**입니다.

---

[메시지 프로토콜 목차](README.md) · [상세 v2 계약](../../../libs/protocol/V2.md)
