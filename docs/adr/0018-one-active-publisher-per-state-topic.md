---
status: accepted
---

# State Topic마다 active publisher는 하나다

한 rider group에서 `RIDE_STATE` active publisher 하나와 `ENVIRONMENT_STATE` active publisher 하나만 둔다. 수신 노드는 Topic ID별 최신값 하나와 publisher의 Rider Node ID만 보관·표시한다. 다른 노드는 받은 sensor state를 새 application message로 다시 publish하지 않는다. Bluetooth Mesh network relay는 원래 source를 유지하는 transport 동작이므로 허용한다. 각 topic은 고정 binary payload와 Schema Revision을 유지하며, 새 topic은 새 Topic ID와 `schema_rev=1`로 추가한다.
