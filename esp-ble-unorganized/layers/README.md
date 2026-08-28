# ESP32 학습 Layers

각 Layer는 이전 firmware 위에 추가 설치되는 bootloader가 아니라, 그 단계만 독립적으로 build하고 Flash할 수 있는 완전한 ESP-IDF 프로젝트다.

- `layer-0`: USB 장치 확인부터 최소 firmware의 실제 부팅 확인까지
- `layer-1`: BLE Controller/Host 초기화와 non-connectable Advertising 시작까지
- `layer-2`: connectable Advertising, GATT Read/Write, ACK, Notification까지
- `layer-3`: 별도 Layer 2 보드를 이름과 Service UUID로 찾는 BLE Active Scan
- `layer-4`: 같은 firmware 두 보드의 GATT Server + Advertising + Active Scan
- `layer-5`: 20-byte custom Advertising packet + CRC + sequence + dedup
- `layer-6`: 같은 firmware의 symmetric forwarding + TTL + direct/relayed path 구분
- `layer-7`: iPhone Provisioning용 표준 Bluetooth Mesh symmetric OnOff Node
- [`layer-8`](layer-8/README.md): Layer 7 기반 + STM32 UART 이벤트를 Mesh 그룹 C001로 송수신
- Layer 6 상태: 두 보드 direct RX/forward TX PASS, 세 고유 보드 relay chain은 미검증
- Layer 7 상태: A/B/C flash/boot, iPhone Provisioning/configuration, one-hop Group OnOff,
  `tx-low`/`tx-normal` read-back PASS; controlled Relay OFF/ON 비교는 미검증

Layer 7의 iPhone nRF Mesh 설정 절차는 [Mesh network manual](../manual/how-to-make-mesh-network.md)에서 확인한다.
Layer 8의 UART 배선·Vendor Model 설정·버튼 시험은 [Layer 8 README](layer-8/README.md),
코드/빌드와 실제 보드의 검증 구분은 [Layer 8 검증 기록](layer-8/VERIFICATION.md)을 따른다.

한 Layer의 성공은 다음 Layer의 성공을 의미하지 않는다. 각 디렉터리의 실행 로그로 `device -> chip -> build -> flash -> runtime`을 따로 확인한다.
