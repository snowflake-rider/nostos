# NOSTOS Hardware Monitor

STM32 3대를 동시에 비교하는 Web Monitor와 단일 보드용 OpenTUI를 제공합니다. 펌웨어 RAM과 GPIO를
약 250 ms마다 짧게 읽고 즉시 실행을 재개하므로 UART에 별도 디버그 로그를 섞지 않습니다.

## Web Monitor 실행

처음 한 번은 저장소 루트에서 보드 인벤토리 예제를 복사하고, 각 노드의 실제 ST-Link serial을 입력한 뒤 해당 항목의
`enabled`를 `true`로 바꿉니다. 이 로컬 파일은 Git에 포함되지 않습니다.

```bash
cp firmware/inventory/boards.example.json firmware/inventory/boards.local.json
```

세 ST-LINK를 연결한 뒤 실행합니다. 프로덕션 화면을 빌드하고 `127.0.0.1:8787`에서 시작합니다.

```bash
bash firmware/tools/fw release-build stm32
cd apps/nostos-hardware-monitor
bun install
bun run web
```

브라우저에서 <http://127.0.0.1:8787>을 엽니다. 하드웨어 없이 화면과 조작을 확인하려면 다음을 사용합니다.

```bash
bun run web:demo
```

모니터는 인벤토리의 `hardwareProfile`로 각 보드의 Release ELF를 자동 선택합니다.

| hardwareProfile | Release variant |
| --- | --- |
| display ON, MPU6050 OFF, DHT11 OFF | `node1-base` |
| display ON, MPU6050 OFF, DHT11 ON | `node2-dht11` |
| display ON, MPU6050 ON, DHT11 OFF | `node3-mpu6050` |

지원하지 않는 조합은 임의의 ELF로 읽지 않고 시작 단계에서 거부합니다. Live 연결 때는 선택된 variant의
전체 application image가 보드 Flash와 일치해야 텔레메트리를 표시합니다. `web:demo`는 로컬 인벤토리
없이도 세 variant를 표시하지만 Flash나 하드웨어에는 접근하지 않습니다.

Web Monitor 기능:

- 등록된 STM32 세 대를 같은 화면에서 동시에 비교
- `Pause all`, `Reconnect all`, 보드별 reconnect
- 100/250/500/1000 ms 샘플링 주기 변경
- 실시간 버튼, FreeRTOS, queue, RGB/audio/buzzer, UART/protocol 상태
- 각 보드의 최근 상태 변화와 오류 이벤트

## 단일 보드 TUI 실행

```bash
cd apps/nostos-hardware-monitor
bun install
bun start -- --node node1
```

등록된 STM32 목록은 다음 명령으로 확인합니다.

```bash
bun start -- --list
```

하드웨어 없이 화면을 확인하려면 `bun start -- --demo`를 사용합니다.

### 키

- `p`: 관측 일시정지/재개
- `r`: debugger 재연결
- `q` 또는 `Esc`: MCU 실행을 재개하고 종료

## 관측 값

- BTN1~BTN4/테스트 버튼의 raw, debounced, armed 상태
- FreeRTOS scheduler, input/service heartbeat, queue/dispatched/reset 통계
- 마지막 메시지와 로컬 event 카운터
- UART 상태와 TX/RX/invalid/dropped 카운터
- RGB, buzzer, audio 상태 및 실제 GPIO 출력
- 단일 protocol 부팅 상태와 수신/중복/거부/overflow 통계
- STOP request 수신, STOP ACK 수신/matched/ignored, protocol TX 실패 카운터와 변화 이벤트

## 3대 STOP 실기 확인

1. 세 STM32/ESP32 pair를 연결하고 Web Monitor를 실행합니다.
2. 각 ESP32 console에서 `status`를 한 번 입력해 시작 카운터를 기록합니다.
3. 송신 STM32의 Button 3을 누릅니다. FALL 시험은 MPU6050 보드에서 별도로 수행합니다.
4. 송신 STM32에서 `protocol TX`와 `STOP ACK matched`가 증가하는지 확인합니다.
5. 다른 두 STM32에서 `STOP Requests`가 각각 증가하고 STOP 출력이 실행되는지 확인합니다.
6. 각 ESP32에서 다시 `status`를 입력해 Mesh RX/TX 완료, `stop_pending`, STOP queue overflow,
   `local_stop_ack_tx_failed`를 함께 기록합니다.

`mesh_tx_api_accepted`만 증가한 것은 전달 성공이 아닙니다. 송신 ACK, 두 수신 노드, 실제 출력까지 모두
확인해야 한 번의 end-to-end STOP 증거가 됩니다.

## 주의

각 보드의 `st-util` 연결 시 MCU가 한 번 재시작됩니다. Web Monitor는 보드별로 45100~45102 포트를
사용합니다. 각 샘플은 MCU를 잠깐 멈췄다가 즉시 재개하며 Flash, option bytes, OTP는 변경하지
않습니다. 현재 52-byte protocol 통계 구조와 일치하지 않는 오래된 ELF는 텔레메트리로 읽지 않고
연결 오류로 표시합니다. 모니터를 강제 종료한 뒤 보드가 멈춰 있으면 ST-Link를 다시 연결하거나 보드를
reset하십시오.
