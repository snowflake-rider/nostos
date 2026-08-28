## 동작 확인 체크리스트

2026-08-28 최신 사용자 확인: **버튼 1·2·3·4의 MP3 음성 청취·RGB·버저 동작 모두 PASS.**
확인 내용: "이제 됏어 다 들려. 1,2,3,4 다 rgb 됐어."
버저 3·4번 확인 대기 안내에 대한 추가 확인: "3,4,도 다 잘돼".
현재 `BUTTON_OUTPUT_TEST=ON` 진단 펌웨어이며 센서 자동 이벤트와 원격 메시지 출력은 꺼져 있다.
아래 체크는 실제 사용자 관찰과 소프트웨어 검사를 구분한다. 버튼 PASS 기록 당시에는
펌웨어·핀·배선을 변경하거나 추가 재생/reset 시험을 하지 않았다.
이후 B6 ESP32만 아래와 같이 새로 설치했다. STM32 진단 모드는 그대로다.

### B6 Layer 8 재설치 — 2026-08-28

- [x] USB serial `44:1B:F6:FF:BA:B4`로 B6 식별.
- [x] 사용자 요청에 따라 B6 전체 flash 삭제(NVS/Mesh 등록 정보 포함).
- [x] 현재 Layer 8 빌드 성공, bootloader/partition table/app 새 설치 및 세 영역 digest 검증.
- [x] `esp32s3_layer_8`의 `BOOT_START`와 `APP_STARTED`, USB `status` 응답 확인.
- [x] UART1 설정 확인: TX=GPIO17, RX=GPIO18, 115200/8N1. 실제 데이터 전달 판정은 별도.
- [x] B6 Mesh 재등록 및 AppKey/model 설정 — `ESP32-L8-BAB6`, 새 주소 `0x0006`, net=0, app=1(`events`), pub=C001, event_ready=1, sub_C001=1, TTL=7, period/retransmit=0.
- [x] 재등록 후 STM32→D6→Mesh→B6 전달 — 버튼 4 `0x13`의 B6 MESH_RX 및 UART_TX 로그 확인. 상대 STM32의 실제 수신/출력 판정은 아님.
- [x] 사용자 4→3→2→1 조작 후 B6 누적 Mesh 수신 4건, UART 송신 accepted 4건, 관련 failed/invalid=0 확인. 3·2·1의 개별 ID 로그는 관찰기 종료 후라 미기록.
- [ ] B6 재등록 후 반복 전달 안정성 — 첫 버튼 4는 76만 수신, B6 미수신. 후속 시험에서 B6 수신 회복; 첫 누락 원인 미확정.
- [x] nRF Mesh Export `Nostos.json` 비공개 로컬 보관 — 원본과 복사본 일치, B6 `0006`/C001 설정 및 키 필드 포함 확인. Import 복원 시험은 별도. 저장 위치는 [설정 가이드](../settings/NRF_MESH_SETUP.md)에 기록.

D6·76·STM32에는 erase/flash/reset을 실행하지 않았다. 초기 설치 후 첫 관찰에서는
USB 응답이 없었으나, B6에 esptool USB reset 절차를 적용한 뒤 위 부팅/상태를 확인했다.
[설치 증거](results/b6-layer8-install-1189fty3/RESULT.md).
[재등록·실제 수신 증거](results/b6-rejoin-8vnbdmzz/RESULT.md).

### 버튼·RGB·버저 부품 시험

- [x] 버튼 1 RGB 동작 — 사용자 PASS. 시험 설정: 빨강, 약 2초.
- [x] 버튼 2 RGB 동작 — 사용자 PASS. 시험 설정: 초록, 약 2초.
- [x] 버튼 3 RGB 동작 — 사용자 PASS. 시험 설정: 파랑, 약 2초.
- [x] 버튼 4 RGB 동작 — 사용자 PASS. 시험 설정: 흰색, 약 2초.
- [x] 버튼 1: 짧은 버저 2회 — 앞선 사용자 PASS.
- [x] 버튼 2: 짧은 버저 2회 — 앞선 사용자 확인.
- [x] 버튼 3: 짧은 버저 2회 — 추가 사용자 확인 PASS.
- [x] 버튼 4: 짧은 버저 2회 — 추가 사용자 확인 PASS.

버튼 1~4의 UART→Mesh 전달은 앞선 별도 시험에서 확인했다. 버튼 4는 재시험 2회.
진단 도중 76번 ESP32가 USB에서 보이지 않은 구간이 있었으나 마지막 재확인에서는 다시
식별됐다. 이번 최종 PASS는 음성/RGB 관찰 결과이며 Mesh 전달 재검증을 뜻하지 않는다.

