# 메시지 프로토콜 — 한 번에 테스트

[테스트 목록](../README.md) · [shared_data 전체 흐름](../../docs/architecture/shared-data/README.md) · [v2 구현 계약](../../libs/protocol/V2.md)

저장소 루트에서 실행합니다. `tests/`는 `scripts/`와 같은 높이입니다.

```sh
bash tests/message-protocol/run.sh
```

**15종 메시지·인코딩·UART·중계·STM32 상태/출력**을 Debug → Release → ASan/UBSan 순서로 검사합니다. 8개 테스트 묶음 × 3개 빌드입니다. 필요한 도구는 C 컴파일러, CMake, Make/Ninja, Python 3이며 자동 설치하지 않습니다. 다른 디렉터리에서 절대 경로로 실행해도 됩니다.

기존 프로젝트 회귀 검사와 실제 v2 펌웨어 빌드까지 모두 실행하려면:

```sh
bash tests/message-protocol/run.sh --all
```

`tools/test-host.sh`에도 새 테스트가 연결되어 있습니다. 실패한 명령이 있으면 shell은 0이 아닌 종료 코드를 반환합니다. 빌드·로그는 실행마다 새로운 임시 폴더에 보관하며 시작할 때 위치를 출력합니다. 기본 실행은 `<임시 폴더>/<variant>-tests.log`에 개별 메시지 결과도 남깁니다.

| 실행 옵션 | 검사 범위 | 추가 도구 |
| --- | --- | --- |
| 옵션 없음 | 메시지 프로토콜 호스트 테스트 24회 | 위의 호스트 도구 |
| `--targets` | v2 ON STM32 Debug/Release + ESP32-S3 전체 빌드 | ARM GNU, Ninja, ESP-IDF v5.5.5 |
| `--all` | 저장소 전체 호스트 회귀 + `--targets` | 위 도구 모두 |

[build_targets.py](build_targets.py)는 설치된 도구만 사용합니다. `PATH`, `ESP_IDF_PATH`/`IDF_PATH`, `IDF_TOOLS_PATH`를 사용하며, 없으면 `~/.local/share/nostos-toolchains`의 기존 설치를 찾습니다. 이 기본 폴더는 `NOSTOS_TOOLCHAINS`로 지정할 수 있습니다. 자동 설치·셸 프로필 변경은 하지 않습니다. 필요한 도구가 없거나 테스트가 실패하면 성공으로 건너뛰지 않고 종료 코드가 0이 아닌 값이 됩니다.

타깃 빌드는 소스를 새 임시 폴더에 복사해 수행합니다. `source-hashes.json`은 복사 당시 입력의 SHA-256, `results.json`은 각 타깃 결과이며 configure/build/size/symbol 로그도 보관합니다. 복사본에만 v2 설정을 적용하고, STM32 ELF에 codec·출력 경로가 실제 남아 있는지와 ESP32가 v2 런타임을 컴파일했는지도 검사합니다. ESP32 주소는 의도적으로 0으로 두고 STM32의 기본 boot hook도 NOT_READY를 유지하므로, 이 빌드 산출물을 배포 준비가 끝난 펌웨어로 취급하지 않습니다.

## 한눈에 보는 테스트 경로

```text
mock_messages.json (고정된 정답 바이트 15종)
      ↓ 메시지 생성 + 실제 nostos_endpoint_publish
송신 STM32 역할 → 순번 발급·로컬 상태 반영·encode → UART 프레임
      ↓ 실제 nostos_bridge (소유 버퍼로 복사)
송신 ESP32 역할 → 가상 Mesh: source2 → relay1 → receiver3
      ↓ 원래 source/session/sequence/내용 유지
수신 ESP32 역할 → UART 프레임 재생성
      ↓ 실제 nostos_receiver / nostos_endpoint
수신 STM32 endpoint → shared_data → RGB·부저 / 오디오 callback
      ↓ 실제 STM32 service·GPIO/VS1003B 드라이버, HAL만 mock
GPIO 핀 상태 / Flash MP3 선택 / DREQ 확인 / 최대32B SPI 전송 검증
```

