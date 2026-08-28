# 학습·설계 기록

[전체 시작 메뉴](../../README.md) · [학습 순서](../02-learning/README.md) · [현재 진행 상태](../01-project/STATUS.md)

이곳은 **어떻게 이해하고 설계했는지**를 찾아보는 기록 목록이다. 처음 공부할 때 전부 읽을 필요는 없다. 직접 작성한 답변, 아직 답하지 않은 질문, 검토 중인 설계는 그대로 보존한다.

## 직접 작성한 학습 기록

| 기록 | 내용 |
| --- | --- |
| [BRINGUP-NOTES](BRINGUP-NOTES.md) | Firmware / SW Bring-up의 상세 설명, 공동 작성한 답변, 이해 확인과 발표 초안 |
| [MY_UNDERSTANDING](MY_UNDERSTANDING.md) | Advertising·GATT·Provisioning·Relay를 내 말로 정리한 이해와 질문 |

학습 기록의 “확인 완료”는 해당 이해 확인에 대한 표시다. 보드 실행이나 통신 검증 완료와 같은 뜻이 아니다.

## Layer 설계·구현 계획 원본

기존 `docs/superpowers/` 경로는 Layer 문서에서 참조하므로 유지한다. 파일 날짜나 계획의 체크 표시만으로 현재 하드웨어 검증 완료를 판단하지 않는다. 진행 중인 설계도 이 목록에 포함된다.

| 대상 | 설계 | 구현 계획 |
| --- | --- | --- |
| Layer 2 — GATT Server | [설계](../superpowers/specs/2026-08-27-layer-2-gatt-server-design.md) | 별도 문서 없음 |
| Layer 3 — Active Scanner | [설계](../superpowers/specs/2026-08-27-layer-3-active-scanner-design.md) | 별도 문서 없음 |
| Layer 4–5 — Dual Role / Packet | [설계](../superpowers/specs/2026-08-27-layer-4-5-dual-role-packet-design.md) | 별도 문서 없음 |
| Layer 6 — Custom Forwarding | [설계](../superpowers/specs/2026-08-27-layer-6-symmetric-forwarding-design.md) | [계획](../superpowers/plans/2026-08-27-layer-6-symmetric-forwarding-plan.md) |
| Layer 7 — Standard Mesh | [설계](../superpowers/specs/2026-08-27-layer-7-standard-mesh-final-design.md) | [계획](../superpowers/plans/2026-08-27-layer-7-standard-mesh-final-plan.md) |
| Layer 8 — STM32 UART ↔ Mesh | [실행 문서](../../layers/layer-8/README.md) | [검증 기록](../../layers/layer-8/VERIFICATION.md) |
| iPhone GPS Mesh | [설계](../superpowers/specs/2026-08-28-iphone-gps-mesh-design.md) | [계획](../superpowers/plans/2026-08-28-iphone-gps-mesh-plan.md) · [검증](../superpowers/plans/2026-08-28-iphone-gps-mesh-verification.md) |

## 2026-08-28 통합·실물 기록

| 기록 | 내용 |
| --- | --- |
| [Layer 8 B6 진단](../../layers/layer-8/B6_SETUP.md) | AppKey 불일치와 Publication 상태를 NVS에서 비교하고, 기존 키를 삭제하지 않는 설정 절차 정리 |
| [Layer 8 빠른 버튼 → Mesh 검증](../../layers/layer-8/FAST_CHECK.md) | STM32·D6·76·B6 로그를 한 관찰 창에서 비교하는 읽기 전용 도구와 보수적 판정 기준 |
| [STM32 버튼/UART 시험](../../stm32-project/integration/stm32/BUTTON_UART_TEST.md) | PB6 → USART2 → ST-LINK VCP까지 확인하고 외부 D1 → ESP32 구간을 미검증으로 분리 |
| [STM32 Event bridge 검증](../../stm32-project/integration/esp32-s3/VERIFICATION.md) | 공통 codec/queue, ESP32·STM32 build와 남은 실제 종단 간 시험 |
| [STM32 SharedState](../../stm32-project/integration/stm32/SHARED_STATE.md) | A/B/C 센서값 저장·조회 모듈과 실제 UART·Dashboard 연결 경계 |

위 기록은 시간순 진행 증거다. 더 나중 문서가 앞선 `NOT_READY` 상태를 갱신할 수 있으므로 현재 요약은 [STATUS](../01-project/STATUS.md)를 먼저 본다.

## 실행 결과와 프로젝트 아이디어는 별도

- 실제 빌드·보드·무선 검증 결과: [진행 상태와 날짜별 기록](../01-project/STATUS.md)
- 다음에 공부할 Layer와 통과 기준: [Layer 로드맵](../02-learning/LAYER-ROADMAP.md)
- 센서 배치·공유 데이터·Task 정책·리뷰 제안: [프로젝트 개요](../01-project/OVERVIEW.md)

## 2026-08-27 문서 위치 변경 안내

| 이전 위치 | 읽을 곳 |
| --- | --- |
| 루트 `PROGRESS.md` | [진행 상태와 검증 기록](../01-project/STATUS.md) |
| 루트 `STEPS.md` | [Layer 로드맵](../02-learning/LAYER-ROADMAP.md) — 실행 증거는 STATUS로 분리 |
| `learning-reference/SEQUENCE.md`와 01–03 문서 | [학습 순서](../02-learning/README.md) |
| `learning-reference/BOOTLOADING.md` | [Build & Boot](../03-reference/BUILD-AND-BOOT.md) |
| `learning-reference/TERMS.md` | [용어](../03-reference/TERMS.md) |
| `learning-reference/BRINGUP-NOTES.md`, `MY_UNDERSTANDING.md` | 위 학습 기록 두 문서 |
| 루트 C3 Mesh 설명·이미지 목록 | [C3 참고 설명](../03-reference/ESP-BLE-MESH-C3.md) · [이미지 출처](../03-reference/IMAGE-MANIFEST.md) |
| `communication-module/docs/distributed-sensor-shared-dashboard.md` | [프로젝트 개요](../01-project/OVERVIEW.md) |

소스·헤더·빌드 설정과 산출물, `layers/`, `examples/`, `images/`, `scripts/`는 문서 정리 대상이 아니다. 통신 모듈 내부 README는 코드 옆에 둔다.
