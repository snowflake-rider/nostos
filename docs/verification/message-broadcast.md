> 이관 주의: 원본의 추가 STM32 시험·진단 명령은 미통합 출력 시험 패치를 전제로 할 수 있습니다. 현재 팀 펌웨어에 자동 반영된 기능으로 해석하지 않습니다. [검증 안내](index.md)를 먼저 확인하세요.

# 8종 메시지 — STM32 USART1에서 모든 수신 ESP32까지

## 시험하는 경로

```text
Mac ── ST-LINK USB / USART2 시험 명령 ──► STM32
                                         │ uart_service_send_message(ID)
                                         │ 실제 USART1 PA9 TX: 1바이트
                                         ▼
                                        D6 GPIO18 UART RX
                                         │ event_encode(): [01, ID]
                                         │ Mesh Vendor Model → C001
                                ┌────────┴────────┐
                                ▼                 ▼
                               76                 B6
                            MESH_RX            MESH_RX
```

송신은 **실제 STM32**가 수행한다. Mac에서 ESP32 USB 콘솔에 ID를 쓰는 시험이 아니다. 버튼/센서 대신 시험 명령으로 ID를 선택하므로, 이 결과가 센서 판정이나 물리 버튼의 성공을 증명하지는 않는다. 76/B6의 UART_TX는 원본 로그에 보존하지만 상대 STM32의 실제 수신·음성·LED는 이번 판정 범위 밖이다.

그룹 수신을 검사하며 Relay 설정은 바꾸지 않는다. 대상 보드 중 Relay가 켜져 있으면 준비 단계에서 중단한다. 이것만으로 주변의 모든 중계 노드 부재나 무선 경로를 증명할 수는 없다. 다중 홉 Relay OFF/ON 비교는 별도 시험이다.

## 1. PC 검사 → MCU 빌드

프로젝트 루트에서:

```sh
bash tools/test-host.sh
```

Python 로그 판정기와 기존 Layer 8 관찰기를 검사하고, STM32/Layer 8의 C 테스트를 ASan+UBSan으로 실행한다. 가짜 HAL/로그 결과는 하드웨어 PASS가 아니다.

STM32 프로젝트 디렉터리에서:

```sh
cd stm32-project/integration/stm32
cmake --preset Debug -B build/message-test/firmware -DBUTTON_OUTPUT_TEST=ON
cmake --build build/message-test/firmware
```

결과는 `build/message-test/firmware/bike_swarm_guard.elf`다. 기존 `build/Debug`나 이전 설치 파일을 새 이미지로 혼동하지 않는다. `BUTTON_OUTPUT_TEST=OFF`인 기본 모드에는 USB 시험 명령이 포함되지 않는다.

## 2. 백업 → Flash → readback → 부팅

Flash는 스크립트가 자동으로 실행하지 않는다. 정확한 STM32 ST-LINK serial을 확인하고 별도 승인된 설치 작업으로 진행한다. 이 작업에서는 사용자가 Flash 검증도 승인했다.

1. 현재 STM32를 USB serial과 ST-LINK serial로 식별한다. 기존 시리얼 관찰기를 종료한다.
2. STM32 현재 전체 Flash 512KiB를 별도 로컬 빌드 디렉터리에 읽어 백업한다. 백업 크기와 SHA-256을 기록하고 외부에 공개하지 않는다.
3. 빌드한 **시험 ELF**를 해당 STM32에 설치하고 download verify 결과를 확인한다. ESP32를 erase하거나 Mesh NVS/키를 변경하지 않는다.
4. 설치한 구간을 다시 읽어 ELF에서 추출한 바이너리와 비교한다.
5. STM32를 재시작하여 새 `OUTPUT_TEST_READY` 및 `MESSAGE_TEST_READY protocol=1` 부팅 로그를 기록한다. 기존 누적 로그와 구분한다.
6. ESP32들의 `status`를 다시 확인한다. reset 구간의 잡음/누적 카운터를 새 송신 건수로 세지 않는다. UART 버퍼 정체가 있으면 자동 재부팅으로 숨기지 않고 중단·진단한다.

복구할 때는 설치 직전 백업 또는 실제로 확인한 이전 ELF를 사용하고 verify·부팅까지 확인한다. **기본 모드 이미지와 기존 오디오 시험 모드 이미지 복구는 서로 다른 작업**이다.

## 3. 준비 확인

프로젝트 루트에서:

```sh
bash tools/hardware/run_message_broadcast.sh
```

기본 명령은 이벤트를 보내지 않는다. ESP32에 `status`, STM32에 시험 기능 조회 `?`만 보낸다. `READY_ONLY_NOT_DELIVERY`는 수신 성공이 아니다.

| 대상 | USB serial로 식별 | 용도 |
| --- | --- | --- |
| STM32 | `066DFF485277504867161930` | USART1 송신 |
| D6 | `14:C1:9F:CE:F0:D4` | UART 수신 → Mesh 그룹 송신 |
| 76 | `14:C1:9F:CE:EC:74` | Mesh 수신 |
| B6 | `44:1B:F6:FF:BA:B4` | Mesh 수신 |

