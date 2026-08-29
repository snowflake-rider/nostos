> 보존 원문: `esp-ble-unorganized/ridar-sensor/README.md`. 당시 경로·명령·검증 결과이며 현재 실행 안내는 저장소 README를 따릅니다.

# SLAMTEC RPLIDAR C1 → ESP32-S3 연결 조사

조사일: **2026-08-28** · 대상: **RPLIDAR C1 / ESP32-S3-N16R8** · 범위: 연결 방법과 이후 펌웨어 작업에 필요한 조건

폴더 이름은 요청한 `ridar-sensor`를 그대로 사용한다. 제품의 공식 이름은 **RPLIDAR C1**이다. 이 문서는 조사 결과이며, **펌웨어 수정·빌드·플래시·리셋·새 시리얼 시험은 수행하지 않았다.** USB 장치 인식, 센서 응답, 실제 스캔 성공을 확인한 문서가 아니다.

## 1. 먼저 결론

**두 방법 모두 구현 가능한 경로다. 다만 현재 USB 연결은 펌웨어만 바꾸면 반드시 작동한다고 확정할 수 없다.** ESP32-S3의 USB Host 지원과 실제 개발보드의 USB 전원 공급 회로는 별도로 확인해야 한다. [Espressif USB Host][usb-host] · [보드 V1.4 회로도][board-sch]

| 비교 | ① USB-C → ESP32-S3 | ② Breadboard / UART → ESP32-S3 |
| --- | --- | --- |
| 데이터 경로 | C1 UART → USB 어댑터 → ESP32 네이티브 USB | C1 UART → 점퍼선 → ESP32 UART |
| ESP32 역할 | USB **Host** + USB 시리얼 드라이버 | UART 송수신 |
| 센서 쪽 USB 어댑터 | 사용 | 제거하거나 데이터 경로에서 분리 |
| 펌웨어 | USB Host + 해당 어댑터 드라이버 + C1 프로토콜 | UART 드라이버 + C1 프로토콜 |
| 전원 확인 | USB VBUS에 적절한 5V가 공급되는지 | C1 VCC에 적절한 5V를 직접 공급하는지 |
| 먼저 해결할 부분 | 어댑터 식별, USB 포트 역할, VBUS 경로 | 커넥터 핀 식별, TX/RX 교차, 공통 GND |

C1 자체의 인터페이스는 **TTL UART**이며, 공식 개발 키트의 USB 어댑터가 이를 USB로 변환한다. 즉, 두 방식은 같은 센서 명령을 다른 통신 경로로 전달한다. [C1 제품 사양][c1-spec] · [C1 키트 매뉴얼 p.5][c1-kit]

**이 프로젝트에 대한 제안:** 배선을 직접 구성해도 된다면 UART가 구현 범위가 작다. 지금 USB 구성을 유지하려면 Host와 전원 경로부터 확인한다. 이는 아래 자료를 비교한 구현 제안이지, 두 방식의 실물 시험 결과가 아니다.

## 2. 두 방식에 공통인 C1 규격

| 항목 | C1 규격 | 연결할 때의 의미 |
| --- | --- | --- |
| 통신 | **460800 baud, 8N1** | 8 data bits / no parity / 1 stop bit |
| UART 신호 | **3.3V TTL** | 5V UART 또는 RS-232 전압을 넣지 않는다 |
| 센서 전원 | **5V DC**, 허용 **4.8~5.2V** | ESP32의 `3V3` 핀에 센서 전원을 연결하지 않는다 |
| 기동 전류 | **800mA typical** | 모터 시작 시의 대표값이며 보장된 최대값이 아니다 |
| 정상 동작 전류 | **230mA typical / 260mA maximum** | 기동 전류와 구분한다 |
| 전원 ripple | 최대 **150mV** | 단순히 5V라는 표기만으로 전원 품질이 확인되지 않는다 |

근거: [C1 데이터시트의 통신·전기 특성, 특히 p.13][c1-datasheet]. 전류 값은 C1 기준이며, ESP32·USB 어댑터·다른 장치의 소비전류가 포함된 시스템 총량이 아니다.

**전원 5V와 데이터 신호 3.3V는 서로 다른 항목이다.** 전원 용량은 C1 기동 전류에 여유를 두고 선정하되, 실제 센서 입력 전압이 허용 범위에 유지되는지도 확인해야 한다. 이는 위 규격에 따른 설계 조건이다.

