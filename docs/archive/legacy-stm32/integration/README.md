> 이관 원문: `stm32-project/integration/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Integration

`stm32` 폴더는 세 팀원이 함께 사용하는 공용 NUCLEO-F411RE 펌웨어입니다.

> 현재 단일 버튼 시험은 **D10/PB6 → USART1(D8 TX / D2 RX)**를 사용합니다.
> USART2는 ST-LINK USB에 송신 바이트의 진단 복사본을 남깁니다.
> [현재 배선과 설치·검증 기록](../../../verification/stm32-button-uart.md)을 먼저 확인하세요.
> 아래의 두 STM32 간 시험과 현재 ESP32 연결 시험은 구분합니다.

새 [`esp32-s3`](esp32-s3/README.md) 폴더에는 STM32의 8종 이벤트를 UART ↔ Mesh로 전달하는 별도 펌웨어가 있습니다. 공통 codec/API는 [`common/protocol`](../../../../libs/protocol/README.md)에 있습니다. 새 경로의 소스·호스트 검사·MCU 빌드는 완료했고, Flash/실물 종단 간 수신은 [미확인 상태로 별도 기록](esp32-s3/VERIFICATION.md)합니다.

[STM32 SharedState](../../../architecture/shared-state.md)는 A·B·C의 센서값을 저장·조회하는 모듈입니다. PC 테스트와 STM32 빌드는 확인했고, 실제 센서·UART 입력과 Dashboard에는 아직 연결하지 않았습니다.

아래 완료 목록은 기존 STM32 시험 기록이며, 이번 ESP32 Mesh 경로의 하드웨어 성공을 뜻하지 않습니다.

## 검증 완료

- [x] 버튼 4개 메시지 생성
- [x] RGB LED 출력
- [x] 액티브 부저 출력
- [x] VS1003B 초기화 및 MP3 재생
- [x] USART1 단독 송수신
- [x] 센서 보드 낙차 이벤트 수신 및 출력
- [x] 센서 보드 후방 경고 수신 및 출력
- [x] 초음파 기능 ON/OFF 빌드
- [x] 통합 펌웨어 전체 빌드

## 추가 하드웨어 검증

- [ ] 한 보드에 MPU6050과 HC-SR04를 포함한 전체 배선 시험
- [ ] 같은 통합 펌웨어를 사용한 두 보드 USART 시험
- [ ] 실제 장착 상태에서 낙차 임계값 조정
- [ ] 실제 장착 상태에서 초음파 센서 유효성 재평가

## 두 보드 USART 시험

```text
보드 A PA9  → 보드 B PA10
보드 A PA10 ← 보드 B PA9
보드 A GND  ↔ 보드 B GND
```

로컬 메시지는 자기 보드에서 처리한 뒤 상대 보드로 전송됩니다. 수신한 메시지는 다시 송신하지 않으므로 두 보드 사이에서 무한 반복되지 않습니다.
