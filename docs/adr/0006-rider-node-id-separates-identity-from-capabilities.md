---
status: accepted
---

# Rider Node ID로 정체성과 capability를 분리한다

한 rider group의 노드는 provisioning 과정에서 정한 `1..10` Rider Node ID로 식별하며 `0`은 미할당으로 둔다. Rider Node ID는 라이더 개인 번호가 아니며 주행 위치, 센서·출력 capability와 독립적이다. 라이더들이 앞뒤 순서를 바꿔도 ID는 바꾸지 않고, protocol은 현재 주행 위치를 저장하지 않는다. 현재 세 장비는 Rider Node 1·2·3으로 배치하지만 capability는 이후 자유롭게 조합하거나 이동할 수 있다. Firmware는 provisioned Mesh primary address를 설정된 address map에 대입해 Rider Node ID를 얻고, protocol과 dashboard는 위치 역할 대신 이 ID를 사용한다.
