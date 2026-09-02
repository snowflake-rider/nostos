---
status: accepted
---

# 별도 Heartbeat 대신 State Update를 반복한다

Application message 종류에 HEARTBEAT를 추가하지 않는다. 각 Active Topic Publisher는 센서값이 변하지 않아도 최신 STATE_UPDATE를 주기적으로 group broadcast한다. 수신 노드는 별도 liveness state 없이 Topic별 마지막 수락한 STATE_UPDATE 도착 시각만으로 `FRESH`, `STALE`, `UNKNOWN`을 판단한다. 정지 중인 RIDE_STATE도 speed 0과 현재 Trip Distance를 반복하고 ENVIRONMENT_STATE도 최신값을 반복한다.
