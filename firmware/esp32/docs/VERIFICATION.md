> 이관 원문: `layers/layer-8/VERIFICATION.md`. 현재 실행 경로는 [팀원 시작 안내](../../../docs/getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Layer 8 검증 기록

기록일: **2026-08-28**. [사용 방법](layer8-background.md).

최신 UART 진단 설치 결과는 바로 아래 절을 따른다. 이후 절들은 초기 설치 당시의
기록이며, 옛 USART2 배선과 옛 이미지 해시를 현재 설정으로 사용하지 않는다.

## 최신 — 2026-08-28 UART 진단 앱 설치

사용자 승인 후 Mac USB에 연결된 D6/76 두 대에 읽기 전용 `status` 진단을 추가했다.
B6는 USB에 없어서 이번에 설치하지 않았다. STM32 펌웨어도 변경하지 않았다.

- BIN: **915856 bytes (`0xdf990`)**.
- BIN SHA-256: `30948bbfeedca03b852f8bf56600b326744e3781b77f42498fc075dd114ccdcc`.
- 기존 앱에서 `tools/check_uart_diag.py --board D6`는 진단 출력 부재로 실패했다.
  새 앱 설치 후 동일 검사와 `--board 76`가 모두 `DIAG_PRESENT`를 출력했다.
- ESP-IDF 5.5.5 build 통과. 기존 Debug/Release/ASan+UBSan 각 4/4,
  Python fast-check 회귀 14/14 통과. 이 회귀 테스트는 전기 신호 검증이 아니다.
- 각 보드의 `0x00000000..0x00186fff`(1601536 bytes)를
  `build/uart-diag-backup.M8B9Nl/{D6,76}-before.bin`에 백업했다.
  파일 권한 0600, 디렉터리 0700. 키를 포함할 수 있으므로 공유 금지.
- 실제 보드 백업의 partition table과 빌드 partition table 일치를 확인했다.
  NVS=`0x9000/24K`, factory=`0x10000/1500K`.
- 앱 `0x10000`에만 쓰고 양쪽 `Hash of data verified.` 확인.
  erase 범위 `0x10000..0xeffff`; bootloader/partition/NVS는 쓰지 않았다.
  각 보드는 설치 후 정상 재부팅하여 새 진단 명령에 응답했다.

| 보드 | USB serial | 설치 후 primary | Mesh 설정 | UART 진단 |
| --- | --- | --- | --- | --- |
| D6 | `14:C1:9F:CE:F0:D4` | `0x0005` | net=0, app=1, pub=C001, sub_C001=1, event_ready=1 | UART1, baud=115201, 8-N-1, flow=0, RX=18/IOMUX, path_enabled=1 |
| 76 | `14:C1:9F:CE:EC:74` | `0x0003` | net=0, app=1, pub=C001, sub_C001=1, event_ready=1 | UART1, baud=115201, 8-N-1, flow=0, RX=18/IOMUX, path_enabled=1 |

설치 후 약 3초 읽기 전용 관찰:

```text
STM32_PA9_LEVEL_SAMPLES [1, 1, 1]
ESP_GPIO18_LEVEL_COUNTS {"D6": {"0": 24}, "76": {"0": 24}}
D6 uart_rx valid=0 noop=0 invalid=0 hw_errors=0
76 uart_rx valid=0 noop=0 invalid=0 hw_errors=0
```

PA9는 SWD로 `GPIOA_IDR`의 bit 9를 읽었고, ESP32는 GPIO input 레지스터를 읽었다.
동일 시점의 오실로스코프 파형/전압 측정은 아니다. STM32 쪽 HIGH와 ESP32 쪽 LOW의
차이는 실제 연결/접촉/외부 회로를 점검할 단서지만, 단선·오배선·단락 중 어느
것인지까지 확정하지 않는다. `DIAG_PRESENT`를 UART/Mesh 버튼 전달 성공으로 취급하지 않는다.

설치 직전 별도 사용자 승인 시험에서는 USART1 DR `0x40011004`에 `0x13`을
정확히 3회 쓰고 각 쓰기 후 TC/TXE=1을 확인했지만 두 ESP32의 UART 수신 증가가
모두 0이었다. 이는 버튼 처리/HAL 호출을 우회한 시험이다. 기존
`message_debug_inject`는 remote 수신 동작을 주입하므로 UART 송신 시험에
사용하지 않았다. 다른 노드 `source=0x0004`의 Mesh 트래픽은 성공 건수에서 제외했다.

사용법과 해석 한계: [UART_DIAGNOSTICS.md](UART_DIAGNOSTICS.md).
진단 후 모든 시리얼 포트 해제 완료.

## 통과한 검사

| 검사 | 결과 | 실제 범위 |
| --- | --- | --- |
| Host Debug | PASS, 4/4 | codec, queue/transport, console parser, event path |
| Host Release | PASS, 4/4 | NDEBUG에서도 CHECK 실행 |
| Host ASan + UBSan | PASS, 4/4 | AppleClang 21.0.0, 메모리·정의되지 않은 동작 검사 |
| ESP32-S3 / ESP-IDF v5.5.5 | BUILD PASS | 앱·bootloader·partition table 생성 |
| USB console 설정 | PASS, 정적 검사 | primary `USB_SERIAL_JTAG`, secondary `NONE` |
| 독립 빌드 의존성 | PASS | compile commands에 `stm32-project`/Layer 7 참조 없음 |
| 이벤트 계약 | PASS | 원본 STM32와 enum ID 일치, core C/H 4개 원본과 동일 |
| 기존 파일 보존 | PASS | Layer 7 + stm32-project의 빌드/생성 의존성 제외 기존 260파일 SHA-256 일치 |
| 셸 문법 | PASS | `bash -n test-host.sh` |
| 빠른 관찰 도구 회귀 검사 | PASS, 14/14 | replay 기반 정상 경로, 누락·중복·잡음·설정 변경·재시작·API 실패를 보수적으로 판정 |

실행 명령:

```bash
bash test-host.sh
ctest --test-dir host-tests/build/sanitized -V
source /Users/kafka/esp/esp-idf-v5.5.5/export.sh
idf.py build
```

호스트 검사 출력 중 핵심:

```text
PASS event path: 8 IDs, 3 simulated origins, repeated UART inputs
PASS event path: exact 2-byte Mesh payload -> 1-byte peer UART, no self echo or RX republish
HARDWARE_UART_AND_MESH=NOT_TESTED
PASS codec: 8 IDs, all 256 byte values, strict length/version/null checks
PASS bridge: shared 32-slot FIFO, reject newest, repeated IDs, wraparound
PASS bridge: no-op/invalid, not-ready discard, 999/1000ms, 64-bit clock
PASS bridge: directions, copied payload/source, self suppression
PASS bridge: exact wire bytes, both API failures, no retries or RX republish
```

`test_event_path`는 실제 codec/queue/transport API를 연결하고, 외부 UART/Mesh 전송만 대체한다.
8종 ID × 3개 송신자 × 같은 입력 2회로 양방향 동작과 수신 후 재발행하지 않는 동작을 확인한다.
수신 API에 테스트가 직접 payload를 넣으므로 **실제 radio 전달·그룹 구독·Mesh 설정 검사가 아니다**.
Release와 sanitizer 로그는 `host-tests/build/<variant>/Testing/Temporary/LastTest.log`에 있다.

빠른 버튼 관찰 도구는 [FAST_CHECK](FAST_CHECK.md)를 따른다. 2026-08-28 문서 갱신 중 다음 호스트 회귀 검사를 다시 실행해 14/14 PASS를 확인했다.

```bash
python3 -m unittest discover -s host-tests -p test_fast_check.py -v
```

이 검사는 가짜 replay 로그의 판정기만 확인한다. 실제 UART 전기 신호나 Mesh radio 전달을 대신하지 않는다.

## 빌드 산출물

- 앱 이름: `esp32s3_layer_8`.
- 앱 이미지: `build/esp32s3_layer_8.bin`, **912016 bytes (`0xdea90`)**.
- 앱 partition: `0x177000`, 남은 공간 `0x98570` (**41%**, 빌드 도구 출력 기준).
- BIN SHA-256: `80454a270c1ec8e10a7cb93929c94e41b01004452fdf877c0f234e18b989cedb`.
- ELF SHA-256: `4ad872c536217cf531d322cd710ceb4062f6647999f5518fe122e0d308c70785`.
- 최종 성공 로그: `build/log/idf_py_stdout_output_19715`.

첫 호스트 구성은 core를 넣기 전 소스 부재로 실패한 뒤, 실제 core 추가 후 통과했다.
첫 ESP32 빌드는 가져온 `off` 경로의 함수명 오타 `mesh_node_send_onofSf` 때문에 실패했다.
Layer 8에서만 `mesh_node_send_onoff`로 고쳐 최종 빌드에 성공했다. 원본은 수정하지 않았다.
성공한 최종 빌드 로그에서 compiler warning/error는 발견하지 않았다.

## 두 대 설치와 당시 초기 실물 상태

2026-08-28 10:20 KST, 사용자 승인 후 기존 빌드 이미지와 같은 SHA-256을 확인하고 설치했다.

| USB 식별자 | 당시 포트 | Layer 8 이름 | 복원된 주소 | Flash / boot / status |
| --- | --- | --- | --- | --- |
| `14:C1:9F:CE:EC:74` | `/dev/cu.usbmodem1101` | `ESP32-L8-EC76` | `0x0003` | PASS / PASS / PASS |
| `44:1B:F6:FF:BA:B4` | `/dev/cu.usbmodem1401` | `ESP32-L8-BAB6` | `0x0004` | PASS / PASS / PASS |

두 보드의 bootloader/app/partition table 각각 `Hash of data verified.`를 확인했다.
부팅 로그는 `Project name: esp32s3_layer_8`, ELF SHA prefix `4ad872c53`,
`UART1_READY TX=GPIO17 RX=GPIO18 115200/8N1`, `[LAYER-8] APP_STARTED`를 확인했다.
각 USB 포트에서 `status`를 두 번 보내 실제 응답을 받았다.

10:20 KST 당시 두 대 모두 `NO_KEY_INDEX_MAP`, `event_ready=0`, `pub=0x0000`, `sub_C001=0`이었다.
이는 설치 실패가 아니라 **새 Vendor Model의 AppKey index 매핑/Bind/Publication/Subscription 설정이 남은 상태**다.
기존 primary 주소와 Relay 상태(EC76 disabled, BAB6 enabled)는 복원됐다.
기존 키/모델의 모든 설정까지 이 status로 검증한 것은 아니다.

- [EC74 백업·Flash 로그](logs/flash-EC74-20260828T102023-KST.log)
- [BAB4 백업·Flash 로그](logs/flash-BAB4-20260828T102023-KST.log)
- [두 대 부팅·status 응답 로그](logs/boot-status-pair-20260828T102023-KST.log)

Flash 전에 각 보드의 NVS `0x9000..0xEFFF` 24576바이트를
`build/preflash-nvs.Fmz6gC/EC74-nvs.bin`, `BAB4-nvs.bin`으로 백업했다.
이 백업에는 Mesh 키가 포함될 수 있으므로 공유하지 않는다. 디렉터리는 소유자 전용이다.
Flash는 bootloader/app/partition table 영역만 갱신했으며 NVS 전체 erase/factory-reset은 하지 않았다.
새 앱의 정상적인 NVS 접근 때문에 부팅 전후 NVS가 바이트 단위로 동일하다는 보장은 하지 않는다.

설치 시 USB 스캔에는 ESP32 두 대만 있었으며 STM32에는 쓰지 않았다.
Mesh 설정 명령/재provisioning/버튼 전송은 실행하지 않았고, 모니터 포트는 확인 후 닫았다.

## 후속 D6 설치·설정 진단·버튼 관찰

### D6 Flash — 11:20 KST

`0x0005-ESP32-D6`에 EC76/BAB6와 같은 BIN SHA-256의 Layer 8 이미지를 기록했다. bootloader/app/partition table의 hash verification과 Flash command exit 0을 확인했고, NVS 전체 삭제와 factory reset은 수행하지 않았다.

D6의 당시 연결은 UART bridge였고 Layer 8 console은 native USB Serial/JTAG를 사용하므로, 그 실행에서는 post-flash `APP_STARTED`와 `status`를 직접 받지 못했다. 따라서 **D6 Flash는 PASS지만 그 실행의 boot/status marker는 NOT_VERIFIED**로 보존한다.

- [D6 백업·Flash 로그](logs/flash-D6-20260828T112007-KST.log)

### B6 설정 진단 — 11:31 KST 전후

현재 NVS 백업 비교에서 EC76과 BAB6는 같은 NetKey를 사용했지만 활성 AppKey 1이 서로 달랐다. EC76은 후속 `status`에서 `event_ready=1`, BAB6는 `event_ready=0`, `pub=0x0000`, `sub_C001=1`이었다. 원인을 숨기기 위해 키나 NVS를 삭제하지 않고 두 노드에 하나의 공용 AppKey와 C001 Publication을 설정하는 절차를 분리해 기록했다.

- [B6 AppKey/Publication 진단](../../../docs/bluetooth-setting/B6_SETUP.md)

### STM32 버튼 관찰 — 11:57 KST

후속 32초 관찰에서는 ESP32 세 대 모두 `event_ready=1`로 기록됐다. 이는 10:20의 두 대 `event_ready=0`과 11:31의 B6 `event_ready=0`보다 나중의 readiness 관찰이다. 다만 재부팅 복원이나 unbind/key delete 처리를 검증한 것은 아니다.

외부 PB6 버튼을 누르는 동안 STM32 ST-LINK VCP에서 `0x13` 4바이트를 받았다. 같은 구간의 ESP32 UART valid 수신 카운터는 EC76 `0→0`, BAB6 `0→0`, D6 `299→299`였고 Mesh TX/RX 카운터도 증가하지 않았다.

따라서 현재 확인된 경계는 **PB6 → STM32 USART2 → ST-LINK VCP**까지다. NUCLEO 외부 D1/PA2 → ESP32 GPIO18, 공통 GND와 Solder Bridge 상태, 이후 Mesh 전송은 아직 확인되지 않았다.

- [STM32 버튼·USART2 설치와 실제 관찰](../../../docs/verification/stm32-button-uart.md)

## 하드웨어 검증 범위와 남은 것

| 단계 | 상태 | 필요한 증거 |
| --- | --- | --- |
| Layer 8 Flash | PASS, 3/3 | EC76/BAB6/D6 write/hash 확인 |
| 명시적 Layer 8 boot marker | PASS, 2/3 | EC76/BAB6의 `esp32s3_layer_8` / APP_STARTED. D6 Flash 실행에서는 native USB boot log 미수집 |
| USB 명령 입력 | PASS, 2/3 | EC76/BAB6 native USB 포트에서 `status` 두 차례 응답. D6 당시 포트는 UART bridge |
| Vendor readiness | 후속 관찰 PASS, 3/3 | 11:57 관찰의 세 노드 `event_ready=1`. 실제 Event 전달 및 설정 회귀와 구분 |
| 설정 저장·복원·해제 | NOT_VERIFIED | 재부팅 복원, unbind/key delete/pub 변경 후 readiness |
| STM32 버튼 → ESP32 UART | PARTIAL | PB6/USART2/ST-LINK VCP는 확인, ESP32 UART valid 수신은 증가하지 않음 |
| ESP32 → 상대 ESP32 Mesh | NOT_VERIFIED | 각 상대의 같은 source/ID MESH_RX |
| 상대 ESP32 → 상대 STM32 | NOT_VERIFIED | UART_TX와 상대 STM32 RX 카운터/ID/출력 |
| 연속 입력 / RTOS 부하 / stack | NOT_VERIFIED | full/expired/오류·watchdog·stack 런타임 관측 |
| Controlled Relay | NOT_VERIFIED | 직접 경로를 차단한 Relay OFF/ON 대조 |

초기 구현 단계에서는 보드를 변경하지 않았고, 후속 설치 단계에서 세 ESP32를 순서대로 Flash했다.
NVS 전체 삭제와 factory reset은 하지 않았다. 이후 nRF Mesh 설정 변경은 위 시간순 기록과 B6 진단 문서를 따른다.
이전 단계에서 관찰한 Layer 7 OnOff 성공은 Layer 8 이벤트 송수신 성공으로 승계하지 않는다.
현재 우선 시험 범위는 STM32 한 건 → 송신 ESP32 `UART_RX`/`MESH_TX` → 두 상대 ESP32의 `MESH_RX`까지다. 상대 STM32 수신은 그다음 단계로 분리한다.

## 참고한 로컬 SDK

- `components/esp_system/Kconfig`: secondary USB 출력은 입력을 지원하지 않아 primary USB console로 선택.
- `components/bt/esp_ble_mesh/api/core/esp_ble_mesh_networking_api.c`: 송신 API가 context/payload를 복사하는 경계 확인.
- Mesh/UART adapter의 실제 동작은 위 호스트 검사로 대신 보증하지 않는다.
