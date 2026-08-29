# Nostos ESP32 — Layer 8 기반 배포 펌웨어

[팀원 시작](../../docs/getting-started/README.md) · [실제 코드](.) · [기존 Layer 8 설명](docs/layer8-background.md)

사용자가 Layer 8을 더 최신 작업본으로 지정하여 팀 배포 기준으로 선택했습니다. UART 진단, USB Serial/JTAG 콘솔, UART1 ↔ Mesh bridge, 호스트 검사와 관찰 도구를 함께 가져왔습니다.

2026-08-28 후속 정리에서 학습용 `code/layers/`를 제거했습니다. 앞으로 기능을 추가하는 곳은 `firmware/esp32/` 하나입니다. Layer 8의 31개 파일을 대조했으며, 현재 펌웨어 및 공통 protocol에 대응 파일이 모두 있습니다. 차이는 프로젝트명·부팅 로그의 프로젝트명·공통 경로·메시지 헤더 주석뿐입니다. 이번 삭제에서 펌웨어 소스와 설정은 변경하지 않았습니다.

## Layer 8과 달라지는 것

- 빌드 프로젝트명과 부팅 로그의 프로젝트명: `nostos_esp32`.
- `main/CMakeLists.txt`와 `host-tests/CMakeLists.txt`: 복사된 `common/` 대신 `libs/protocol/`을 참조.
- 파일 위치: `firmware/esp32/`.

그 외 Layer 8 C 코드와 UART·Mesh 설정은 유지합니다. 공통 protocol의 C 구현은 Layer 8 사본과 동일하며, 메시지 ID 헤더의 기존 차이는 설명 주석뿐입니다.

기존 통합 ESP32 코드는 [legacy](https://github.com/snowflake-rider/nostos/tree/e234d90de76d4c750b77ae44f84182f0bc3e78a7/code/legacy/esp32-event-bridge)에 보존했습니다. 그 코드의 `mesh_node_send_onofSf` 오타는 이전 통합본 빌드를 막아 올바른 함수명으로 수정했지만 팀 배포 기준으로 사용하지 않습니다.

이번 경로 정리의 검사 결과는 [재구성 기록](../../docs/archive/restructure/README.md), 첫 이관의 결과는 [과거 이관 기록](../../docs/archive/records/NOSTOS-MIGRATION.md)을 확인합니다. 과거 Layer 8 실물 기록을 새 빌드의 Flash·무선 수신 완료로 표시하지 않습니다.

## 빌드와 호스트 검사

ESP-IDF v5.5.5 환경을 준비한 뒤 저장소 루트에서:

```sh
cd firmware/esp32
idf.py build
```

보드 없는 호스트 검사(Debug/Release/ASan·UBSan)는 이 디렉터리에서 `bash test-host.sh`로 실행합니다. CMake와 Make 또는 Ninja를 사용하며 ESP-IDF나 USB 접속은 필요 없습니다. 하드웨어·배선 절차는 [팀원 시작](../../docs/getting-started/README.md)을 따릅니다.

## v2 프로토콜 선택 경로

`CONFIG_NOSTOS_PROTOCOL_V2=y`일 때 새 프레임/최대64B 본문을 처리하는 `bridge_runtime_v2.c`를 빌드합니다. 기본은 기존v1이며 실제 provisioned 주소↔source 설정 없이 v2를 시작하지 않습니다. UART 핀·Mesh 키·Relay 설정을 자동 변경하지 않습니다. [구현 계약과 배포 경계](../../libs/protocol/V2.md) · [호스트 one-stop 테스트](../../tests/message-protocol/README.md).

검증된 세 보드 프로필은 [profiles/v2.json](profiles/v2.json)에 있습니다. 각 빌드는 별도
`build-v2-*`와 생성 `sdkconfig`를 사용하므로 공용 설정을 덮어쓰지 않습니다.

```sh
bash firmware/esp32/scripts/v2-profile.sh build D6
bash firmware/esp32/scripts/v2-profile.sh app-flash D6 --port /dev/cu.usbmodem21301
```

`app-flash`는 선택한 보드의 USB serial·VID/PID와 포트 점유를 다시 검사한 뒤 app partition만
기록합니다. Mesh 설정이 있는 NVS, partition table, bootloader는 쓰거나 지우지 않습니다.