## 3. 방법 ① — USB-C → ESP32-S3

### 3.1 실제 연결 구조

준비물은 **C1과 해당 USB 어댑터**, 커넥터에 맞는 **데이터 지원 케이블·Host/OTG 어댑터**, **ESP32 COM용 Mac 연결 케이블**, 그리고 3.3절에서 확인할 **센서용 VBUS 전원 경로**다. 어댑터의 센서 쪽 USB 단자 모양을 먼저 확인한다. 단순 C-to-C 케이블 하나가 모든 보드에서 Host 연결과 전원을 해결한다고 가정하지 않는다.

```text
RPLIDAR C1
   │  TTL UART
   ▼
C1 USB 어댑터                       Mac
   │  USB 데이터 + VBUS                │ USB
   ▼                                  ▼
ESP32-S3의 USB / OTG 포트          ESP32-S3의 COM 포트
   │                                  │
ESP32-S3 USB controller            보드의 USB-UART 변환 칩
   │                                  │
USB Host 펌웨어                    UART0 콘솔 / 업로드
```

공식 C1 키트 매뉴얼은 센서 어댑터에 **CP2102**를 사용한다고 설명한다. 다만 사용 중인 어댑터가 그 키트와 동일한지, 다른 리비전인지 아직 식별하지 않았다. 추후 USB descriptor의 VID/PID와 칩 표기를 확인한 뒤 드라이버를 정한다. **ESP32 보드의 COM 변환 칩과 C1 어댑터의 변환 칩은 별개다.** [C1 키트 매뉴얼 p.5][c1-kit] · [보드 회로도][board-sch]

### 3.2 USB와 COM은 왜 다른가?

| 보드 포트 | 데이터가 연결되는 곳 | C1을 연결했을 때 |
| --- | --- | --- |
| **USB / OTG** | ESP32-S3의 네이티브 USB | Host 펌웨어가 USB 어댑터를 인식할 수 있는 경로 |
| **COM / UART** | USB-UART 변환 칩 → ESP32 UART | 보드의 이 포트는 PC에 연결되는 USB 장치 쪽이며 C1의 Host가 되지 않음 |

위 구분은 [YD-ESP32-S3 제조사 회로도][board-sch]에 근거한다. 보드의 정확한 제조사·리비전은 아직 확정하지 않았으므로 **사진의 위/아래보다 실제 `USB`, `COM` 실크와 해당 리비전 회로를 우선한다.** ESP32-S3의 내부 USB PHY 신호는 `GPIO20 = D+`, `GPIO19 = D−`다. 이 핀을 UART의 TX/RX로 취급하지 않는다. [Espressif USB 배선 자료][usb-pins]

### 3.3 USB-C 케이블만으로 전원 공급이 보장되지는 않는다

USB-C의 **커넥터 모양**, 데이터의 **Host/Device 역할**, 전원의 **Source/Sink 역할**은 구분해야 한다. Host 프로그램을 실행하는 것만으로 보드에 없는 VBUS 공급 회로가 생기지는 않는다. 데이터 통신이 가능한 케이블과 보드에 맞는 Host/OTG 연결 구성이 필요하다. [TI의 USB Type-C 역할 설명][type-c] · [Espressif USB Host 전원·PHY 설명][usb-phy]

현재 사진과 유사한 **YD-ESP32-S3의 공식 V1.4 회로도**를 읽으면 다음과 같다. 아래 내용은 해당 회로도에 한정되며 실물 리비전이나 현재 납땜 상태의 확인 결과가 아니다. [V1.4 회로도][board-sch]

| 회로도 항목 | 확인한 연결 |
| --- | --- |
| `USB1` — USB-UART / CH343P 쪽 | VBUS → `D1` → 보드 `5V` |
| `USB2` — 네이티브 USB 쪽 | VBUS → `D2` → 보드 `5V` |
| `USB-OTG` 링크 | `D2`를 우회해 USB2 VBUS와 보드 5V를 연결하는 경로 |
| `IN-OUT` 링크 | 헤더의 `5Vin` 경로에 있는 `D3` 우회 경로. `USB-OTG`와 다름 |

