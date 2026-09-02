---
status: accepted
---

# 낙상 감지는 Stop Request를 생성한다

물리 Button 3과 로컬 낙상 감지는 모두 동일한 Stop Request를 생성하며 Stop Reason만 BUTTON 또는 FALL로 구분한다. Nostos protocol은 별도의 FALL·FALL_CLEAR message나 원격 incident lifecycle을 공유하지 않는다. Rider Node 3은 정상에서 낙상으로 전이할 때 요청을 한 번 생성하고 낙상이 지속되는 동안 반복하지 않는다.
