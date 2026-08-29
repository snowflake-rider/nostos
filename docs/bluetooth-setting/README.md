# Bluetooth Mesh 빠른 설정

[문서 목록](../README.md)

모든 ESP32 노드에 **같은 설정을 반복 적용**한다. 노드마다 달라지는 것은 앱에서 보이는 이름과
자동으로 배정되는 **Unicast Address**뿐이다. 각 노드에 별도의 네트워크·AppKey·그룹을 만들지 않는다.

## 공통으로 사용할 값

| 항목 | 모든 노드에 넣을 값 |
| --- | --- |
| Network Key | `Primary Network Key` — index `0` |
| Application Key | `Events` — index `1`, 모든 노드에서 같은 항목 선택 |
| Element / Model | 첫 Element의 Vendor Model `Company 0x02E5 / Model 0x0001` |
| Publication | `0xC001`, 같은 AppKey, TTL `7`, Period `0`, Retransmit `0` |
| Subscription | `0xC001` |
| GATT Proxy / Relay | Proxy `Enabled`, 첫 근거리 시험은 Relay `Disabled` |

## 1. 네트워크에서 한 번만 준비

이미 만들어져 있으면 이 단계는 건너뛴다.

1. nRF Mesh 앱에서 **기존 Nostos 네트워크**를 연다.
2. Application Key `Events`를 하나만 만든다.
3. Group `Nostos Events`를 주소 **`0xC001`**로 하나만 만든다.

이후 모든 노드에서 위 AppKey와 Group을 그대로 선택한다. 같은 이름의 새 키를 노드마다 만들면 안 된다.

## 2. 각 노드에서 똑같이 반복

헷갈리지 않도록 설정할 노드 한 대만 가까이 켜고 다음 순서를 반복한다.

1. `Add Node` → 대상 ESP32 선택 → 기존 NetKey → `No OOB`로 Provision한다.
2. 대상 노드의 `Application Keys`에서 `Events`를 **Add**한다.
3. 첫 Element의 Vendor Model `0x02E5 / 0x0001`을 연다.
4. `Bound Application Keys`에 `Events`를 **Bind**한다.
5. Publication을 `C001 / Events / TTL 7 / Period 0 / Retransmit 0`으로 저장한다.
6. Subscription에 `C001`을 추가한다.
7. 앱이 자동 배정한 Unicast Address만 기록하고 다음 노드로 넘어간다.

```text
노드 1: Provision → AppKey Add → Vendor Bind → Pub C001 → Sub C001
노드 2: Provision → AppKey Add → Vendor Bind → Pub C001 → Sub C001
노드 3: Provision → AppKey Add → Vendor Bind → Pub C001 → Sub C001
```

## 3. 완료 확인

각 ESP32에 `status`를 보내 아래 항목만 확인한다.

```text
event_ready=1 pub=0xc001 sub_C001=1 ttl=7 period=0 retransmit=0
```

- `event_ready=0`: AppKey Add → Vendor Bind → Publication 순서로 다시 확인한다.
- `sub_C001=0`: Vendor Model의 Subscription에 `C001`을 추가한다.
- 위 값이 맞아도 실제 수신 확인은 별도다. 한 노드에서 버튼을 한 번 누르고 다른 노드의 `MESH_RX`를 확인한다.

## 하지 말 것

- 정상 노드나 기존 Nostos 네트워크를 먼저 삭제하지 않는다.
- 노드마다 새 NetKey, AppKey, `C001` 그룹을 만들지 않는다.
- 모든 노드에 같은 Unicast Address를 직접 지정하지 않는다. 주소는 서로 달라야 한다.
- 실제 NetKey·AppKey 값이나 Mesh Export JSON을 Git에 저장하지 않는다.

문제가 있을 때만 [상세 설정·복구 문서](nrf-mesh-setup.md)를 보고,
B6의 과거 키 불일치는 [B6 진단 기록](B6_SETUP.md)을 참고한다.
검증 당시 주소·배선은 [하드웨어·설정 기록](01-hardware-and-settings.md)에 있다.
현재 nRF Mesh Export의 키 제외 요약은 [현재 설정 스냅샷](current-settings.md)에 있다.
