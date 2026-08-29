---
date: 2026-08-29
topic: shared-state-feature-review-and-extensibility
status: proposal
---

**전체 흐름 통합본:** [shared_data → BLE Mesh → STM32 출력](../architecture/shared-data/README.md). 이 문서는 원본 기능별 근거와 확장성 리뷰 기록입니다.

# 원본 기능 재검토와 확장 가능한 공유 상태

[KAF-373](https://linear.app/kafkasnowflake/issue/KAF-373) · [메시지 구조·codec](2026-08-28-message-struct-codec.md) · [전체 구상](2026-08-28-message-protocol-extension-brainstorm.md)

## 1. 검토 범위와 결론

2026-08-29 `git ls-remote`로 원본 `main`/HEAD가 **940ff2408998d79e181e5b8322ba5e678c038871**임을 확인하고, 같은 커밋의 별도 clone에서 앱·서비스·드라이버·README를 다시 읽었습니다. 원본 기준은 [이 커밋](https://github.com/snowflake-rider/stm32-project/tree/940ff2408998d79e181e5b8322ba5e678c038871)이며 다른 실험 브랜치는 이번 필수 기능 목록에 합치지 않습니다. GitHub 웹 조회는 404였지만 Git 원격 ref 조회와 로컬 clone 검토는 성공했습니다.

**온도·습도·속도만 있는 구조체로는 부족합니다.** 라이더별 후방 상태, 낙상/SOS 사건, 측정 상태·최신성, 연결 관찰도 필요합니다. 다만 이 모든 필드를 한 패킷으로 보내지 않습니다. 메시지는 변경된 한 종류를 전달하고, 각 STM32가 여러 메시지를 모아 로컬 상태를 구성합니다.

이번 작업은 소스 리뷰와 설계 문서입니다. 펌웨어·기존 `shared_state` 구현·보드 설정을 변경하지 않습니다. 아래 새 이름과 동작은 제안이며 작동하는 제품 API가 아닙니다.

### 유지하는 사용자 합의

- 역할은 1~3. 역할별 기능을 사전 정의하므로 `capabilities`를 전송하지 않습니다.
- heartbeat 내용은 현재 상태 `status` 1바이트입니다. 정확한 상태 비트는 아직 미정입니다.
- 온습도 내용은 2바이트: 온도 22.5~47.5°C/0.1°C, 습도 0~100%/0.5%p. 습도 양자화 손실을 허용합니다.
- 저장과 전송에 정수를 사용합니다. MCU 내부 센서 알고리즘의 float까지 이번에 변경하지 않습니다.
- 당사자만 자신의 낙상을 해제합니다. 타인은 자기 장치의 소리만 잠시 끕니다.

## 2. 원본 기능 → 필요한 데이터 추적표

아래 근거 파일은 모두 위 원본 커밋의 `integration/stm32/MyApp/` 아래입니다. “공유”는 네트워크로 의미를 전달한다는 뜻이며 전체 저장소를 전송한다는 뜻이 아닙니다.

| 기능 / 원본 근거 | 필요한 데이터 | 위치·전송 정책 | 재검토 결과 |
| --- | --- | --- | --- |
| 감속·가속·안전/응원·정지 버튼: `hw/button.c`, `service/message_service.c:101` | 요청 종류, 발신자, 메시지 식별자 | 일회성 이벤트 + 제한된 처리 큐 | 실제 속도값과 구분. `last_request` 하나로 연속 요청을 덮어쓰지 않음 |
| 속도 표시: `service/swarm_state.c:31`, `display_service.c:51` | `speed_kmh_x10`, 측정 상태, 출처, 샘플 식별, 수신 시각 | 노드별 공유 측정값 | 원본은 수신·표시만 있음. 실제 속도 생산 경로는 여전히 미정 |
| DHT11: `service/environment_service.c:27` | 온도·습도 코드, 각 값의 상태, 새 샘플 식별 | 2바이트 내용 + 공통 헤더 | 원본은 실패 시 송신을 생략함. 우리 안은 오류/미측정/범위 밖/오래됨 구분 필요 |
| HC-SR04: `service/safety_detector.c:84` | `rear_state` = 미확인/안전/경고, 최신 상태 식별·시각, 센서 상태 | SAFE/WARNING은 공유; 거리 원시값은 기본 로컬 | 기존 메시지에는 있지만 새 저장소 설계의 명시 필드가 부족했음 |
| MPU6050 낙상: `service/safety_detector.c:119` | 발신자별 낙상 사건 키, 활성/종료, 마지막 적용 정보 | 확정된 사건만 기본 공유 | 단일 `fall_detected`/`fallen_bike_id`로 동시 낙상을 표현할 수 없음 |
| 의심·카운트다운: `service/safety_detector.c:105`, `display_service.c:74` | 로컬 감지 단계, 남은 시간/시작 시각 | `local_safety`, 기본 미전송 | 다른 자전거의 카운트다운 표시까지 요구하지 않음 |
| 버튼4 취소·재감지: `ap/app.c:136`, `service/safety_detector.c:194` | 본인 사건 참조, 로컬 취소/재무장 단계, 정상 자세 유지 시간 | 본인 확정 사건 해제만 공유; 재감지 타이머는 로컬 | 타인의 낙상을 자기 버튼으로 해제하는 원본 동작은 채택하지 않음 |
| SOS: `service/message_service.c:59`, `service/swarm_state.c:46` | 낙상과 분리된 SOS 사건 상태 | 독립 사건 슬롯 | 원본은 수신 경로가 있고 낙상과 같은 비트에 저장. 생성 버튼/센서 경로와 SOS 해제 규칙은 별도 미정 |
| UART 링크: `service/swarm_state.c:16,67` | 로컬 UART 수신 여부·마지막 정상 프레임 시각 | 로컬 링크 상태 | 내 ESP32와의 연락을 다른 라이더의 연결 상태로 사용하면 안 됨 |
| 원격 heartbeat: `service/swarm_state.c:59` | 노드별 `status`, 마지막 수신·heartbeat 시각, 관찰 상태 | status 1바이트 공유 + 로컬 관찰값 | 원본은 수신 처리만 있고 송신 생산자가 없음. UART 링크만으로 원격 3명 상태를 대체할 수 없음 |
| SSD1306: `service/display_service.c:43,64,74,85` | 표시할 노드/측정값/사건, 로컬 countdown/rearm, 표시기 준비 상태 | 공유 상태를 읽어 로컬 렌더링 | 화면 픽셀·문자열·화면 갱신 tick은 전송하지 않음 |
| RGB LED·부저·VS1003B: `service/alert.c`, `message_service.c`, `audio_service.c` | 경고 우선순위, 활성 사건, 로컬 음소거 대상/기한, 재생 상태 | 이벤트 의미는 공유; 출력 상태·음원·재생 위치는 로컬 | FALL/SOS 전용 MP3는 원본에 없음. 낙상 해제로 후방 경고·다른 사건까지 지우면 안 됨 |
| 역할별 빌드: `common/app_config.h:4` | `source_id → role → 사전 기능표` | 공통 정적 설정, 기능 목록 미전송 | 원본 역할표와 우리 센서 기본값이 다르므로 그대로 이식하지 않음 |
| CRC·sequence·ACK: `common/swarm_packet.h`, `service/uart_service.c` | 프레임 검사, 중복/순서 기록, 실패 진단 | 전송 계층/수신 처리 상태 | ACK ID는 있으나 처리 의미 미구현. 수신/적용 완료를 보장한다고 볼 수 없음 |

원본 [역할 설정](https://github.com/snowflake-rider/stm32-project/blob/940ff2408998d79e181e5b8322ba5e678c038871/integration/stm32/MyApp/common/app_config.h)은 1=선두/속도, 2=DHT11, 3=MPU6050, OLED 공통, 초음파 기본 OFF입니다. 반면 현재 [NOSTOS 설정](../../firmware/stm32/MyApp/common/app_config.h)은 초음파·낙상 기본 ON입니다. 사용자 합의는 역할 번호와 사전 기능표 사용이며, NOSTOS의 실제 역할별 센서 배정까지 원본과 동일하게 확정한 것이 아닙니다. 기능표와 실제 빌드 옵션을 맞추는 작업이 필요합니다.

원본 README가 기능 구현을 설명해도 통합 실물 검증과 같지는 않습니다. [원본 검증 목록](https://github.com/snowflake-rider/stm32-project/blob/940ff2408998d79e181e5b8322ba5e678c038871/integration/README.md)에는 ESP32 heartbeat·속도 수신·세 자전거 전달·DHT11 장시간 안정성 시험 등이 남아 있습니다.

원본 메시지 enum의 실제 메시지 13종도 모두 대응했습니다. `MSG_NONE`/`MSG_UNKNOWN`은 실제 기능 메시지에서 제외합니다.

| 분류 | 확인한 원본 이름 |
| --- | --- |
| 버튼 4종 | `MSG_SPEED_DOWN_REQUEST`, `MSG_SPEED_UP_REQUEST`, `MSG_SAFETY_REMINDER`, `MSG_STOP_REQUEST` |
| 후방 2종 | `MSG_REAR_SAFE`, `MSG_REAR_WARNING` |
| 사건 3종 | `MSG_FALL_DETECTED`, `MSG_SOS`, `MSG_FALL_CLEAR` |
| 측정 2종 | `MSG_SPEED_UPDATE`, `MSG_ENV_UPDATE` |
| 링크/확인 2종 | `MSG_HEARTBEAT`, `MSG_ACK` (ACK 의미 미구현) |

## 3. 현재 코드/이전 초안에서 보강할 부분

1. **현재 저장소는 센서 3종만 담습니다.** [shared_state.h](../../firmware/stm32/MyApp/service/shared_state.h)는 A/B/C별 속도·온도·기울기 `float`, `has_value`, `updated_ms`와 공통 stale timeout만 있습니다. 습도, 오류 사유, 후방 상태, 사건, 연결 관찰은 없습니다. 실제 앱 연결도 아직 없습니다.
2. **현재 최신성 판정은 새 샘플 검증이 아닙니다.** [shared_state_update](../../firmware/stm32/MyApp/service/shared_state.c)는 호출하면 같은 값도 시각을 갱신합니다. 재전송·지연 패킷을 먼저 걸러야 합니다. 모든 측정에 같은 timeout을 적용하기보다 종류별 정책이 필요합니다.
3. **출처를 STM32까지 보존해야 합니다.** 현재 [event_bridge.c](../../libs/protocol/event_bridge.c)는 Mesh 발신 주소를 받지만 UART에는 이벤트 ID 1바이트만 보냅니다. 라이더별 저장을 위해 새 경로에서 출처/식별 정보를 유지해야 합니다.
4. **원본 단일 낙상 상태를 복사하지 않습니다.** [swarm_state.c:46](https://github.com/snowflake-rider/stm32-project/blob/940ff2408998d79e181e5b8322ba5e678c038871/integration/stm32/MyApp/service/swarm_state.c#L46)은 FALL/SOS를 하나로 합칩니다. CLEAR는 출처·사건 없이 전체 해제합니다. [message_service.c:50](https://github.com/snowflake-rider/stm32-project/blob/940ff2408998d79e181e5b8322ba5e678c038871/integration/stm32/MyApp/service/message_service.c#L50)은 후방 상태까지 초기화합니다. 이 세 문제는 구조체에 필드를 추가하는 것만으로 해결되지 않고 적용 규칙도 바꿔야 합니다.
5. **필수 공유값과 진단 원시값을 구분합니다.** 기존 저장소의 `tilt_deg` 칸은 있지만 원본 낙상 판정은 가속도/자이로를 사용하고 각도 텔레메트리를 보내지 않습니다. 기울기 각도·거리 원시값·GPS·배터리를 필수 전송값으로 단정하지 않습니다. 나중에 필요할 때 새 메시지로 추가합니다.
6. **초음파 실패를 안전으로 바꾸는 기존 정책을 검토해야 합니다.** 원본 [safety_service.c:82](https://github.com/snowflake-rider/stm32-project/blob/940ff2408998d79e181e5b8322ba5e678c038871/integration/stm32/MyApp/service/safety_service.c#L82)와 현재 [NOSTOS safety_service](../../firmware/stm32/MyApp/service/safety_service.c)는 연속 무응답 뒤 거리를 100cm·유효로 대입합니다. 이를 새 저장소에서 실제 정상 측정과 구분하지 못하면 미확인/오류 필드를 추가해도 의미가 없습니다. 원거리 무반사와 센서 고장을 구분 못 하는 경우 “측정 불명”으로 남길 정책 및 오류 전달 경로가 필요합니다. 이번 리뷰에서 기존 동작을 수정하지는 않았습니다.

## 4. `shared_data`가 실제로 가질 내용

권장 이름은 기존 제안의 **`nostos_network_state_t`**입니다. C 변수 이름은 `shared_data`로 써도 됩니다. 한 STM32가 소유하는 로컬 사본이며, 세 보드의 메모리가 자동으로 동일해지는 구조가 아닙니다.

```text
nostos_network_state_t shared_data
└─ nodes[3] : nostos_node_state_t
   ├─ source_id                         누구의 상태인가
   ├─ environment                      온도·습도 및 측정 메타데이터
   ├─ speed                            실제 속도 및 측정 메타데이터
   ├─ rear                             후방 안전/경고/미확인 및 최신성
   ├─ fall                             해당 라이더의 낙상 사건 상태
   ├─ sos                              해당 라이더의 SOS 사건 상태
   ├─ health                           heartbeat status와 관찰 시각
   └─ reachability                     최근 원격 연락과 관찰 상태

별도 로컬 소유 상태
├─ node_config                         발신자·역할·기능표·허용 메시지
├─ local_link                          UART 및 자기 ESP32 준비 상태
├─ local_safety                        카운트다운·취소·재감지·원시 센서
├─ local_ui                            사건별 음소거·출력/화면 상태
├─ request_queue                       일회성 버튼 요청의 제한된 처리 큐
└─ rx_context                          세션·중복·종류별 순서·종료 사건 기록
```

### 필드 목록 제안

| 묶음 | 필드와 타입/의미 | 비고 |
| --- | --- | --- |
| 노드 식별 | `source_id: uint8_t`, 등록된 1~3 | 배열 인덱스 0~2 및 role과 변환을 명시 |
| 환경값 | `temperature_c_x10: int16_t`, `humidity_pct_x10: uint16_t` | wire는 1바이트 코드 두 개, 저장소는 복원된 정수값 |
| 속도값 | `speed_kmh_x10: uint16_t` | 값 0은 정지; 미측정/오류와 구분. 단위·상한 최종 확인 필요 |
| 보고 메타데이터 | `report_session_id: uint32_t`, `report_sequence: uint16_t`, `report_received_ms: uint32_t`, `has_report: bool` | 오류 포함 최신 수락 보고의 식별자/시각. 환경 보고 한 쌍은 이 정보를 공유할 수 있음 |
| 값별 메타데이터 | `has_value: bool`, `quality: uint8_t`, `value_received_ms: uint32_t` | 온도·습도 각각 독립 보관. quality=미측정/유효/오류/범위 미만/범위 초과. STALE은 시각·정책으로 계산 |
| 후방 | `state: uint8_t` (미확인/안전/경고), 상태 식별자·수신 시각·품질 | 센서 오류/연결 끊김이면 안전으로 변경하지 않음 |
| 사건 | `phase: uint8_t` (미확인/활성/종료), `incident_session_id: uint32_t`, `incident_id: uint16_t`, 마지막 적용 메시지 식별·수신 시각 | 노드마다 FALL/SOS 분리. 소유자는 상위 source_id |
| 건강 보고 | `status: uint8_t`, `has_report: bool`, `received_ms: uint32_t` | heartbeat에 온습도 등 측정값을 넣지 않음. 0으로 초기화됐다는 사실만으로 정상 판정 금지 |
| 원격 연락 | `seen: bool`, `last_valid_rx_ms: uint32_t`, `last_heartbeat_rx_ms: uint32_t`, `heartbeat_seen: bool` | 연결 상태는 시각/세션/timeout에서 산출. 각 라이더별 관리 |
| 로컬 UART | `seen`, `last_valid_frame_ms`, 링크 상태/오류 카운터 | 원격 node reachability와 별도 |
| 로컬 안전 | `fall_phase`, countdown 시작/남은 시간, `upright_since_ms`, `rearm_pending` | 기존 safety_service/detector가 소유. 공유 저장소에 중복 복제하지 않음 |
| 로컬 음소거 | 사건 키(발신자·종류·세션·사건 번호), 음소거 시작·기간 | A의 사건① 음소거가 A의 사건② 또는 B의 사건까지 가리지 않음 |
| 요청 큐 | 종류·발신자·메시지 키·수신 시각, 용량/만료 정책 | 일회성 요청은 지속 상태를 덮어쓰는 센서 필드로 만들지 않음 |

정확한 enum 값, 메모리 배치/크기, 큐·종료기록 용량은 구현 단계에서 결정합니다. 위 타입의 합계나 `sizeof`를 전송 크기로 사용하지 않습니다. 역할별 기능표는 정적 설정에만 있으며 `capabilities`를 wire나 노드별 동적 목록으로 재도입하지 않습니다.

### 측정과 사건의 갱신 규칙

- 정상 샘플을 받으면 값·품질·샘플 키·두 수신 시각을 갱신합니다. 오류 보고는 품질/보고 시각/샘플 키를 갱신하되 이전 정상값과 그 시각을 보존할 수 있습니다. 화면은 그 이전 값을 새 정상값으로 표시하지 않습니다.
- 환경 보고는 한 번의 측정 시도의 온도/습도 코드 한 쌍입니다. 한쪽 오류는 그 코드로 표시합니다. 예전 정상값을 새 값처럼 끼워 넣지 않습니다. 두 값의 품질은 독립적으로 저장합니다.
- `report_session_id/report_sequence`는 보존한 예전 정상값이 아니라 **최신 수락 보고**의 키입니다. 보고 키/시각은 환경 한 쌍이 공유할 수 있지만 각 값의 `has_value/quality/value_received_ms`는 분리합니다. 정상 온도+오류 습도 보고가 와도 예전 정상 습도의 시각을 연장하지 않습니다. 보존한 예전 값으로 새 측정 메시지를 만들지 않습니다.
- 같은 샘플 재전송은 같은 식별자를 유지합니다. 센서 메시지의 새 순번은 새 측정 시도에만 부여합니다. heartbeat·캐시 재전송으로 측정 시각을 갱신하지 않습니다. 종류별 순서/중복 처리를 분리해 늦게 온 낙상을 다른 센서 순번 때문에 버리지 않습니다.
- 원본 후방 메시지는 상태가 바뀔 때만 전송합니다. 후방 상태에 timeout을 적용하려면 정상 센서의 새 측정에 근거한 주기 상태 재보고도 필요합니다. 같은 SAFE/WARNING 재보고로 음성을 반복 재생하지 않습니다. 센서 오류는 SAFE가 아니며 heartbeat의 최종 상태 비트 또는 별도 후방 상태 메시지로 전달할 규칙이 아직 필요합니다. 현재 빈 SAFE/WARNING payload만으로는 오류 원인을 표현할 수 없습니다.
- 로컬 수신 시각은 측정 시각 그 자체가 아닙니다. 전송 지연/재시도에 상한을 두고 수신 기준 오래됨을 표시합니다. 절대 측정 시각이나 정밀한 age가 필요해지면 해당 새 메시지에 추가합니다. 다른 MCU tick과 내 tick을 직접 비교하지 않습니다.
- 사건은 `(source_id, 사건 종류, incident_session_id, incident_id)`로 구분합니다. 동일 사건은 활성→종료로만 이동하고 재발은 새 ID입니다. 소유권을 확인한 CLEAR가 FALL보다 먼저 도착해도 종료기록을 남겨 지연된 FALL이 부활하지 않게 합니다.
- 종료기록은 무한히 쌓지 않습니다. 재시도 최대 수명과 기록 유지/용량·승인 세션 규칙을 함께 정합니다. 오래된 기록 하나만 기억하는 것으로 모든 역순·재부팅 문제가 해결됐다고 가정하지 않습니다. 수신기 재부팅 후 복구 정책도 미정입니다.
- 한 라이더의 낙상 해제는 다른 낙상·SOS·후방 경고를 지우지 않습니다. 출력은 남아 있는 활성 상태에서 다시 결정합니다. SOS 해제는 별도 합의 전까지 FALL_CLEAR에 연결하지 않습니다.
- 상태 갱신은 검증된 메시지만 단일 앱 실행 흐름에서 수행합니다. ISR/다른 Task는 큐로 전달하고 UI에는 일관된 스냅샷을 제공합니다. 현재 저장소의 단일 소유권 원칙을 유지합니다.

원본 오디오는 재생 중 새 요청을 무시합니다. 따라서 위 요청 큐는 원본의 기존 보장 사항이 아니라 누락을 줄이기 위한 후속 제안입니다. 용량·우선순위·만료 시 버림/진단 정책을 명시해야 하며 모든 요청이 반드시 음성으로 재생된다고 약속하지 않습니다.

## 5. 다른 데이터가 추가돼도 확장하는 방법

**권장: 공통 헤더 + 메시지 종류별 고정 내용 + 새 종류 등록.** 기존 온습도 2바이트와 heartbeat 1바이트를 바꾸지 않습니다. 공통 헤더 9바이트는 아직 제안이며 이번 리뷰에서 늘리지 않습니다.

이 절의 “구형 수신기”는 **같은 v2 프레임·공통 헤더와 미지원 건너뛰기 규칙을 지원하는 이전 기능 버전**을 뜻합니다. 현재 raw 1바이트 UART/v1 펌웨어가 이에 해당하지 않습니다. 현재 v1→새 v2 전환은 기존 계획대로 격리된 구성에서 양쪽을 맞춰 진행해야 합니다.

| 방식 | 선택 |
| --- | --- |
| 하나의 거대한 구조체를 매번 전송 | 사용하지 않음. 새 필드마다 모든 패킷이 커지고 구형 수신기와 어긋남 |
| 고정 메시지 + 새 type | 첫 구현에 권장. 작은 패킷·정확한 길이 검사·영향 분리가 쉬움 |
| 모든 필드를 TLV(Type-Length-Value)로 전송 | 당장은 보류. 각 항목에 식별자·길이 비용이 생김 |
| 별도 확장 메시지 안에서만 TLV | 향후 가변 진단 항목 묶음이 실제 필요하면 추가 가능 |

### 호환성 계약

1. **한 번 배포한 `(version, type)`의 의미·단위·정확한 길이는 변경하지 않습니다.** 온습도에 배터리를 덧붙이지 않고 별도 `BATTERY_UPDATE`를 추가합니다. 더 넓은 온도 범위가 필요하면 `ENVIRONMENT_EXTENDED`처럼 새 종류를 정의합니다. 이름은 예시이며 숫자 ID는 아직 배정하지 않습니다.
2. **새 데이터 추가는 새 type, 공통 헤더 해석 변경은 새 version입니다.** 기존 type 번호를 다른 의미로 재사용하지 않습니다. 기존 수신기가 새 센서를 이해할 수는 없지만 기존 기능을 계속 처리할 수 있게 만듭니다.
3. **공통 프레임 검증과 내용 해석을 분리합니다.** UART는 바깥 경계·길이·CRC/시간초과를 먼저 검사합니다. Mesh는 수신 API의 길이를 사용합니다. 지원하는 공통 헤더인지 확인한 다음 type별 decoder로 분기합니다.
4. **모르는 type은 완성된 한 메시지 단위로 건너뜁니다.** payload를 구형 1바이트 명령으로 재해석하지 않습니다. 같은 헤더 버전이면 경계는 알 수 있으므로 다음 메시지는 정상 처리할 수 있습니다. 상태/출력/측정 최신성을 바꾸지 않고 미지원 진단을 남깁니다.
5. **모르는 version, 잘못된 길이/예약값/범위, 과대 패킷은 적용하지 않습니다.** 모르는 version의 내부를 v2의 9바이트 헤더로 가정해 읽지 않습니다. 미지원과 손상을 구분합니다. 신뢰할 수 없는 UART 길이로 무한 대기하지 않도록 상한·타임아웃·재동기화를 정의합니다. 정확한 UART framing은 아직 별도 설계 대상입니다.
6. **기존 필드의 예약 비트를 조용히 재해석하지 않습니다.** heartbeat 1바이트로 부족한 상세 센서 상태는 새 `HEALTH_DETAIL` 메시지로 확장할 수 있습니다. 기존 status 비트 정의는 한번 배포하면 유지합니다.
7. **수신 완료와 적용 완료는 구분합니다.** 선택적 새 텔레메트리를 무시해도 기존 기능은 유지되지만, 모르는 긴급 명령을 처리 성공이라고 응답하면 안 됩니다. ACK를 추가한다면 어떤 메시지의 어떤 처리 단계에 대한 결과인지 정의해야 합니다. 원본 ACK를 그대로 완료 확인으로 사용하지 않습니다.
8. **크기는 제한합니다.** 제안 상한 `NOSTOS_MAX_MESSAGE_BYTES = 64`는 공통 헤더 포함이며 아직 승인/하드웨어 검증 전입니다. 기존 알려진 메시지는 이 상한보다 작아도 해당 고정 길이와 정확히 일치해야 합니다. UART 외부 포장·분할·큐 메모리 비용은 따로 계산합니다. 임의 길이 malloc이나 무제한 TLV 항목은 허용하지 않습니다.

기본 프레임/헤더를 검사하는 `envelope_parse()`와 내용을 해석하는 `message_decode()`를 구분하는 인터페이스를 제안합니다. 후자는 단순 bool 대신 `OK`, `UNSUPPORTED_VERSION`, `UNSUPPORTED_TYPE`, `BAD_LENGTH`, `BAD_VALUE`, `TOO_LARGE` 같은 **로컬 반환 결과**를 사용합니다. 이것은 wire에 필드를 추가한다는 뜻이 아닙니다. `OK`도 codec 성공일 뿐 소유권·세션·상태 적용 완료가 아닙니다.

ESP32는 알려진 공통 헤더·출처/경로 정책·크기·전송 무결성을 통과한 메시지를 payload 의미와 분리하여 중계할 수 있게 설계합니다. 미지원 type의 불투명 중계 여부는 명시적 정책으로 정하고, 모르는 version은 임의로 전달하지 않습니다. Mesh 수신을 다시 Mesh로 응용 재방송하지 않으며, 로컬 UART 게시 경로와 Mesh→로컬 UART 경로를 구분합니다. 현재 bridge는 알려진 8개 이벤트를 검사한 후 중계하므로 이 분리는 아직 구현되지 않았습니다.

### 새 데이터 하나를 추가할 때

예를 들어 배터리 잔량을 추가한다면:

1. 새 type 이름·ID, 정수 단위·유효 범위·오류값·길이·발신 가능 역할을 메시지 등록표에 정의합니다.
2. 공통 라이브러리에 해당 encoder/decoder와 정상·오류 예제를 추가합니다.
3. 해당 생산자와 수신 소비자만 구현하고 `nodes[i].battery` 같은 저장 슬롯·최신성 정책을 추가합니다. 기존 env/heartbeat 크기와 의미는 유지합니다.
4. 구형 수신기는 새 type을 미지원으로 건너뛰고 기존 온습도·낙상은 계속 처리하는지 검증합니다. 새 데이터가 꼭 필요한 기능은 지원되는 상대인지 배포 시 확인합니다.

메시지 등록표에는 `type / 고정 길이 또는 최대 길이 / 단위 / 품질·오류 / 허용 역할 / 전송 범위 / 처리 우선순위 / 갱신·만료 정책 / decoder / 적용 함수`를 모읍니다. 새 종류마다 여러 switch의 규칙이 서로 달라지는 것을 줄이기 위한 공통 기준입니다. 동적 capabilities 메시지는 추가하지 않으며 역할 기능표와 펌웨어 배포 조합을 함께 관리합니다.

TLV는 실제 필요해질 때 **새 전용 type 내부**에만 넣습니다. 그때 알 수 없는 선택 항목은 길이로 건너뛰고, 필수 항목 미지원·중복 태그·남은 길이 초과·부분 헤더·항목 수 초과는 전체 적용을 거부하도록 계약해야 합니다. 기존 온습도·heartbeat·사건을 즉시 TLV로 변환하지 않습니다.

## 6. 지금 추가하지 않는 데이터

- **거리·기울기 원시값:** 후방/낙상 기능 자체는 판단 결과만 공유해도 됩니다. 원격 숫자 표시·분석 요구가 생기면 새 측정 type으로 확장합니다. 기존 로컬 진단값은 보존합니다.
- **GPS:** 원본 통합 기능 목록에는 없습니다. NOSTOS의 [기존 iOS GPS codec](../../apps/ios-gps-mesh/GPSCore/Sources/GPSCore/GPSCodec.swift)은 별도 24바이트 형식이며 속도 필드가 없습니다. 이를 변경/통합했다는 전제 없이 추후 위치 메시지로 검토합니다.
- **배터리·LiDAR·추가 센서:** 확장 예시이지 이미 구현된 생산자나 필수 필드가 아닙니다.
- **MP3·OLED 프레임버퍼·버튼 raw/debounce·I2C 주소·센서 필터 히스토리·UART CRC:** 드라이버/로컬 진단/전송 계층 소유이며 shared_data의 네트워크 payload가 아닙니다.

## 7. 검증과 남은 결정

문서 검증은 원본/현재 소스 추적, 원본 메시지 enum 전체의 기능표 대응 확인, 링크·코드펜스·공백 검사로 수행합니다. 기존 소스의 전후 해시를 비교합니다. 이 검토에서는 새 수신기를 구현하지 않으므로 아래 호환성/상태 시나리오는 **앞으로 구현 후 실행할 검사**입니다.

실제 검증 결과: 원본 enum 13종의 문서 대응, 원본 고정 커밋 링크 5개의 파일/행 범위, 기능표 근거 파일 존재 여부, 문서 공백·코드펜스 검사 통과. `python3 tools/check_repository.py`는 PASS, `pandoc -f gfm -t json`과 `git diff --check -- docs/brainstorms`도 통과했습니다. 공통 protocol·STM32 MyApp·ESP32 main의 C/H 59개 파일은 검토 전후 SHA-256 동일합니다. 기능 추적과 확장/상태 불변조건은 읽기 전용 서브 에이전트 2명이 교차 검토했습니다. 펌웨어 빌드·호스트 회귀 테스트·실물 통신 시험은 이번 문서 리뷰 범위에서 수행하지 않았습니다.

- 현재 8종 이벤트와 새 env/speed/heartbeat의 정상/오류/최대 크기, 알려진 type의 정확한 길이.
- `정상 기존 → 미지원 신규 → 정상 기존` 스트림, 미지원 payload 안의 `0x13`/SOF, 잘린 프레임/과대 길이/CRC 오류 후 복구.
- 구형 수신기가 새 텔레메트리를 무시하되 기존 기능을 처리하고, 새 긴급 명령의 적용 성공을 거짓 보고하지 않음.
- 온습도 경계/오류·부분 측정 실패, 같은 샘플 재전송과 heartbeat가 정상값 freshness를 연장하지 않음.
- A/B 동시 낙상, CLEAR 선도착, 오래된 CLEAR 뒤 새 사건, FALL과 SOS 동시 상태, 해제 후 남아 있는 후방 경고 재표시.
- 로컬 UART 정상/원격 B 단절, 센서 오류/미장착, 소유권 위조·역순·세션 변경·수신기 재부팅.
- 새 데이터 유입과 큐 포화 중 긴급 메시지 우선 처리, 최대 길이/수량·재시도 상한.

남은 결정: 실제 역할별 기능표, 속도 생산자, status 비트, SOS 발생/해제 UI, 세션 승인·종료기록 수명·재시도 상한, opaque 중계 정책, 최대 메시지 크기/전송량, UART framing과 Mesh 분할 비용. 현재 제안 헤더 9바이트로는 온습도 본문 11바이트·heartbeat 10바이트이므로, payload 압축만으로 비분할 전송이 해결되지는 않습니다.