**회로 해석:** V1.4에서 `USB-OTG` 링크가 열려 있다면, COM에서 공급된 보드 5V가 `D2`를 거슬러 네이티브 USB VBUS로 나가지 못한다. 따라서 **ESP32 전원 LED가 켜져 있어도 C1 전원 공급은 별개**다. 또한 USB2의 CC1/CC2에는 각각 5.1kΩ 접지 저항이 표시되어 있어, 일반 C-to-C 케이블만으로 원하는 Host/전원 Source 구성이 된다고 가정하지 않는다. [회로도][board-sch] · [USB-C 역할 설명][type-c]

**이 조사만 보고 `USB-OTG`나 `IN-OUT`를 납땜하거나 두 전원을 묶지 않는다.** 먼저 실물 리비전, 링크 상태, 케이블·어댑터, VBUS의 공급·역류 방지 경로를 확인한다. 필요한 경우 전원 스위치와 전류 제한을 포함한 Host 전원 구성을 별도로 설계한다. [Espressif USB Host 전원 설명][usb-phy]

### 3.4 필요한 펌웨어와 현재 SDK의 출발점

ESP-IDF에는 CP210x·FTDI·CH34x USB 시리얼 장치를 위한 **`cdc_acm_vcp`** 예제가 있다. 이는 C1 전용 드라이버는 아니고 USB 어댑터와 바이트를 교환하는 출발점이다. CP2102가 맞다면 CP210x VCP 드라이버가 후보가 된다. [공식 예제 README][vcp-example]

이 컴퓨터에서 읽어 확인한 SDK는 `/Users/kafka/esp/esp-idf-v5.5.5`이며 관련 파일은 다음과 같다.

| 파일 | 확인한 내용 / 이후 수정할 부분 |
| --- | --- |
| 로컬 VCP 예제 README (원본에 포함되지 않은 참조: `/Users/kafka/esp/esp-idf-v5.5.5/examples/peripherals/usb/host/cdc/cdc_acm_vcp/README.md`) | USB-UART VCP 장치 연결 방법 |
| 로컬 idf_component.yml (원본에 포함되지 않은 참조: `/Users/kafka/esp/esp-idf-v5.5.5/examples/peripherals/usb/host/cdc/cdc_acm_vcp/main/idf_component.yml`) | `usb_host_cp210x_vcp`, `usb_host_vcp` 등 의존성 |
| 로컬 cdc_acm_vcp_example_main.cpp (원본에 포함되지 않은 참조: `/Users/kafka/esp/esp-idf-v5.5.5/examples/peripherals/usb/host/cdc/cdc_acm_vcp/main/cdc_acm_vcp_example_main.cpp`) | Host 설치, VCP 등록·열기, line coding, RX callback |

원본 예제는 **115200 baud**와 테스트 문자열을 사용한다. **수정하지 않은 예제를 그대로 플래시해 C1이 스캔할 것으로 기대하면 안 된다.** C1용으로는 460800/8N1, 바이너리 명령 송신, 수신 버퍼와 패킷 파서, timeout·분리·재연결 처리가 필요하다. 예제의 문자열 출력 `handle_rx()`만으로 거리값이 해석되지는 않으며, 예제의 DTR/RTS 설정을 C1 모터 제어로 가정하지 않는다. [v5.5.5 예제 소스][vcp-code] · [C1 규격][c1-datasheet]

```text
USB Host 초기화
→ 장치 enumeration / VID·PID 확인
→ 맞는 VCP 드라이버 열기
→ UART line coding을 460800/8N1로 설정
→ C1 정보·상태 요청
→ 지원 스캔 모드 선택
→ 바이너리 수신 버퍼 → C1 패킷 해석 → 각도·거리값
```

위는 구현 순서 제안이며 실제 실행 로그가 아니다. 드라이버 API는 [Espressif 예제][vcp-code], 센서 프로토콜은 [SLAMTEC SDK][sdk]를 기준으로 연결한다.

### 3.5 USB Host를 쓰면 로그 포트도 바꿔야 한다

ESP32-S3의 **USB-OTG와 USB Serial/JTAG는 내부 PHY 하나를 공유**한다. 외부 PHY가 없는 일반 구성에서 내부 PHY를 USB Host가 사용하면 동일한 네이티브 USB의 Serial/JTAG 콘솔을 동시에 사용하지 못한다. 이번 구성의 콘솔 경로는 **Mac → COM → UART0**를 제안한다. eFuse 변경이나 외부 PHY 추가를 이번 작업에 포함하지 않는다. [Espressif PHY 설명][usb-phy]