### 낙상 감지

- [ ]  넘어짐 감지 후 일정 시간이 지나면 LED가 빨간색으로 점멸한다.
- [ ]  넘어짐 감지 후 일정 시간이 지나면 부저가 빠른 주기로 울린다.

### 후방 장애물 감지

- [ ]  초음파 센서가 후방 장애물을 감지하면 LED가 노란색으로 점멸한다.
- [ ]  후방 장애물 감지 시 부저가 여러 차례 울린다.
- [ ]  장애물 감지가 해제되면 LED가 녹색 상태로 돌아온다.

### MP3 재생

- [x] 버튼 1을 누르면 할당된 MP3가 재생된다. — 실제 청취 사용자 PASS.
- [x] 버튼 2를 누르면 할당된 MP3가 재생된다. — 실제 청취 사용자 PASS.
- [x] 버튼 3을 누르면 할당된 MP3가 재생된다. — 실제 청취 사용자 PASS.
- [x] 버튼 4를 누르면 할당된 MP3가 재생된다. — 실제 청취 사용자 PASS.
- [x] 이어폰 또는 스피커를 연결했을 때 VS1003B의 재생 출력을 확인할 수 있다. — 사용자 청취 확인; 사용한 출력 장치 종류는 미기록.

### 오디오 진단 기록

- [x] 사용자 확인: 오디오 7개 신호는 PB/PC Morpho 핀에 직접 연결. 괄호 D번호는 개인 표시.
- [x] `.ioc`/GPIO/SPI2 코드가 제공한 PB/PC 핀 표와 일치. 핀 설정 변경 없음.
- [x] 실제 VS1003B 드라이버 호스트 회귀 테스트: 2528바이트 후 DREQ LOW 정지,
  2초 타임아웃, tick wrap, 일시적인 흐름 제어, 재초기화, SPI 오류 처리. ASan/UBSan 전체 5/5.
- [x] 기본/진단 MCU 빌드 성공, 이전 전체 Flash 백업, 새 진단 ELF flash/verify/reset 성공.
- [x] 정상 SCI 값 관찰 — MODE=0800, STATUS=0033, CLOCKF=9800, VOL=5050,
  AUDATA=3E80, DECODE_TIME=2 및 서비스 audio=OK 확인. 지속적인 초기화 성공을 보장하는 판정은 아니다.
- [ ] 코덱 reset 후 초기화의 반복 안정성 — 진단 중 정상 응답과 MODE_MISMATCH가 모두 관찰됨.
- [ ] 별도 SDI echo 진단 명령의 실행 확인 — 진단 당시 초기화 실패로 명령 전송 전 중단.
- [ ] 내장 사인 시험음 청취 — 아직 실행하지 않음.
- [x] 수정 후 실제 버튼 1~4의 MP3 청취 — 최종 사용자 PASS.
- [ ] 장시간 반복/전원 재인가 후 재생 안정성 — 별도 시험 미실시.

### 속도계 센서

- [ ] Read XOSS sensor

앞선 로그에는 DOWN 19341바이트 공급 완료가 있었지만 실제 음성은 들리지 않았다.
그 뒤 재부팅 관찰에서는 먼저 누른 **1번**이 2528바이트/DREQ LOW에서 멈췄고,
후속 2번은 BUSY_SKIPPED였다. 이는 '2번을 최초 재생한 시험'이 아니다.
무한 BUSY 처리는 수정했다. 이후 사용자가 배선을 다시 확인하고 마지막에 네 버튼의 음성·
RGB·버저가 모두 동작한다고 확인했다. **최종 실물 판정은 PASS로 갱신**하며, 앞선 무음/초기화
실패의 원인이나 어느 조치로 회복됐는지는 확정하지 않는다.

추가 진단 이력: SPI를 1MHz→125kHz로 잠시 낮춰도 초기화 실패가 있었으며 1MHz 복구를
레지스터로 확인했다. RESET 핀은 시험 후 HIGH로 복구했다. 이 진단에서는 펌웨어·핀 매핑을
변경하지 않았다. 원시 로그는 `results/codec-l7sm0pzc/`, `results/codec-slow-spi-573wvggb/`,
`results/codec-reset-lines-fhz980lt/`, `results/codec-tobr3ad2/`, `results/codec-hx5sl54c/`에 있다.

도구: [testing 안내](README.md). 최신 증거:
`../stm32-project/integration/stm32/build/codec-diagnosis._udglp4l/`.
로그의 데이터 완료/명령 전달 성공이나 스크립트 종료 코드는 실제 음성 PASS가 아니다.
