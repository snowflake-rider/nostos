> 이관 원문: `communication-module/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../docs/getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Communication Module

[전체 시작 메뉴](../../docs/archive/records/esp-ble-original-index.md) · [프로젝트 개요](../../docs/archive/project/OVERVIEW.md) · [진행 상태](../../docs/archive/project/STATUS.md)

팀원들이 공통 C API로 버튼 메시지와 Head의 평균 속도를 전달하고, 수신 데이터를 사용할 수 있도록 만드는 통신 모듈이다. 버튼의 STM32 경로와 Head ESP32의 Bluetooth 속도 센서 경로를 분리한다.

현재는 공통 이벤트/속도 메시지, 8칸 이벤트 큐, 최근 5개 속도 이동평균, 유효성·만료·주기 전송 로직과 이를 묶는 **송신 측 공통 API**를 구현했다. 버튼 우선 처리, 속도 기아 방지, 한 호출당 전송 시도 상한을 호스트에서 검증한다. 실제 센서 어댑터, UART 패킷 인코딩, 보드 연결, Bluetooth 송수신, Mesh Relay와 B/C 수신부는 아직 구현하지 않았다.

팀원용 API는 [service/comm.h](service/comm.h), 사용 계약은 [Service API 문서](service/README.md), 실행 가능한 예제는 [service/examples/main.c](service/examples/main.c)부터 읽는다.

확장 목표인 **센서 분산 배치 + Mesh 공유 데이터 + 동일한 대시보드**와 아이디어 리뷰는 [프로젝트 개요와 설계 리뷰](../../docs/archive/project/OVERVIEW.md)에 정리했다. 해당 문서의 SharedState와 RTOS Task 분할은 아직 구현되지 않은 제안이며, 위의 현재 구현 상태와 구분한다.

별도의 [Layer 학습 경로](../../docs/archive/learning/LAYER-ROADMAP.md)와 이 모듈의 통합 검증은 구분한다. Layer의 실제 보드 결과는 [진행 상태](../../docs/archive/project/STATUS.md)에 기록하며, 그 결과가 이 모듈의 UART·Bluetooth·Mesh Relay 성공을 뜻하지는 않는다.

## 담당 범위

- 버튼 담당 팀원: 센서 읽기, 디바운싱, 버튼 입력 해석 후 이벤트 코드 전달.
- Head 속도 경로: Bluetooth 센서의 측정값을 ESP32에서 수신·해석하고, 최근 5개의 평균만 발행한다. 이 경로는 STM32를 거치지 않는다. 센서별 서비스와 속도 계산은 이후 어댑터에서 확인한다.
- 통신 모듈 담당: 공통 C API, 이벤트 대기열, 이동평균과 주기 전송 정책, UART 패킷 처리, ESP32 통신 펌웨어, 수신 API, 오류와 상태 보고.
- 노드 사이의 통신은 Bluetooth를 사용한다. STM32가 사용하는 버튼/출력 경로는 로컬 UART로 ESP32에 연결할 계획이다.
- 현재 프로젝트의 무선 구현 대상은 ESP32-S3와 표준 Bluetooth Mesh다. 사용자 정의 Advertising 재방송이나 GATT 연결형 Relay와 혼합하지 않는다.

## 디렉터리

- [common](common/README.md): 공유 메시지 정의. 실제 전송 바이트 형식과는 구분한다.
- [event-driven](event-driven/README.md): 버튼 메시지 1, 2, 3의 발생 기반 처리.
- [periodic](periodic/README.md): 최근 5개 속도의 이동평균과 유효성·주기 기반 처리.
- [service](service/README.md): 팀원용 공통 API와 이벤트/속도 전송 순서 조정.

event-driven과 periodic은 동작을 따로 이해하고 검증하기 위한 구분이다. 이제 service가 두 동작을 하나의 API로 지원한다. 실제 wire packet 규약은 아직 별도로 구현해야 하며 두 경로가 공유한다.

## Layer가 끝나기 전에 개발할 수 있는 부분

`../layers/`는 Bluetooth 기반 기능을 단계별로 확인하는 학습·검증 경로이고, 이 디렉터리는 팀 배포용 통신 모듈의 개발 경로다. 모든 Layer 완료를 기다리지 않고 무선 하드웨어에 의존하지 않는 부분부터 개발할 수 있다.

| 지금 개발·시험할 수 있는 부분 | 하드웨어 또는 해당 Layer 검증이 필요한 부분 |
| --- | --- |
| API 계약, 메시지 코드, 값의 단위 | 실제 STM32 센서 코드와의 통합 |
| 버튼 이벤트 대기열, 용량 초과 처리 | STM32와 ESP32의 실제 UART 송수신 |
| 최근 5개 평균, 주기 판단, 오래된 값 처리 | 실제 BLE 속도 센서 및 Mesh Provisioning, AppKey/Model Bind |
| 패킷 인코딩·디코딩과 잘못된 입력 처리 | 실제 두 노드 사이 사용자 메시지 송수신 |
| 가짜 시간과 전송 대역을 이용한 로직 테스트 | 직접 수신 불가 조건에서 세 노드 Relay 검증 |

가짜 전송 대역(mock transport)은 보낼 데이터를 테스트가 기록하는 장치다. Bluetooth나 Relay를 흉내 내 성공으로 판정하는 장치가 아니다. 실제 무선 구현은 이후 같은 인터페이스에 연결하되, 무선에서 지원 가능한 크기·빈도·실패 처리를 다시 검증한다.

## 구현 순서와 현재 상태

1. `event-driven` 구현 완료: 메시지 1, 2, 3의 FIFO 큐, 잘못된 코드와 대기열 초과 처리, 호스트 테스트 및 가짜 전송 예제.
2. `periodic` 구현 완료: 가짜 속도와 시간 입력으로 최근 5개 평균, 유효성/만료, 주기 전송과 BUSY 처리를 검증한다.
3. service 구현 완료: 단일 진입 API, 설정 가능한 버튼 burst 한도, due 속도에 대한 전송 기회, 한 process당 최대 한 번의 전송 콜백, BUSY 상태 보존을 호스트에서 검증한다.
4. 공통 메시지의 wire packet 인코딩/디코딩과 잘못된 패킷 처리를 구현한다.
5. 센서 제품 확인 후 Head의 BLE 센서 어댑터를 만든다. STM32 버튼 경로의 UART 어댑터와 보드 실행 흐름도 별도로 통합·검증한다.
6. 검증된 Mesh 기능에 연결하고 수신 API/만료 처리 및 실제 송수신·Relay를 시험한다.

현재 구현은 MCU의 HAL, ESP-IDF, 실제 센서 없이 컴퓨터에서 시험하는 C11 로직이다. 이벤트 큐는 8칸을 모두 사용하는 고정 크기 ring buffer이며, 이동평균은 별도의 5칸 윈도를 사용한다. 동적 할당 없이 단일 실행 흐름에서 사용하며 ISR이나 여러 Task에서 동시에 호출하지 않는다. Periodic의 전송 주기와 만료 간격은 초기화 인자로 지정하고, 패킷 바이트 배치는 아직 확정하지 않았다.

## 호스트에서 실행

`esp-ble/` 디렉터리에서 실행한다. ESP-IDF 환경은 필요하지 않다.

```sh
cmake -S communication-module -B communication-module/build -DCMAKE_BUILD_TYPE=Debug
cmake --build communication-module/build
ctest --test-dir communication-module/build --output-on-failure
./communication-module/build/event_queue_demo
./communication-module/build/periodic_demo
./communication-module/build/comm_service_demo
```

메모리 오류 검사를 포함하려면 별도 빌드 디렉터리를 사용한다.

```sh
cmake -S communication-module -B communication-module/build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCOMM_ENABLE_SANITIZERS=ON
cmake --build communication-module/build-sanitize
ctest --test-dir communication-module/build-sanitize --output-on-failure
```

전체 CTest 항목은 7개다. `event_queue` 12개, `moving_average` 6개, `periodic` 11개, `comm_service` 12개로 총 41개의 동작 검사와 세 사용 예제를 실행한다. `periodic_demo`는 결측/평균/만료를, `comm_service_demo`는 버튼과 속도 혼합 처리 및 BUSY 뒤 최신 평균 재시도를 보여준다. 통과 결과는 호스트 로직 검증이며 실제 UART/Bluetooth 검증이 아니다.

## 공통 계약

- Periodic/Event-driven은 작업 시작 시점의 구분이다. Synchronous/Asynchronous 또는 노드 간 시간 동기화와 같은 뜻이 아니다.
- 팀원 API는 입력과 전송 처리를 분리한다. `comm_post_button()`/`comm_update_speed()`는 데이터를 접수하고, `comm_process()`만 전송 콜백을 호출한다. 콜백도 빠르게 반환해야 한다.
- 버튼 우선권은 접수 건수로 제한한다. 속도가 유효하고 due이며 전송부가 접수 가능한 조건에서 기아를 방지한다. 한 호출당 한 건이라는 상한은 초당 처리량이나 지연 상한 보장이 아니다.
- 로컬 API 접수, ESP32 접수, 상대 애플리케이션 수신은 서로 다른 상태다. 성공 반환값 하나로 합치지 않는다.
- 이벤트는 정해진 용량의 대기열에 보관하고, 가득 차면 명시적인 실패 상태를 반환한다.
- 속도는 최근 5개 측정의 평균을 사용한다. 5개 전이나 만료 후에는 valid=false이며, 정상 측정된 평균 0과 구분한다. 갱신이 끊기면 과거 평균을 정상 최신값처럼 계속 보내지 않는다.
- 주기 설정은 전송 요청 주기를 뜻한다. 무선 수신 간격이나 종단 간 지연을 보장하지 않는다.
- 표준 Mesh Relay는 ESP32의 Mesh 스택이 수행한다. 수신 메시지를 STM32에서 새 메시지로 재발행하는 방식으로 대체하지 않는다.

## 검증 기록 원칙

각 단계는 별도로 기록한다. 아직 실행하지 않은 테스트는 PASS로 표시하지 않는다.

1. 호스트 로직 테스트
2. MCU 빌드
3. 대상 보드 Flash와 Runtime
4. 실제 UART 송수신
5. 실제 Bluetooth 사용자 메시지 송수신
6. 직접 수신 불가 조건에서 Relay OFF/ON 비교

호스트 테스트 통과는 UART, Bluetooth 또는 Relay 성공의 증거가 아니다. 기존 Layer의 검증 결과도 이 모듈에 자동으로 승계하지 않는다. 같은 보드에 새 펌웨어를 Flash하면 이전 Layer 애플리케이션이 교체되므로 보드별 역할을 확인한 뒤 진행한다.

관련 문서: [Layer 로드맵](../../docs/archive/learning/LAYER-ROADMAP.md), [진행 상태와 검증 기록](../../docs/archive/project/STATUS.md).
