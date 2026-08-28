> 이관 원문: `communication-module/periodic/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Periodic Communication

Head가 받은 최근 5개 속도 측정값의 이동평균을 계산하고, 정해진 간격마다 평균 메시지의 전송 접수를 요청하는 경로다.

상태: 이동평균, 유효성/만료 관리, 주기 전송, 호스트 테스트와 가짜 전송 예제를 구현했다. 실제 센서, Bluetooth, Mesh Relay, B/C 수신부는 아직 연결하지 않았다.

## 입력과 책임

- 목표 입력 경로: Bluetooth 속도 센서 → Head ESP32 → 5개 이동평균 → Mesh 발행. 이번 속도 경로는 STM32를 거치지 않는다. 기존 버튼 경로와 분리한다.
- 이 코드에 넣는 입력은 이미 속도로 해석된 `float` 값이며 단위는 cm/s다. 센서가 km/h를 주면 변환하고, 회전수/시간을 주면 센서 어댑터에서 속도를 계산한 뒤 넘긴다.
- 제품명, 서비스 UUID, Notification/Read 방식, 연결/재연결과 실제 속도 계산은 센서 어댑터 단계에서 확인한다. 센서가 없다고 예제값을 실제 측정의 대체값으로 쓰지 않는다.
- 새 정상 측정마다 `update()`를 한 번 호출한다. 타이머가 돌 때 같은 측정을 새 샘플로 반복 삽입하지 않는다.
- 평균은 Head에서만 계산한다. B/C는 Head의 평균 메시지를 전달/사용하며 다시 평균을 내지 않는다. 실제 경로와 Relay 증명은 별도 단계다.

## 파일과 API

- [moving_average.h](../../../code/communication-module/periodic/moving_average.h), [moving_average.c](../../../code/communication-module/periodic/moving_average.c): 최근 5개 유한 측정값과 평균. 5개 전에는 평균을 제공하지 않는다.
- [comm_periodic.h](../../../code/communication-module/periodic/comm_periodic.h), [comm_periodic.c](../../../code/communication-module/periodic/comm_periodic.c): 속도 입력, 유효 여부, 만료, 전송 시점 판단.
- [가짜 입력 예제](../../../code/communication-module/periodic/examples/main.c): 처음 값 없음 → 5개 준비 → BUSY/재시도 → 만료 → 유효한 정지값.
- [이동평균 테스트](../../../code/communication-module/periodic/tests/test_moving_average.c), [주기 전송 테스트](../../../code/communication-module/periodic/tests/test_periodic.c): 공개 API를 통한 검증.

| API | 역할 |
| --- | --- |
| `comm_periodic_init()` | 주기, 만료 간격, 시작 시각 설정. 처음에는 유효한 값 없음 |
| `comm_periodic_update()` | 새 속도 측정 추가. 음수, NaN, Inf 거절 |
| `comm_periodic_read()` | 현재 평균과 `valid` 확인. 전송 일정은 소비하지 않음 |
| `comm_periodic_poll()` | 유효하고 전송 시각이면 콜백을 최대 한 번 호출 |
| `comm_periodic_invalidate()` | 연결 끊김/센서 오류 알림. 윈도를 비우고 다시 5개 대기 |

## 데이터 없음과 실제 정지

`comm_speed_data_t`는 `average_cm_s`와 `valid`를 함께 가진다. `valid=false`일 때 숫자 필드를 속도로 해석하지 않는다.

| 상황 | read 결과 | valid |
| --- | --- | --- |
| 측정 없음 / 1~4개 / 명시적 무효화 직후 | `COMM_PERIODIC_NOT_READY` | false |
| 최근 정상 측정 5개와 유효 기간 내 | `COMM_PERIODIC_OK` | true |
| 마지막 정상 측정 이후 만료 간격 이상 경과 | `COMM_PERIODIC_STALE` | false |
| 정상 측정 0을 5개 수신 | `COMM_PERIODIC_OK`, 평균 0 | true |

NOT_READY/STALE에서는 출력 숫자를 0으로 초기화하지만 이는 기본 속도가 아니다. poll은 이때 송신 콜백을 호출하지 않는다. 현재는 무효화 메시지를 무선으로 보내지 않으며, B/C의 초기 상태 및 수신 타임아웃은 수신부 구현 단계에서 별도로 처리해야 한다.

인자/시각 오류가 나면 read 출력값을 변경하지 않으므로 반환 상태를 먼저 확인한다. 잘못된 입력 샘플은 기존 정상 윈도를 바꾸거나 최신 측정 시각을 갱신하지 않는다. 센서 연결이 끊겼다고 확정된 경우에는 invalidate를 호출한다.

## 시간과 만료 계약

- `period_ms`, `stale_after_ms`는 0보다 큰 설정값이다. 하드코딩한 기본값은 없다. 예제만 200ms/1,000ms를 사용한다.
- 만료 간격은 실제 센서 측정 간격과 지연을 고려해 정한다. 측정이 1초마다 오는데 만료를 1초로 잡으면 매번 윈도가 초기화될 수 있다.
- 모든 API의 `now_ms`는 같은 시계의 단조 증가하는 64-bit 밀리초 값이다. 같은 시각은 허용하지만 뒤로 가면 INVALID_TIME이다. 벽시계나 원격 노드 시각을 섞지 않는다.
- 32-bit HAL tick을 그대로 넣어 wraparound시키지 않는다. 해당 보드 어댑터에서 연속적인 64-bit 시간으로 확장해야 한다.
- 유효성은 마지막 정상 측정 시각을 기준으로 한다. `현재 - 마지막 측정 >= stale_after_ms`이면 만료다.
- 새 입력이 오기까지의 간격이 만료 간격 이상이면, poll을 중간에 호출하지 않았어도 윈도를 초기화하고 새 측정 5개를 기다린다.
- 최근 5개는 샘플 개수 기준이며 시간 가중 평균이 아니다. 측정 간격이 불규칙하면 윈도가 나타내는 시간 길이도 달라진다.

## 주기와 BUSY 계약

첫 전송 기준은 초기화 시각에서 한 주기 뒤다. 그 시각에 5개가 준비되지 않았다면 기다리고, 나중에 준비됐을 때 최신 평균 한 건을 요청한다.

성공 후에는 초기화 시각에 맞춘 주기 기준을 유지한다. 예를 들어 0ms에 초기화하고 주기가 200ms일 때, 첫 poll이 950ms였다면 한 건만 접수하고 다음 기준은 1,000ms다. 200/400/600/800ms의 과거 전송을 몰아서 수행하지 않는다.

콜백이 false(BUSY)를 반환하면 일정을 소비하지 않는다. 다음 poll에서 현재 평균을 새로 읽어 재시도한다. 과거 평균을 큐에 쌓지 않는다. 지연이나 BUSY 복구 직후 실제 접수 간격은 설정 주기보다 짧아질 수 있으므로, 주기는 무선 수신 간격이나 최소 송신 간격 보장이 아니다.

콜백의 true는 데이터를 복사하거나 동기적으로 처리해 접수했다는 뜻이며, 상대 노드 수신 ACK가 아니다. 콜백은 빠르게 반환해야 하고, 임시 메시지 포인터를 보관하거나 같은 state를 재진입해 수정해서는 안 된다. 모든 API는 단일 실행 흐름용이다.

## 이동평균 정책

제공받은 `SlidingWindow` 구조를 바탕으로 최근 5개를 순환 저장한다. 평균 대비 ±50% 거절은 제외했다. 정지/출발을 잘못 거절하지 않기 위해서다. 다만 이동평균 자체의 지연은 남으며, 20에서 0으로 갑자기 바뀌면 평균은 16 → 12 → 8 → 4 → 0으로 내려간다.

5칸을 매번 double로 다시 합산하여 이전 윈도의 큰 값 때문에 누적되는 오차를 줄인다. 동적 할당이나 라이브러리 내부 printf는 없다. 센서의 물리적 최대 속도나 이상치 판정 정책은 아직 추가하지 않았다.

## 호스트 검증과 실행

이동평균 6개 검사와 주기 전송 11개 검사를 제공한다. Service 통합 테스트를 포함한 현재 전체 구성과 실행 방법은 [상위 README](../README.md)를 따른다. 팀원은 개별 periodic API 대신 [Service API](../service/README.md)를 사용한다.

```sh
./communication-module/build/periodic_demo
```

Periodic 최초 구현 시 macOS AppleClang의 Debug, Release, ASan+UBSan 빌드 모두 당시 CTest 5/5 통과를 확인했다. 가짜 입력 예제의 전체 출력도 기대값과 일치했다. LeakSanitizer는 이 플랫폼에서 지원하지 않아 검사에 포함하지 않는다.

이 결과는 호스트 로직 검증이다. 실제 BLE 센서 수신, 동시에 동작하는 Mesh 스택, 실제 B/C 전달, 이동 중 통신 성능은 증명하지 않는다.

[상위 개발 범위](../README.md)
