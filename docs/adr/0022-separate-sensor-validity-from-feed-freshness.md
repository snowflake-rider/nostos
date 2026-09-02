---
status: accepted
---

# Sensor Validity와 Feed Freshness를 분리한다

RIDE_STATE와 ENVIRONMENT_STATE payload는 공통으로 `sensor_valid:u8`를 가진다. `1`은 측정값 사용 가능, `0`은 센서 미측정 또는 오류를 뜻한다. sensor_valid가 0인 STATE_UPDATE도 publisher 통신이 살아 있음을 나타내므로 Feed Freshness 시간을 갱신하지만 dashboard는 숫자값을 표시하지 않는다. sensor_valid가 0이면 해당 숫자 payload 필드는 모두 0이어야 한다. 상세 오류 code는 추가하지 않는다.
