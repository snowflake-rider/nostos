---
status: accepted
---

# State Update 하나에는 Topic 하나만 넣는다

각 STATE_UPDATE는 한 Rider Node의 State Topic 하나와 그 최신 payload만 전달한다. 모든 노드가 두 고정 Topic을 받더라도 각 Topic의 message와 cache는 독립적으로 유지하며 여러 Topic을 하나의 bundle이나 snapshot으로 묶지 않는다. 이 구조는 한 센서만 바뀌었을 때 다른 센서까지 재전송하지 않게 한다.
