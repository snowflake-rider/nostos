# nRF Mesh로 Layer 8 네트워크 설정·복구하기

이 문서는 **ESP32-S3의 Layer 8 펌웨어**를 iPhone의 **nRF Mesh** 앱으로 등록하고,
STM32 버튼·센서 이벤트를 다른 ESP32에 전달하도록 설정하는 절차다.
Android도 설정값은 같지만 메뉴 위치와 이름이 다를 수 있다. **nRF Connect가 아니라 nRF Mesh**를 사용한다.

작성 기준: 2026-08-28의 이 저장소 코드·설치 로그와 Nordic 공식 앱 자료.
앱 메뉴는 공식 iOS 소스에서 확인했다. 초안 작성 뒤 사용자가 이 순서대로 B6를 설정했고,
USB 로그로 새 주소 `0x0006`, 준비 상태, D6발 이벤트 수신을 확인했다. 앱 화면을 직접 조작한 것은 아니다.

**최근 진행:** B6 재등록·C001 수신 확인 완료. 이미 복구된 B6를 이 문서를 보고 다시 지우지 않는다.
첫 시험 누락과 확인 범위는 [재등록 결과](../testing/results/b6-rejoin-8vnbdmzz/RESULT.md)를 따른다.

> 지금 B6만 복구한다면: **기존 네트워크 유지 → 이전 B6 항목 정리 → B6 재등록 → 기존 공용 AppKey 추가·Bind → C001 Publication·Subscription → status 확인** 순서다.
> D6·76이 쓰는 네트워크를 지우거나 새 네트워크를 만들 필요가 없다.

## 1. 어디부터 시작할까?

| 현재 상황 | 시작할 곳 | 하지 말아야 할 것 |
| --- | --- | --- |
| B6처럼 한 보드만 전체 삭제·새 설치함 | 10절의 B6 복구 → 4~8절 | 정상인 다른 보드와 앱 네트워크까지 초기화 |
| 처음으로 모든 보드를 새 네트워크에 등록함 | 2~8절 전체 | 노드마다 별도 네트워크·별도 AppKey 생성 |
| 등록은 됐는데 이벤트만 안 옴 | 9절 증상별 점검 | 원인 확인 전 키·NVS 삭제 |
| 휴대폰 앱 데이터가 사라짐 | 11절 백업 Import | 같은 이름으로 만든 새 네트워크를 기존 네트워크라고 생각하기 |

### 전원과 USB

각 ESP32에는 전원이 필요하다. **Mesh 통신 자체에 모든 보드의 Mac USB 연결이 필요한 것은 아니다.**
펌웨어 설치·로그 확인에는 데이터 USB를 쓰고, 설치 후에는 적절한 독립 전원으로도 동작한다.
이 가이드는 이미 Layer 8이 설치된 보드를 대상으로 한다. 앱은 펌웨어를 Layer 8로 바꿔 주지 않는다.

## 2. 먼저 기억할 개념과 설정값

### 설정은 네 단계로 나뉜다

| 단계 | 의미 | 앱에서 확인할 내용 |
| --- | --- | --- |
| Provision | 보드를 네트워크 구성원으로 등록 | NetKey, 노드별 Unicast Address |
| Node AppKey Add | 그 노드에 이벤트용 공용 키 전달 | 노드의 Application Keys |
| Model AppKey Bind | 노드 안의 특정 Model이 그 키를 사용하도록 연결 | Vendor Model의 Bound Application Keys |
| Publication / Subscription | 보낼 그룹 / 받을 그룹 지정 | 같은 Vendor Model의 C001 설정 |

노드 목록에 나타나는 것만으로 이벤트 설정이 끝난 것은 아니다.
**AppKey를 네트워크에 만들기, 노드에 Add하기, Model에 Bind하기는 서로 다른 작업**이다.
Configuration Server는 노드별 Device Key(DevKey)로 관리하므로 이벤트 AppKey를 Bind하는 대상이 아니다.

### Layer 8 필수 설정표 — 모든 참여 노드에 동일하게 적용

| 항목 | 설정값 |
| --- | --- |
| 펌웨어 | `esp32s3_layer_8` — 세 보드 모두 같은 프로젝트 |
| Element | 첫 번째이자 유일한 Element, 코드에서는 `elements[0]` |
| 이벤트 Model | **Vendor Model**, Company ID `0x02E5`, Model ID `0x0001` |
| Network Key | 참여 노드가 공유하는 기존 NetKey |
| Application Key | 네트워크에서 한 번 만든 **동일한 AppKey 항목** |
| Model Bind | 위 AppKey를 Vendor Model에 Bind |
| Publication Address | **`0xC001`** — 이름 예: `nostos events` |
| Publication AppKey | Bind한 것과 같은 AppKey |
| Publication TTL | **숫자 `7` 직접 지정**, `Default`로 두지 않기 |
| Publication Period | Disabled / `0` |
| Publication Retransmit Count | Disabled / `0` |
| Publication Retransmit Interval | `50 ms` / steps `0` — 아래 주의사항 참조 |
| Friendship Credentials | OFF / 기본 Network Credentials |
| Subscription | **`0xC001` 포함** |
| GATT Proxy | Enabled — 앱 연결·설정용 |
| Relay | 첫 근거리 시험에서는 Disabled; 중계 시험 때 별도로 설정 |

