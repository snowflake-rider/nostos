> 이관 원문: `docs/02-learning/LAYER-ROADMAP.md`. 현재 실행 경로는 [팀원 시작 안내](../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# ESP32-S3 BLE 학습 Layer Roadmap

[전체 시작 메뉴](../04-records/esp-ble-original-index.md) · [기초 학습 순서](README.md) · [진행 상태와 검증 기록](../01-project/STATUS.md)

이 문서는 `layers/`의 **학습 순서와 단계별 통과 기준**이다. 아래 기준은 목표이지 PASS 결과가 아니다. 실제 완료 범위·날짜·보드별 로그는 [STATUS](../01-project/STATUS.md)에 모은다.

## 핵심 원칙

- 각 Layer는 이전 firmware 위에 추가 설치되지 않는다.
- 각 Layer는 독립적으로 build하고 Flash하는 완전한 ESP-IDF application이다.
- 새 Layer를 Flash하면 보드의 이전 application은 교체된다.
- ESP-IDF는 각 Layer를 build할 때 second-stage bootloader, partition table, application image를 함께 만든다.
- 한 Layer의 성공은 다음 Layer의 성공을 자동으로 증명하지 않는다.

## Layer별 증가 규칙

새 Layer는 바로 앞 Layer에서 확인한 흐름을 복사한 뒤 학습 기능을 한 가지씩 추가한다.

```text
Layer 0: device -> chip -> build -> flash -> app_main
Layer 1: Layer 0 + NVS/Bluetooth 초기화 + non-connectable Advertising
Layer 2: Layer 1 + connectable Advertising + GATT Read/Write/Notification
Layer 3: independent scanner + exact peer name/Service UUID match + RSSI
Layer 4: same image + GATT Server + connectable Advertising + Active Scanning
Layer 5: Layer 4 + 20-byte packet + CRC16 + sequence + dedup + queue worker
Layer 6: Layer 5 + symmetric forwarding + TTL decrement + logical/path dedup
Layer 7: independent standard ESP-BLE-MESH + Provisioning + Generic OnOff + Relay
Layer 8: Layer 7 Mesh + STM32 UART event IDs + Vendor group C001 + peer UART output
```

한 Layer에 다음 단계 기능을 미리 섞지 않는다. 실패하면 어느 기능에서 문제가 생겼는지 구분하기 어려워지기 때문이다.

## Layer별 목표와 통과 기준

| Layer | 배울 것 | 확인할 기준 |
| --- | --- | --- |
| [0](../layers/layer-0/README.md) | Device → Chip → Build → Flash → Runtime | 대상 칩 확인, Flash 성공, 새 앱의 BOOT_SUCCESS / RUNTIME_OK |
| [1](../layers/layer-1/README.md) | non-connectable Advertising | 시작 로그와 별도 스캐너의 실제 광고 수신 |
| [2](../layers/layer-2/README.md) | GATT Server | 연결, Read/Write, ACK, Notification, 연결 해제 뒤 Advertising 재개 |
| [3](../layers/layer-3/README.md) | Active Scan | 별도 보드의 이름과 Service UUID를 함께 식별하고 TARGET_RX 확인 |
| [4](../layers/layer-4/README.md) | 동일 이미지의 GATT + ADV + SCAN | 두 보드 각각의 초기화와 양방향 PEER_RX |
| [5](../layers/layer-5/README.md) | Custom packet / CRC / sequence / dedup | 호스트 packet 검사와 두 보드의 양방향 CRC 정상 packet 수신 |
| [6](../layers/layer-6/README.md) | Custom forwarding / TTL / direct·relayed path | 두 보드 direct RX·forward TX, 별도로 세 고유 노드의 동일 origin/sequence relay chain |
| [7](../layers/layer-7/README.md) | 표준 Bluetooth Mesh | Provisioning, AppKey·Model Bind, group OnOff 수신, 직접 경로 차단 조건의 Relay OFF/ON 비교 |
| [8](../layers/layer-8/README.md) | STM32 UART ↔ Mesh 이벤트 | UART RX → C001 송신 → 상대 Mesh RX → UART TX, 별도로 상대 STM32 수신/출력 확인 |

Layer 6의 custom forwarding과 Layer 7의 표준 Mesh는 다른 구현이다. Layer 6의 pair 결과로 세 보드 relay를, Layer 7의 unprovisioned boot로 Mesh 메시지 수신을 대신 증명하지 않는다.

## 공통 실행 순서

프로젝트 root에서 ESP-IDF 설치 상태를 확인한다.

```bash
./scripts/check-esp-idf.sh
```

현재 USB/serial 장치를 확인한다.

```bash
./scripts/device_profile.sh
```

실행할 Layer로 이동하여 그 Layer의 `README.md`와 `bootload.sh`를 사용한다.

```bash
cd layers/layer-0
./bootload.sh
```

여러 serial 장치가 있으면 확인한 포트를 명시한다.

```bash
./bootload.sh --port /dev/cu.usbmodemXXXXXXXX
```

포트 이름은 USB를 다시 연결하면 바뀔 수 있으므로 문서의 예전 값을 그대로 사용하지 않는다.

## 증거를 분리해서 기록하기

| 단계 | 확인할 것 | 다음 단계로 넘어가는 조건 |
|---|---|---|
| Device | macOS에 USB serial 장치가 보이는가 | 대상 `/dev/cu...` 확인 |
| Chip | ROM bootloader가 ESP32-S3로 응답하는가 | `chip_id` 결과 확인 |
| Build | bootloader, partition table, application binary가 생성되는가 | `idf.py build` exit code 0과 파일 확인 |
| Flash | 세 image가 보드 Flash에 기록되는가 | write/hash verification과 hard reset 확인 |
| Runtime | 새 application의 marker가 수신되는가 | 해당 Layer marker 확인 |
| BLE scan | 스마트폰/Scanner가 Advertising을 실제 수신하는가 | 이름, address, RSSI 기록 |
| Mesh scan | Provisioner가 Unprovisioned Device를 찾는가 | 실제 scan 화면/기록 확인 |
| Provision | 주소와 key가 할당되는가 | 앱 상태와 serial callback 기록 |
| Configure | AppKey와 Model Bind가 완료되는가 | 대상 Model 상태 기록 |
| Message | Unicast/Group OnOff가 도착하는가 | sender/destination/state log 확인 |
| Relay | 중간 Node를 거친 수신인가 | 직접 수신 불가 조건에서 Relay ON/OFF 비교 |

Build 성공만으로 Flash나 Runtime을 PASS 처리하지 않는다. Provisioning 성공과 AppKey/Model Bind 성공도 서로 다른 단계로 기록한다.

## 관련 자료

- [Layers 사용 원칙](../layers/README.md)
- [Layer 0 실행 방법](../layers/layer-0/README.md)
- [Layer 1 실행 방법](../layers/layer-1/README.md)
- [Layer 2 실행 방법](../layers/layer-2/README.md)
- [Layer 3 실행 방법](../layers/layer-3/README.md)
- [Layer 4 실행 방법](../layers/layer-4/README.md)
- [Layer 5 실행 방법](../layers/layer-5/README.md)
- [Layer 6 실행 방법](../layers/layer-6/README.md)
- [Layer 7 실행 및 iPhone 설정 방법](../layers/layer-7/README.md)
- [Layer 8 STM32 UART와 Mesh 이벤트 전송](../layers/layer-8/README.md)
- [ESP32-C3 Generic OnOff 예제](../examples/esp32c3/generic-onoff-node/README.md)
- [ESP-BLE-MESH 한국어 설명](../03-reference/ESP-BLE-MESH-C3.md)
- [BLE Mesh 학습 용어](../03-reference/TERMS.md)
- [내가 이해한 BLE Mesh 용어](../04-records/MY_UNDERSTANDING.md)

ESP32-C3 예제는 표준 Generic OnOff 구조를 읽기 위한 별도 예제다. 현재 `layers/`의 실제 target은 ESP32-S3이므로 두 target의 build/Flash 결과를 섞어 기록하지 않는다.
