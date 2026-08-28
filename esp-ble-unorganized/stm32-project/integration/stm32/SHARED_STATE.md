# SharedState: STM32에 센서값 모아 두기

**각 STM32가 A·B·C의 센서값을 보관합니다.** ESP32는 통신, STM32는 값 관리와 화면 표시를 맡습니다.

이번에는 **저장·조회 모듈과 컴퓨터 예제**를 만들었습니다. 실제 센서·UART와의 연결, 화면 출력은 아직 하지 않았습니다.

## 1. 파일은 두 개입니다

- [shared_state.h](MyApp/service/shared_state.h): 구조체와 사용할 함수.
- [shared_state.c](MyApp/service/shared_state.c): 값 저장·조회와 오래됨 판정.

```text
자기 센서값 ─────────┐
                    ├→ SharedState → Dashboard → 화면
ESP32 → UART 수신값 ─┘
```

위 흐름 중 **SharedState만 준비된 상태**입니다. `app_process()`에서 아직 호출하지 않습니다.

저장 공간은 A·B·C × 속도·온도·기울기, 총 9칸입니다. 아직 값이 없는 칸은 미수신으로 남습니다. 기울기 계산은 센서 담당 코드의 일이며, 이 모듈은 이미 계산한 숫자만 받습니다.

## 2. B의 온도 저장하기

```c
shared_state_t state;
shared_state_init(&state, 5000U); // 이 예제에서는 5초가 지나면 '오래됨'

shared_state_update(&state,
                    SHARED_NODE_B,
                    SHARED_SENSOR_TEMPERATURE_C,
                    28.0f,  // 온도 28도
                    1000U); // 이 STM32가 받은 시각: 1000ms
```

**B의 온도 칸만 바뀝니다.** A의 속도나 C의 기울기는 유지됩니다. `5000U`는 예제 설정이며, 실제 기준은 센서 전송 주기에 맞춰 정해야 합니다.

이 코드는 설명용 발췌입니다. `init`과 `update`의 성공 여부까지 확인하는 [실행 가능한 예제](host-tests/shared_state_example.c)도 있습니다.

## 3. 화면에서 읽을 때

```c
shared_sensor_value_t reading;
shared_value_status_t status = shared_state_get(
    &state, SHARED_NODE_B, SHARED_SENSOR_TEMPERATURE_C,
    1100U, &reading); // 조회하는 현재 시각
```

| 반환 상태 | 화면에서 할 일 |
| --- | --- |
| `SHARED_VALUE_MISSING` | `--` 표시. 아직 유효한 값을 받지 못함 |
| `SHARED_VALUE_FRESH` | `reading.value` 표시 |
| `SHARED_VALUE_STALE` | 마지막 값과 함께 `오래됨` 표시 |
| `SHARED_VALUE_INVALID_ARGUMENT` | 잘못된 호출. 반환 데이터를 표시하지 않음 |

실제 0도를 받으면 **정상값 0도**입니다. 미수신과 다릅니다. 같은 28도를 다시 받아도 받은 시각은 갱신합니다. 조회 자체는 갱신 시각을 바꾸지 않습니다.

## 4. 보드 없이 실행하기

저장소 루트에서 CMake와 C 컴파일러가 있는 터미널로 실행합니다. STM32용 툴체인은 이 검사에 필요 없습니다.

```sh
cmake -S integration/stm32/host-tests -B integration/stm32/host-tests/build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build integration/stm32/host-tests/build/debug
ctest --test-dir integration/stm32/host-tests/build/debug --output-on-failure
./integration/stm32/host-tests/build/debug/shared_state_example
```

예제 출력:

```text
B 온도: -- (미수신)
B 온도: 28.0 C (정상)
B 온도: 28.0 C (오래됨)
B 온도: 0.0 C (정상)
```

가짜 값과 시각으로 실행하는 예제입니다. 실제 UART·Bluetooth·화면을 시험한 것은 아닙니다.

## 5. 나중에 연결할 때 지킬 것

- 한 STM32 앱이 `shared_state_t` 한 개를 소유하고, 시작할 때 한 번 초기화합니다. 예제처럼 매번 초기화하면 이전 값이 사라집니다.
- 자기 센서값과 UART에서 받은 다른 라이더의 값을 같은 `shared_state_update()`로 넣습니다. 원격 수신값을 새 로컬 센서값처럼 다시 송신하지 않습니다.
- A/B/C는 앱에서 정한 출처입니다. Mesh 주소를 그대로 배열 번호로 쓰지 않습니다.
- 현재 UART는 **이벤트 번호 1바이트**만 전달합니다. 출처·센서 종류·숫자를 보낼 형식은 다음 단계에서 추가해야 합니다. 구조체 메모리를 그대로 전송하지 않습니다.
- 이 모듈의 '최신값'은 마지막으로 `update()`에 넣은 값입니다. 오래된 패킷·중복 패킷을 가려내는 순서번호 처리는 통신 쪽에서 별도로 해야 합니다.
- `now_ms`는 **값을 저장하는 STM32 자신의 시각**입니다. `HAL_GetTick()` 같은 같은 시계를 update/get에 사용하며, 다른 노드의 시각과 빼지 않습니다. 센서의 실제 측정 시점부터 걸린 시간 전체를 보장하지는 않습니다.
- 32비트 tick이 0으로 한 번 넘어가는 경우는 처리합니다. 다만 마지막 갱신부터 실제 경과 시간이 약 49.7일 이상이면 계산이 모호해지므로 장기 보관용 시계는 별도 확장이 필요합니다.
- NaN·무한대는 거부하고 이전 값·시각을 유지합니다. 센서별 물리 범위 검사와 보정은 센서 담당 코드가 합니다.
- 함수는 `app_process()` 같은 **한 실행 흐름에서** 호출합니다. UART 인터럽트는 수신만 하고 앱 루프가 갱신하도록 연결합니다. 여러 Task가 접근하게 되면 별도 보호가 필요합니다.

기존 센서·이벤트 처리와 ESP32 코드는 바꾸지 않았습니다. `dashboard.c/.h`는 실제 화면을 연결하는 단계에서 추가합니다.

## 6. 이번에 확인한 것

- PC 테스트: Debug, Release, ASan·UBSan 각각 2개 통과(동작 검사 + 예제 실행).
- 검사 내용: 출처별 값 분리, 실제 0과 미수신 구분, 오래됨 경계, 같은 값 재수신, 잘못된 값·인수 거부, tick 넘침, 복사본 조회.
- STM32 빌드: Debug/Release × 초음파 ON/OFF × 낙차 감지 ON/OFF, 총 8개 조합 통과. 새 소스도 컴파일됩니다.
- 보드에 쓰기(Flash), 실제 수치 송수신, 화면 출력은 **아직 검사하지 않았습니다.** 빌드에 포함됐다는 것과 앱에서 호출된다는 것은 다릅니다.

[프로젝트 처음으로](../../README.md) · [통신 흐름 설명](../esp32-s3/README.md)