현재 Layer 8은 [sdkconfig.defaults](../../../firmware/esp32/sdkconfig.defaults)에 `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`가 있고, [main.c](../../../firmware/esp32/main/main.c)의 컴파일 조건도 이를 요구한다. 따라서 나중에 USB Host를 통합하려면 **콘솔 설정·컴파일 조건·콘솔 입출력을 함께 검토**해야 한다. COM에 Mac을 꽂았다는 사실만으로 현재 앱의 로그 경로가 자동 변경되지는 않는다.

## 4. 방법 ② — Breadboard → ESP32-S3 UART

### 4.1 브레드보드는 연결을 도와주는 도구

브레드보드 경로에서는 USB Host를 거치지 않고 **C1의 UART 신호를 ESP32의 UART로 직접 연결**한다. C1의 UART·전원 인터페이스는 [키트 매뉴얼 p.12~13][c1-kit], ESP32 UART 설정은 [Espressif UART 문서][uart-doc]를 따른다.

브레드보드 자체는 금속 접점으로 선을 연결하는 부품이다. 전원 레일의 `+`, `−` 색 표시는 사용 구분이며, 자체적으로 5V를 만들거나 신호 전압을 바꾸지 않는다. 접점·정격은 제품마다 확인해야 하며, 다른 제품의 정격을 현재 브레드보드에 적용하면 안 된다. [브레드보드 제조사 BB830 설명][breadboard]

### 4.2 연결 제안

준비물은 **C1 커넥터에 맞는 케이블·브레이크아웃**, **브레드보드와 점퍼선**, **C1 기동 전류를 고려한 regulated 5V 전원**, **ESP32와 Mac 연결 케이블**이다. 브레드보드 전원 모듈을 사용할 경우에는 모듈의 출력 전압·전류 정격을 먼저 확인한다. 아래 배선도는 센서 전원과 ESP32 전원을 분리하고 GND만 공유하는 제안이다.

**아래 GPIO15/16과 UART2는 제안값이다. 실제 연결·핀 사용 여부가 검증된 값이 아니며, 배선 전에 해당 보드의 실크와 현재 회로를 대조한다.** GPIO 번호는 헤더에서 센 순서가 아니다.

```text
외부 regulated 5V ───────────── C1 VCC
외부 전원 GND ───────┬───────── C1 GND
                    └───────── ESP32 GND

C1 TX ──────────────────────── ESP32 GPIO15 (UART2 RX 제안)
C1 RX ◀─────────────────────── ESP32 GPIO16 (UART2 TX 제안)

Mac ── USB ── ESP32 COM          ← ESP32 자체 전원 / 이후 콘솔

외부 +5V와 ESP32의 +5V는 이 제안에서 서로 연결하지 않는다.
C1 USB 어댑터는 이 UART 데이터 경로에서 분리한다.
```

| C1 신호 | 연결 대상 | 설명 |
| --- | --- | --- |
| `VCC` | 충분한 전류 용량의 regulated 5V | 센서 전원. GPIO나 3V3에 연결하지 않음 |
| `GND` | 외부 전원 GND + ESP32 GND | 통신 전압의 기준을 공유 |
| `TX` | ESP32 `RX` — GPIO15 제안 | 센서가 보내는 데이터를 ESP32가 수신 |
| `RX` | ESP32 `TX` — GPIO16 제안 | ESP32가 보내는 명령을 센서가 수신 |

공식 키트 문서의 배선 색은 VCC=빨강, TX=노랑, RX=초록, GND=검정이다. 커넥터는 XH2.54-5P로 설명되지만, **실물 커넥터를 어느 방향에서 보는지 확인하지 않고 핀 순서나 남는 접점의 용도를 추측하지 않는다.** 색상만 믿지 말고 센서·케이블의 핀 정의를 대조한다. [C1 키트 매뉴얼 p.12~13][c1-kit]

### 4.3 안전하게 구성할 순서

다음은 위 회로의 배선 체크리스트다. 멀티미터가 없으므로 현재 단계에서 실제 전압·접점 연속성을 측정했다고 판단하지 않는다.

