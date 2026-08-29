> 보존 원문: `esp-ble-unorganized/README.md`. 당시 경로·명령·검증 결과이며 현재 실행 안내는 저장소 README를 따릅니다.

# ESP32 BLE — 분산 센서와 공유 대시보드

여러 라이더의 보드에 센서를 나누어 연결하고, 공통 ESP32 통신 모듈과 Bluetooth Mesh를 통해 같은 데이터 항목을 함께 사용하는 프로젝트다. 현재 무선 학습 대상은 **ESP32-S3**이며, ESP32-C3 자료는 별도 참고 예제다.

## 여기서 시작

| 지금 하려는 일 | 먼저 읽을 문서 |
| --- | --- |
| 프로젝트 목표·센서 배치·결정 사항 이해 | [프로젝트 개요](../project/OVERVIEW.md) |
| 프로젝트 생성부터 BLE까지 순서대로 학습 | [학습 시작](../learning/README.md) |
| ESP32-S3 세 대로 Bluetooth Mesh network 재현 | [Mesh network manual](../../hardware/how-to-make-mesh-network.md) |
| STM32 버튼 메시지를 ESP32 Mesh로 전달 | [Layer 8 — UART ↔ Mesh](../../../firmware/esp32/docs/layer8-background.md) |
| STM32 → ESP32 → Mesh 메시지 형식과 생성 과정 이해 | [Message Protocol](../../architecture/message-protocol/reference.md) |
| 8종 메시지의 실제 UART → Mesh 그룹 수신 검사 | [Testing](testing/README.md) |
| ESP32-S3-N16R8 GPIO·모듈 패드·UART 핀 확인 | [핀 배치와 전체 핀 정의표](../../hardware/esp32-s3-pinout.md) |
| STM32 공용 펌웨어·버튼/UART 실물 진행 확인 | [Bike Swarm Guard STM32](../legacy-stm32/README.md) |
| iPhone GPS를 Mesh로 공유하는 앱 확인 | [GPS Mesh iPhone 앱](../../../apps/ios-gps-mesh/README.md) |
| 팀원용 통신 모듈 API 사용·개발 | [Communication Module](../../../experiments/communication-module/README.md) |
| 구현·실물 검증 범위와 남은 작업 확인 | [진행 상태와 검증 기록](../project/STATUS.md) |

**지금 통신 모듈을 개발한다면:** [프로젝트 개요](../project/OVERVIEW.md) → [모듈 README](../../../experiments/communication-module/README.md) → [Service API](../../../experiments/communication-module/service/README.md) 순서로 읽는다.

학습을 처음 시작한다면: [01. 프로젝트 생성·빌드](../learning/01-project-start.md) → [02. Flash·부팅](../learning/02-hardware-bringup.md) → [03. BLE Advertising](../learning/03-ble-advertising.md) → [Layer 로드맵](../learning/LAYER-ROADMAP.md) 순서로 읽는다.

## 전체 구조

