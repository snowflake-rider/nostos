# STM32 Team Project

팀원 3명이 NUCLEO-F411RE 보드를 한 개씩 맡아 독립 모듈을 만들고, 마지막에 공통 통신 규칙으로 연결하는 프로젝트다.

```text
Module 01: 센서 → STM32 #1 → UART → 통신 보드 #1
Module 02: 센서 → STM32 #2 → UART → 통신 보드 #2
Module 03: 입력 → STM32 #3 → UART → 통신 보드 #3
                                         ))) BLE / Bluetooth Mesh
```

## 문서

- [CubeMX 프로젝트 시작 방법](CUBEMX_SETUP.md)
- [프로젝트 폴더 구조](PROJECT_STRUCTURE.md)
- [공통 규칙](common/README.md)
- [모듈 목록](modules/README.md)
- [통합 시험](integration/README.md)

## 핵심 원칙

- 팀원 한 명이 STM32 보드 한 개와 자기 모듈을 맡는다.
- 각 `stm32/` 폴더에는 독립적으로 빌드 가능한 전체 CubeMX 프로젝트를 둔다.
- 센서 드라이버는 각 모듈에서 관리한다.
- UART, 메시지 형식, 노드 설정처럼 세 모듈이 함께 쓰는 코드와 규칙은 `common/`에서 관리한다.
- `03-communication`에서 개발한 최종 통신 펌웨어는 세 통신 보드에 공통 적용한다.
