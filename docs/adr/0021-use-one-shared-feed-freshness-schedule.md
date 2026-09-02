---
status: accepted
---

# 두 Shared State Feed는 같은 freshness 시간을 사용한다

RIDE_STATE와 ENVIRONMENT_STATE Active Topic Publisher는 값이 변하지 않아도 2초마다 STATE_UPDATE를 group broadcast한다. 수신 노드는 마지막 수락한 update로부터 6초를 초과하면 feed를 STALE로, 20초를 초과하면 UNKNOWN으로 바꾼다. STALE은 마지막 값을 경고와 함께 유지하고 UNKNOWN은 숫자값을 지운다. Topic별 timer 상수를 따로 두지 않는다.
