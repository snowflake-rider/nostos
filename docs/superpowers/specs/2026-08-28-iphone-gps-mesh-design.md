> 이관 원문: `docs/superpowers/specs/2026-08-28-iphone-gps-mesh-design.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# iPhone GPS → Bluetooth Mesh — 첫 버전 설계

작성일: 2026-08-28

상태: 2026-08-28 사용자 상세 설계 승인 완료. 구현 진행. 실기 GPS·Bluetooth·잠금 검증은 별도 수행한다.

### 2026-08-28 후속 승인: 독립 GPS 표시와 지도

사용자가 기존 앱에 GPS 값과 지도 표시를 추가하고, **지도 배경에는 인터넷을 허용하되 ESP32 데이터 전송은 Bluetooth로 유지**하는 범위를 승인했다. 아래 초기 설계의 지도 제외·앱 전체 Bluetooth-only·공유 중지 시 모든 위치 수집 종료 조건은 다음 내용으로 갱신한다. wire contract·Mesh 설정·실기 검증 요구는 그대로다.

- 권한이 있으면 앱 전면에서 Mesh 연결 없이 Core Location을 수신해 좌표와 MapKit 지도의 최근 위치 핀을 함께 갱신한다. 위치 이력은 저장하지 않는다.
- 화면용 수집은 배경 진입 시 중지한다. 사용자가 LIVE Mesh 공유를 시작한 경우에만 배경 수집을 요청한다. Mesh 공유 중지 후에도 앱 전면에서는 GPS 표시가 이어진다.
- 지도 배경은 인터넷을 사용하며 오프라인 지도를 보장하지 않는다. 자체 서버나 인터넷을 통한 Mesh GPS 전송은 추가하지 않는다.
- 화면에는 유효한 대략적 위치도 정확도와 함께 표시한다. 송신에는 기존 정확도 50m 이내·측정 나이 5초 이내 조건을 적용한다. 오래된 화면 값은 경과 시간과 새 위치 대기를 표시한다.
- Simulator 좌표에는 모의 GPS 표시를 붙인다. TEST Mesh 패킷은 화면 GPS 샘플과 분리한다.

## 1. 목표와 확정 범위

iPhone 한 대가 위치를 수집하고 Bluetooth Mesh 그룹에 배포한다. ESP32 세 대는 같은 위치 메시지를 받아 시리얼 로그로 출력한다. iPhone 화면을 잠그거나 다른 앱을 사용하는 동안에도 사용자가 시작한 위치 공유 세션을 유지한다.

| 항목 | 결정 |
| --- | --- |
| 위치 공급자 | iPhone 15 Pro Max 한 대 |
| 실기 OS | 사용자가 알려준 iOS 26.6.1 |
| 앱 | Build iOS Apps를 활용한 Swift/SwiftUI 네이티브 앱 |
| 수신 장치 | 기존 ESP32-S3 세 대, 표준 ESP-BLE-MESH |
| 운용 중 통신 | Bluetooth만 사용. 서버·인터넷·Wi-Fi 게이트웨이 없음 |
| 기존 네트워크 | nRF Mesh에서 세 노드와 그룹 OnOff 동작을 사용자가 확인 |
| 그룹 | 기존 설정의 0xC000을 가져와 확인 후 사용 |
| 결과 확인 | 각 ESP32의 시리얼 로그. iPhone의 송신 표시와 구분 |
| 첫 버전 제외 | LiDAR, 충돌 판단, STM32 전달, 외부 디스플레이, 지도, 이동 경로 저장 |

GPS 공유 대상은 iPhone의 위치이다. ESP32 각각의 위치를 측정하거나 추정하지 않는다. 이 앱은 안전 경고 장치나 정밀 위치 계측기의 성능을 보장하지 않는다.

개발 도구·패키지 취득과 최초 설치 과정의 인터넷 사용은 운용 중 Bluetooth-only 조건과 구분한다. 설치 후 위치 공유에는 외부 네트워크가 필요하지 않도록 한다.

## 2. 현재 확인한 기반과 보호 경계

- 현재 작업 경로: /Users/kafka/Workspace_AI/esp-ble
- Xcode 26.6, build 17F113 설치를 명령으로 확인했다. 실제 iPhone 페어링·서명·설치는 미확인이다.
- layers/layer-7/main/mesh_node.c의 Element 0에는 Configuration Server, Generic OnOff Server, Generic OnOff Client가 있다. Vendor Model은 없다.
- layers/layer-7/sdkconfig.defaults에는 GATT Proxy와 Mesh 설정 저장 기능이 켜져 있다.
- 기존 OnOff 실기 동작은 사용자 확인 사항이다. 이번 문서 작성 과정에서 새로 무선 시험하지 않았다.
- 기존 build/CMakeCache.txt는 이전 /Users/kafka/Documents/Notion 경로를 가리킨다. 새 예제는 독립된 깨끗한 빌드 디렉터리를 사용한다.
- 현재 작업 경로는 Git 저장소가 아니다. 문서를 위해 Git 초기화나 임의 커밋을 하지 않는다.

보호 대상은 기존 layers 전체, communication-module, stm32-project와 기존 빌드 산출물이다. 이들을 직접 고치지 않고 다음 신규 경로에서 구현한다.

| 신규 경로 | 책임 |
| --- | --- |
| apps/ios-gps-mesh/ | iOS 앱, Swift 단위 테스트, 앱 문서 |
| examples/esp32s3/gps-mesh-node/ | Layer 7에서 파생한 독립 GPS Mesh 펌웨어와 C 테스트 |
| docs/superpowers/specs/2026-08-28-iphone-gps-mesh-design.md | 본 설계와 계약 |

파생 펌웨어는 기존 파일의 저작권·라이선스 표기를 보존한다. STM32 연동 코드를 미리 추가하거나 공용 통신 모듈을 재구성하지 않는다.

## 3. 구성과 선택 이유

    iPhone: Core Location → 위치 유효성 검사 → 전송 정책 → Mesh 메시지
                                      │
                              BLE GATT Proxy 연결
                                      │
                           지정한 ESP32 한 대
                                      │
                         표준 Mesh 그룹 0xC000
                                      │
                   ESP32 세 대의 GPS 모델 → 시리얼 출력

iPhone은 Mesh 메시지를 생성하는 앱이자 BLE Central이다. 연결된 ESP32는 Proxy 역할을 하며, 세 ESP32 모두 수신 모델을 가진다. Proxy 노드 자신도 그룹 수신 대상으로 검증한다. 이 도식은 데이터 흐름이며 고정된 다중 홉 경로를 의미하지 않는다.

선택한 방식은 Nordic iOS Mesh 라이브러리와 표준 GATT Proxy이다. 라이브러리가 Mesh 암호화·프로토콜·시퀀스를 처리하고 앱은 위치 메시지를 정의한다. Swift Package Manager로 통합하되 구현 시작 시 검토한 버전과 의존성 해석 결과를 고정한다.

대안 비교:

- 순수 Safari/PWA: 기본 Web Bluetooth와 잠금 중 연속 위치 수집의 제약 때문에 제외.
- 사용자 정의 GATT → ESP32에서 Mesh 메시지 재생성: 별도 게이트웨이 프로토콜과 펌웨어 역할이 추가되므로 이번 버전에서 제외.
- 표준 Generic Location Model: 좌표 전달 대안이지만 이번 버전의 세션·샘플 번호·측정 정확도를 한 계약에 담기 위해 GPS 전용 Vendor Model을 선택.

Vendor Model은 표준 Bluetooth Mesh의 애플리케이션 모델 확장이다. Layer 6의 사용자 정의 Advertising/CRC/Relay 프로토콜로 바꾸지 않는다.

## 4. iOS 책임 분리와 화면

| 구성 요소 | 책임 |
| --- | --- |
| LocationSource | 실제 Core Location 또는 테스트 위치 공급. 권한·오류·측정값 전달 |
| ShareSession | 사용자의 시작/중지 의도, 세션 ID, 위치 품질·신선도·전송 빈도 관리 |
| GPSCodec | 24바이트 메시지 encode/decode. UI·Bluetooth에 독립 |
| MeshTransport | 네트워크 가져오기, 송신자 식별, 지정 Proxy 연결·재연결, SDK 송신 결과 |
| SecureMeshStore | 키와 네트워크·시퀀스 상태의 안전한 보관 |
| SwiftUI 화면 | 위 구성 요소의 상태 표시와 사용자 명령 |

앱은 다음 순서로 사용한다.

1. 네트워크 설정 파일 가져오기.
2. 네트워크·그룹·AppKey 인덱스 및 대상 노드 주소 확인. 키 원문은 표시하지 않음.
3. 가져온 네트워크의 Proxy 노드 한 대를 선택하여 연결.
4. 위치 권한을 허용하고 '위치 공유 시작'.
5. 잠금/다른 앱 사용 중 공유 유지.
6. 앱으로 돌아와 상태를 확인하거나 '공유 중지'.

주 화면에는 공유 상태, 위도·경도, 정확도, 마지막 측정 시각/경과 시간, Proxy 이름·주소·연결 상태, 그룹 주소, SDK가 수락한 송신 수, 마지막 송신 오류를 표시한다. 별도 설정 화면에는 네트워크 가져오기와 Proxy 선택만 둔다.

네트워크 가져오기나 Proxy 선택은 공유를 중지한 상태에서만 허용한다. 테스트 위치 모드는 설정의 명시적인 개발 기능으로 분리하고 화면에 TEST를 표시한다. 실제 위치로 자동 가장하지 않는다.

노드 목록은 '설정에 포함된 노드'이지 실시간 온라인 목록이 아니다. 첫 버전에 노드별 GPS ACK/상태 조회를 추가하지 않으므로 앱은 '3/3 수신 완료'를 표시하지 않는다.

## 5. 잠금 상태와 권한

첫 구현은 CLLocationManager의 표준 위치 업데이트를 사용한다. 공유 시작은 앱이 전면에 있을 때 사용자가 직접 수행한다. 위치 background mode와 Bluetooth central background mode를 선언한다.

- 위치 권한은 When In Use로 시작하고, 활성 위치 공유 세션에 필요한 백그라운드 업데이트를 설정한다. 화면 잠금 요구만으로 Always 권한을 무조건 요구하지 않는다.
- 공유 중에만 allowsBackgroundLocationUpdates를 활성화하고 자동 위치 일시중지 정책을 명시적으로 구성한다. 공유 중지 시 위치 업데이트를 종료한다.
- 위치·Bluetooth 접근 이유는 권한 안내에 한국어로 명시한다. 정밀 위치가 꺼졌거나 권한이 거부된 경우 품질 조건을 충족하는지 별도로 판단한다.
- UI 타이머로 계속 실행되는 것에 의존하지 않는다. 위치·BLE 이벤트가 들어오는 시점에 처리한다. 정확한 1 Hz 위치 취득을 약속하지 않는다.
- 화면 잠금과 앱 전환은 지원 대상으로 시험한다. 사용자의 강제 종료, 기기 재부팅, OS의 프로세스 종료 뒤 무조건 자동 재개하는 것은 이번 버전의 보장 범위가 아니다.
- 앱 재실행 후 자동으로 위치 공유를 켜지 않는다. 사용자가 다시 시작한다. 저장한 Mesh 주소와 시퀀스 상태는 별도로 복원한다.
- Bluetooth가 꺼지거나 위치 권한이 철회되면 송신 가능한 것처럼 표시하지 않는다. 위치 권한 철회는 공유 세션을 종료하며 재허용 후에도 사용자 시작이 필요하다.

위치·Bluetooth의 백그라운드 권한이 무제한 CPU 실행이나 끊김 없는 전달을 보장하지 않는다. 실기에서 디버거를 분리하고 검증한다.

## 6. 위치 품질·전송·재연결 정책

이 절의 수치는 안전 성능 기준이 아니라 첫 통신 데모의 명시적인 초기값이다. 변경 시 테스트 기대값과 문서를 함께 수정한다.

| 정책 | 초기값 |
| --- | --- |
| 송신 빈도 | 새로운 유효 측정에 대해 최대 1회/초 |
| 송신 시 위치 나이 | 측정 후 5초 이내 |
| 허용 정확도 | horizontalAccuracy가 유효하고 0~50 m |
| ESP32 stale 기준 | 새로운 수락 샘플이 10초 동안 없을 때 |
| 송신 대기 공간 | 처리 중인 메시지 외에 최신 측정값 한 개만 보관 |
| 앱 수준 ACK/재전송 | GPS 메시지별 ACK 및 동일 샘플 반복 전송 없음 |
| Proxy 선택 | 첫 버전은 사용자가 지정한 한 노드. 자동 다른 노드로 전환하지 않음 |

좌표는 유한수이며 위도 -90~90, 경도 -180~180 범위여야 한다. 0도 좌표를 오류로 취급하지 않는다. 정확도가 음수/NaN이거나 측정 시각이 현재보다 미래이면 유효 위치로 전송하지 않는다. 절전/실내/위치 오차 때문에 조건을 만족하지 못하면 그 이유를 표시한다.

Core Location이 배열을 전달하면 최신 유효 측정만 고려한다. 측정 시각은 이전에 수락한 측정보다 새로워야 한다. rate limit은 monotonic clock으로 계산하고, 시계 변경으로 위치가 미래로 보이면 새 정상 측정을 기다린다.

새 측정 이벤트가 왔을 때 1초가 지나지 않았거나 전송이 처리 중이면 최신 값만 남긴다. 다음 위치/전송 완료/연결 완료 이벤트에서 나이와 빈도를 다시 검사한다. 타이머가 정확히 매초 깨워준다고 가정하지 않는다. 정지 상태나 업데이트가 없는 동안 동일 좌표를 새 샘플로 만들어 heartbeat처럼 보내지 않는다.

연결이 끊기면 공유 의도를 유지한 채 지정 Proxy에 재연결을 시도한다. 서비스 UUID를 지정한 검색과 OS의 연결 이벤트를 사용하고 실패 시 즉시 반복하지 않는다. 오류 후 실행 기회가 있을 때 1, 2, 4, 8, 최대 30초의 backoff를 적용하며, 이 시간은 백그라운드에서 보장된 예약 시간이 아니다.

연결 복구 시 5초를 넘은 측정은 버리고 새 위치를 기다린다. 장시간 대기열을 만들어 지난 이동 경로를 몰아서 보내지 않는다. 공유 중지를 누르면 대기 측정을 버리고 새 GPS 송신을 막는다. 이미 SDK/무선 계층이 수락한 한 메시지는 뒤늦게 도착할 수 있으며 취소를 보장하지 않는다.

STALE은 '최근 위치 메시지가 없음'을 뜻하며 Bluetooth 연결 끊김이나 이동 여부를 단정하는 표시가 아니다.

## 7. GPS wire contract v1

24바이트 고정 길이 parameters를 사용한다. 아래 정수는 모두 little-endian이다. C 구조체 메모리를 그대로 송신하거나 Swift의 메모리 배치를 가정하지 않고 필드별로 직렬화한다.

| offset | bytes | 필드 | 규칙 |
| --- | --- | --- | --- |
| 0 | 1 | version | 1 |
| 1 | 1 | flags | bit 0: TEST, 나머지 0. 실제 위치는 0 |
| 2 | 2 | accuracy_dm | uint16, 미터 × 10을 올림. 최대 500 |
| 4 | 4 | session_id | uint32, 공유 시작마다 새 난수, 0 금지 |
| 8 | 4 | sample_seq | uint32, 세션 내 첫 송신 후보 1부터 증가 |
| 12 | 4 | measured_at | uint32, 측정 UTC Unix 초, 소수점 내림 |
| 16 | 4 | latitude_e7 | int32, 위도 × 10^7, 가장 가까운 정수로 반올림 |
| 20 | 4 | longitude_e7 | int32, 경도 × 10^7, 가장 가까운 정수로 반올림 |

좌표 반올림의 정확한 중간값은 0에서 멀어지는 방향으로 처리한다. 변환 전 범위·유한수·overflow를 검사한다. measured_at의 32비트 범위를 넘는 시각은 거부하며 포맷 변경 없이 wrap하지 않는다.

sample_seq는 SDK로 제출할 새 메시지를 만들 때 부여한다. 송신 실패로 번호가 비는 것은 허용한다. 같은 세션에서 재사용하지 않는다. 최대값 다음에는 새 session_id로 세션을 갱신한다. 이 번호는 Bluetooth Mesh 네트워크의 SEQ와 별개이며 재생 공격 방지를 대체하지 않는다.

테스트 메시지도 동일 포맷을 쓰고 TEST 비트를 반드시 켠다. 고정 좌표 실기 시험에서는 시각을 실제 생성 시각으로 넣는다. 다음 값은 encode/decode 전용 golden vector이며 실제 GPS 측정이나 최신 데이터가 아니다.

    version=1, TEST=1, accuracy=5.0m, session=0x01020304, sample=1
    measured_at=1700000000, latitude=37.5665000, longitude=126.9780000
    parameters(hex):
    01 01 32 00 04 03 02 01 01 00 00 00 00 f1 53 65
    68 31 64 16 20 4e af 4b

실험 전용 Vendor namespace는 기존 Espressif 예제 기반 CID 0x02E5를 유지하고 client model 0x1000, server model 0x1001, opcode selector 0x30을 사용한다. GPS opcode의 wire bytes는 F0 E5 02이다. 이 값은 이 실험망 내부의 예제 식별자이며 사용자에게 발급된 회사 ID나 출시용 프로토콜 식별자가 아니다. 다른 Vendor 장치가 있는 네트워크나 제품 배포에 그대로 사용하지 않는다.

GPS 메시지는 unacknowledged group message이다. parameters 24바이트는 단일 unsegmented 메시지에 맞지 않으므로 Mesh stack의 segmentation/reassembly를 사용한다. 1개 GPS 샘플이 1개 무선 패킷이라는 가정을 하지 않으며 손실·지연·재전송의 영향을 실제로 측정한다.

## 8. 기존 Mesh 가져오기와 저장

기존 네트워크를 초기화하지 않는다. nRF Mesh에서 사용자가 내보낸 설정 파일을 앱의 파일 선택기로 가져온다. 키를 채팅·코드·명령행 인자로 복사하도록 요구하지 않는다.

가져오기에서는 네트워크 ID, NetKey/AppKey 인덱스, 노드·그룹·주소 범위와 데이터 구조를 검사한다. 다른 네트워크의 비슷한 이름을 가진 Proxy에 연결하지 않도록 가져온 네트워크의 identity를 검증한다.

새 앱은 기존 nRF Mesh provisioner의 송신 주소와 시퀀스 카운터를 복제하여 사용하지 않는다. SDK의 provisioner/address allocation을 이용해 중복 없는 로컬 송신 주소와 할당 범위를 등록한다. 같은 네트워크를 다시 가져와도 새 앱의 기존 송신 주소·SEQ high-water 상태를 낮추지 않는다. 안전하게 병합할 수 없으면 가져오기를 거부한다. 여러 앱 사이의 이후 주소 할당 변경은 갱신된 설정 파일로 조정해야 하며 파일 복사가 자동 동기화를 뜻하지 않는다.

구현 시 SDK의 SEQ 영속화·재시작 동작을 검토하고 crash/relaunch 테스트로 주소/SEQ 재사용이 없는지 확인한다. iPhone 재부팅 직후 첫 잠금 해제 전에는 송신을 시작하지 않는다.

키는 Keychain의 ThisDeviceOnly 및 AfterFirstUnlock 접근 정책으로 보관한다. SDK가 사용하는 네트워크 데이터베이스·시퀀스 파일은 앱 전용 저장소에 두고 잠금 중 접근 가능한 file protection 정책을 명시한다. SDK 저장 포맷에 키가 포함되면 그 파일도 비밀 데이터로 취급하고 백업·로그·소스 관리에서 제외한다. 저장 파일 접근 실패 시 기본 키나 초기 시퀀스로 계속 보내지 않고 중단한다.

앱은 네트워크 내보내기·키 교체·노드 초기화·범용 provisioner UI를 제공하지 않는다. 외부 서버로 위치나 Mesh 설정을 전송하지 않는다. 위치 이력은 앱에서 영구 저장하지 않고 최신값과 제한된 진단 상태만 메모리에 둔다.

## 9. ESP32 확장과 비파괴 적용

세 보드는 동일한 신규 펌웨어를 사용한다. 기존 Configuration/OnOff 모델과 Element 수·기존 모델 순서를 유지하고 GPS Vendor Server를 추가한다. 기존 device UUID 생성·노드 identity·partition layout·NVS namespace를 보존한다. 사용자 설정 Relay 상태를 임의로 바꾸지 않는다.

배포 순서는 다음과 같다.

1. 사용자가 지정한 세 포트의 기기 identity와 현재 네트워크·키 인덱스·그룹·모델을 읽기 전용으로 기록한다. 키 원문은 로그에 남기지 않는다.
2. 기존 네트워크 export와 기존 펌웨어 복구 방법을 확보한다.
3. 기존 source/build를 덮어쓰지 않고 새 예제를 독립 빌드한다.
4. 먼저 한 보드만, 사용자가 확인한 포트에 erase 없이 갱신한다.
5. 기존 provisioning/OnOff 상태가 유지되는지 확인한다. 실패하면 나머지 보드 적용을 멈춘다.
6. 새 Composition Data를 읽어 GPS Vendor Server를 확인하고 해당 모델에 기존 AppKey를 bind한 뒤 기존 그룹을 subscribe한다.
7. 같은 확인을 나머지 두 보드에 반복하고 갱신된 네트워크 정보를 새 iOS 앱에 가져온다.

모델 추가 후에는 기존 nRF Mesh의 Composition Data 캐시가 오래될 수 있다. 실제 새 모델을 읽는 비파괴 갱신 경로를 먼저 확인한다. 캐시 갱신이나 NVS 호환성 문제가 있으면 보드를 지워 해결하지 않고 원인·영향을 보고한다. 기존 설정 유지가 검증되기 전에는 무조건 보존된다고 단정하지 않는다.

ESP32 decoder는 길이가 정확히 24인지, version/flags/좌표/정확도/식별자가 유효한지 검사한다. session_id, sample_seq, measured_at은 0을 허용하지 않는다. 바이트 배열을 정렬되지 않은 정수 포인터로 캐스팅하지 않는다. source 주소도 함께 기록한다.

세션·번호로 최근 중복을 구분하고 같은 source/session에서 이전 번호의 수신은 최신값을 덮어쓰지 않는다. 현재 세션과 직전 세션을 기억해 이전 세션으로 되돌아가는 지연 메시지를 무시한다. 첫 버전은 설정한 iPhone 송신자 한 명의 위치를 처리하며 다른 source는 수신 진단만 남기고 최신 위치로 채택하지 않는다.

송신자 주소는 새 앱에서 로컬 주소 할당이 끝난 뒤 각 보드의 신규 시리얼 명령 'gps-source 0xNNNN'으로 설정한다. 유효한 unicast 주소 0x0001~0x7FFF만 허용하고, 기존 노드 주소와의 중복은 앱에서 먼저 차단한다. 새 GPS 설정은 기존 layer7 namespace와 분리한 gpsdemo namespace에 변경 시에만 저장한다. 부팅 시 미설정이면 GPS_SOURCE_UNSET을 출력하고 GPS 최신값을 채택하지 않는다. 주소를 바꾸면 수신 세션·최신값 상태를 비운다. 기존 OnOff 명령은 이 설정에 영향받지 않는다.

수신 최신값과 local monotonic 수신 시각을 RAM에 저장한다. RTC가 동기화되어 있다고 가정하지 않는다. 10초 stale 판정은 새 수락 샘플의 local 수신 시각을 기준으로 하며, duplicate/invalid 메시지로 갱신하지 않는다. 송신자가 측정 나이를 검증하고 수신자는 measured_at을 로그로 남긴다. 네트워크 지연을 포함한 절대 측정 나이를 수신 노드가 정확히 계산한다고 주장하지 않는다.

로그 형식의 예시는 아래와 같다. 이는 기대 형식이며 실제 성공 로그가 아니다.

    [GPS_RX] src=0x.... dst=0xC000 session=0x........ seq=42 mode=LIVE
             measured_at=... lat=37.5665000 lon=126.9780000 accuracy_m=5.0
    [GPS_STALE] src=0x.... session=0x........ last_seq=42 elapsed_ms=10000
    [GPS_REJECT] reason=invalid_length

stale 진입은 상태가 바뀔 때 한 번 출력하고, 새 샘플 수락 시 fresh로 복귀한다. 값 수신·검증·로그 외의 센서 처리나 STM32 전송은 추가하지 않는다.

## 10. 검증과 완료 기준

| 단계 | 증거와 통과 기준 |
| --- | --- |
| 보호 경계 | 기존 Layer 7 및 제외된 소스가 변경되지 않음 |
| Codec | Swift와 C에서 golden vector 일치, 경계값·잘못된 길이·버전·flags·정확도 거부, C ASan/UBSan 통과 |
| 정책 단위 테스트 | fake clock/location/transport로 1초 제한, 5초 expiry, 최신값 한 개, 중지, 오류·재연결 검증 |
| 빌드 | iOS 앱·테스트 빌드, 새 ESP32-S3 예제 clean build. 빌드 성공은 무선 성공이 아님 |
| 기존 기능 회귀 | GPS 펌웨어 적용 후 기존 provisioning 및 그룹 OnOff 유지 |
| 고정 좌표 실기 | TEST 모드 샘플 30개 중 30개를 세 보드에서 같은 session/seq/값으로 확인하는 근거리 통제 시험 |
| 실제 위치 | outdoor 유효 위치가 세 보드에서 LIVE로 수신되고 이동에 따라 변경 |
| 잠금 지속 | 디버거 분리, 이동 중 화면 잠금 15분. 세 보드 로그와 앱 진단의 sample ID를 비교하고 누락·긴 공백을 기록 |
| Proxy 재연결 | 지정 Proxy 전원을 30초 끊고 복구. 앱을 열거나 잠금을 풀지 않고 새 유효 위치 수신 재개를 확인 |
| 중지·권한 | 중지 후 새 샘플 생성 없음, 대기값 제거, 이미 수락된 in-flight 메시지와 구분. 권한 철회 후 자동 재시작 없음 |
| 실제 오프라인 | Wi-Fi·셀룰러 데이터 없이 Bluetooth를 켠 실기 상태에서 위 위치 공유 확인 |

잠금 시험의 초기 수용 기준은 유효 송신 샘플 대비 각 노드 수신율 95% 이상, 정상 위치 공급·연결 상태에서 원인 불명의 수신 공백 10초 초과 없음이다. 이는 통제 시험의 목표값이지 Mesh 전체 환경의 보장이다. 위치 자체가 10초 이상 갱신되지 않으면 통신 성공으로 덮지 않고 위치 중단으로 기록하고 시험을 재평가한다.

재연결 시험은 Proxy 광고가 다시 보인 후 60초 이내 수신 재개를 초기 목표로 측정한다. iOS 백그라운드 검색 제한 때문에 미달하면 사실대로 기록하며 즉시 수신을 보장했다고 주장하지 않는다. Node 재부팅으로 로컬 중복 상태가 초기화되는 경우도 구분한다.

세 보드의 수신만으로 Relay 경유를 증명하지 않는다. 이 버전의 필수 통과는 그룹 수신이며, 특정 Relay 경로 증명은 직접 경로를 통제한 별도 시험이다. 앱의 SDK 송신 수락, Proxy 전송, 각 노드 수신은 각각 다른 증거다.

실기 시험에는 사용자의 iPhone 연결·개발자 모드·서명, 비밀 네트워크 export, 정확히 확인한 보드 포트, 사용자의 잠금·이동 조작이 필요하다. 이것들이 없을 때 완료한 단위/빌드 결과만 보고하고 실기 항목은 미검증으로 남긴다.

## 11. 설계 검토 결과와 다음 단계

2026-08-28 자체 검토에서 다음을 확인했다.

- 사용자 요구와 비목표를 분리하고 iPhone 한 대의 위치만 공유함을 명시했다.
- 잠금 중 실행과 강제 종료·재부팅을 구분했다.
- GPS 샘플 번호와 Mesh SEQ를 구분하고 다른 앱의 송신 주소 복제 위험을 다뤘다.
- 패킷의 길이·필드·단위·endian·golden vector가 일치한다.
- 분할 전송, 데이터 신선도, 위치 이벤트 중단, 동일 좌표 반복을 구분했다.
- 기존 모델·NVS·소스 보호와 Composition Data 갱신 실패 시 중단을 명시했다.
- 테스트 데이터, SDK 송신 수락, 실기 수신, Relay 검증을 혼동하지 않도록 했다.

사용자가 본 문서를 승인했다. 상세 구현 계획에 따라 Build iOS Apps의 SwiftUI·빌드·디버깅 흐름으로 진행한다. iOS 시뮬레이터 결과로 실제 GPS·Bluetooth·잠금 동작을 대신 증명하지 않는다.

설계 승인 시점에는 이 문서만 작성된 상태였다. 이후 구현·빌드 결과는 구현 계획과 각 예제 README에 기록한다. 플래시·Mesh 설정 변경·실기 설치는 별도 확인이 필요하다.

## 12. 근거

아래는 플랫폼·SDK 근거이다. 전송 빈도·품질 임계값·GPS 포맷·시험 수용 기준은 이 프로젝트의 설계 선택이다.

- [Apple: 백그라운드 위치 처리](https://developer.apple.com/documentation/corelocation/handling-location-updates-in-the-background)
- [Apple: 위치 권한 선택](https://developer.apple.com/documentation/corelocation/requesting-authorization-to-use-location-services)
- [Apple: Core Bluetooth 백그라운드 처리](https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/CoreBluetooth_concepts/CoreBluetoothBackgroundProcessingForIOSApps/PerformingTasksWhileYourAppIsInTheBackground.html)
- [Apple DTS: 백그라운드 실행 제한](https://developer.apple.com/forums/thread/685525)
- [Nordic: iOS Mesh 라이브러리](https://github.com/nordicsemi/IOS-nRF-Mesh-Library)
- [Nordic: 송신 provisioner 주소 구분](https://devzone.nordicsemi.com/f/nordic-q-a/56624/regarding-nrf-mesh-multi-provisioner)
- [Espressif ESP-IDF v5.5.5: Vendor Server 예제](https://github.com/espressif/esp-idf/blob/v5.5.5/examples/bluetooth/esp_ble_mesh/vendor_models/vendor_server/main/main.c)
- 현재 로컬 기반: layers/layer-7/main/mesh_node.c, layers/layer-7/sdkconfig.defaults, layers/layer-7/README.md