```text
esp-ble/
├── README.md                         # 공통 시작 메뉴
├── manual/
│   ├── README.md                     # 실물 작업 manual 목록
│   └── how-to-make-mesh-network.md   # nRF Mesh로 세 ESP32-S3 network 구성
├── how-to-make-mesh-network.md       # 이전 경로 호환 안내
├── docs/
│   ├── 01-project/
│   │   ├── OVERVIEW.md               # 목표·합의·제안·미결정 사항
│   │   └── STATUS.md                 # 진행 상태·남은 검증·기존 실행 증거
│   ├── 02-learning/
│   │   ├── README.md                 # 학습 순서
│   │   ├── 01-project-start.md
│   │   ├── 02-hardware-bringup.md
│   │   ├── 03-ble-advertising.md
│   │   └── LAYER-ROADMAP.md           # Layer별 목적과 통과 기준
│   ├── 03-reference/
│   │   ├── ESP32-S3-N16R8-PINOUT.md  # 모듈 핀 정의·원본 이미지·UART 연결 메모
│   │   ├── TERMS.md
│   │   ├── BUILD-AND-BOOT.md
│   │   ├── ESP-BLE-MESH-C3.md
│   │   └── IMAGE-MANIFEST.md
│   ├── 04-records/
│   │   ├── README.md                 # 학습·설계 기록 목록
│   │   ├── BRINGUP-NOTES.md
│   │   └── MY_UNDERSTANDING.md
│   └── superpowers/                  # Layer·GPS 설계와 구현 계획 원본
├── apps/
│   └── ios-gps-mesh/                 # iPhone GPS → GATT Proxy → Mesh 앱
├── communication-module/            # 공통 C API와 코드 옆 상세 README
├── message-protocol/                # STM32 UART 1바이트 → Mesh payload 형식과 코드 흐름
├── layers/                           # Layer 0–8 독립 ESP-IDF 학습 프로젝트와 로그
├── examples/
│   ├── esp32c3/                      # 별도 ESP32-C3 참고 예제
│   └── esp32s3/gps-mesh-node/        # iPhone GPS 수신용 ESP32-S3 확장 예제
├── stm32-project/                    # STM32 공용 펌웨어와 Event Mesh bridge 원본
├── images/                          # 원본 이미지
└── scripts/                         # 기존 환경·장치 확인 스크립트
```

각 프로젝트의 소스·헤더·CMake·설정 파일과 `build*` 디렉터리는 기존 위치를 사용한다. 문서 디렉터리에서 빌드하지 않으며, 각 안내에 적힌 **실행 위치**를 따른다.

## 필요할 때 찾아보기

- [BLE / Mesh 용어](../../architecture/glossary.md)
- [빌드 파일·sdkconfig·부팅 개념](../reference/BUILD-AND-BOOT.md)
- [ESP32-C3 Mesh 한국어 참고 설명](../reference/ESP-BLE-MESH-C3.md) · [C3 Generic OnOff 예제](../../../experiments/examples/esp32c3/generic-onoff-node/README.md)
- [이미지 출처와 SHA-256](../reference/IMAGE-MANIFEST.md)
- [직접 작성한 이해·질문과 설계 기록](../records/README.md)
- [Layer 8 설치·실물 검증 기록](../../verification/layer8-latest-import.md)
- [현재 버튼 배선용 빠른 Layer 8 관찰](../../../firmware/esp32/docs/FAST_CHECK.md)
- [iPhone GPS Mesh 구현 검증 기록](../designs/plans/2026-08-28-iphone-gps-mesh-verification.md)

## 문서를 읽고 갱신하는 원칙

- **목표와 제안:** 프로젝트 개요에서 확인한다. 설계 문서가 존재한다고 구현이 완료된 것은 아니다.
- **현재 진행 상태:** `STATUS.md`에 기록한다. 학습 로드맵에는 실제 PASS 결과를 반복해서 적지 않는다.
- **API 사용 계약:** 통신 모듈의 코드 옆 README에서 관리한다.
- **실행 증거:** 소스, 빌드, Flash, 부팅, 직접 수신, Relay를 따로 기록한다. 기존 기록은 당시 결과이며 이번 실행의 성공을 보장하지 않는다.
- **전송 방식:** Layer 6의 custom Advertising forwarding과 Layer 7의 표준 Bluetooth Mesh는 별도 실습이다. Layer의 성공은 통신 모듈 통합 성공으로 자동 승계되지 않는다.
- **복사본 경계:** `stm32-project/integration/esp32-s3`는 팀 통합 원본이고, Layer 8은 독립 학습용 복사본이다. 계약 변경 시 두 경로를 함께 확인한다.

`docs/superpowers/`는 기존 Layer 문서의 링크를 지키기 위해 경로를 유지한다. 처음부터 읽을 필요는 없으며 [기록 목록](../records/README.md)에서 필요한 설계·계획을 찾는다.