**`C001`은 이벤트, `C000`은 선택 사항인 Generic OnOff 시험용**이다.
기존 [Layer 7 매뉴얼](../manual/how-to-make-mesh-network.md)의 C000 설정만으로 버튼·낙상 이벤트가 준비되지는 않는다.
Vendor Company ID `0x02E5`는 현재 학습용 구현의 값이며 제품용 할당 ID 안내는 아니다.

설정값 근거: [mesh_node.h](../layers/layer-8/main/mesh_node.h),
[mesh_node.c의 composition·refresh_snapshot](../layers/layer-8/main/mesh_node.c).

### 노드 식별표

아래는 2026-08-28 설치·점검 기록이다. 주소와 설정은 재등록 후 반드시 다시 기록한다.

| 물리 라벨 | Layer 8 BLE 이름 | USB serial | 당시 Mesh 상태 |
| --- | --- | --- | --- |
| 76 | `ESP32-L8-EC76` | `14:C1:9F:CE:EC:74` | 기존 주소 `0x0003` |
| D6 | `ESP32-L8-F0D6` | `14:C1:9F:CE:F0:D4` | 기존 주소 `0x0005` |
| B6 | `ESP32-L8-BAB6` | `44:1B:F6:FF:BA:B4` | 전체 삭제 뒤 재등록 완료, 새 주소 **`0x0006`** |

B6의 옛 주소 `0x0004`는 현재 주소가 아니다. 재등록 때 앱이 배정하는 **다른 노드와 겹치지 않는 새 주소**를 쓴다.
USB serial의 끝자리와 BLE 이름의 끝자리가 다른 것은 위 표대로 구분한다.
`/dev/cu.usbmodem1201` 같은 포트 번호는 USB 재연결 시 바뀌므로 고정 식별자로 쓰지 않는다.
관련 설치 기록: [Layer 8 검증](../layers/layer-8/VERIFICATION.md).

## 3. 앱에서 네트워크·키·그룹 준비

1. 휴대폰 Bluetooth를 켜고 nRF Mesh의 Bluetooth 권한을 허용한다.
2. **기존 보드와 연결할 때는 기존 네트워크를 그대로 연다.** `Settings`의 Network Keys와 Application Keys를 확인한다.
3. 정말 처음부터 새 네트워크를 만드는 경우에만 앱의 초기 생성 화면에서 이름을 정하고 NetKey를 생성한다.
4. 이벤트용 AppKey를 결정한다. 기존 네트워크 복구라면 정상 노드의 **Vendor Publication에서 사용하는 AppKey 항목**을 선택한다.
5. 완전한 신규 구성이라면 `Settings → Application Keys → +`에서 공용 AppKey를 **한 번만** 만든다. 이름 예: `Nostos Events`. Bound Network Key는 사용할 NetKey로 지정한다.
6. `Groups → +`에서 이벤트 그룹을 만든다. 이름은 `nostos events`, 주소는 **`C001`(16진수)**로 지정한다. 이미 있으면 재사용한다.

2026-08-28 D6·76의 직전 기록은 NetKey index `0x0000`, 이벤트 AppKey index `0x0001`이었다.
현재 앱과 정상 노드에서도 같은 항목인지 확인해서 B6에 적용한다. **인덱스 `1`을 가진 새 키를 만드는 것이 아니다.**
새 네트워크의 인덱스가 `0` 또는 다른 값이어도 코드가 특정 AppKey 인덱스를 강제하지는 않는다.

키의 이름·인덱스가 같아도 다른 네트워크에서 새로 만들면 실제 키가 다를 수 있다.
키 값을 문서에 복사하지 말고 **같은 네트워크 안의 동일한 키 항목을 모든 노드에서 선택**한다.
그룹 이름도 표시용이다. 자동으로 `C000`이 배정됐다면 이벤트 그룹 주소를 `C001`로 맞춘다.

