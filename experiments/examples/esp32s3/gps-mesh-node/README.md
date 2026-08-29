> 이관 원문: `examples/esp32s3/gps-mesh-node/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../../docs/getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# GPS Mesh 수신 노드 (ESP32-S3)

기존 `layers/layer-7`에서 파생한 독립 예제. 원본은 변경하지 않는다. Configuration Server / Generic OnOff Server / Generic OnOff Client를 같은 Element와 같은 순서로 유지하고 `CID 02E5 / model 1001` GPS Vendor Server를 추가했다. 같은 device UUID 생성, `layer7` metadata namespace와 파티션 기본값을 사용한다.

빌드 성공과 기존 NVS/OnOff가 실기에서 유지된다는 증거는 다르다. **아직 보드에 플래시하지 않았다.** 모델 추가에 따른 Mesh settings/Composition Data 호환성은 한 보드부터 검증해야 한다.

## 빌드/테스트

```sh
cd /Users/kafka/Workspace_AI/esp-ble
source /Users/kafka/esp/esp-idf-v5.5.5/export.sh
idf.py -C examples/esp32s3/gps-mesh-node -B /tmp/gps-mesh-esp-build -DIDF_TARGET=esp32s3 build
cmake -S examples/esp32s3/gps-mesh-node/host-tests -B /tmp/gps-mesh-c-tests
cmake --build /tmp/gps-mesh-c-tests
ctest --test-dir /tmp/gps-mesh-c-tests --output-on-failure
```

호스트 테스트는 ASan/UBSan과 `-Wall -Wextra -Werror`를 사용한다. ESP-IDF 5.5.5로 빌드했다. 기존 layer-7 build 폴더를 재사용하지 않는다.

## 설정 및 출력

새 모델을 기존 AppKey에 bind하고 group 0xC000에 subscribe한 뒤, iOS 앱이 할당한 송신 주소를 각 보드에서 설정한다.

```text
gps-source 0xNNNN
status
```

NNNN은 앱에 표시된 실제 4자리 unicast 주소(0001~7FFF)이다. 예제 문자열 그대로 보내지 않는다. source가 없으면 `GPS_SOURCE_UNSET`이며 최신 GPS를 채택하지 않는다. 설정은 `gpsdemo/source` NVS에 변경 시에만 저장하고 `GPS_SOURCE_SET`으로 확정한다. 주소 변경 시 RAM의 최신 위치/세션 상태를 비운다.

- `GPS_RX`: src/dst/session/seq/LIVE 또는 TEST/측정 Unix 초/좌표/정확도.
- `GPS_REJECT`: 길이·필드 오류, 다른 송신자, 중복/이전 번호, 큐 포화.
- `GPS_STALE`: 마지막 **새 수락 샘플** 이후 local monotonic 10초 경과 때 한 번. 중복/오류는 stale 시간을 갱신하지 않는다.
- `GPS_STATUS`: 송신자 설정·최신값 존재 여부·stale. 온라인/연결 판정이 아니다.

RTC 동기화를 가정하지 않는다. wire measured_at은 로그로 남기고 절대 end-to-end 측정 나이를 보드가 정확히 계산한다고 주장하지 않는다. 현재/직전 session만 RAM에서 기억한다. 재부팅하면 이 앱 수준 중복 캐시는 초기화되며 Mesh replay protection과 별개다.

Mesh callback은 바이트를 고정 크기 큐에 복사한다. 별도 GPS task 하나가 decode/source 설정/NVS/로그/stale을 처리한다. 센서 처리·STM32 UART·디스플레이 코드는 없다.

기존 `on`, `off`, `on-unack`, `off-unack`, `tx-low`, `tx-normal`, `status` 명령을 유지한다. 새 예제의 시리얼 `factory-reset`은 비활성이다. NVS 초기화 오류 시 자동 erase하지 않고 정지한다. 원격 Configuration Node Reset은 Mesh 표준 동작이므로 nRF Mesh에서 실수로 실행하지 않도록 주의한다.

배포 전 네트워크 export와 복구용 원래 펌웨어를 확보하고 포트를 확인한다. 한 보드만 erase 없이 갱신 → 기존 OnOff 회귀 → 새 Composition Data 확인 순서로 진행한다. Composition Data 캐시나 기존 Mesh NVS 설정 복원에 문제가 생기면 나머지 보드 적용을 멈춘다. 파티션/NVS를 지워 해결하지 않는다.

24-byte wire contract와 실기 절차는 `../../../apps/ios-gps-mesh/README.md`와 설계 문서에 있다. CID 02E5는 실험용 Espressif 예제 namespace이며 출시용 사용자 회사 ID가 아니다.
