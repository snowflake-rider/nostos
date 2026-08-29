# 아키텍처

[문서 목록](../README.md)

**궁금한 주제 하나를 골라 읽는 공간**입니다. 메시지 규칙과 코드 배치 설명을 나눕니다.

| 알고 싶은 것 | 시작할 곳 |
| --- | --- |
| 메시지를 어떻게 만들고 전달하고 읽나? | [메시지 프로토콜 스터디 노트](message-protocol/README.md) |
| 새 shared_data에서 BLE Mesh와 RGB·부저·음성까지 어떻게 이어지나? | [shared_data 통합 설계 — 구현 전 제안](shared-data/README.md) |
| 어느 코드가 무엇을 담당하나? | [코드 구조](code-structure/README.md) |
| 프로그램 안에서 상태를 어떻게 공유하나? | [SharedState](shared-state.md) |
| 낯선 말은 무슨 뜻인가? | [용어](glossary.md) |

```text
architecture/
├── README.md
├── message-protocol/    # 메시지 규칙과 단계별 스터디 노트
├── code-structure/      # 코드 배치·역할·의존성
├── shared-data/         # 새 공유 상태·Mesh·출력 통합 설계 (구현 전)
├── shared-state.md
└── glossary.md
```

메시지 공부는 [message-protocol/README.md](message-protocol/README.md)에서 이어갑니다. 기존 구성 요소 안내와 전체 연결 그림은 [code-structure/README.md](code-structure/README.md)로 모았습니다.

빌드·배선은 [시작 안내](../getting-started/README.md), 실제 시험 결과는 [검증 안내](../verification/index.md), 이전 설계 설명은 [기존 프로젝트 개요](../archive/project/OVERVIEW.md)를 참고하세요.