1. **전원을 모두 분리한 상태에서** 배선한다. ESP32 양쪽 헤더를 같은 연결 줄에 단락시키지 않도록 중앙 홈을 사이에 두고 배치한다. 보드가 너무 넓으면 억지로 꽂지 말고 점퍼선으로 연결한다.
2. 전원 레일이 어디까지 연결되는지 해당 브레드보드 배선도를 확인한다. 중간이 나뉜 제품도 있으므로 길게 인쇄된 색선만으로 전체 연결을 가정하지 않는다.
3. C1에 맞는 5V 전원과 GND를 배선하고, **ESP32와 GND만 공통**으로 연결한다. 이 제안에서는 Mac USB 전원과 외부 5V의 양극을 직접 묶지 않는다.
4. TX→RX, RX←TX를 연결한다. 센서 USB 어댑터가 C1 RX를 동시에 구동하지 않도록 분리한다. **USB 어댑터 TX와 ESP32 TX라는 두 출력을 같은 센서 RX에 함께 연결하지 않는다.**
5. 짧고 안정적인 전원 배선을 사용한다. C1의 기동 전류를 작은 브레드보드 전원 모듈이나 가는 점퍼선이 감당한다고 추정하지 않는다.
6. 전원·극성·핀 배치를 다시 확인한 뒤, 후속 시험 펌웨어로 정보 요청부터 시작한다.

센서 전원 조건은 [C1 데이터시트][c1-datasheet], TX/RX 설정은 [ESP32 UART 문서][uart-doc]에 근거한다. 이 체크리스트는 실물 배선 검증을 대신하지 않는다.

### 4.4 UART 펌웨어와 기존 Mesh 배선 충돌 방지

ESP-IDF의 `driver/uart.h`와 `esp_driver_uart` 컴포넌트를 사용한다. 이후 설정 흐름은 `uart_param_config()` → `uart_set_pin()` → `uart_driver_install()`이며, `uart_write_bytes()` / `uart_read_bytes()`로 바이트를 주고받는다. 값은 **460800 / 8N1 / hardware flow control 없음**으로 계획한다. [Espressif UART 문서][uart-doc] · [C1 데이터시트][c1-datasheet]

현재 [bridge_runtime.c](../../../firmware/esp32/main/bridge_runtime.c)는 **UART1 / TX GPIO17 / RX GPIO18 / 115200**을 STM32↔Mesh 통신에 사용한다. [현재 핀 기록](../../hardware/wiring.md)도 이 경로를 설명한다. 따라서 C1용으로 기존 UART1을 재초기화하거나 17/18에 센서를 끼워 넣지 않는다. **UART2 + 별도 GPIO**를 검토하는 이유다.

UART 연결은 네이티브 USB PHY를 쓰지 않는다. 따라서 USB Host 방식에 필요한 PHY 전환은 없지만, 로그를 COM으로 옮기려면 여전히 앱 콘솔 설정을 별도로 바꿔야 한다. GPIO15/16의 최종 배정과 UART2 사용은 다른 주변장치의 점유 여부까지 확인한 뒤 확정한다. [ESP32 UART 문서][uart-doc] · [현재 Layer 8 소스](../../../firmware/esp32/main/main.c)

## 5. 두 방식에서 공통으로 필요한 C1 프로토콜

USB가 인식되거나 UART 바이트가 들어오는 것만으로 각도·거리값이 만들어지는 것은 아니다. **C1 명령과 응답을 해석하는 코드**가 필요하다. SLAMTEC의 공식 C++ SDK는 C1을 지원하며, 장치 정보·상태·지원 스캔 모드 조회와 스캔 데이터 해석의 기준으로 사용할 수 있다. ESP-IDF에 그대로 추가해 빌드가 끝난다는 뜻은 아니며, 전송 계층과 실행 환경에 맞는 이식 또는 필요한 프로토콜 구현이 필요하다. [공식 SDK][sdk] · [SDK 드라이버 소스][sdk-driver]

| 초기 확인 명령 | 송신 바이트 | 확인하려는 것 |
| --- | --- | --- |
| `STOP` | `A5 25` | 스캔 중지 요청. 그 자체의 응답을 성공 조건으로 삼지 않음 |
| `GET_INFO` | `A5 50` | 모델·펌웨어·하드웨어 버전·일련번호 |
| `GET_HEALTH` | `A5 52` | 센서 상태·오류 코드 |