앱의 키·그룹 관리 기능: [Nordic 공식 nRF Mesh 안내](https://github.com/nordicsemi/IOS-nRF-Mesh-Library#nrf-mesh-sample-app).

## 4. 보드 한 대를 Provision

처음에는 대상 보드를 가까이에 두고 한 대씩 등록한다. 이름이 헷갈리면 대상만 켜서 확인한다.

1. `Network → + / Add Node`에서 미등록 장치를 검색한다.
2. 예를 들어 B6라면 **`ESP32-L8-BAB6`**를 선택한다.
3. Provisioning에 사용할 Network Key가 3절의 기존 NetKey인지 확인한다.
4. `Provision`을 누른다. 인증 방식 선택이 나타나면 현재 펌웨어는 OOB 입력·출력을 구현하지 않았으므로 `No OOB`를 사용한다. 통제된 실습 환경에서만 진행한다.
5. 완료 후 할당된 Unicast Address를 기록하고, 앱 이름을 `새 주소-ESP32-B6`처럼 알아보기 쉽게 지정한다.
6. 노드 설정에 다시 연결하여 Composition Data와 Element/Model 목록을 읽는다. 자동 조회가 실패하면 앱에서 다시 연결·조회한다.

기대 로그 예시이며, 아래 주소가 고정값은 아니다.

```text
PROVISIONED primary=0x0006; Vendor model configuration still required
```

첫 Element에는 Configuration Server, Generic OnOff Server, Generic OnOff Client,
**Vendor Model `Company 0x02E5 / Model 0x0001`**이 보여야 한다.
Vendor Model이 없으면 다음 설정을 진행하지 말고 9절의 펌웨어·Composition 항목을 확인한다.

앱 연결이 끊겼다면 같은 네트워크의 가까운 Proxy 노드를 통해 다시 연결한다.
iPhone은 GATT Proxy를 통해 Mesh 설정 메시지를 보낸다. Proxy 연결과 실제 이벤트 전달은 별도로 검증한다.
[Nordic의 iOS GATT Proxy 설명](https://github.com/nordicsemi/IOS-nRF-Mesh-Library#bluetooth-mesh-library-for-ios).

## 5. AppKey Add → Vendor Model Bind

### 5-1. 노드에 키 추가

1. `Network → 대상 노드 → Application Keys`를 연다.
2. `+ / Add`에서 3절의 **공용 AppKey**를 선택한다.
3. 성공 응답을 기다리고 노드의 키 목록에 해당 항목이 나타나는지 확인한다.

앱 버전에 따라 일부 설정이 자동으로 수행될 수 있다. 자동 완료 표시만 믿지 말고 키 목록을 확인한다.
현재 펌웨어는 노드당 AppKey 3개를 수용한다. 가득 찼다는 오류가 나면 키를 계속 생성하거나 임의로 삭제하지 않는다.

### 5-2. Model에 키 연결

1. 같은 노드에서 `첫 번째 Element → Vendor Model`을 연다.
2. **Company `0x02E5`, Model `0x0001`**인지 확인한다. Model ID와 AppKey index는 다른 번호다.
3. `Bound Application Keys → + / Bind Application Key`에서 같은 키를 선택한다.
4. 저장 후 해당 Model의 Bound Application Keys 목록에 나타나는지 확인한다.

Generic OnOff Server/Client에만 Bind하면 이벤트 설정이 되지 않는다.
AppKey Add·Model Bind·Publication·Subscribe 메뉴의 기본 흐름은
[Nordic 공식 설정 예제](https://devzone.nordicsemi.com/guides/short-range-guides/b/mesh-networks/posts/guide-to-the-thingy52-mesh-provisioning-demo)와 같지만,
이 프로젝트에서는 그 예제의 Thingy 모델·키 개수 대신 **위 Vendor Model과 공용 이벤트 키**를 사용한다.

## 6. 같은 Vendor Model의 Publication·Subscription 설정

### 6-1. Publication — 어디로 보낼까?

1. Vendor Model 화면의 `Publication / Set Publication`을 연다.
2. Publish Address/Destination에서 `nostos events` **`0xC001`**을 선택한다.
3. Application Key는 방금 Bind한 공용 이벤트 키를 선택한다.
4. **TTL=7, Period=Disabled(0), Retransmit Count=Disabled(0)**을 지정한다.
5. Retransmit Interval은 **50 ms**로 맞추고 Friendship Credentials는 OFF로 둔다.
6. `Done / Save / Set`으로 저장하고 성공 응답을 기다린다. 다시 열어 값이 유지되는지 확인한다.

**이 펌웨어에서 특히 주의할 두 가지:**

- TTL의 `Default`는 숫자 7과 저장값이 다르다. 네트워크 기본 TTL이 7이어도 Publication에는 **7을 직접 입력**한다.
- 코드가 Retransmit의 count와 interval을 합친 **raw 값 전체가 0인지** 검사한다.
  Count만 0으로 내리고 예전 interval이 남으면 `event_ready=0`이 될 수 있다.
  iOS에서 Count=Disabled 때문에 Interval이 `N/A`로 잠기면, 같은 편집 화면에서
  **Count를 잠시 1 → Interval 50 ms → Count 0으로 복귀 → 최종 저장**한다.
  중간 Count=1 상태에서는 저장하지 않는다. 저장 후 `status`의 **`retransmit=0`**을 확인한다.

이 interval 주의사항은 현재 펌웨어의 엄격한 준비 조건과
[공식 iOS Publication 편집 코드](https://github.com/nordicsemi/IOS-nRF-Mesh-Library/blob/fa0967b74e669cd925e6a5bc7b442031d69a796b/Example/Source/View%20Controllers/Network/Configuration/SetPublicationViewController.swift)를 대조한 안내다.
Node의 **Network Transmit / Relay Retransmit**와 Model의 **Publication Retransmit**는 다른 설정이므로 혼동하지 않는다.

### 6-2. Subscription — 어느 그룹을 받을까?

1. 같은 Vendor Model에서 `Subscriptions → + / Subscribe`를 연다.
2. `nostos events` **`0xC001`**을 선택하고 저장한다.
3. 해당 Model의 Subscription 목록에 `C001`이 있는지 확인한다.

앱의 Groups 목록에 그룹을 만든 것만으로는 노드의 Model이 그 그룹을 구독하지 않는다.
**각 노드의 Vendor Model에 Publication과 Subscription을 모두 설정**한다.

## 7. 나머지 노드도 확인하고 status로 검증

신규 구성이라면 모든 보드에 4~6절을 반복한다. B6만 복구 중이라면 정상 D6·76은
설정을 다시 만들지 말고 기존 값과 키가 맞는지만 확인한다.
첫 시험은 가까운 거리에서 GATT Proxy=Enabled, Relay=Disabled로 진행한다.
이는 직접 전달 시험이며, 다중 홉 Relay 시험은 별도로 해야 한다.

### 이 Mac에서 읽기 전용 확인

다른 시리얼 모니터를 먼저 닫는다. 다음 스크립트는 USB serial로 대상을 찾아
`status`만 보내며 DTR/RTS를 변경하지 않는다. 아래 Python 경로는 이 Mac 기준이다.

```bash
cd /Users/kafka/Workspace_AI/esp-ble
/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python layers/layer-8/tools/check_uart_diag.py --board B6
/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python layers/layer-8/tools/check_uart_diag.py --board D6
/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python layers/layer-8/tools/check_uart_diag.py --board 76
```

목표 출력 예시: 실제 주소와 키 인덱스는 앱에서 설정한 값을 따른다.

```text
STATUS name=ESP32-L8-BAB6 primary=0x0006 event_ready=1 net=0x0000 app=0x0001 pub=0xc001 sub_C001=1 ttl=7 period=0 retransmit=0 relay=0
```

| 확인 필드 | 합격 조건 |
| --- | --- |
| `primary` | 0이 아닌 Unicast Address, 다른 노드와 중복 없음 |
| `net`, `app` | 실제 공용 키 설정과 일치. `net=0xffff`이면 미준비 |
| `event_ready` | `1` — 이 펌웨어의 송신 준비 조건 |
| `pub`, `sub_C001` | `pub=0xc001` **그리고** `sub_C001=1` |
| `ttl`, `period`, `retransmit` | 각각 `7`, `0`, `0` |

**`event_ready` 검사에는 Subscription이 포함되지 않는다. `sub_C001=1`을 별도로 확인한다.**
또한 두 노드의 `net/app` 숫자가 같다고 실제 키 bytes까지 같다는 증거는 아니다.
스크립트의 `DIAG_PRESENT`는 진단 응답이 있다는 뜻이며 Mesh 설정·실제 수신 PASS가 아니다.

설정 완료 후 대상 보드를 한 대씩 재부팅하고, 같은 `status` 값이 유지되는지 다시 확인한다.
이는 앞으로 수행할 재현 절차이며 모든 보드의 재부팅 검증이 완료됐다는 기록은 아니다.

## 8. 실제 버튼 이벤트로 확인

시험 중 움직이는 장치가 없도록 하고, 정지 요청 버튼 한 번처럼 영향이 분명한 입력으로 확인한다.
현재 버튼 4의 이벤트 ID는 `0x13`이다. 낙상부터 발생시키지 말고 먼저 통신 경로를 확인한다.

### 로그를 계속 열어 두는 방법 — 이 Mac 기준

먼저 USB serial과 현재 포트의 대응을 출력한다.

```bash
/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python -m serial.tools.list_ports -v
```

2절의 USB serial과 일치하는 ESP32 포트를 찾은 뒤, 아래 `XXXX`를 그 포트 번호로 바꾼다.
터미널 창을 보드별로 따로 열고 서로 다른 포트를 지정한다. 모니터가 열린 동안 같은 포트에
7절의 진단 스크립트를 동시에 실행하지 않는다.

```bash
/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python -m esp_idf_monitor \
  --port /dev/cu.usbmodemXXXX --baud 115200 --target esp32s3 \
  --no-reset --eol LF --disable-address-decoding
```

열린 콘솔에서 `status`를 입력하고 Enter를 누른다. 종료는 `Ctrl+]`다.
`--no-reset`은 모니터 시작 시 자동 reset을 끈다. 이 명령은 flash를 쓰지 않는다.

1. 송신·수신 ESP32의 USB 로그를 각각 열고 `status`로 준비 상태를 확인한다.
2. 송신 STM32의 버튼을 한 번 누른다.
3. 송신 로그의 `UART_RX`, `MESH_TX`와 **상대 노드 각각의** `MESH_RX`, `UART_TX`를 확인한다.
4. 수신 STM32가 연결되어 있다면 그 MCU의 수신과 실제 출력까지 확인한다.

기대 로그 예시:

```text
송신 ESP32: UART_RX id=0x13 result=queued
송신 ESP32: MESH_TX id=0x13 source=... api=accepted
수신 ESP32: MESH_RX source=... id=0x13 result=queued
수신 ESP32: UART_TX id=0x13 source=... api=accepted
```

`MESH_TX api=accepted`나 `MESH_STACK complete_ok`는 상대 수신 확인이 아니다.
`UART_TX`도 상대 STM32가 실제 받았다는 증거는 아니다. 위 단계를 나눠서 판정한다.
`on/off` 명령과 앱의 OnOff 스위치는 C000 시험이므로 **C001 버튼 이벤트 시험을 대신하지 않는다.**

작성 시점의 STM32는 `BUTTON_OUTPUT_TEST=ON` 진단 모드라 **센서 자동 이벤트와 원격 메시지 출력이 꺼져 있다.**
따라서 Mesh 수신 로그가 정상이어도 원격 STM32의 RGB·버저가 안 나올 수 있다.
낙상 통합 전에는 센서가 연결된 보드의 펌웨어·배선과 수신 STM32 모드를 별도로 확인한다.
상태 기록: [testing/checklist.md](../testing/checklist.md), 배선: [PIN.md](PIN.md).

## 9. 문제가 생겼을 때: 지우기 전에 확인

| 증상 | 확인 순서 |
| --- | --- |
| Add Node 검색에 안 나옴 | 전원·Bluetooth 권한·거리 확인 → 이미 등록된 노드인지 확인. 등록된 노드는 미등록 장치 목록에서 찾지 않는다. `primary=0`인데 안 보이면 부팅 로그·대상 리셋 상태 확인 |
| 앱 목록에는 있지만 설정이 Timeout | 같은 기존 네트워크인가 → 가까운 Proxy에 연결됐나 → 대상에 전원이 있나. 오래된 앱 항목만 남아 있을 수도 있음 |
| Layer 8 Vendor Model이 없음 | 실제 실행 프로젝트가 `esp32s3_layer_8`인가 → Composition 재조회. Layer 7 캐시가 남아 있으면 해당 노드만 재등록하는 절차 검토 |
| `primary=0x0000` | 미등록 상태. Provision부터 진행 |
| `net=0xffff`, `NO_KEY_INDEX_MAP` | Node AppKey Add가 됐는지, 그 키의 Bound NetKey가 맞는지 확인. Layer 8은 AppKey Add 시 인덱스 연결 정보를 저장함 |
| `event_ready=0` | AppKey Add → Vendor Bind → Publication AppKey/C001 → 숫자 TTL 7 → Period 0 → raw Retransmit 0 순으로 확인 |
| `event_ready=1`, `sub_C001=0` | Vendor Model에 C001 Subscribe 추가. 송신 준비와 수신 그룹 설정은 별개 |
| 양쪽 준비 완료인데 상대 `MESH_RX` 없음 | 공용 AppKey 항목이 정말 같은가 → 수신 C001 구독 → 송신 이벤트 로그·거리 확인. 같은 이름·인덱스만 비교하지 않기 |
| `UART_RX`가 아예 없음 | STM32 TX → ESP32 GPIO18 RX, 공통 GND, 115200/8N1, STM32 이벤트 송신 확인. Mesh 설정을 지우지 않기 |
| `MESH_RX`는 있지만 STM32 출력 없음 | ESP32 GPIO17 TX → STM32 RX, 수신 펌웨어 모드와 실제 수신 확인. 현재 진단 모드의 원격 출력 제한 확인 |
| 앱의 Configured 표시만 켜져 있음 | Model 설정과 실제 status를 확인. 표시/스위치 자체는 통신 검증이 아님 |
| USB 재연결 후 예전 포트가 없어짐 | USB serial로 다시 찾기. B6/D6/76을 포트 번호로 추측하지 않기 |

`NO_KEY_INDEX_MAP`만 보고 즉시 NVS를 지우지 않는다. 현재 펌웨어에서 공용 키의 AppKey Add를
다시 수행할 수 있는지 먼저 확인한다. 앱이 이미 추가된 키라며 재전송을 제공하지 않으면 임의로
건강한 노드의 키를 삭제하지 말고, **문제 노드만 재등록**하거나 별도의 키 마이그레이션을 계획한다.

과거 [B6 키 불일치 진단](../layers/layer-8/B6_SETUP.md)은 이전 시점의 기록이다.
그 문서의 옛 B6 주소나 새 키 생성 예시를 현재 복구에 그대로 적용하지 않는다.

## 10. 한 보드만 초기화됐을 때 — B6 복구 절차

2026-08-28 B6는 전체 flash를 지우고 Layer 8을 새로 설치한 뒤 아래 순서로 복구했다.
다음은 향후 같은 상황이 생겼을 때 재현하는 절차이며, 현재 B6를 다시 초기화하라는 뜻이 아니다.
보드의 Mesh 정보는 사라졌어도 휴대폰에는 이전 B6 항목이 남아 있을 수 있다.

1. 기존 네트워크를 **먼저 Export**한다. 방법은 11절.
2. 기존 D6·76의 이벤트 AppKey와 C001 설정을 확인해 기록한다. 이 노드들은 초기화하지 않는다.
3. B6가 실제로 미등록 상태(`primary=0x0000`, BLE 이름 `ESP32-L8-BAB6`)인지 확인한다.
4. 앱에 옛 B6 항목이 있으면 **그 B6 항목만** `Remove Node`로 앱의 로컬 DB에서 제거한다. 이미 지워진 보드에 옛 DevKey로 Reset을 보내도 응답하지 않을 수 있다.
5. `Add Node`에서 B6를 찾아 4절의 Provision을 수행한다. **앱의 사용 가능한 새 주소**를 쓰고 옛 `0x0004`를 억지로 재사용하지 않는다.
6. 5~6절대로 **기존 공용 AppKey**와 C001 설정을 적용한다.
7. `event_ready=1`, `sub_C001=1` 및 실제 상대 수신을 확인하고 새 상태를 Export한다.

### Reset과 Remove는 다르다

| 메뉴/동작 | 실제 효과 | 사용할 상황 |
| --- | --- | --- |
| 보드 RESET 버튼 / 전원 재인가 | 다시 부팅. 보통 NVS의 Mesh 정보는 유지 | 일시적인 상태 확인 |
| 앱의 **Reset Node** | 대상 보드에 Mesh Node Reset 요청, 등록 해제 및 로컬 항목 제거 | 기존 키로 통신 가능한 노드 한 대를 의도적으로 재등록할 때 |
| 앱의 **Remove Node** | 앱 DB의 항목만 제거. 살아 있는 보드의 Mesh 정보는 지우지 않음 | 이미 초기화되거나 없어졌다고 확인한 보드의 옛 항목 정리 |
| 앱의 **Reset Mesh Network** | 앱의 네트워크 DB 초기화. 보드 전체가 자동 초기화되는 것은 아님 | 전체 새 구성을 계획했고 Export도 확보했을 때만 |
| PC의 전체 flash erase | 대상 보드의 앱·NVS·Mesh 정보를 삭제. 펌웨어 재설치 필요 | 펌웨어 복구가 필요한 경우에만 대상 확인 후 별도 수행 |

살아 있는 노드를 `Remove Node`로만 지우면 그 노드는 계속 기존 네트워크에 참여할 수 있다.
또한 이전 주소를 초기화된 노드에 재사용하면 상대의 replay 보호 상태와 맞지 않을 수 있으므로
문제 노드는 새 Unicast Address로 등록하고 정상 노드들의 상태를 보존하는 방식으로 복구한다.
주소별 Sequence Number가 뒤로 돌아가면 메시지가 버려지는 원리는
[Nordic의 replay 보호·Import 설명](https://devzone.nordicsemi.com/f/nordic-q-a/125466/nrf-mesh-android-app-mesh-network-import-issues)을 참고한다.

현재 Layer 8은 **원격 Node Reset 후 자동 재부팅하지 않는다.** 앱의 Reset Node가 성공한 뒤
미등록 검색에 안 나타나면 그 보드만 RESET 버튼/전원 재인가 후 다시 검색한다.
USB 콘솔의 `factory-reset` 명령은 비활성화되어 있다.

메뉴 의미 근거: [Nordic NodeViewController](https://github.com/nordicsemi/IOS-nRF-Mesh-Library/blob/fa0967b74e669cd925e6a5bc7b442031d69a796b/Example/Source/View%20Controllers/Network/Configuration/NodeViewController.swift),
[SettingsViewController](https://github.com/nordicsemi/IOS-nRF-Mesh-Library/blob/fa0967b74e669cd925e6a5bc7b442031d69a796b/Example/Source/View%20Controllers/Settings/SettingsViewController.swift).

## 11. 다음 복구를 위해 네트워크 Export 보관

### 정상 동작 확인 후

1. iOS의 `Settings`에서 내보내기/정리 버튼을 열고 `Organize → Export`를 선택한다. 버전에 따라 Import/Export 메뉴로 보일 수 있다.
2. 복구용으로는 **Export Everything**을 사용한다. 옵션을 따로 선택한다면 **Device Keys도 포함**한다.
3. 날짜·네트워크명을 붙여 본인만 접근 가능한 위치에 보관한다. 예: `Nostos-L8-2026-08-28.json`.
4. 새 노드 등록·주소 변경·키/Model 설정 변경 후에는 새 Export를 남긴다.

DevKey는 노드 재설정·구성에 필요하다. 키를 뺀 일부 Export는 완전한 복구 백업이 아니다.
Export에는 NetKey/AppKey/DevKey가 포함될 수 있으므로 **Git, 공개 드라이브, 채팅, 스크린샷에 올리지 않는다.**
이 `settings/`에는 절차만 보관하고 실제 네트워크 JSON·키 원문은 저장하지 않는다.
공식 옵션 근거: [ExportViewController](https://github.com/nordicsemi/IOS-nRF-Mesh-Library/blob/fa0967b74e669cd925e6a5bc7b442031d69a796b/Example/Source/View%20Controllers/Settings/ExportViewController.swift).

### 이 Mac에 보관한 실제 백업 — 2026-08-28 18:21 KST

사용자가 내보낸 `/Users/kafka/Downloads/Nostos.json`을 다음 비공개 위치에 원본 그대로 복사했다.
원본 Downloads 파일은 변경하거나 삭제하지 않았다.

`/Users/kafka/.local/share/esp-ble/mesh-backups/20260828-182112-hbst05kz/Nostos.json`

- 원본과 복사본 5591 bytes가 일치하며, 같은 폴더의 `manifest.json`에 SHA-256을 기록했다.
- 저장 파일 권한 `0600`, 폴더 `0700`. Git 작업 폴더 밖이며, 암호화된 저장소라는 뜻은 아니다.
- JSON 안에 NetKey 1개, AppKey 2개, 노드 4개와 각 DevKey 필드가 포함된 것을 확인했다. 키 값은 출력하지 않았다.
- B6 주소 `0006`, Vendor Model `02E50001`, Bind 1, Publication/Subscription `C001`, TTL 7, period/retransmit count 0을 확인했다.
- 이는 파일·설정 내용 확인이다. 실제 Import 복원 시험은 수행하지 않았다.

### 앱 데이터가 사라졌거나 다른 휴대폰으로 복구할 때

1. 현재 앱 데이터가 남아 있으면 먼저 Export한다. **Import는 현재 앱 설정을 덮어쓸 수 있다.**
2. `Organize → Import`에서 가장 최근의 정상 Export를 선택한다.
3. 네트워크·노드·주소·키 항목과 이 휴대폰이 사용할 Provisioner를 확인한다. 휴대폰 변경·앱 재설치로 기존 상태를 잃었다면 아래 주의사항을 먼저 적용한다.
4. 가까운 Proxy에 연결하고 실제 노드의 Model 설정을 다시 읽는다. 7~8절의 status·전달 검증을 수행한다.

Import는 **휴대폰 DB를 복원하는 작업**이다. 이미 전체 삭제한 B6에 키를 다시 넣는 작업은 아니므로
그 B6는 여전히 10절의 재등록이 필요하다. 오래된 Export도 보드의 최신 상태와 다를 수 있다.
복제한 동일 Provisioner 설정으로 여러 휴대폰을 동시에 사용하지 않는다.
백업도 기존 앱도 잃었다면 같은 이름으로 만든 네트워크는 복구가 아니다. 전체 참여 보드를
새로 등록해야 할 수 있으므로, 정상 보드를 포함한 초기화 범위를 먼저 정하고 진행한다.

### 다른 휴대폰·앱 재설치 후: Provisioner 주소 주의

JSON에 네트워크 정보가 있어도 휴대폰의 최신 **Sequence Number**까지 복원되는 것은 아니다.
이전 Provisioner 주소로 번호가 다시 0부터 시작하면 다른 노드가 replay로 보고 메시지를 버릴 수 있다.
Import 후 연결은 되는데 설정 요청만 계속 Timeout이면 이 경우도 점검한다.

기존 휴대폰을 사용할 수 있다면 `Settings → Provisioners → +`에서 복구용 Provisioner를 만들고,
**이전에 쓰지 않은 Unicast Address와 기존 Provisioner에 겹치지 않는 할당 범위**를 지정한다.
그 항목을 포함해 Export한 뒤 새 휴대폰의 Import 과정에서는 그 새 Provisioner를 선택한다.
기존 앱 상태를 잃은 경우에도 앱의 Provisioner 관리에서 새 로컬 Provisioner가 배정되는지 확인한다.
주소를 확실히 구분할 수 없으면 JSON의 주소·Sequence Number를 임의로 고치지 말고 중단한다.
이 절차는 ESP32의 NetKey/AppKey를 새로 만드는 작업이 아니다.
[Nordic 공식 지원 설명](https://devzone.nordicsemi.com/f/nordic-q-a/125466/nrf-mesh-android-app-mesh-network-import-issues).

## 12. 선택 사항: Generic OnOff로 C000 시험

이 단계는 **C001 이벤트 설정에 필수가 아니다.** 기존 OnOff 연습도 할 때만 사용한다.

| Model | AppKey Bind | Publication | Subscription |
| --- | --- | --- | --- |
| Generic OnOff Client | 참여 노드가 공유하는 OnOff용 AppKey | `C000`, 그 AppKey, TTL 7 | 이번 시험에서는 불필요 |
| Generic OnOff Server | 같은 OnOff용 AppKey | 이번 시험에서는 불필요 | `C000` |

모든 참여 노드에서 Client·Server를 모두 설정한 뒤 콘솔 `on/off`와 상대 `ONOFF_RX`로 확인한다.
이것이 성공해도 Vendor 이벤트 경로는 8절로 따로 검증한다. OnOff 시험을 하지 않았다면
`onoff_ready=0`이어도 C001의 `event_ready=1`, `sub_C001=1`과는 별개다.

## 13. 재설정 완료 체크리스트

- [ ] 원하는 기존 네트워크 또는 의도한 새 네트워크를 선택했다.
- [ ] 대상마다 Layer 8 실행과 물리 보드 라벨을 확인했다.
- [ ] 각 Unicast Address가 서로 다르고, 재등록한 새 주소를 기록했다.
- [ ] 같은 공용 AppKey를 각 Node에 Add하고 Vendor Model에 Bind했다.
- [ ] Vendor Publication: C001 / 동일 AppKey / TTL 7 / Period 0 / raw Retransmit 0.
- [ ] 각 Vendor Model이 C001을 Subscribe한다.
- [ ] 모든 참여 노드에서 `event_ready=1` **및** `sub_C001=1`을 확인했다.
- [ ] 한 대씩 재부팅한 뒤 설정이 유지되는지 확인했다.
- [ ] 버튼 한 번에 송신 로그와 상대 노드들의 실제 수신 로그를 확인했다.
- [ ] 상대 STM32의 출력 여부는 MCU 모드·배선과 함께 별도로 판정했다.
- [ ] 최신 네트워크 Export를 비공개 위치에 보관했다.

| 재설정 날짜 | 물리 보드 | 새 Unicast Address | NetKey index | AppKey index | event_ready / sub_C001 | 상대 수신 확인 |
| --- | --- | --- | --- | --- | --- | --- |
| 작성 | B6 | 작성 | 작성 | 작성 | 작성 | 작성 |
| 작성 | D6 | 작성 | 작성 | 작성 | 작성 | 작성 |
| 작성 | 76 | 작성 | 작성 | 작성 | 작성 | 작성 |

키 원문은 위 기록표에 적지 않는다.

### 추가 참고

- [Layer 8 설정·코드 설명](../layers/layer-8/README.md): 모델·프로토콜 참고. 예전 단일 버튼 설명과 현재 버튼 배선은 구분한다.
- [읽기 전용 UART 진단](../layers/layer-8/UART_DIAGNOSTICS.md): `status` 의미와 물리 전달 검증의 한계.
- [Nordic 공식 Android 앱/라이브러리](https://github.com/nordicsemi/Android-nRF-Mesh-Library): Android 화면이 다를 때 참고.
