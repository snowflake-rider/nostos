> 이관 원문: `docs/04-records/MY_UNDERSTANDING.md`. 현재 실행 경로는 [팀원 시작 안내](../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# 내가 이해한 BLE Mesh 용어

[기록 목록](README.md) · [학습 순서](../02-learning/README.md) · [전체 시작 메뉴](esp-ble-original-index.md)

> 아래는 기존 이해·질문과 검토 기록이다. 작성 중인 항목과 당시 설명을 보존했으며, 현재 검증 상태는 [STATUS](../01-project/STATUS.md)에서 확인한다.

이 문서는 Advertising, GATT, Provisioning, Relay를 내가 이해한 방식으로 직접 설명하는 학습 기록이다.

각 항목에는 다음 내용을 적어 본다.

- 내 말로 설명한 뜻
- 현재 ESP32-S3 BLE Mesh 프로젝트에서 하는 역할
- 아직 헷갈리거나 확인하고 싶은 부분

참고 문서: [TERMS.md](../03-reference/TERMS.md)

---

## Advertising(광고)

### 내가 이해한 뜻

Advertising은 다른 장치와 연결을 만들지 않은 상태에서도 주변에 짧은 무선 정보를 방송하는 방식이다.

무선 전파는 주변 전체로 퍼지지만, Bluetooth Mesh 메시지에는 Unicast 또는 Group 같은 목적지 주소가 들어갈 수 있다. 따라서 주변 노드가 모두 전파를 들을 수 있어도 목적지에 해당하는 노드만 메시지를 처리한다.

### 우리 프로젝트에서 하는 역할

`layer-1`에서는 ESP32-S3가 `ESP32-LAYER-1`이라는 이름을 비연결
Advertising하는 기본을 확인했다. 이후 `layer-2`에서 connectable
Advertising과 GATT Server를, `layer-3`에서 다른 보드의 Advertising을 찾는
Active Scan을 확인했다. `layer-4`에서는 같은 firmware를 올린 두 보드가
GATT Server, connectable Advertising, Active Scan을 동시에 실행하고 서로의
Advertising을 `PEER_RX`로 수신하는 것까지 실물 검증했다.

이 Layer 4 통신은 아직 표준 Bluetooth Mesh가 아니라 일반 BLE
Advertising/Scanning의 직접 수신이다. Head, Tail과 custom packet은 이
단계에서 아직 구현하지 않았다.

`layer-5`에서는 일반 BLE Manufacturer Specific Data 안에 version, type,
TTL, sender, recipient, sequence, payload, CRC가 있는 20-byte custom packet을
넣었다. 두 보드가 `HELLO` packet을 서로 직접 수신하고 CRC와 중복 여부를
검사하는 것까지 확인했다. packet 안에 TTL과 recipient field가 있어도
Layer 5는 수신 packet을 다시 보내지 않으므로 Relay가 아니다.

`layer-6`에서는 모든 보드가 같은 firmware로 origin packet을 만들고 상대
packet을 받으면 TTL만 1 감소시켜 다시 Advertising한다. original sender,
sequence, recipient, payload는 유지하고 변경된 bytes로 CRC를 다시 만든다.
두 보드에서 direct RX와 forward TX를 양방향 확인했다. 이것은 표준
Bluetooth Mesh가 아니라 우리가 만든 custom forwarding protocol이며,
세 번째 고유 보드의 relayed RX는 아직 미검증이다.

`layer-7`에는 표준 Bluetooth Mesh Composition, Generic OnOff Server/Client,
PB-GATT/PB-ADV, GATT Proxy와 Relay Feature가 구현되어 있다. 두 보드의
unprovisioned boot까지 확인했지만 아직 iPhone Provisioning과 Mesh message
수신은 확인하지 않았다. Head와 Tail은 무선 전송 방식이 아니라
애플리케이션에서 정할 자전거 대열의 역할이다.

### 헷갈리거나 확인하고 싶은 부분

Advertising을 하기 위해 모든 노드가 Server와 Client 역할을 동시에 가질 필요는 없다. Advertising과 Scanning은 무선 송수신 방식이고, Client와 Server는 Bluetooth Mesh Model의 애플리케이션 역할이다.

애플리케이션은 어떤 이벤트를 어떤 목적지에 언제 보낼지 결정한다. Bluetooth Mesh 스택은 실제 Advertising 전송, 암호화, TTL, 중복 처리, 재전송 같은 무선 전달 절차를 담당한다.

---

## GATT(속성 기반 통신 구조)

### 내가 이해한 뜻

GATT는 BLE 장치 두 대가 연결된 후 데이터를 Service와 Characteristic 구조로 읽고 쓰는 규칙이다.

Service는 관련 기능을 묶은 큰 항목이고, Characteristic은 그 안에서 실제로 읽거나 쓰거나 알림으로 받을 수 있는 데이터 항목이다. GATT Server가 데이터 항목을 제공하고 GATT Client가 이를 읽거나 쓰며, Server가 Notify 또는 Indicate로 변화를 알릴 수도 있다.

### 우리 프로젝트에서 하는 역할

GATT를 연결된 BLE 장치 사이의 속성 기반 데이터 통신 규칙이라고 이해하면 된다.

`layer-1`에는 GATT가 없지만 `layer-2`부터 GATT Server를 구현했고,
`layer-4`에서도 같은 Read/Write/Notification 동작을 유지한다. 이것은 일반
BLE GATT 학습 기능이다. 앞으로의 Bluetooth Mesh 과정에서는 스마트폰
Provisioner가 연결하는 `PB-GATT`와 스마트폰이 Mesh 네트워크에 접근하는
GATT Proxy에서도 GATT를 사용한다. 일반 Mesh 노드끼리 메시지를 전달하는
주된 경로는 Advertising Bearer이다.

### 헷갈리거나 확인하고 싶은 부분

GATT의 Client/Server 역할과 Bluetooth Mesh Model의 Client/Server 역할은 이름은 같아 보여도 서로 다른 계층의 역할이라는 점을 기억한다.

---

## Provisioning(네트워크 가입 절차)

### 내가 이해한 뜻

Provisioning은 새 Bluetooth Mesh 장치를 특정 Mesh 네트워크의 구성원으로 안전하게 등록하는 절차이다

### 우리 프로젝트에서 하는 역할

Provisioning된 노드는 NetKey, DevKey, Unicast Address 등을 받아 해당 Mesh 네트워크의 구성원이 된다. Provisioning되지 않은 장치는 그 네트워크의 암호화된 메시지를 정상적으로 보내거나 해독할 수 없다.

Provisioning 뒤에는 Composition Data 확인, AppKey 추가, AppKey와 사용할 Model의 Bind 같은 Configuration 과정도 필요하다.

### 헷갈리거나 확인하고 싶은 부분

이 프로젝트에서는 스마트폰을 Provisioner로 사용한다. 스마트폰은 새 노드를 Provisioning하고 AppKey, Model Bind, Relay 같은 초기 설정을 할 때 주변에 있으면 된다.

Provisioning과 Configuration 정보가 노드의 NVS에 저장된 뒤에는 노드끼리 Mesh 통신할 때 스마트폰이 계속 주변에 있을 필요가 없다. 나중에 스마트폰으로 명령을 보내거나 상태를 확인하거나 설정을 바꾸려면 스마트폰이 다시 통신 범위 안에서 GATT Proxy 등에 연결해야 한다.

스마트폰이 Provisioner라는 이유만으로 Mesh 메시지를 계속 전달하는 Relay가 되는 것은 아니다.

---

## Relay(중계)

### 내가 이해한 뜻

원래 송신 노드의 전파가 최종 수신 노드에 직접 닿지 않을 때, Relay가 활성화된 중간 노드가 수신한 Mesh 메시지를 다시 전송하는 기능이다.

TTL이 Relay 동작 자체를 수행하는 것은 아니다. TTL은 Relay를 한 번 거칠 때마다 감소하여 메시지가 무한히 재전송되는 것을 막고 최대 전달 범위를 제한한다.

### 우리 프로젝트에서 하는 역할

Head와 Tail이 직접 통신할 수 없을 만큼 멀어졌을 때도 메시지를 전달하려면 중간 노드의 Relay가 필요하다. 가까운 거리에서는 Relay가 꺼져 있어도 직접 통신할 수 있다.

모든 노드를 Relay로 만들 필요는 없으며, 프로젝트에서는 필요한 중간 노드만 Relay로 설정한다. Layer 6의 custom forwarding과 표준 Bluetooth Mesh Relay는 구분한다. Layer 7 source에는 표준 Mesh Provisioning, key/Model 설정 callback과 Relay Feature가 포함되어 있지만, 실제 Relay는 아직 검증하지 않았다.

### 헷갈리거나 확인하고 싶은 부분

직접 전파 범위를 넘어 자전거 대열 전체에 메시지를 전달하는 것이 최종 목표이므로 Relay는 핵심 프로젝트 기능이다. 다만 실제 성공 여부는 Head와 Tail이 직접 통신할 수 없는 조건에서 중간 노드의 Relay를 껐을 때와 켰을 때를 비교하여 증명해야 한다.

---

## 네 용어를 연결한 전체 흐름

1. `layer-1`에서 ESP32-S3가 연결 없이 이름을 Advertising하는 기본을 확인했다.
2. `layer-2`에서 GATT 연결과 데이터 Read/Write/Notification을 확인했다.
3. `layer-3`과 `layer-4`에서 다른 보드의 Advertising 직접 수신과 동일 firmware의 양방향 ADV + SCAN을 확인했다.
4. `layer-5`에서 CRC와 sequence가 있는 custom packet의 양방향 직접 송수신을 확인했다.
5. `layer-6`에서 custom packet의 TTL을 줄여 재송신하는 두 보드 forwarding을 확인했고, 세 보드 relayed RX는 남아 있다.
6. `layer-7` Mesh Node가 Provisioning 전 `ESP32-MESH-XX`로 자신의 존재를 알린다.
7. 스마트폰 Provisioner가 노드를 발견하고 `PB-ADV` 또는 `PB-GATT`로 Provisioning한다.
8. 노드는 NetKey, DevKey, Unicast Address를 받고 Mesh 네트워크에 가입한다.
9. 스마트폰이 AppKey 추가, Model Bind, Relay 설정 같은 Configuration을 수행한다.
10. 설정이 끝나면 스마트폰은 주변에서 떠나도 되며, 노드들은 저장된 Mesh 정보로 서로 통신한다.
11. 애플리케이션이 목적지 주소를 포함한 메시지를 만들고, Mesh 스택이 Advertising Bearer로 전송한다.
12. 목적지 노드가 직접 전파 범위 밖에 있으면 Relay가 활성화된 중간 노드가 TTL을 감소시키고 메시지를 다시 전달한다.
13. 스마트폰으로 명령, 상태 확인 또는 설정 변경이 필요할 때만 다시 주변에서 Mesh 네트워크에 연결한다.

## 검토 기록

작성한 내용을 검토받은 뒤, 잘못 이해했던 부분과 수정한 내용을 여기에 기록한다.

- 잘못 이해했던 부분: Advertising 메시지에는 목적지가 없고, 모든 노드가 Server와 Client여야 하며, TTL이 Relay 동작을 수행한다고 생각했다.
- 올바른 설명: 무선 전파는 방송되지만 Mesh 메시지에는 목적지가 있다. Advertising과 Model의 Client/Server는 다른 계층의 개념이다. Relay 노드가 메시지를 다시 전송하고 TTL은 전달 횟수를 제한한다.
- 기억할 핵심: 스마트폰 Provisioner는 가입과 초기 설정 때 필요하지만, 설정 완료 후 노드끼리 통신할 때 계속 주변에 있을 필요는 없다.
