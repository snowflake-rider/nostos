---
status: accepted
---

# Wheel distance는 현재 boot의 Trip Distance다

RIDE_STATE의 `wheel_distance`는 영구 odometer가 아니라 현재 boot에서 XOSS 휠 회전으로 누적한 Trip Distance다. 값을 flash에 저장하거나 재부팅 후 복원하지 않고 0에서 다시 시작한다. 센서값을 아직 받지 못한 재부팅 직후 상태는 0 km가 아니라 `unknown`으로 표시하고, 유효한 첫 RIDE_STATE 이후부터 누적값을 표시한다.