ESP32 런타임 시험은 실제 `bridge_runtime_v2.c`의 UART parser, worker 처리 단계, Mesh 수신 callback에 **15종 모두를 양방향으로** 통과시킵니다. RTOS 스케줄링과 ESP-IDF 드라이버/무선 API는 mock입니다. 별도의 3노드 relay 시험은 공통 C 라이브러리의 실제 endpoint 송신·수신 함수를 연결한 네트워크 모델입니다. 두 시험을 합쳐 실제 BLE stack이나 전파를 검증했다고 표현하지 않습니다.

## mock 데이터

[mock_messages.json](mock_messages.json)이 사람이 읽을 수 있는 고정 테스트 데이터입니다. [generate_fixtures.py](generate_fixtures.py)는 이를 C 배열로 옮기기만 하고, 제품 encoder를 호출해 정답을 만들지 않습니다.

| 종류 | 예시 | 공통 본문 크기 |
| --- | --- | --- |
| 감속·가속·안전/응원·정지 | 각각 다른 type, payload 없음 | 각9B |
| 후방 안전·경고·미확인 | 정상값과 센서 오류 구분 | 각9B |
| FALL·SOS·FALL_CLEAR·SOS_CLEAR | 사건 세션1, 사건 번호11/12 | 각15B |
| 속도 | 25.3km/h → `253` | 12B |
| 온습도 | 36.2°C / 60.5% → `89 79` | 11B |
| heartbeat | sensor/output fault bits → `03` | 10B |
| ACK | source3의 STOP 순번7에 대한 응답 | 18B |

온습도 생산 입력 60.3%가 60.5%로 양자화되는 경우도 별도로 검증합니다. 등록표에 새 메시지를 추가하고 fixture를 빠뜨리면 검사에 실패합니다. 현재 등록 메시지는 15종이며 ACK는 수신된 참조/결과를 표현할 뿐 자동 신뢰성·재전송 기능은 아닙니다.

## 무엇을 검사하나?

| 묶음 | 실제 검사 |
| --- | --- |
| `codec` | 15종 golden roundtrip, 모든 잘림 위치, 출력 보존, 온도 int16/습도 uint16 전체 입력, 코드 조합65,536개, 순번 소진 |
| `uart` | CRC 기준값 `123456789→29B1`, 15종 프레임, flag/escape, 최대64B, 시간초과·CRC·과대길이 후 복구 |
| `state` | 노드별 값, 품질, 최신성, 종류별 순서, CLEAR 선도착, 다른 라이더 사건 보존, FALL/SOS 독립, 음소거, 세션 승인, 큐·종료기록 포화 |
| `bridge` | 실제 Mesh 주소↔source 검증, 메모리 수명, 큐 포화·만료, 자기 echo 차단, 재방송 금지, 미지원 v2 type 운반 |
| `relay` | **모든15종**의 endpoint publish→UART→bridge→mock relay→수신 endpoint→상태/출력 callback. OFF→ON→OFF, TTL0/1, 중복2회. CLEAR는 활성 사건을 먼저 준비해 실제 해제 확인 |
| `fuzz` | 고정 seed로 잘못된 입력100,000개, 파서 메모리 안전성·정상 프레임 복구 |
| `stm32_outputs` | HAL RX callback→512칸 ring→메인 루프, overflow 복구, 초음파 무응답→UNKNOWN/회복, 실제 RGB/부저 GPIO, 네 가지 실제 MP3 배열, DREQ low 대기, SPI 최대32B, 음소거, 요청 만료, SPI 실패 |
| `esp32_runtime` | 실제 v2 런타임 UART→Mesh와 Mesh→UART에 **15종 전체** 전달, 원문·버퍼 수명 보존, 부분 FIFO 쓰기, bounded timeout, 출처/자기 주소 검증, 실패 후 자동 재송신 없음 |

예상 최종 출력:

```text
MESSAGE_PROTOCOL=PASS; MOCK_UART_AND_RELAY=PASS; REAL_BLE_RF=NOT_TESTED; HARDWARE_OUTPUTS=NOT_TESTED
```

## Relay 검증의 범위

