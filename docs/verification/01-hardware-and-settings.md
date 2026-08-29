> 이관 원문: `verified/01-hardware-and-settings.md`. 현재 실행 경로는 [팀원 시작 안내](../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# 01. 검증 당시 보드·배선·설정

검증일: 2026-08-28 (KST). 이 문서는 당시 연결과 소스·런타임 상태를 기록하며, 새 핀 배치를 제안하는 문서가 아니다.

## 보드 식별

| 역할 | 보드 | 식별값 | 당시 Mac 포트 |
| --- | --- | --- | --- |
| 버튼 처리·UART 송신 | NUCLEO-F411RE | ST-LINK `066DFF485277504867161930` | `/dev/cu.usbmodem1102` |
| STM32 직결·Mesh 송신 | ESP32-S3 D6 | USB serial `14:C1:9F:CE:F0:D4`, 이름 `ESP32-L8-F0D6` | `/dev/cu.usbmodem1301` |
| Mesh 수신 | ESP32-S3 76 | USB serial `14:C1:9F:CE:EC:74`, 이름 `ESP32-L8-EC76` | `/dev/cu.usbmodem1201` |

USB serial 식별값과 보드 이름의 끝 두 글자는 서로 다를 수 있다. 재연결 시 포트 번호도 달라질 수 있으므로 이름이나 포트 숫자만으로 보드를 정하지 않는다.

## 검증 경로의 핀

| 연결·기능 | 설정 | 확인 수준 |
| --- | --- | --- |
| STM32 → D6 데이터 | STM32 **D8/PA9 USART1 TX → D6 GPIO18 UART1 RX** | 실제 UART 수신 확인 |
| 공통 기준 | STM32 GND ↔ 브레드보드 공통 GND ↔ D6 GND | 사용자 배선 변경 후 전달 성공; 전 구간 연속성 계측은 하지 않음 |
| 테스트 버튼 | STM32 **D10/PB6 ↔ 버튼 ↔ GND**, 내부 Pull-up, 누르면 LOW | 소스 확인 + 사용자 3회 누름과 이벤트 3회 대응 |
| Mac 진단 출력 | STM32 USART2 PA2/PA3, ST-LINK USB | 실제 송신 복사본 `13` 관찰 |
| STM32 PA10/D2 | USART1 RX 설정 유지 | 버튼으로 변경하지 않음; 역방향 UART 전달은 이번 검증 아님 |
| D6 GPIO17 | UART1 TX 설정 | 이번 단방향 시험의 성공 판정 대상 아님 |

흰 신호선이 GPIO11에 연결된 사진을 확인한 뒤, 사용자가 GPIO18로 이동했다. 이후 D6 런타임에서 `mapped_gpio=18`, `path_enabled=1`, 대기 레벨 HIGH가 확인됐다. 대기 레벨만으로 파형 품질을 검증한 것은 아니다.

전원은 각 보드의 USB를 사용했다. 공통 GND와 전원 레일은 구분하며, 보드 간 3V3/5V 레일 연결을 검증 조건으로 삼지 않는다. 브레드보드 전체 레일이 이어져 있다고 가정하지 않는다.

## UART 설정

- STM32 USART1: **115200 baud / 8 data bits / no parity / 1 stop bit / no flow control**.
- D6 UART1 런타임 진단: `baud=115201 data=8 parity=none stop=1 flow=0`.
- D6 RX: `mapped_gpio=18 path_enabled=1`; 버튼 시험에서 수신 버퍼는 0으로 유지됐다.
- 실제 UART 시험 데이터: 정지 요청 `MSG_STOP_REQUEST`, 값 **`0x13` 한 바이트**. 문자열 `"13"`을 보낸 것이 아니다.

## 버튼 시험의 Mesh 설정

| 항목 | D6 | 76 |
| --- | --- | --- |
| Primary address | `0x0005` | `0x0003` |
| NetKey index | `0x0000` | `0x0000` |
| AppKey index | `0x0001` | `0x0001` |
| Publication address | `0xc001` | `0xc001` |
| `sub_C001` / `event_ready` | `1` / `1` | `1` / `1` |
| TTL / Relay | `7` / `0` | `7` / `0` |

위 값은 `status` 응답으로 확인했다. 키 **인덱스**이지 비밀 키 값이 아니다. 두 보드의 Relay 설정이 0인 이번 관찰로 다중 홉 중계를 검증했다고 말하지 않는다.

## 소스 근거

- [CubeMX IOC](../../firmware/stm32/nostos_stm32.ioc): PA9 TX, PA10 RX, PB6 TEST_BUTTON.
- [STM32 main.c](../../firmware/stm32/Core/Src/main.c): UART 초기화·버튼 GPIO·USB trace 연결.
- [button.c](../../firmware/stm32/MyApp/hw/button.c): PB6 정지 요청, 30ms 디바운스, 눌림 전이 처리.
- [ESP32 bridge_runtime.c](../../firmware/esp32/main/bridge_runtime.c): UART1, TX17/RX18, UART·Mesh 로그.

현재 파일 링크는 이후 수정될 수 있다. 당시 실제 결과의 기준은 [보존된 증거](05-evidence.md)다.
