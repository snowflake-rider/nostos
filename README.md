# Nostos

STM32에서 버튼·센서 입력과 LED·부저·음성 출력을 처리하고, ESP32-S3의 Bluetooth Mesh로 라이더 사이의 이벤트를 공유하는 팀 프로젝트입니다.

**빌드·실행하는 것은 `code/`, 읽는 자료와 검증 기록은 `docs/`에 있습니다.**

## 시작하기

- [팀원 시작 안내](docs/00-team/START.md): 기능 확인 → STM32 빌드 → ESP32 빌드 → 배선 → 버튼 송수신 확인
- [문서 목록](docs/README.md): 학습, 배선, API, 검증 기록
- [이번 이관과 검사 결과](docs/04-records/NOSTOS-MIGRATION.md)

## 코드 위치

| 위치 | 역할 |
| --- | --- |
| [code/firmware/stm32](code/firmware/stm32/) | 공용 STM32 프로젝트. `MyApp`의 기존 기능 구분을 유지합니다. |
| [code/firmware/esp32](code/firmware/esp32/) | **Layer 8 기반** 팀 배포용 UART ↔ Mesh 펌웨어. 빌드 이름은 `nostos_esp32`입니다. |
| [code/common/protocol](code/common/protocol/) | STM32와 팀 ESP32가 함께 사용하는 메시지 ID·codec·queue |
| [code/communication-module](code/communication-module/) | 별도로 개발한 이벤트·주기 처리 C API와 호스트 테스트 |
| [code/layers](code/layers/) | Layer 0–8 독립 학습 프로젝트 |
| [code/examples](code/examples/) | ESP32-C3 및 ESP32-S3 GPS 참고 예제 |
| [code/apps](code/apps/) | iPhone GPS Mesh 앱과 Swift 테스트 |
| [code/scripts](code/scripts/) | 환경 확인·모니터·저장소 검사 도구 |
| [code/legacy/esp32-event-bridge](code/legacy/esp32-event-bridge/) | 이전 통합 ESP32 소스 보존본. 팀 배포 기준이 아닙니다. |

STM32의 `nostos_stm32`와 ESP32의 `nostos_esp32`는 각각 빌드합니다. SDK·툴체인과 빌드 산출물은 저장소에 포함하지 않습니다.

## 공통 기능과 담당 기능

모든 보드가 버튼, LED, 부저, 음성 출력, UART·Mesh 통신을 공통으로 사용합니다. 센서 읽기·보정·판정은 해당 기능 담당 코드에서 처리합니다. 센서가 없는 보드도 원격 이벤트를 받아 알림을 실행하는 구조입니다.

이번 이관에서는 센서 기본값, 핀, 메시지 ID, 알고리즘을 바꾸지 않았습니다. 기존 `communication-module`과 SharedState를 새 전송 프로토콜로 통합한 것도 아닙니다.

## 검증 범위

[기존 실물 기록](docs/verified/README.md)은 당시 보드·소스·배선의 결과입니다. 새 경로에서의 빌드 성공과 새 펌웨어의 실물 시험은 구분합니다. 이관 작업에서는 보드 Flash, reset, Mesh 설정 변경을 수행하지 않습니다.