- 호스트 모델에서 source2와 receiver3는 직접 연결되지 않습니다. relay1이 켜지고 TTL≥2인 경우에만 receiver3에 도달합니다. 원래 발신 주소와 payload를 보존하고, STM32는 중복을 한 번만 적용합니다.
- 송신 endpoint가 직접 만든 UART 프레임에서 시작하며, 고정 golden과 송신 바이트를 대조합니다. 수신 endpoint의 값/품질·사건 상태·출력 callback까지 검사합니다. 온습도는 송신 입력 60.3%가 송신/수신 상태표 모두 60.5%로 반영되는지 확인합니다. 해제 메시지가 전달되지 않으면 기존 긴급 사건이 남는 것도 확인합니다.
- TTL은 **mock Mesh 계층**에만 있습니다. 제품 `shared_data`나 공통 본문에 TTL을 추가하지 않았습니다. 실제 장치에서는 ESP-IDF가 처리합니다.
- 이 shell은 USB/시리얼을 열지 않으며 Flash·reset·provisioning·키·Relay 설정을 변경하지 않습니다.
- 실제 전파 중계는 별도의 실물 검증입니다. [기존 Relay 절차](../mesh/04-relay/README.md)는 배치/대조군 참고이며 **기존 v1 메시지용이므로 v2 시험 성공으로 재사용할 수 없습니다.** v2 장치 배포·출처 매핑·STM32 세션 복구를 준비한 후 별도 승인된 실물 시험이 필요합니다.

## 배포 전 남는 경계

공통 프로토콜과 호스트 테스트는 구현되어 있습니다. ESP32에는 명시적으로 선택하는 v2 런타임이 추가됐고 STM32에는 새 endpoint/출력 서비스가 있습니다. **현재 기본 앱은 여전히 v1**입니다. STM32의 선택형 ISR→메인 루프 경로도 연결했으며, 실제 부팅의 승인 세션/순번 영속 복구는 [구현 계약의 연결 절차](../../libs/protocol/V2.md#펌웨어-연결과-배포-경계)를 따릅니다. 기본값을 바꿔 혼합 네트워크에 바로 보내지 않습니다.

현재 Mac의 `~/.local/share/nostos-toolchains`에서 ARM GNU 15.3.Rel1과 ESP-IDF v5.5.5를 확인해 v2 타깃 전체 빌드까지 통과했습니다. 앞서 기본 설치 경로에서 도구를 찾지 못한 결과를 갱신합니다. 빌드 성공은 RTOS 동시성·실물 UART/RF·스피커 동작 검증을 대체하지 않습니다.

## 이번 구현에서 실행한 검증

2026-08-29 기준:

- `bash tests/message-protocol/run.sh`: 8묶음×Debug/Release/ASan·UBSan = 24회 PASS.
- 완료 감사에서 relay 시험의 시작점을 실제 endpoint publish로 확장하고, ESP32 런타임의 양방향 시험을 온습도 1종에서 전체 15종으로 확장한 뒤 24회 재검사 PASS.
- `bash tests/message-protocol/run.sh --all`: 새 검사와 기존 공통/STM32/ESP32/통신/GPS C 검사, Python 회귀, v2 ON STM32 Debug/Release 및 ESP32-S3 전체 빌드 PASS.
- ARM GCC의 `-Werror=array-parameter`가 찾은 UART·bridge 함수 선언/정의 표기 차이를 맞춘 뒤 다시 빌드했습니다. STM32에 `-Werror`를 적용하고, ELF에 공통 codec·audio/VS1003B 경로가 포함되는지 확인했습니다.
- STM32 ELF 크기(text/data/bss, 바이트): Debug `134808/104/6120`, Release `119012/108/6120`. ESP32 앱 이미지 `0xdf080`바이트, 앱 파티션 41% 여유. 이는 해당 소스/도구 설정의 결과이며 런타임 stack 최대치 측정은 아닙니다.
- `clang --analyze`로 새 공통 C5파일 검사: 진단 없음.
- shell 구문/잘못된 인자/컴파일러 실패 시 nonzero 전달, 문서 링크 검사 PASS.
- 기본 경로의 도구 부재로 실패했던 초기 빌드는 위의 설치 위치 탐색 및 실제 전체 빌드로 해결했습니다. 새 도구를 설치하지 않았습니다.

실제 펌웨어 부팅·RTOS 경쟁·Mesh 분할·RF 중계·GPIO 전압/소리는 이번 호스트 결과에 포함되지 않습니다. 기본v1을 변경하거나 보드에 Flash하지 않았습니다.
