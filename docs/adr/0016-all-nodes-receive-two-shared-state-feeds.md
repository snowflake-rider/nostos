---
status: accepted
---

# 모든 노드는 두 Shared State Feed를 받는다

선택형 sensor subscription과 그 영속 설정을 제거한다. 모든 Rider Node는 `RIDE_STATE`와 `ENVIRONMENT_STATE`를 항상 수신·보관·표시한다. RIDE_STATE는 속도와 Trip Distance를 함께 전달하고 ENVIRONMENT_STATE는 온도와 습도를 함께 전달한다. 각 feed는 active publisher 하나의 최신값과 발신 Node ID만 보관한다. 낙상은 State Topic이 아니라 로컬 판정 후 생성하는 `STOP_REQUEST(reason=FALL)`이다.
