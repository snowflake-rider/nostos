# 실물 GATT 캡처

목표는 센서가 표준 CSC를 쓰는지 확인하고, 구현에 필요한 원시 notification을 확보하는 것이다. 이 단계에서는 NOSTOS 보드를 Flash하거나 Mesh 설정을 바꾸지 않는다.

## 준비

- XOSS 센서와 충분한 CR2032 배터리
- 센서를 안전하게 회전시킬 수 있는 자전거 스탠드
- iPhone 또는 Android의 `nRF Connect for Mobile`
- XOSS 앱, 자전거 컴퓨터 등 센서에 연결될 수 있는 다른 앱은 종료

센서를 휠 허브에 단단히 고정하고 손·옷·케이블이 휠에 닿지 않게 한다. 센서는 움직임이 없으면 절전 상태가 될 수 있으므로 검색 직전에 휠을 돌린다.

## 1. 광고 확인

1. nRF Connect에서 `Scan`을 시작한다.
2. 휠을 5~10초 돌려 센서를 깨운다.
3. `XOSS`, `SPD`, `CSC`, `VORTEX`처럼 보이는 이름을 찾는다.
4. 후보를 하나만 남길 수 있도록 센서를 멈췄다가 다시 돌리며 RSSI와 광고 재출현을 비교한다.
5. 아래 항목을 캡처한다.

   - 광고 이름
   - RSSI
   - Advertised Service UUID 목록
   - Manufacturer Data 원문
   - Bluetooth 주소 형식(public/random). 공유 시 주소 앞부분은 마스킹해도 된다.

이름만으로 연결 대상을 고정하지 않는다. 같은 이름의 센서가 주변에 있을 수 있으므로 이후에는 사용자가 확인한 주소와 Service UUID를 함께 사용한다.

## 2. 서비스 확인

1. 후보 장치에서 `Connect`를 누른다.
2. 서비스 검색이 끝나면 전체 Services 화면을 캡처한다.
3. 아래 UUID를 찾는다.

| UUID | 이름 | 기대 동작 |
| --- | --- | --- |
| `0x1816` | Cycling Speed and Cadence Service | 표준 CSC 서비스 |
| `0x2A5B` | CSC Measurement | `Notify` |
| `0x2A5C` | CSC Feature | `Read` |
| `0x2902` | Client Characteristic Configuration | notification 활성화 |
| `0x180F` / `0x2A19` | Battery Service / Battery Level | 선택 사항 |

`0x1816`이 없다면 모든 16-bit/128-bit Service UUID와 Characteristic 속성(Read/Write/Notify/Indicate)을 기록한다. 이 경우 임의 UUID에 값을 쓰지 않는다.

## 3. notification 캡처

1. `0x2A5B` 오른쪽의 notification 활성화 버튼을 누른다.
2. 첫 notification은 기준값으로 저장한다.
3. 휠을 일정하게 10초 정도 돌린다.
4. 연속 notification 3개 이상을 원시 hex와 수신 시각을 포함해 저장한다.
5. 휠을 멈추고 5초 이상 기다린 뒤 추가 notification이 오는지 기록한다.
6. 연결을 끊고 센서를 다시 깨워 재연결 가능 여부를 확인한다.

필요한 최소 자료:

```text
advertised_name:
advertised_services:
connected_services:
csc_feature_0x2A5C:
notification_1:
notification_2:
notification_3:
stop_behavior:
reconnect_behavior:
```

기록은 [장치 관찰 JSON](samples/device-observation.example.json)과 [notification CSV](samples/csc-notifications.example.csv)를 복사해 채운다.

## 판정

- `0x1816`과 notify 가능한 `0x2A5B`가 있음: 표준 CSC 경로로 구현한다.
- `0x1816`은 있으나 wheel revolution flag가 없음: 현재 모드가 cadence일 수 있으므로 XOSS 앱/제품 설명서에서 speed 모드로 바꾼 뒤 재확인한다.
- 제조사 전용 Service만 있음: read-only 관찰과 notification 캡처부터 하고, 쓰기 명령은 공식 문서 또는 검증된 캡처가 있을 때만 사용한다.
- 연결 자체가 안 됨: 다른 앱 연결, 절전, 배터리, 동시 연결 제한을 먼저 확인한다.

