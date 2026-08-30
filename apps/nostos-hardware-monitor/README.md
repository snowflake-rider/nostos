# NOSTOS Hardware Monitor

STM32 3대를 동시에 비교하는 Web Monitor와 단일 보드용 OpenTUI를 제공합니다. 펌웨어 RAM과 GPIO를
약 250 ms마다 짧게 읽고 즉시 실행을 재개하므로 UART에 별도 디버그 로그를 섞지 않습니다.

## Web Monitor 실행

세 ST-LINK를 연결한 뒤 실행합니다. 프로덕션 화면을 빌드하고 `127.0.0.1:8787`에서 시작합니다.

```bash
cd apps/nostos-hardware-monitor
bun install
bun run web
```

브라우저에서 <http://127.0.0.1:8787>을 엽니다. 하드웨어 없이 화면과 조작을 확인하려면 다음을 사용합니다.

```bash
bun run web:demo
```

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
bun start -- --node connected-stm32-no-mpu-dht
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
- 마지막 메시지와 로컬/원격 router 카운터
- UART 상태와 TX/RX/invalid/dropped 카운터
- RGB, buzzer, audio 상태 및 실제 GPIO 출력
- v2 protocol 부팅 상태와 수신/중복/거부/overflow 통계

## 주의

각 보드의 `st-util` 연결 시 MCU가 한 번 재시작됩니다. Web Monitor는 보드별로 45100~45102 포트를
사용합니다. 각 샘플은 MCU를 잠깐 멈췄다가 즉시 재개하며 Flash, option bytes, OTP는 변경하지
않습니다. 모니터를 강제 종료한 뒤 보드가 멈춰 있으면 ST-Link를 다시 연결하거나 보드를 reset하십시오.
