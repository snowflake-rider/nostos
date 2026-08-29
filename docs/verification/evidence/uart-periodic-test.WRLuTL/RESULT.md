# 기존 프로젝트 UART 임시 시험 결과

최신 결과: **GPIO11 → GPIO18 배선 이동 + D6 재부팅 후, STM32의 0x13 10개를 D6 UART에서 10개 모두 수신했다.** 아래 초기 실패 기록은 비교를 위해 보존한다.

2026-08-28 14:10–14:12 KST. 새 프로젝트나 IOC를 만들지 않았다.

## 결과

- STM32 NUCLEO-F411RE: 기존 PA9/USART1 설정으로 약 1초마다 0x13을 10번 송신.
- 버튼/센서/출력 앱 처리만 임시 우회하고 기존 uart_service 전송·USB trace 경로를 사용했다.
- 실제 MCU RAM: 송신 시도 10, HAL 성공 10, HAL 실패 0.
- ST-LINK USB 진단 복사본: 바이너리 0x13 10개. 관찰 간격 약 0.996초.
- D6 ESP32 UART_RX 0x13: 관찰 0건. UART valid 1 → 1, noop 45 → 45, invalid 18 → 18, hw_errors 1 → 1.
- 76 ESP32 UART_RX 0x13: 관찰 0건. UART valid 0 → 0.
- D6의 드라이버 수신 버퍼는 관찰 시작·종료 모두 11바이트. 시험 중 증가하지 않았다.
- 기존 다른 노드(source 0x0004)의 Mesh 수신은 STM32 UART 성공 증거에서 제외했다.
- 결론: 버튼 없이도 STM32 → ESP32 UART 전달 성공을 확인하지 못했다.

## 추가 단서

- 시험 종료 후 GPIOA IDR=0x0000830C: PA9 입력 판독 HIGH.
- GPIOA MODER=0xA82801A0, AFRH=0x00000770: PA9/PA10 AF7.
- USART1 BRR=0x8B, CR1=0x202C: 기존 UART 설정 유지.
- D6 status는 GPIO18, RX path enabled=1, baud=115201, 8-N-1, flow=0.
- D6 GPIO18 레벨은 여섯 번의 status 표본에서 모두 LOW.
- 서로 다른 시점의 디지털 판독이며 실제 전압/파형/단선을 확정하지 않는다.
  다음 단계는 D8/PA9 → D6 GPIO18, 공통 GND의 실제 핀·연속성 확인이다.

## 검증과 복구

- STM32 임시 Debug 빌드 및 CubeProgrammer download verify 성공.
- 기존 호스트 테스트 3/3 PASS (ASan+UBSan).
- main.c를 시험 전 파일과 바이트 단위 동일하게 복구했다.
- 원래 Debug ELF도 재빌드 후 시험 전 ELF와 바이트 단위 동일함을 확인했다.
- 시험 전 MCU Flash 512KB를 백업하고, 시험 후 그대로 복구·verify·reset했다.
- 복구 후 전체 512KB를 다시 읽어 원본 백업과 cmp 일치를 확인했다.
- Flash SHA-256 (시험 전/복구 후 동일):
  `2ea8bf1a0ee1de1ae90fe1cb61d3bcb9307cd62f0642523bfd8e8cff459e133f`.
- STM32 Git diff/status는 시험 전과 동일하다. 기존 사용자 변경은 보존했다.
- ESP32 펌웨어·Mesh 설정, IOC, 핀 설정은 변경하지 않았다. 시리얼 포트는 모두 닫았다.
- 시험 자료만 Git에서 제외되는 이 build 하위 폴더에 남겼다. Flash 백업은 공유/커밋하지 않는다.

## 증거 파일

- `observation.log`: 설치 검증, 시각별 USB trace, ESP32 status와 집계.
- `stm32-runtime.log`: 실제 MCU 송신 카운터와 GPIO/UART 레지스터.
- `restore.log`, `restore-readback.log`: 원래 펌웨어 복구 및 다시 읽기.
- `main.c.test`, `test.elf`: 이번 임시 시험의 소스·이미지 (현재 보드에 설치된 버전 아님).

## GND 이동 후 재시험

사용자가 ESP32 GND를 브레드보드로 옮겼다고 알려 동일 test.elf를 재사용했다.
실제 레일 연결 상태는 사진/측정으로 확인하지 않았다.

