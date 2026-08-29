# 현재 Bluetooth Mesh 설정

[빠른 설정](README.md)

`Nostos.json` Export에서 키 값을 제외하고 정리한 현재 상태다.

| 항목 | 값 |
| --- | --- |
| 네트워크 이름 | `Nostos` |
| Export 형식 | `1.0.1` |
| Export 시각 | `2026-08-28T14:58:39Z` (`2026-08-28 23:58:39 KST`) |
| 원본 로컬 경로 | `docs/bluetooth-setting/private-network/Nostos.json` |
| SHA-256 | `305127a97981d1e48fc659aca7cf67efe5ae1ed2a708238bc18f361f6b64ded3` |

## 공용 키와 그룹

키의 실제 값은 기록하지 않는다.

| 종류 | 이름 | Index / Address | 용도 |
| --- | --- | --- | --- |
| NetKey | `Primary Network Key` | `0` | 모든 노드가 공유하는 네트워크 |
| AppKey | `COMMON-ONOFF` | `0` | `C000` OnOff 시험 |
| AppKey | `Events` | `1` | `C001` 이벤트 송수신 |
| Group | `Nostos` | `0xC000` | 선택적인 OnOff 시험 |
| Group | `Nostos Events` | `0xC001` | 실제 Nostos 이벤트 |

## 등록된 노드

| 역할 / 이름 | Unicast Address | Element 수 |
| --- | --- | --- |
| iPhone Provisioner | `0x0001` | 2 |
| ESP32 `76` | `0x0003` | 1 |
| ESP32 `D6` | `0x0005` | 1 |
| ESP32 `B6` | `0x0006` | 1 |

ESP32 세 대는 모두 `Primary Network Key`와 `Events` AppKey를 사용하고,
Vendor Model의 Publication과 Subscription을 `0xC001`로 맞춘다.

## ESP32 Vendor Model 일치 상태

| 노드 | 노드에 저장된 AppKey index | Vendor Bind | Publication | Subscription | TTL / Period / Retransmit |
| --- | --- | --- | --- | --- | --- |
| `76` | `0`, `1` | `1` | `0xC001`, AppKey `1` | `0xC001` | `7` / `0` / `0` |
| `D6` | `0`, `1`, `2` | `1` | `0xC001`, AppKey `1` | `0xC001` | `7` / `0` / `0` |
| `B6` | `0`, `1` | `1` | `0xC001`, AppKey `1` | `0xC001` | `7` / `0` / `0` |

이벤트 Vendor Model 설정은 세 노드가 같다. D6에만 AppKey index `2`가 추가로 남아 있지만
현재 Export의 공용 AppKey 목록에는 index `2`가 없고 Vendor Model도 이를 사용하지 않는다.
실물 상태를 다시 확인하기 전에는 이 항목을 임의로 삭제하지 않는다.

## 원본 보안

- 원본 JSON에는 NetKey, AppKey, Device Key가 포함되어 있으므로 Git에서 제외된다.
- 원본 파일 권한은 소유자만 읽고 쓸 수 있는 `600`이다.
- 새 Export로 교체하면 JSON 유효성과 SHA-256을 다시 확인하고 이 요약의 날짜·주소를 갱신한다.
- 원본 키나 JSON 전체를 Linear 댓글, 로그 또는 공유 문서에 붙여 넣지 않는다.
