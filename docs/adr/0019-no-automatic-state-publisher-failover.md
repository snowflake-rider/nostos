---
status: accepted
---

# State publisher는 자동 failover하지 않는다

Active Topic Publisher의 State Update가 중단되어도 다른 Rider Node를 자동 publisher로 선출하거나 cached value를 대신 publish하지 않는다. 각 dashboard는 마지막 수락한 update 이후 경과 시간에 따라 feed를 `FRESH`, `STALE`, `UNKNOWN`으로 전환한다. STALE은 마지막 값을 명확한 경고와 함께 유지하고 UNKNOWN은 숫자값을 표시하지 않는다. 센서를 다른 장비로 옮기면 해당 장비 profile을 명시적으로 변경한다.