명령 바이트는 [공식 command 정의][sdk-cmd]와 [protocol sync 정의][sdk-protocol]에서 확인했다. 응답 descriptor의 길이·종류를 검증한 뒤 payload를 읽어야 하며, USB 수신 callback 한 번이나 UART read 한 번이 센서 패킷 하나와 일치한다고 가정하지 않는다.

스캔은 장치가 지원하는 모드와 응답 형식을 먼저 확인한다. SDK의 `getAllSupportedScanModes()`, `getTypicalScanMode()`, `startScan()` / `startScanExpress()`가 참고 지점이다. **모든 C1 수신을 무조건 옛 5바이트 Standard Scan으로 파싱한다는 설계를 확정하지 않는다.** [SDK의 모드 조회·선택 구현][sdk-driver]

**C1의 모터는 스캔 명령에 연동된다.** C1 데이터시트 p.14는 모터를 단독으로 시작·정지하는 방식이 아니라고 설명한다. 전원만 연결했을 때 회전하지 않는 현상은 전원 부족의 증거가 아니다. 키트 매뉴얼 일부의 `MOTOCTL` 문구나 A1/A2/A3 예제의 DTR/PWM 배선을 그대로 C1에 적용하지 않는다. [C1 데이터시트 p.14][c1-datasheet] · [SDK의 모델별 모터 설명][sdk]

## 6. 멀티미터가 없을 때의 후속 검증 계획

아래는 **나중에 시험 펌웨어를 준비한 뒤** 수행할 계획이다. 성공한 단계만 따로 기록한다.

| 단계 | USB 방식 | UART 방식 | 이 단계가 증명하지 않는 것 |
| --- | --- | --- | --- |
| 1. 연결 준비 | 포트·케이블·VBUS 공급 경로 확인 | 핀·전원·공통 GND 확인 | 실제 센서 응답 |
| 2. 통신 경로 | enumeration, VID/PID, VCP 열기 | UART 설정 완료 | USB 어댑터 인식만으로 C1 구동 성공 |
| 3. 센서 식별 | `GET_INFO` 응답 해석 | 동일 | 모터 기동 시 전원 안정성 |
| 4. 상태 | `GET_HEALTH` 응답 해석 | 동일 | 유효 거리 측정 |
| 5. 스캔 | 회전 + 유효 각도·거리 수신 | 동일 | 장시간·최악 조건에서의 신뢰성 |
| 6. 지속 시험 | 예: 5분간 수신, disconnect·오류·리셋 기록 | 예: 5분간 수신, UART 오류·리셋 기록 | 전압·ripple의 규격 충족 |

5분은 이 문서가 제안하는 초기 시험 시간이며 제조사의 인증 기준이 아니다. 관찰 대상의 위치를 바꾸었을 때 해당 각도의 거리값도 달라지는지 확인한다. **모터만 회전**, **바이트만 수신**, **올바른 거리값 수신**은 서로 다른 결과다.

문제 분리는 다음 순서로 제안한다.

- **USB 미인식:** VBUS 공급, Host/Device 역할, 데이터 케이블, 어댑터 드라이버를 각각 확인한다.
- **인식되지만 `GET_INFO` 무응답:** 460800/8N1, 어댑터↔C1 연결, 송신 명령·수신 버퍼를 확인한다.
- **UART 무응답:** TX/RX 교차, GND, GPIO 실크, 센서 전원을 확인한다.
- **스캔 시작 시 끊김·재부팅:** 전원 부족 가능성과 소프트웨어 오류를 분리한다. 이 증상만으로 전원 문제라고 단정하지 않는다.

멀티미터 없이 지속 스캔에 성공하면 **그 시험 조건에서 동작했다**고 말할 수 있다. 실제 입력 전압·순간 전압 저하·ripple은 여전히 미측정이다. 센서를 Mac에 직접 연결해 공식 SDK로 시험하는 방법도 센서·어댑터를 따로 확인하는 데 도움이 되지만, 그 성공이 ESP32의 VBUS 공급 능력을 증명하지는 않는다. [공식 SDK의 macOS 지원][sdk]

## 7. 후속 구현 전 확인 목록