`/dev/cu.usbmodem...` 번호는 재연결하면 바뀔 수 있어 고정하지 않는다. 지정한 보드 하나라도 없으면 중단한다. 자동으로 2대 시험으로 축소하지 않는다.

준비 조건: 각 보드의 고유 unicast 주소, event_ready=1, pub/sub C001, 동일 NetKey/AppKey index, TTL=7, period/retransmit=0, Relay=0, UART1 115200/8N1, pending/buffered=0. 같은 key index라고 실제 key 값도 같은 것은 아니다. 실제 수신 검사가 뒤따라야 한다.

## 4. 메시지마다 실제 송신

```sh
# 8종 각 1회
bash tools/hardware/run_message_broadcast.sh --send

# 8종 각 3회: 24번 송신, 수신 노드 2대 각각 24건 기대
bash tools/hardware/run_message_broadcast.sh --send --repeat 3
```

| 순서 | ID | 의미 |
| --- | --- | --- |
| 1 | `10` | 감속 요청 |
| 2 | `11` | 가속 요청 |
| 3 | `12` | 안전 알림 |
| 4 | `13` | 정지 요청 |
| 5 | `20` | 후방 안전 |
| 6 | `21` | 후방 경고 |
| 7 | `30` | 낙차 감지 |
| 8 | `31` | SOS |

각 ID를 보내고 기본 2초 관찰 후 전후 카운터를 비교한다. UART/Mesh 전송 실패 시 같은 ID를 자동 재시도하지 않는다. STM32 송신 확인 자체가 없으면 후속 시험도 중단한다. 다른 단계의 실패는 기록하고 다음 ID로 진행하여 메시지별 결과를 남긴다.

STM32의 시험 명령은 **`m` → 준비 응답 → 1초 안에 binary ID 한 바이트**다. 준비 응답 없이 ID를 보내지 않는다. 한 ID를 처리하면 즉시 해제되며, 잘못된 ID도 해제하고 거부한다. `seq`는 STM32 시험 로그의 송신 시도 번호일 뿐 UART/Mesh payload에 새로 추가한 필드가 아니다. 원래 `1바이트 ↔ 2바이트` 프로토콜은 그대로다.

시험 중에는 버튼을 누르지 않는다. 수신 ESP32에 다른 장치를 연결했다면 낙차/SOS 등 실제 이벤트가 UART로 출력될 수 있음을 확인한다. 시험 명령 자체는 송신 STM32에서 로컬 오디오·LED·버저를 실행하지 않는다.

## 5. PASS 기준과 기록

각 회차에 아래 다섯 단계가 **정확히 1건씩** 있어야 한다.

1. STM32 `MESSAGE_TEST_TX id=... uart=OK seq=...`.
2. D6 `UART_RX id=... result=queued`.
3. D6 `MESH_TX id=... source=<D6 주소> api=accepted`.
4. 76 `MESH_RX source=<D6 주소> id=... result=queued`.
5. B6 `MESH_RX source=<D6 주소> id=... result=queued`.

ID/source/건수, UART 유효 입력과 Mesh 송신·수신 카운터 증가량을 대조한다. 누락·중복·다른 ID/source·잡음 증가·카운터 reset·설정 변화·송신 실패·수신 버퍼 정체는 PASS로 처리하지 않는다. 과거 누적 invalid/noop 값 자체가 아니라 **현재 회차에서 증가했는지**를 검사한다.

`PASS_OBSERVED`는 호스트 수집 로그의 일치 관찰이다. 앱 payload에 고유 sequence나 ACK가 없으므로 개별 무선 패킷의 인과관계와 장시간 무손실을 보장하지 않는다. 타임스탬프도 호스트가 로그를 받은 시각이며 정밀 RF 지연 측정값이 아니다.

실행마다 새 `build/hardware-results/messages-.../`가 생긴다:

- `devices.json`: 이번에 식별한 실제 USB 포트.
- `raw.jsonl`, `console.log`: 전체 수집 로그, 호스트 시험 명령과 회차 경계.
- `summary.json`: 메시지별 결과·누락/실패 사유·전후 상태·카운터 증가량.
- `RESULT.md`: 사람이 읽는 회차별 결과표. 미실행 회차는 실행 건수로 구분한다.

`--out <새 디렉터리>`로 저장 위치를 정할 수 있다. 기존 디렉터리는 덮어쓰지 않는다. `Ctrl-C` 또는 결과 디렉터리에 `stop` 파일을 만들면 중단하고 모든 포트를 닫는다. 중단된 시험은 `INCOMPLETE`이며 완료한 행의 결과만 보존한다.

의도적으로 두 ESP32만 시험할 때는 `--peers 76`처럼 명시한다. 이 결과는 B6 수신 검증으로 확대하지 않는다. `--source`를 바꿀 때는 실제 STM32 UART 배선도 해당 보드로 연결되어 있어야 한다.

[Testing 처음으로](../archive/imported/testing/README.md) · [현재 메시지 형식](../architecture/message-protocol/reference.md)
