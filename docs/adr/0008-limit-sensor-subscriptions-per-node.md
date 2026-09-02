---
status: superseded by ADR-0015
---

# Rider Node의 sensor subscription은 최대 3개로 제한한다

각 Rider Node는 Mesh 안에서 State Topic을 `0..3`개 구독한다. 구독한 topic만 최신값을 보관하고 dashboard에 표시한다. Stop Request와 Pace Request는 sensor subscription이 아닌 group control message이므로 이 제한에 포함하지 않는다. Topic catalog에는 향후 새 센서를 추가할 수 있지만 개인 심박수 같은 생체 데이터는 Nostos의 수집·공유 범위에 넣지 않는다.