- [ ] 실제 ESP32 보드 제조사·리비전과 `USB` / `COM` 식별
- [ ] 실제 C1 USB 어댑터 칩·VID/PID 및 케이블의 데이터 연결 확인
- [ ] USB를 선택하면 VBUS 공급·보호·역류 방지와 Host 연결 구성 확인
- [ ] UART를 선택하면 C1 커넥터 방향·핀과 외부 5V 전원 확인
- [ ] UART2 / GPIO15·16 후보가 현재 다른 장치와 충돌하지 않는지 확인
- [ ] 기존 UART1 GPIO17·18 STM32↔Mesh 연결 보존
- [ ] USB Host를 선택하면 Layer 8 콘솔을 COM/UART0로 옮길 범위 검토
- [ ] 대상 보드·기존 펌웨어·NVS 백업 범위를 확정한 뒤 별도 승인을 받아 플래시
- [ ] 실제 검증 로그에는 장치 인식 / 정보 응답 / 스캔 / 지속 시험을 구분해서 기록

## 8. 공식 출처

1. [SLAMTEC C1 제품 사양][c1-spec] — C1의 TTL UART 및 baud rate.
2. [SLAMTEC C1 데이터시트 v1.0][c1-datasheet] — 전기 특성, 통신, 모터 동작. 페이지는 PDF 표시 기준.
3. [SLAMTEC C1 개발 키트 매뉴얼 v1.0][c1-kit] — CP2102 어댑터, 커넥터·신호 정의.
4. [SLAMTEC RPLIDAR SDK][sdk] — C1 지원, 호스트 예제, 프로토콜·모드 선택.
5. [ESP-IDF USB Host 문서][usb-host] / [ESP-USB PHY 설명][usb-phy] — Host 계층, 전원·PHY 제약.
6. [ESP-IDF v5.5.5 VCP 예제][vcp-example] / [소스][vcp-code] / [USB 핀 배정][usb-pins].
7. [ESP-IDF UART 문서][uart-doc] — UART 설정·송수신 API.
8. [YD-ESP32-S3 제조사 V1.4 회로도][board-sch] — 포트·전원 경로. 현재 실물과 동일하다는 보장은 없음.
9. [Texas Instruments USB Type-C 역할 설명][type-c] — 데이터 역할과 전원 역할 구분.
10. [BusBoard Prototype Systems BB830][breadboard] — 브레드보드 접점·레일·제품별 정격의 예.

[c1-spec]: https://www.slamtec.com/en/c1/spec
[c1-datasheet]: https://wiki.slamtec.com/download/attachments/83066883/SLAMTEC_rplidar_datasheet_C1_v1.0_en.pdf?api=v2&modificationDate=1700531479533&version=1
[c1-kit]: https://wiki.slamtec.com/download/attachments/83066883/SLAMTEC_rplidarkit_usermanual_C1_v1.0_en.pdf?api=v2&modificationDate=1700531526658&version=1
[sdk]: https://github.com/Slamtec/rplidar_sdk
[sdk-driver]: https://github.com/Slamtec/rplidar_sdk/blob/master/sdk/src/sl_lidar_driver.cpp
[sdk-cmd]: https://github.com/Slamtec/rplidar_sdk/blob/master/sdk/include/sl_lidar_cmd.h
[sdk-protocol]: https://github.com/Slamtec/rplidar_sdk/blob/master/sdk/include/sl_lidar_protocol.h
[usb-host]: https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/usb_host.html
[usb-phy]: https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_host.html#external-phy-configuration
[vcp-example]: https://github.com/espressif/esp-idf/tree/v5.5.5/examples/peripherals/usb/host/cdc/cdc_acm_vcp
[vcp-code]: https://github.com/espressif/esp-idf/blob/v5.5.5/examples/peripherals/usb/host/cdc/cdc_acm_vcp/main/cdc_acm_vcp_example_main.cpp
[usb-pins]: https://github.com/espressif/esp-idf/blob/v5.5.5/examples/peripherals/usb/README.md
[uart-doc]: https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/uart.html
[board-sch]: https://github.com/vcc-gnd/YD-ESP32-S3/blob/main/5-public-YD-ESP32-S3-Hardware%20info/YD-ESP32-S3-SCH-V1.4.pdf
[type-c]: https://www.ti.com/document-viewer/lit/html/SSZTB15/GUID-9C081873-D60C-462A-ABFB-9A4E87F564F0
[breadboard]: https://www.busboard.com/BB830
