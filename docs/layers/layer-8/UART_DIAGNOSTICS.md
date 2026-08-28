> 이관 원문: `layers/layer-8/UART_DIAGNOSTICS.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Layer 8 — 읽기 전용 UART 진단

2026-08-28 추가. `status` 응답에서 실제 UART 레지스터/핀 설정을 읽는다.
UART 메시지, 핀 설정, pull-up/down, Mesh 키·그룹을 변경하지 않는다.
GPIO interrupt를 추가하지 않으며 UART FIFO를 소비하거나 비우지 않는다.

## 실행

시리얼 포트는 한 프로그램만 연다. 기존 fast-check/monitor를 종료한 다음:

```bash
cd /Users/kafka/Workspace_AI/esp-ble/layers/layer-8
/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python tools/check_uart_diag.py --board D6
/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python tools/check_uart_diag.py --board 76
```

USB serial로 보드를 찾으며 DTR/RTS를 토글하지 않는다. `status`만 보내고 최대
2초 안에 포트를 해제한다. `DIAG_PRESENT`는 진단 응답을 읽었다는 뜻이지,
STM32 → ESP32 UART 통신 성공이 아니다.

일반 콘솔이나 `fast-check`에서 보내는 기존 `status`에도 같은 줄이 포함된다.
`fast-check`의 판정은 계속 실제 버튼 바이트·UART 수신·Mesh 수신 건수로 한다.

## 출력 해석

| 로그 | 확인 대상 |
| --- | --- |
| `UART_DIAG` | UART 번호, 실제 baud/data/parity/stop/flow, 드라이버 수신 버퍼에 남은 바이트 수 |
| `UART_DIAG_RX` | RX 입력의 matrix/IOMUX 경로, 연결 GPIO, 입력 경로 활성화 여부, 순간 입력 레벨 |
| `UART_DIAG_TX` | 설정 대상 TX GPIO의 실제 IOMUX/매트릭스 기능과 출력 enable |
| `UART_DIAG_PIN` | 배선 대상으로 지정한 GPIO18의 실제 입력·출력·pull-up/down 및 순간 레벨 |
| `UART_DIAG_ERROR` | 진단용 getter 실패. 값이 정상이라고 간주하지 않는다. |

현재 기대값은 UART1, 115200 근처의 baud, data=8, parity=none, stop=1,
flow=0(흐름 제어 없음), RX GPIO18, TX GPIO17이다.

기본 UART 핀은 **IOMUX로 직접 연결**될 수 있다. 따라서
`matrix_gpio=-1`만 보고 RX 미연결이라고 판단하면 안 된다. `route`,
`mapped_gpio`, `path_enabled`를 함께 본다. TX의 `route=other`는 지정 TX 핀이
해당 UART 신호로 설정되지 않았다는 뜻이며, 다른 GPIO로 재배치됐는지까지
전체 핀을 검색하는 기능은 아니다.

`UART_DIAG_TX output`은 GPIO enable 레지스터의 raw bit다. IOMUX 직접 경로의
주변장치 출력까지 이 bit 하나로 판정할 수 없으므로, `route=iomux output=0`을
UART 송신 꺼짐으로 해석하지 않는다.

`level`은 명령을 처리한 순간의 digital 값이다. 입력이 비활성화됐으면
`-1`로 표시한다. HIGH 한 번은 전압·접지 연속성·통신 성공의 증거가 아니고,
LOW 한 번도 단락의 확정 증거가 아니다. 짧은 UART 파형은 이 순간 조회로
놓칠 수 있다. 실제 수신은 `UART_RX id=...`와 카운터 증가로 따로 확인한다.

## 변경 및 검증 범위

- 구현: [uart_diag.c](../../../code/layers/layer-8/main/uart_diag.c), 기존 `bridge_runtime_log_status()`에서 호출.
- 실제 상태는 ESP-IDF UART/GPIO getter와 GPIO matrix 레지스터를 읽는다.
  설정 상수만 다시 출력하는 진단이 아니다.
- 기존 앱에 대해 위 실물 응답 검사에서 `FAIL: missing diagnostic status`를
  확인한 뒤 구현했다. 새 앱의 같은 검사를 재실행해 배포까지 검증한다.
- 기존 host-test는 이벤트 변환·큐·명령 parser 회귀 테스트이며 GPIO 전기 신호를
  검증하는 테스트가 아니다.
- 새 진단 기능은 Layer 8에만 추가한다. STM32, Layer 7, 팀 통합 복사본은 수정하지 않는다.

## 설치 안전 경계

실제 USB serial과 플래시 partition table을 확인하고, 각 보드의 기존
bootloader/partition/NVS/앱 영역을 `build/uart-diag-backup.*` 아래에 백업한다.
백업에는 Mesh 키가 포함될 수 있으므로 공유하거나 커밋하지 않는다.
설치는 factory 앱 주소 `0x10000`에만 수행한다. `erase_flash`, bootloader,
partition table, NVS 쓰기는 하지 않는다. 설치 후 재부팅은 필요하며,
기존 primary/AppKey/publication/subscription 값은 `status`로 재확인한다.

공식 참고: [ESP-IDF 5.5 UART getter](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32s3/api-reference/peripherals/uart.html),
[GPIO 설정 조회](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/gpio.html).