- STM32 MCU 카운터: 시도 10, HAL 성공 10, 실패 0. USB 0x13 복사본 10개.
- D6의 새 UART_RX 0x13: 0건. valid 1 → 1, noop 52 → 52, invalid 23 → 23, hw_errors 1 → 1.
- 76의 새 UART_RX 0x13: 0건. valid 0 → 0.
- D6 GPIO18은 모든 status 표본에서 LOW. 시험 후 STM32 PA9의 입력 판독은 HIGH.
- GND 위치 변경 후에도 수신 성공을 확인하지 못했다. 다음은 실제 공통 GND 및 D8→GPIO18 배선 확인이다.
- source와 일반 Debug ELF는 재시험 중에도 변경하지 않았다.
- 재시험 후 원래 512KB Flash 백업을 재설치하고 전체 download verify 및 reset 성공을 확인했다.
- 재시험 증거: `observation-after-gnd.log`, `stm32-runtime-after-gnd.log`, `restore-after-gnd.log`.

## GPIO18 이동 후 시험 — UART 10/10 수신 확인

1. 사용자가 사진에서 확인된 GPIO11 신호선을 GPIO18로 옮겼다.
2. D6 GPIO18의 status 레벨은 LOW에서 HIGH로 바뀌었다.
3. 첫 시험에서는 STM32 10회 송신에도 D6 UART_RX 0회였다. 이전 수신 버퍼가 254바이트였고 STM32 reset 후 240바이트로 정체되어 있었다.
4. D6만 esptool `--no-stub run`으로 재부팅했다. ESP32 Flash/NVS/키 쓰기는 하지 않았다.
5. D6 재부팅 후 buffered=0, primary=0x0005, app=0x0001, pub=0xc001, sub_C001=1, event_ready=1을 확인했다.
6. 같은 STM32 test.elf를 다시 쓰지 않고 STM32 reset으로 유한 시험을 재시작했다.
7. STM32 USB 0x13 10개와 D6 `UART_RX id=0x13 result=queued` 10개가 시간상 대응했다.
8. MCU RAM에서 시도 10, HAL 성공 10, 실패 0을 확인했다.

대표 실제 로그:

```text
8.341s STM32: USB_TRACE hex=13
8.344s D6: I (27121) LAYER_8_UART: UART_RX id=0x13 result=queued
...
17.304s STM32: USB_TRACE hex=13
17.306s D6: I (36081) LAYER_8_UART: UART_RX id=0x13 result=queued
```

주의: STM32 reset 직후, 첫 시험 바이트 송신 전에 다른 ID 4개·noop 103개·invalid 27개가 수신됐다.
이 구간은 정상 송신 성공에서 제외했다. 첫 0x13 송신 직전과 종료 시점 사이에는
valid 4→14, noop 103→103, invalid 27→27, hw_errors 0→0, buffered 0을 확인했다.
정상 상태의 UART 전달은 확인했지만 reset 시점 잡음과 오류 후 수신 정체의 상세 원인은 별도 진단 대상이다.

Mesh API 수락 로그와 `No outbound bearer found` 경고도 있었지만, 이 시험은 상대 Mesh 수신·중계 성공을 판정하지 않는다.
기존 관찰 도구가 Mesh 원문을 집계하므로 상대의 source/id 대응 증거가 별도로 필요하다.

복구: 시험 직전에 읽은 512KB Flash가 종전 원본 백업과 같음을 확인했고,
그 이미지를 시험 후 재설치하여 전체 verify 및 reset 완료했다. source와 일반 Debug ELF는 변경하지 않았다.
새 프로젝트/IOC를 만들지 않았고, PA10 및 기존 버튼 핀 설정은 그대로다.

증거: `observation-after-gpio18.log`, `d6-reboot-gpio18.log`,
`observation-gpio18-after-d6-reset-02.log`, `stm32-runtime-after-gpio18.log`, `restore-after-gpio18.log`.
첫 재부팅 직후 status 조회가 응답하지 않았고, 그 조회가 포트를 점유한 동안 시작한 관찰 실행은
exclusive lock 오류로 STM32 reset 전에 종료됐다. 포트를 해제한 후 위 `-02.log` 시험이 정상 실행됐다.

복구 후 최종 상태: STM32 원본 Flash 복구/reset 후 D6의 buffered=240 정체가 다시 관찰됐다.
STM32를 그대로 둔 채 D6만 한 번 더 재부팅했다. 첫 status 조회는 진단 응답 누락으로 실패했고,
두 번째 조회에서 uptime=17941ms, buffered=0, GPIO18 HIGH, 기존 Mesh 설정 유지가 확인됐다.
이 마지막 조회는 수신 준비 상태 확인이며 새로운 UART 전달 시험은 아니다.
재부팅은 수신 정체의 임시 해소이고, 원인 수정은 하지 않았다.
증거: `d6-reset-after-restore.log`, `d6-final-uart-diag.log`, `d6-final-uart-diag-02.log`.
