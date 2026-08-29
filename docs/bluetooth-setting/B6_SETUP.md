> 이관 원문: `layers/layer-8/B6_SETUP.md`. 현재 실행 경로는 [팀원 시작 안내](../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# B6 진단 결과와 nRF Mesh 설정

2026-08-28, 실제 두 ESP32의 현재 NVS 백업을 비교한 결과다.
키 원문이나 키 해시는 이 문서에 포함하지 않는다.

## 확인된 원인

| 검사 | EC76 (`0x0003`) | BAB6 (`0x0004`) |
| --- | --- | --- |
| Mesh NetKey 인덱스 | `0` | `0` |
| 실제 NetKey | 두 보드 동일 | 두 보드 동일 |
| Mesh AppKey 1의 연결 | NetKey `0` | NetKey `0` |
| 실제 활성 AppKey 1 | **두 보드 서로 다름** | **두 보드 서로 다름** |
| Layer8 보조 연결 기록 | `AppKey 1 → NetKey 0` | 모든 슬롯 비어 있음 |

NVS 페이지·엔트리·블롭 CRC를 검사했다. 활성 AppKey 16바이트만 비교했으며,
키 갱신용 미사용 슬롯 차이를 키 불일치로 판단한 것이 아니다. 두 보드의 AppKey 0은 동일했다.

B6의 `event_ready=0`은 비어 있는 Layer8 보조 기록으로 설명된다.
하지만 그 기록만 복구해도 AppKey 1이 서로 다르므로 이벤트 통신 문제는 남는다.
보조 기록이 언제 비었는지, AppKey 1이 언제 달라졌는지는 이 백업만으로 확정하지 못한다.

11:31:57 KST의 후속 `status`에서는 76이 `event_ready=1`을 유지했고,
B6는 `event_ready=0`, `pub=0x0000`, `app=0x0000`, `ttl=0`, `sub_C001=1`이었다.
백업 시점과 달리 B6 Publication이 비어 있으므로, 아래 절차에서 Publication도 저장해야 한다.
조회 도중 76의 USB 포트가 `1101`에서 `1201`로 바뀌어 MAC을 재확인한 후 재조회했다.

## 기존 키를 삭제하지 않고 다시 맞추는 절차

**아래는 실행 완료 기록이 아니라 사용자가 앱에서 수행할 설정 안내다.**
새 키를 쓸 때는 B6뿐 아니라 76도 같은 키로 맞춰야 한다.

1. nRF Mesh의 현재 네트워크 **Application Keys**에서 새 키를 **한 번만** 만든다.
   - 이름 예: `Events-L8-v2`.
   - Bound Network Key: 기존 `Primary Network Key` (NetKey index `0`).
   - 인덱스: 앱이 배정하는 미사용 인덱스. 예를 들어 `0x0002`라면 두 노드 모두 그 값을 쓴다.
   - 같은 이름의 키를 노드마다 새로 만들지 않는다. 키 원문을 채팅에 붙여넣지 않는다.
2. 각 노드의 Application Keys에서 **위에서 만든 동일한 키 항목**을 Add한다.
3. 각 노드의 첫 번째 Element → Vendor Model (`Company 0x02E5`, `Model 0x0001`)로 들어간다.
4. Bound Application Keys에 그 새 키를 바인딩한다.
5. Publication의 Application Key도 그 새 키로 선택하고 아래 표대로 저장한다.
6. `nostos events` 구독을 확인한다. 기존 키, 기존 노드, 전체 네트워크는 삭제하지 않는다.

현재 펌웨어의 키 수용량은 3개이며, 백업 시점에는 각 노드에 AppKey 0과 1 두 개가 있었다.
추가 중 오류가 나면 더 생성하거나 삭제하지 말고 해당 오류를 확인한다.

## B6 설정표

| 항목 | 값 |
| --- | --- |
| Node | `ESP32-L8-BAB6`, 현재 주소 `0x0004` |
| Element | 첫 번째 Element |
| Vendor Company ID / Model ID | `0x02E5` / `0x0001` |
| Bound Application Keys | 위에서 한 번 만든 공용 `Events-L8-v2` 추가 |
| Publication → Application Key | 같은 `Events-L8-v2` |
| Publication → Address | `nostos events`, 실제 주소 `0xC001` |
| Publication → TTL | `7` |
| Publication → Period | Disabled / `0` |
| Publication → Retransmit Count | `0` |
| Publication → Retransmit Interval | 편집 가능하면 최소 `50 ms` (raw retransmit `0`) |
| Subscriptions | `nostos events` (`0xC001`) 포함 |
| All Nodes | 이번 `event_ready=0`의 원인이 아님. 변경 불필요 |

76에도 같은 키 바인딩과 Publication 설정을 적용한다. 노드 주소는 서로 다르게 유지한다.
Vendor Model ID `0x0001`과 AppKey 인덱스는 다른 개념이다.

## 설정 후 확인

새 키의 인덱스가 `0x0002`인 경우 목표 출력은 다음과 같다. 실제 성공 로그가 아니다.

```text
event_ready=1 net=0x0000 app=0x0002 pub=0xc001 sub_C001=1 ttl=7 period=0 retransmit=0
```

`AppKey Add` 알림을 정상 수신하면 기존 Layer8 코드가 새 인덱스 연결을 기록한다.
그 뒤에도 `net=0xffff`라면 반복해서 키를 지우지 말고 펌웨어의 알림·기록 처리를 점검한다.
양쪽 `event_ready=1`만으로 무선 전송 성공은 아니다. STM32 버튼을 한 번 누른 뒤
송신 보드 `UART_RX`/`MESH_TX`와 상대 보드 `MESH_RX`/`UART_TX`를 확인해야 한다.

이번 진단은 현재 NVS를 읽었으며 펌웨어 재설치, 키 변경, NVS 전체 삭제는 수행하지 않았다.
백업 파일은 `build/key-diagnostic-nvs.fPltjc/` 아래 소유자 전용 권한으로 보관한다.
이 백업에는 비밀 키가 있으므로 공유하지 않는다.

참고: [Nordic nRF Mesh 앱의 네트워크·키 관리](https://github.com/nordicsemi/IOS-nRF-Mesh-Library).
