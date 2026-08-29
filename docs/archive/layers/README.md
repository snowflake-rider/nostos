> 이관 원문: `layers/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# ESP32 학습 Layers — 과거 참고 기록

2026-08-28: 학습용 `code/layers/` 전체를 제거했습니다. 현재 개발·빌드 대상은 [Layer 8 기반 팀 ESP32](../../../firmware/esp32) 하나이며, 이후 기능도 이 프로젝트에서 확장합니다. `esp-ble-unorganized/` 안의 원본 사본은 이번 작업에서 변경하지 않았습니다.

이 폴더의 학습 설명과 관찰 로그는 과거 증거로 보존합니다. 아래의 독립 프로젝트 설명·실행 명령은 당시 기준입니다. 삭제된 Layer 0–7 소스 링크는 [삭제 직전 Git 스냅샷](https://github.com/snowflake-rider/nostos/tree/e234d90de76d4c750b77ae44f84182f0bc3e78a7/code/layers)을 가리키며 비공개 저장소 접근 권한이 필요합니다. Layer 8 코드 링크는 현재 펌웨어 또는 공통 protocol을 가리킵니다.

각 Layer는 이전 firmware 위에 추가 설치되는 bootloader가 아니라, 그 단계만 독립적으로 build하고 Flash할 수 있는 완전한 ESP-IDF 프로젝트다.

- `layer-0`: USB 장치 확인부터 최소 firmware의 실제 부팅 확인까지
- `layer-1`: BLE Controller/Host 초기화와 non-connectable Advertising 시작까지
- `layer-2`: connectable Advertising, GATT Read/Write, ACK, Notification까지
- `layer-3`: 별도 Layer 2 보드를 이름과 Service UUID로 찾는 BLE Active Scan
- `layer-4`: 같은 firmware 두 보드의 GATT Server + Advertising + Active Scan
- `layer-5`: 20-byte custom Advertising packet + CRC + sequence + dedup
- `layer-6`: 같은 firmware의 symmetric forwarding + TTL + direct/relayed path 구분
- `layer-7`: iPhone Provisioning용 표준 Bluetooth Mesh symmetric OnOff Node
- [`layer-8`](../../../firmware/esp32/docs/layer8-background.md): Layer 7 기반 + STM32 UART 이벤트를 Mesh 그룹 C001로 송수신
- Layer 6 상태: 두 보드 direct RX/forward TX PASS, 세 고유 보드 relay chain은 미검증
- Layer 7 상태: A/B/C flash/boot, iPhone Provisioning/configuration, one-hop Group OnOff,
  `tx-low`/`tx-normal` read-back PASS; controlled Relay OFF/ON 비교는 미검증

Layer 7의 iPhone nRF Mesh 설정 절차는 [Mesh network manual](../../hardware/how-to-make-mesh-network.md)에서 확인한다.
Layer 8의 UART 배선·Vendor Model 설정·버튼 시험은 [Layer 8 README](../../../firmware/esp32/docs/layer8-background.md),
코드/빌드와 실제 보드의 검증 구분은 [Layer 8 검증 기록](../../../firmware/esp32/docs/VERIFICATION.md)을 따른다.

한 Layer의 성공은 다음 Layer의 성공을 의미하지 않는다. 각 디렉터리의 실행 로그로 `device -> chip -> build -> flash -> runtime`을 따로 확인한다.
