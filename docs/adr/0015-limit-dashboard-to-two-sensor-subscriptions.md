---
status: superseded by ADR-0016
---

# Dashboard sensor subscription은 최대 두 개로 제한한다

작은 dashboard에 맞춰 각 Rider Node는 State Topic을 `0..2`개만 구독·보관·표시한다. 현재 Topic catalog는 속도와 휠 주행거리를 함께 전달하는 `RIDE_STATE` 한 개, 온도와 습도를 함께 전달하는 `ENVIRONMENT_STATE` 한 개다. 각 Topic은 구독 한 칸을 사용한다. 낙상은 State Topic으로 구독하지 않고 로컬 판정 후 `STOP_REQUEST(reason=FALL)`만 생성한다. Stop Request와 Pace Request는 sensor subscription 수에 포함하지 않는다.
