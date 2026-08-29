# Redpill 수업 코드 — 김현수 (Alex)

**Redpill 수업에서 배운 김현수의 코드를 모듈로 정리한 학습 자료입니다.
NOSTOS 프로젝트에 적절히 적용하고 사용 근거를 설명하여 가산점을 받는 것을 목표로 합니다.**
가산점 부여 여부는 수업 평가 기준과 담당자의 판단에 따르며, 이 폴더 생성만으로 가산점이나 펌웨어 적용 완료를 의미하지 않습니다.

- 수업: [빨간 약을 삼킨 개발자: 온디바이스AI SW개발(1기)](https://app.notion.com/p/kafka-snowflake/AI-SW-1-28b2ae70087182bcb3dd812e10ffbda3)
- 제출물 이름: **김현수** — 사용자가 Alex의 제출물 이름으로 확인했습니다.
- 수집일: 2026-08-28. 당시 페이지에서 읽은 코드 기준이며 Notion은 수정하지 않았습니다.
- 수집: [KAF-376](https://linear.app/kafkasnowflake/issue/KAF-376). 이름 정리·C 검토: [KAF-379](https://linear.app/kafkasnowflake/issue/KAF-379).
- 현재 상태: **독립 호스트 실행 예제**. 기존 STM32·ESP32 펌웨어에 연결하지 않았습니다.

## 빠르게 실행하기

C11 컴파일러와 CMake가 필요합니다. 아래 명령은 **NOSTOS 저장소 루트**에서 실행합니다.

```sh
cmake -S redpill -B redpill/build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake --build redpill/build/Debug
./redpill/build/Debug/redpill_demo --list
./redpill/build/Debug/redpill_demo 15
./redpill/build/Debug/redpill_demo --all
./redpill/build/Debug/redpill_demo --test
```

인자 없이 실행하면 전체 데모를 실행합니다. 날짜 번호는 `--list`에 나온 것만 지원합니다.
실제 UART 포트는 열지 않습니다. `redpill/` 안에서 실행할 때는 위 경로에서 앞의 `redpill/`을 빼면 됩니다.

## 구조

```text
redpill/
├── main.c                  # 유일한 실행 진입점: 날짜 선택 / 전체 / 테스트
├── <기능>.c                # 과제별 구현과 rpXX_demo()
├── <기능>.h                # 다른 코드에서 사용할 API·타입
├── CMakeLists.txt          # redpill_modules 정적 라이브러리 + redpill_demo
├── test.sh                 # Debug / Release / ASan·UBSan / 출처 검사
├── tests/                  # 반환값·경계·표준 함수 비교와 CLI 출력 검사
└── originals/              # 수정하지 않은 제출 코드 22개 + 출처/해시 목록
```

기존 각 과제의 `main()`을 `rpXX_demo()`로 옮기고, 공통 `main.c`에서 호출합니다.
외부에 공개하는 함수·타입에 `rpXX_` 접두어를 붙여 이름 충돌을 피했습니다.
출력 전용 보조 함수는 파일 내부 `static`으로 유지했습니다.
원문 문서 안의 `main()`은 기록이며 컴파일 대상이 아닙니다.
파일 이름은 `debounce.c`, `ring_buffer.h`처럼 기능만 표시합니다.
수업 번호는 출처 추적용 문서·원문 파일·CLI 날짜 선택에 남기고, 기존 `rpXX_` 공개 API는 호환성을 위해 유지했습니다.

## 모듈과 원문

| Day | 내용 | 구현 / 헤더 | 김현수 제출 원문 |
| --- | --- | --- | --- |
| 4 | 8비트 반전 | [C](bits.c) / [H](bits.h) | [원문](originals/day04.md) |
| 5 | 1비트 개수: Naive·Kernighan·SWAR | [C](popcount.c) / [H](popcount.h) | [원문](originals/day05.md) |
| 6 | 32비트 좌우 회전 | [C](rotate.c) / [H](rotate.h) | [원문](originals/day06.md) |
| 7 | XOR 체크섬 | [C](checksum.c) / [H](checksum.h) | [원문](originals/day07.md) |
| 8 | 겹치는 메모리 복사 | [C](memmove.c) / [H](memmove.h) | [원문](originals/day08.md) |
| 9 | 바이트 단위 제네릭 swap | [C](swap.c) / [H](swap.h) | [원문](originals/day09.md) |
| 10 | 연속 데이터 영역의 2차원 배열 | [C](matrix.c) / [H](matrix.h) | [원문](originals/day10.md) |
| 11 | enum + 함수 포인터 계산기 | [C](calculator.c) / [H](calculator.h) | [원문](originals/day11.md) |
| 12 | 구조체 멤버 오프셋 | [C](offset.c) / [H](offset.h) | [원문](originals/day12.md) |
| 13 | 고정 크기 메모리 풀 | [C](pool.c) / [H](pool.h) | [원문](originals/day13.md) |
| 14 | 16바이트 단위 Hexdump | [C](hexdump.c) / [H](hexdump.h) | [원문](originals/day14.md) |
| 15 | 원형 버퍼 | [C](ring_buffer.c) / [H](ring_buffer.h) | [원문](originals/day15.md) |
| 16 | 연결 리스트 생성·역순·해제 | [C](linked_list.c) / [H](linked_list.h) | [원문](originals/day16.md) |
| 17 | 비트맵 리소스 할당 | [C](bitmap.c) / [H](bitmap.h) | [원문](originals/day17.md) |
| 18 | 최소 힙 우선순위 큐 | [C](min_heap.c) / [H](min_heap.h) | [원문](originals/day18.md) |
| 22 | Delta-list 소프트웨어 타이머 | [C](timer.c) / [H](timer.h) | [원문](originals/day22.md) |
| 23 | 원문을 변경하지 않는 토크나이저 | [C](tokenizer.c) / [H](tokenizer.h) | [원문](originals/day23.md) |
| 24 | 연속 샘플 기반 디바운싱 | [C](debounce.c) / [H](debounce.h) | [원문](originals/day24.md) |
| 25 | 이동 평균 + 50% 범위 필터 | [C](moving_average.c) / [H](moving_average.h) | [원문](originals/day25.md) |
| 26 | 생산자·소비자 순차 시뮬레이션 | [C](producer_consumer.c) / [H](producer_consumer.h) | [원문](originals/day26.md) |
| 33 | UART 채팅 입력·엔터·백스페이스 | [C](uart_chat.c) / [H](uart_chat.h) | [v1](originals/day33-v1.md) / [v2](originals/day33-v2.md) |

각 원문 문서에 개별 Notion 제출물 링크를 넣었습니다.
[manifest.json](originals/manifest.json)은 수집 여부, 제출자, 출처, 원문 코드 SHA-256을 기록합니다.
SHA-256은 원문 코드 블록의 UTF-8 텍스트를 검사하며 Notion 페이지 전체나 이미지를 검증하는 값이 아닙니다.

### 포함 범위

- **개인 과제 20개**: Day 4–18, 22–26.
- **UART 2개 버전**: 공동 페이지의 `김현수`, `김현수 2` 구역만 원문으로 보존했습니다. `홍성록` 구역은 가져오지 않았습니다. 실행 모듈은 두 번째 버전의 `collect_input`을 기반으로 합니다.
- **Day 19·20·27·28**: 해당 데이터베이스에서 김현수 이름의 제출물을 찾지 못해 구현하지 않았습니다.
- **Day 1–3**: 수업 본문에 문제/예시는 있으나 김현수 제출 코드로 식별하지 못해 가져오지 않았습니다. Day 21은 제공된 페이지의 과제 목록에 없었습니다.
- **Day 34**: [홍성록 / 김현수 리드 스위치 실습](https://app.notion.com/p/10d2ae7008718375969f013a27e6c904)은 설명·사진·영상만 확인되며 C 구현 코드가 없어 새 구현을 만들지 않았습니다.
- Notion 생성자가 모두 Alex로 표시된 자료도 있어 **생성자 정보로 저자를 추정하지 않았습니다.**

### UART 코드(Day 33)는 어디서 왔나?

원본 수업 페이지의 **`uart 채팅 프로그램` → `Day 33` 데이터베이스 →
[김현수, 홍성록 제출 페이지](https://app.notion.com/p/3952ae7008718257bffd818ba48c8329) → `김현수 2`** 구역입니다.
`33`은 제가 새로 붙인 번호가 아니라 원본 데이터베이스 이름입니다.
[uart_chat.c](uart_chat.c)는 그 구역의 `collect_input()`에서 엔터 전송·백스페이스 처리를 분리한 **호스트용 변형**입니다.
STM32 HAL/DMA/인터럽트 초기화 코드는 옮기지 않았으며, 원문 두 버전은 [v1](originals/day33-v1.md), [v2](originals/day33-v2.md)에 남아 있습니다.

## 원문에서 바꾼 부분

원문은 `originals/`에 보존하고, 다음 변경은 실행 모듈에만 적용했습니다.

| 대상 | 변경과 이유 |
| --- | --- |
| 공통 | 진입점 통합, 헤더 분리, 공개 API 이름 구분, 독립 CMake 빌드와 검증 추가 |
| Day 5 | 정의 없는 미사용 선언 제거. 32비트 입력을 보존하는 builtin 호출과 비 GNU 컴파일러용 SWAR 대체 경로 |
| Day 6 | 누락된 `<stdbool.h>` 추가 |
| Day 8 | 서로 다른 배열의 포인터 대소 비교 대신 바이트 주소의 동등 비교로 겹침 방향을 확인 |
| Day 10 | 행 포인터·데이터 크기 계산의 `size_t` 곱셈 초과 검사 |
| Day 11 | 무한 입력 대기를 피하도록 원래 4개 연산을 비대화형 데모로 통합. 연산 선택은 함수 포인터 배열을 유지하며, 잘못된 ID·0 나눗셈·정수 범위 초과 검사를 추가 |
| Day 12 | 원래의 주소 0 기반 매크로는 원문에만 보존. 실행본은 **실제 객체의 바이트 주소 차이**로 오프셋을 계산하는 `RP12_MEMBER_OFFSET` 사용. 원래의 타입 인자/주소 0 과제 조건과 다른 안전한 변형임 |
| Day 13 | 정렬 명시, free-list 링크를 `memcpy`로 읽고 쓰기, 외부/중간 주소·중복 해제 거부. `pool_free`가 성공 여부 반환 |
| Day 18 | 내부 태스크를 재삽입할 때 `realloc` 이후 해제된 메모리를 읽지 않도록 입력 태스크를 미리 복사. ASan으로 실패 재현 후 회귀 검사 추가 |
| Day 23 | 데모 문자열을 `static const`로 두어 정적 커서보다 문자열 수명이 짧아지지 않도록 함 |
| Day 26 | 시간 기반 난수 시드를 `1U`로 고정하여 같은 환경에서 재현 가능하게 함 |
| Day 33 | HAL 송신을 동기 콜백으로 분리, 전역 상태를 구조체로 이동, 백스페이스 시 감소 후 제거된 바이트를 지우도록 수정. ST 저작권 고지 보존 |

## 사용할 때 지킬 조건

- **학습용·단일 실행 흐름 기준**입니다. ISR/RTOS에서 공유하려면 동기화·오류 처리·메모리 정책을 별도로 설계해야 합니다. `volatile`만으로 스레드 안전성이 생기지 않습니다.
- 버퍼 API의 포인터와 길이는 실제 유효한 저장공간을 가리켜야 합니다. 크기를 전달하지 않는 API에서 버퍼 크기를 자동으로 알아내지는 않습니다.
- Day 7: `rp07_update_packet_checksum(packet, n)`에는 **n+1바이트**가 필요합니다. XOR 체크섬은 암호학적 인증이 아니며 기존 NOSTOS 프로토콜을 대체하지 않습니다. 예제 데이터의 실제 체크섬은 `0x45`입니다.
- Day 9: 같은 타입의 두 객체를 크기만큼 교환합니다. 부분적으로 겹치는 영역은 지원하지 않습니다.
- Day 10: 반환된 배열은 `rp10_free_matrix()`로 해제합니다. Day 16 연결 리스트는 `rp16_destroy()`, Day 18 힙의 `tasks`는 `free()`로 해제합니다. 살아 있는 컨테이너를 재초기화하지 않습니다.
- Day 13: **10개 × 32바이트의 바이트 저장공간**입니다. payload는 `memcpy` 또는 `unsigned char`로 접근합니다. 임의 구조체 포인터 캐스팅을 위한 범용 malloc 대체품이 아닙니다. 풀은 사용 중 이동·복사·재초기화하지 않습니다.
- Day 15: 8칸 중 한 칸을 비워 **7바이트**를 저장합니다. 생성 후 `rp15_rb_init()`이 먼저 필요합니다.
- Day 17: 최대 20개 리소스. Day 18: 최대 100개 태스크이며 숫자가 낮은 priority가 먼저 나옵니다. 같은 priority의 순서는 보장하지 않습니다.
- Day 18: 내부 태스크를 입력으로 재삽입할 수 있습니다. 삽입 성공 후 내부 배열 포인터는 다시 얻어야 하며, `rp18_extract()`의 출력 포인터는 내부 배열과 겹치면 안 됩니다.
- Day 22: 원래의 **단일 전역 스케줄러**를 유지합니다. 초기화 후 1ms마다 `rp22_Tick()`을 호출하는 모사이며 자동으로 실제 시간을 재지 않습니다. ID 중복 방지는 호출자 책임이고 0ms는 거부합니다. 기존 타이머가 모두 만료된 뒤에만 재초기화하고, 콜백에서 스케줄러에 재진입하지 않습니다. 할당 실패는 원문처럼 메시지로 보고합니다.
- Day 23: 정적 커서를 공유합니다. non-NULL 인자는 새 문자열, NULL은 이어 읽기이며 원문 문자열이 살아 있어야 합니다. 빈 토큰은 건너뜁니다. 여러 파서를 동시에 실행할 수 없습니다.
- Day 24: 초기화 성공 후 사용하며, 반환값은 출력 값이 아니라 **상태 변경 여부**입니다. 출력은 `stable_output`에 있습니다.
- Day 25: 원본대로 **첫 5개 유한값은 워밍업으로 수용**, 이후 평균의 ±50% 밖이면 거부합니다. 거부된 샘플은 버퍼에 넣지 않습니다. 평균이 0이면 이후 0만 수용하는 한계도 유지했습니다. `fabsf`는 사용하지 않습니다.
- Day 26: 생산·소비를 순서대로 호출하는 모사입니다. 실제 병렬 실행용 큐가 아닙니다.
- Day 33: 초기화 성공 후 입력합니다. CR/LF에서 전송하고 빈 줄은 무시합니다. 최대 63바이트, 초과 바이트는 원문대로 버립니다. 콜백은 데이터를 반환 전 소비/복사해야 하고 재진입하면 안 됩니다. 원문 HAL/DMA/IRQ 설정은 설치하지 않았습니다.

## NOSTOS 가산점 활용 후보

| 프로젝트에서 설명할 적용 | 수업 코드 | 적용 시 남길 증거 |
| --- | --- | --- |
| 버튼 튐 제거 | Day 24 | 원신호/안정 출력과 임계 샘플 수 비교 |
| 센서 노이즈 완화 | Day 25 | 필터 전후 값, 워밍업·이상값 거부 정책 |
| UART 데이터 보관·진단 | Day 15, 14, 33 | 버퍼 초과/줄 편집 시험, 실제 송수신 로그 |
| 이벤트 우선순위·지연 처리 | Day 18, 22 | 처리 순서와 타이밍 검증 |
| 데이터 처리·메모리 학습 | Day 6–10, 13, 17, 23 | 입력/출력 계약, 경계 검사, 사용 이유 |

예를 들어 별도 프로그램에서 디바운서를 쓰려면 아래 헤더와 해당 `.c`를 함께 빌드합니다.
기존 NOSTOS 코드에 자동으로 삽입하지는 않았습니다.

```c
#include "debounce.h"

rp24_Debouncer button;
if (rp24_init_debouncer(&button, 3)) {
    bool changed = rp24_debounce(&button, raw_button);
    /* changed가 true이면 button.stable_output을 새 상태로 처리 */
}
```

## 검증

전체 검사에는 Python 3도 필요합니다.

```sh
bash redpill/test.sh
```

- Debug / Release / ASan·UBSan 각각 CTest 24개: 21개 데모, 전체 실행, 잘못된 날짜, 경계 검사.
- 경계 검사는 Release에서도 꺼지지 않는 `CHECK`로 수행합니다. 모든 8비트 반전, 여러 32비트 입력의 bit count/회전, `memmove`와 겹침 비교, 할당 크기 초과, pool/bitmap/ring/heap 한계, 같은 시점 타이머, 문자열 불변성, 필터와 UART 편집을 포함합니다.
- [verify.py](tests/verify.py): 원문 코드 22개 SHA-256, 단일 main, CLI 오류·예제 결과·로컬 링크 확인.
- 빌드 산출물은 `redpill/build/`에만 생성하고 Git에서 제외합니다.
- **실물 보드·UART/DMA·ISR 타이밍·NOSTOS 통합은 미검증**입니다. 원문도 그대로 빌드 가능하다고 주장하지 않습니다.
- 이번 변경은 독립 폴더뿐이므로 기존 펌웨어/공통 프로토콜 빌드와 루트 `tools/test-host.sh`는 변경 검증 대상에 포함하지 않았습니다.

### 최초 수집 검증 — 2026-08-29

macOS / AppleClang 21에서 `bash redpill/test.sh` 통과:
Debug·Release·ASan/UBSan 각각 **24/24**, 총 72개 CTest 실행 성공.
각 경계 검사 실행에서 **140,849개 CHECK** 성공. 원문 22개 해시와 21개 데모 출력/CLI/링크 검사도 통과했습니다.
각 `*.h` 단독 C11 구문 검사와 각 `*.c`의 Clang 정적 분석을 수행했으며 진단은 없었습니다.

### 이름 정리와 C 코드 검토 — 2026-08-29

`embedded-systems` 스킬 지침으로 21개 모듈의 포인터 수명·버퍼 경계·할당·초기화 계약을 검토했습니다.
실행 소스/헤더 42개를 기능 이름으로 변경하고 include·CMake·문서·manifest 매핑을 함께 갱신했습니다.
원문 Markdown 22개는 변경 전후 파일 해시까지 동일합니다.

- **수정한 결함:** 최소 힙의 내부 원소를 `rp18_insert(&heap, &heap.tasks[0])`로 재삽입하면서 배열이 커지면, `realloc()`이 기존 배열을 해제한 뒤 그 원소를 읽었습니다. 수정 전 ASan에서 `heap-use-after-free`를 재현했습니다.
- **수정:** 입력 태스크를 지역 변수에 먼저 복사하고 그 값으로 삽입합니다. 기존 단위 검사에 재할당을 동반한 내부 원소 재삽입·재추출 검사를 추가했습니다.
- **검증:** `bash redpill/test.sh` 재실행 성공. Debug·Release·ASan/UBSan 각각 24/24, 경계 검사 **140,855 CHECK**, 원문 해시·새 파일 매핑·CLI·출력·링크 검사 통과.
- **정적 검사:** 21개 헤더의 C11 단독 컴파일과 루트 C 파일 22개의 Clang 정적 분석에서 진단 없음.
- 기존 수업 코드의 단일 실행 흐름·워밍업·자원 관리 조건은 위의 API 계약을 따릅니다. 실제 펌웨어 통합·보드 검증은 수행하지 않았습니다.
