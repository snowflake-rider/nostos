> 이관 원문: `docs/03-reference/TERMS.md`. 현재 실행 경로는 [팀원 시작 안내](../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# BLE Mesh 학습 용어

[전체 시작 메뉴](../archive/records/esp-ble-original-index.md) · [학습 순서](../archive/learning/README.md)

이 문서는 현재 ESP32-S3 학습 프로젝트에서 자주 나오는 여섯 용어를 설명한다. 이 프로젝트는 ESP-IDF와 FreeRTOS 위에서 BLE의 기초부터 표준 Bluetooth Mesh의 Generic OnOff Server 노드까지 단계적으로 학습한다.

## Advertising(광고)

### 뜻

Advertising은 BLE 장치가 주변에 짧은 무선 정보를 내보내는 방식이다. 아직 특정 장치와 연결하지 않은 상태에서도 주변 장치가 이 정보를 발견할 수 있다. 정보를 받으려는 장치는 Scanning(스캔)을 수행한다.

쉽게 말하면 다음과 같다.

> Advertising은 장치가 주변을 향해 “나 여기 있어”라고 알리는 전파 방송이다.

### 이 프로젝트에서는

아직 Provisioning되지 않은 ESP32-S3 노드는 스마트폰 Provisioner가 자신을 찾을 수 있도록 정보를 내보낸다. 현재 펌웨어는 `PB-ADV`와 `PB-GATT`를 모두 켜므로, Provisioner는 광고 기반 경로 또는 GATT 기반 경로로 Provisioning을 시작할 수 있다.

Provisioning이 끝난 뒤에도 표준 Bluetooth Mesh는 무선 메시지를 전달할 때 광고 채널을 이용할 수 있다. 하지만 이것은 Bike Swarm Guard에서 정의한 사용자 패킷을 일반 BLE Advertising으로 재방송하는 방식과 같지 않다. 두 방식은 메시지 형식, 주소, 보안 키, 중복 처리, Relay 규칙이 서로 다르다.

현재 소스에서 연결되는 부분은 다음과 같다.

- `esp_ble_mesh_node_prov_enable(...)`: `PB-ADV`와 `PB-GATT`를 활성화한다.
- `provisioning_link_open` 로그: 실제 Provisioning 연결이 어느 경로로 열렸는지 보여 준다.
- 장치 이름 `BikeMesh-S3`: Provisioning 전 장치를 구분하는 데 사용한다.

## GATT(속성 기반 통신 구조)

### 뜻

GATT는 BLE 장치 두 대가 연결된 뒤 데이터를 일정한 구조로 읽고 쓰는 규칙이다. 데이터를 Service(서비스)와 Characteristic(특성)으로 나누며, 한 장치가 값을 제공하고 다른 장치가 그 값을 읽거나 쓰거나 알림으로 받을 수 있다.

쉽게 말하면 다음과 같다.

> GATT는 두 장치가 연결한 뒤, 정해진 데이터 칸을 읽고 쓰는 방식이다.

Advertising이 여러 주변 장치에 짧은 정보를 알리는 동작이라면, GATT는 보통 특정 상대와 연결을 만든 뒤 데이터를 주고받는 동작이다.

### 이 프로젝트에서는

현재 펌웨어의 핵심 응용 기능은 표준 Bluetooth Mesh의 Generic OnOff Server이다. 일반적인 사용자 정의 GATT 서비스로 On/Off 값을 받는 프로그램이 아니다.

다만 GATT는 이 프로젝트에서 다음 두 경로에 사용될 수 있다.

- `PB-GATT`: 스마트폰 Provisioner가 노드와 연결하여 Provisioning 데이터를 전달하는 경로이다.
- GATT Proxy: 스마트폰처럼 광고 기반 Mesh 통신을 직접 처리하기 어려운 장치가 GATT 연결을 통해 Mesh 네트워크와 메시지를 주고받도록 돕는 기능이다.

따라서 이 프로젝트에서 “GATT 연결에 성공했다”는 사실만으로 Provisioning, AppKey 연결, Generic OnOff 수신, Relay 동작까지 성공했다고 볼 수 없다. 각 단계의 로그를 따로 확인해야 한다.

## Provisioning(네트워크 가입 절차)

### 뜻

Provisioning은 새 Bluetooth Mesh 장치를 특정 Mesh 네트워크의 구성원으로 안전하게 등록하는 절차이다. 단순히 장치를 검색하거나 GATT로 연결하는 것과 다르다.

Provisioning 과정에서 노드는 다음과 같은 네트워크 정보를 받는다.

- NetKey: 같은 Mesh 네트워크의 통신을 보호하는 네트워크 키
- DevKey: Provisioner가 해당 노드를 개별적으로 설정할 때 사용하는 장치 키
- Unicast Address: Mesh 네트워크 안에서 노드를 식별하는 고유 주소
- IV 관련 상태: 메시지 보안과 재전송 공격 방지에 필요한 네트워크 상태

쉽게 말하면 다음과 같다.

> Provisioning은 새 노드에게 네트워크 열쇠와 주소를 주어 Mesh 네트워크에 정식 가입시키는 과정이다.

### 이 프로젝트에서는

스마트폰 앱이 Provisioner 역할을 하고 ESP32-S3가 가입할 Node 역할을 한다. 현재 펌웨어는 화면이나 키패드로 인증값을 입력하는 방식 없이 `PB-ADV` 또는 `PB-GATT` 경로를 사용한다.

소스의 `provisioning_callback(...)`에서 다음 진행 상태를 로그로 확인할 수 있다.

- `provisioning_enabled`: Provisioning을 받을 준비가 됨
- `provisioning_link_open`: Provisioner와 절차가 시작됨
- `provisioned`: NetKey와 Unicast Address 등을 받고 가입이 완료됨
- `provisioning_link_closed`: Provisioning 연결이 닫힘

Provisioning 완료는 네트워크 가입 완료를 뜻하지만, Generic OnOff 기능을 바로 사용할 준비가 모두 끝났다는 뜻은 아니다. 이후 Configuration 단계에서 Composition Data를 확인하고, AppKey를 추가하고, 그 AppKey를 Generic OnOff Server Model에 연결해야 한다.

## Relay(중계)

### 뜻

Relay는 한 노드가 수신한 Mesh 네트워크 메시지를 다시 전송하여 원래 송신자의 직접 전파 범위 밖에 있는 노드까지 메시지가 도달하도록 돕는 표준 Bluetooth Mesh 기능이다.

쉽게 말하면 다음과 같다.

> Relay는 중간 노드가 Mesh 메시지를 한 번 더 전달해 통신 범위를 이어 주는 기능이다.

Relay 노드는 모든 메시지를 무조건 계속 반복하지 않는다. 메시지의 TTL, Sequence Number, 이미 본 메시지를 기록하는 캐시 같은 Mesh 규칙을 사용하여 불필요한 반복과 무한 재전송을 막는다.

### 이 프로젝트에서는

현재 펌웨어에는 Relay 기능을 지원하는 설정이 포함되어 있지만, 첫 부팅 상태는 `Disabled`이다. Provisioner가 선택한 중간 노드에 `Config Relay Set`을 보내야 그 노드가 Relay로 동작한다.

소스의 `config_server`에서 다음 항목이 이 동작과 연결된다.

- `.relay = ESP_BLE_MESH_RELAY_DISABLED`: 처음에는 Relay를 끈다.
- `.relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20)`: Relay가 켜졌을 때의 재전송 횟수와 간격을 정한다.
- `.default_ttl = 7`: 이 노드가 새로 보내는 Mesh 메시지의 기본 TTL이다.

세 노드 실험에서는 다음을 따로 증명해야 한다.

1. 송신 노드와 수신 노드가 가까울 때 직접 메시지가 도착한다.
2. 중간 노드의 Relay가 꺼진 상태에서는 먼 수신 노드에 도착하지 않는다.
3. 중간 노드의 Relay를 켠 뒤에는 같은 조건에서 먼 수신 노드에 도착한다.

이 프로젝트의 Relay는 표준 Bluetooth Mesh 네트워크 계층의 중계 기능이다. Bike Swarm Guard의 사용자 정의 방식처럼 애플리케이션이 sender, sequence, TTL을 직접 해석하고 패킷을 다시 만드는 Relay와는 구분해야 한다.

## FreeRTOS(실시간 운영체제 커널)

### 뜻

FreeRTOS는 마이크로컨트롤러에서 여러 작업을 Task(태스크)로 나누어 실행하도록 돕는 실시간 운영체제 커널이다. 각 Task의 우선순위와 실행 시간을 Scheduler(스케줄러)가 관리한다.

FreeRTOS가 제공하는 대표 기능은 다음과 같다.

- Task: 독립적으로 실행되는 작업 단위
- Queue: Task 사이에서 데이터를 안전하게 전달하는 통로
- Semaphore와 Mutex: 여러 Task가 공유 자원을 동시에 잘못 사용하는 것을 막는 도구
- Software Timer: 일정 시간이 지난 뒤 함수를 실행하는 타이머
- Scheduler: 우선순위와 대기 상태에 따라 실행할 Task를 선택하는 기능

쉽게 말하면 다음과 같다.

> FreeRTOS는 한 개의 마이크로컨트롤러가 BLE 처리, 센서 확인, 버튼 처리, 로그 출력 같은 여러 일을 순서에 맞게 나누어 실행하도록 돕는 작업 관리자이다.

### 이 프로젝트에서는

현재 프로젝트는 ESP-IDF를 사용하며, ESP-IDF 내부의 실행 기반으로 FreeRTOS를 사용한다. `app_main()`도 FreeRTOS가 시작한 Task 환경에서 실행된다.

`layer-0`과 `layer-1`의 실제 소스는 다음 FreeRTOS 기능을 사용한다.

- `#include "freertos/FreeRTOS.h"`: FreeRTOS의 기본 정의를 가져온다.
- `#include "freertos/task.h"`: Task 관련 API를 가져온다.
- `pdMS_TO_TICKS(1000)`: 1,000밀리초를 FreeRTOS Tick 단위로 변환한다.
- `vTaskDelay(...)`: 현재 Task를 일정 시간 대기시키고 그동안 다른 Task가 실행될 수 있게 한다.

현재 Layer에서는 `vTaskDelay()`를 이용해 1초마다 상태 로그를 출력한다. 앞으로 기능이 많아지면 센서 입력, 버튼 처리, BLE 이벤트 처리 등을 별도의 Task와 Queue로 나눌 수 있다.

FreeRTOS 자체가 Advertising, GATT 또는 Bluetooth Mesh를 구현하는 것은 아니다. FreeRTOS는 작업 실행과 Task 간 데이터 전달을 관리하고, 실제 Bluetooth 기능은 ESP-IDF의 Bluetooth Controller, Bluedroid 또는 ESP-BLE-MESH 스택이 처리한다.

## Zephyr(임베디드 RTOS 플랫폼)

### 뜻

Zephyr는 여러 마이크로컨트롤러 보드를 지원하는 오픈 소스 실시간 운영체제이자 임베디드 개발 플랫폼이다. Kernel뿐 아니라 장치 Driver, Bluetooth, 네트워크, 전원 관리, 파일 시스템, 빌드와 설정 체계도 함께 제공한다.

Zephyr 프로젝트에서 자주 만나는 구성요소는 다음과 같다.

- Kernel: Thread, Queue, Semaphore, Timer 같은 실행 기능
- Devicetree: 보드의 CPU, 메모리, GPIO, UART 같은 Hardware 구성을 기술
- Kconfig: 사용할 기능을 선택하는 설정 체계
- CMake와 West: 프로젝트 구성, 의존성 관리, Build와 Flash에 사용하는 도구
- Bluetooth Stack: BLE Advertising, GATT, Bluetooth Mesh 등을 제공하는 통신 스택

쉽게 말하면 다음과 같다.

> Zephyr는 RTOS 커널뿐 아니라 보드 설정, Driver, Bluetooth Stack, 빌드 도구까지 하나의 체계로 제공하는 임베디드 개발 플랫폼이다.

### 이 프로젝트에서는

현재 프로젝트는 Zephyr를 사용하지 않는다. 현재 선택한 구성은 다음과 같다.

```text
ESP32-S3
  → ESP-IDF
  → FreeRTOS
  → Bluedroid 또는 ESP-BLE-MESH
  → 현재 학습 애플리케이션
```

Zephyr를 선택한다면 현재 ESP-IDF 프로젝트에 작은 라이브러리처럼 추가하는 것이 아니라, Zephyr의 API, Devicetree, Kconfig, West 빌드 방식에 맞춘 별도의 펌웨어 프로젝트로 옮겨야 한다.

```text
ESP32-S3
  → Zephyr
  → Zephyr Kernel과 Bluetooth Stack
  → Zephyr용으로 작성한 학습 애플리케이션
```

따라서 현재 학습에서는 ESP-IDF와 FreeRTOS 흐름을 유지한다. 나중에 Zephyr를 비교 실험하려면 별도 디렉터리와 별도 Build 결과를 만들고, 정확한 ESP32-S3 보드의 Bluetooth 지원과 실제 Advertising, GATT, Mesh 동작을 단계별로 다시 검증해야 한다.

## FreeRTOS와 Zephyr의 핵심 차이

| 구분 | FreeRTOS | Zephyr |
|---|---|---|
| 기본 성격 | 실시간 운영체제 커널 | 완성형 임베디드 RTOS 플랫폼 |
| 현재 프로젝트 | ESP-IDF 내부에서 사용 중 | 사용하지 않음 |
| Bluetooth 제공 주체 | ESP-IDF의 Bluetooth Stack | Zephyr Bluetooth Stack |
| 현재 코드 예 | `vTaskDelay()`, `pdMS_TO_TICKS()` | 현재 코드에 없음 |
| 도입 방식 | ESP-IDF 실행 기반에 이미 포함 | 별도 Zephyr 프로젝트로 Porting 필요 |

## 여섯 용어의 관계

```text
ESP32-S3가 Provisioning 전 정보를 Advertising으로 알림
    ↓
스마트폰 Provisioner가 장치를 발견함
    ↓
PB-ADV 또는 PB-GATT 경로로 Provisioning 수행
    ↓
노드가 NetKey, DevKey, Unicast Address를 받아 Mesh 네트워크에 가입
    ↓
AppKey 추가 및 Generic OnOff Server Model 연결
    ↓
Generic OnOff 메시지 송수신
    ↓
필요한 중간 노드의 Relay를 켜서 메시지 도달 범위를 확장
```

이 네트워크 동작을 실행하는 현재 펌웨어의 아래쪽 기반에는 ESP-IDF와 FreeRTOS가 있다. Zephyr는 이 기반에 동시에 추가하는 구성요소가 아니라, ESP-IDF 대신 선택할 수 있는 별도 플랫폼이다.

핵심 구분은 다음과 같다.

- Advertising은 주변에 무선 정보를 알리는 BLE 전송 방식이다.
- GATT는 연결된 두 장치가 구조화된 데이터를 주고받는 방식이다.
- Provisioning은 새 노드를 표준 Bluetooth Mesh 네트워크에 가입시키는 절차이다.
- Relay는 이미 Mesh에 가입한 중간 노드가 Mesh 메시지를 다시 전달하는 기능이다.
- FreeRTOS는 현재 ESP-IDF 펌웨어 안에서 Task 실행과 시간을 관리하는 RTOS 커널이다.
- Zephyr는 Kernel, Driver, Bluetooth Stack과 빌드 체계를 함께 제공하는 별도 임베디드 RTOS 플랫폼이다.
