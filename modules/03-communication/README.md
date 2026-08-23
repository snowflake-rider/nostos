# Module 03 - Communication

STM32와 통신 보드 사이 UART 및 BLE/Bluetooth Mesh를 담당한다.

## 개발 순서

1. STM32 UART 송수신 확인
2. ESP32-S3와 Pico 2 WH에서 UART 수신 확인
3. 두 보드의 기본 BLE 비교
4. 동일 보드 3개로 Bluetooth Mesh 시험
5. 최종 보드 선택
6. 최종 통신 펌웨어를 세 노드에 적용

## 폴더

- `stm32/`: UART 시험용 CubeMX 프로젝트
- `esp32-s3/`: ESP-IDF 기반 통신 펌웨어
- `pico2/`: Pico SDK 기반 비교 펌웨어
- `docs/`: 배선, 시험 결과와 문제 해결 기록
