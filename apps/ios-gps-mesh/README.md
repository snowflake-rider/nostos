> 이관 원문: `apps/ios-gps-mesh/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../docs/getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# GPS Mesh iPhone 앱

Mesh 연결 없이 iPhone의 GPS 값과 MapKit 지도를 표시하고, 사용자가 공유를 시작하면 Bluetooth GATT Proxy → 표준 Bluetooth Mesh 그룹 0xC000으로 보낸다. ESP32의 위치를 측정하는 앱은 아니다. 지도 배경은 인터넷을 사용하지만 ESP32 데이터 전송은 Bluetooth만 사용한다. 자체 서버·경로 저장·STM32 코드는 없다.

## 현재 상태 (2026-08-28)

- Swift GPSCore 테스트 및 C 수신 테스트 통과.
- ESP32-S3 새 펌웨어 독립 빌드 통과.
- Xcode의 명시적 target 빌드로 arm64 iPhone용 **unsigned 앱 번들 생성** 통과.
- 초기에는 Simulator runtime이 없었지만, 이후 사용자가 설치한 iOS 26.5 / iPhone 17 Simulator에서 scheme 빌드·설치·실행에 성공했다. 메인 화면, 설정 진입, 메인 복귀를 확인했다.
- Debug의 `ONLY_ACTIVE_ARCH=YES`로 앱과 Swift Package의 Simulator 아키텍처 불일치를 수정했다. 추가 빌드 인자 없이 재빌드·재설치·실행을 확인했다.
- Mesh 설정 전에도 메인 화면에서 위치 권한을 요청할 수 있다. Simulator의 시스템 권한 창과 설정 화면을 확인했고, 앱 재실행 후 `앱 사용 중 허용` / `정밀 위치 켬` 표시를 확인했다. 권한이 있으면 전면에서 GPS 표시를 시작하지만 Mesh 공유는 자동 시작하지 않는다.
- MapKit 지도·최근 GPS 핀·위도/경도/정확도/갱신 시각/오래된 값 표시를 추가했다. Simulator에서 서울시청(37.5665, 126.9780) → 종로 인근(37.5700, 126.9820)의 모의 좌표 변경과 지도 표시, 현재 위치로 이동 버튼을 확인했다. 실제 GPS 수신 검증은 아니다.
- 실제 iPhone용 서명 빌드와 `codesign` 검증에 성공했다. 설치 명령은 연결 종료 오류를 반환했지만, 이후 기기의 앱 목록에서 `GPS Mesh / dev.kafkalab.GPSMesh / 0.1.0 (1)` 설치를 확인했다.
- 실제 iPhone 실행 요청은 기기가 잠겨 있다는 `Locked` 오류로 거부됐다. 실기 UI 실행·GPS·Bluetooth·잠금 중 공유/재연결은 **미검증**이다. Simulator 화면 확인은 실기 동작 증거가 아니다. 전체 UI 기능/모의 Mesh 전송 시험도 아직 아니다.

기기에서 확인: iPhone 15 Pro Max / iOS 26.6.1 (23G83), 페어링 완료, Developer Mode 활성, 개발용 DDI 서비스 준비 완료. Mac에서 확인한 Xcode: 26.6 (17F113), iPhoneOS SDK 26.5.

## 열기 및 빌드

`GPSMesh.xcodeproj`를 Xcode에서 연다. 앱 target은 GPSMesh, deployment target은 iOS 26.0이다. 필요하면 Xcode Settings → Components에서 iOS 플랫폼을 설치한다. 현재 사용자 승인에 따라 Signing & Capabilities는 `Hyun Kim (Personal Team)`, Automatic signing, Bundle Identifier `dev.kafkalab.GPSMesh`로 설정돼 있다. 다른 개발자 환경에서는 본인의 Team과 Bundle Identifier로 설정해야 한다.

```sh
cd /Users/kafka/Workspace_AI/esp-ble/apps/ios-gps-mesh
xcodebuild -resolvePackageDependencies -project GPSMesh.xcodeproj -scheme GPSMesh
xcodebuild -project GPSMesh.xcodeproj -target GPSMesh -configuration Debug -sdk iphoneos CODE_SIGNING_ALLOWED=NO SYMROOT=/tmp/gps-mesh-ios-build build
swift test --package-path GPSCore --scratch-path /tmp/gps-mesh-core-tests
```

`-target`는 이 Mac에서 확인한 unsigned 빌드 우회 경로다. 정상 플랫폼 환경에서는 `-scheme GPSMesh -destination 'generic/platform=iOS'`를 사용할 수 있다. 앱을 기기에 설치하려면 서명이 필요하다. `Package.swift`는 SDK 크로스 컴파일 확인용이며 설치 가능한 앱을 만들지 않는다.

현재 환경에서 성공한 실기용 서명 빌드:

```sh
xcodebuild -project GPSMesh.xcodeproj -scheme GPSMesh -configuration Debug -destination 'generic/platform=iOS' -derivedDataPath /tmp/gps-mesh-sdk.zpiU71/device-build build
codesign --verify --deep --strict --verbose=2 /tmp/gps-mesh-sdk.zpiU71/device-build/Build/Products/Debug-iphoneos/GPSMesh.app
```

실기 설치는 확인됐지만 실행 요청 당시 기기가 잠겨 있었다. 잠금을 해제하고 홈 화면의 GPS Mesh를 열어 위치 권한과 실제 좌표 표시를 확인해야 한다. Simulator의 권한 설정은 실기에 적용되지 않는다. 앱 아이콘 디자인은 후속 작업으로 남겨 둔다.

NordicMesh **4.8.0 / ae3a3a4762b44a9b05ddee3ab2d0164dae073443**, CryptoSwift **1.8.5**를 Xcode `Package.resolved`에 고정했다. 의존성 다운로드와 지도 배경 로딩에는 인터넷을 사용한다. Mesh 위치 데이터 전송에는 인터넷을 사용하지 않는다. 오프라인 지도는 보장하지 않는다.

## GPS와 지도만 확인하기

1. 위치 권한을 허용하고 앱을 열면 Mesh 설정 없이 GPS 값을 받는다.
2. 지도 핀과 숫자는 같은 Core Location 샘플을 사용한다. 지도를 직접 움직이면 자동 추적을 멈추며, 우측 상단 위치 버튼으로 다시 따라간다.
3. 새 위치가 5초 넘게 없으면 마지막 값을 유지하면서 `새 위치 대기`와 경과 시간을 표시한다. 정확도가 50m를 넘는 유효 위치도 화면에는 경고와 함께 표시하지만 Mesh 송신 기준은 완화하지 않는다.
4. 공유하지 않을 때 앱을 배경으로 보내면 위치 업데이트를 중단하고 전면 복귀 시 다시 요청한다. Mesh 공유 중지는 전송만 중지하며, 앱을 보고 있으면 GPS 화면은 계속 갱신한다.
5. Simulator에는 `SIMULATOR · 모의 GPS`를 표시한다. Simulator의 Location 설정 또는 아래 명령으로 시험할 수 있다. 고정 좌표를 한 번 넣고 갱신하지 않으면 `새 위치 대기`가 나타나는 것이 정상이다.

```sh
xcrun simctl location C3D40D1A-6128-45C4-9337-2393319E533D set 37.5665,126.9780
```

UDID는 이 Mac의 iPhone 17 Simulator 값이다. 다른 기기에서는 `xcrun simctl list devices booted`로 확인한다. TEST 고정 좌표는 Mesh 송신 시험 전용이며 GPS 지도 표시를 덮어쓰지 않는다.

## 실기 준비 순서

위치 권한은 Mesh 준비와 독립적으로 먼저 설정할 수 있다. 앱 메인 → **위치 권한 허용** → **앱을 사용하는 동안 허용**. 이미 거부했다면 **앱 설정 열기**에서 변경한다. 이 요청은 [Core Location의 When In Use 권한 요청](https://developer.apple.com/documentation/corelocation/cllocationmanager/requestwheninuseauthorization())을 사용한다. 허용 후 전면에서 GPS 표시를 시작하며, ESP32 전송은 아래 준비 후 별도로 시작한다.

1. 기존 네트워크 export와 기존 ESP32 펌웨어/복구 절차를 확보한다. 키 원문을 채팅이나 소스에 붙여 넣지 않는다.
2. 정확한 보드 포트를 확인한 뒤 새 `examples/esp32s3/gps-mesh-node`를 **한 보드부터** 적용한다. 이 구현 작업에서는 플래시하지 않았다.
3. 기존 OnOff가 유지되는지 확인한다. 새 Composition Data의 `02E5:1001` Vendor Server를 읽고 기존 AppKey bind / 0xC000 subscribe를 세 보드에 설정한다. 기존 캐시 갱신이 안 되면 멈춘다. erase/reset으로 우회하지 않는다.
4. **세 GPS 모델 설정을 반영한 새 nRF Mesh JSON**을 파일 앱에 준비한다.
5. 앱 설정 → JSON 가져오기. 앱은 세 GPS 모델에 공통으로 bind된 AppKey 하나를 확인한다. 새 로컬 provisioner는 기존 할당 범위·노드 주소 밖의 한 주소를 사용한다.
6. 표시된 `gps-source 0xNNNN`을 세 보드의 시리얼에 입력한다. `GPS_SOURCE_SET` 로그를 확인한다. 새 주소를 다른 앱에 재할당하지 않는다. 가져오기 파일은 실시간 동기화가 아니다.
7. Proxy 검색 → 한 장치 선택 → 연결 확인. 네트워크 identity와 연결 후 실제 Proxy 주소를 검증한다. 표시되는 노드 목록은 온라인 목록이 아니다.
8. 먼저 TEST로 30개 고정 좌표를 세 시리얼 로그와 비교한다. 이후 TEST를 끄고 실외에서 LIVE를 시작한다.

첫 버전은 재가져오기·키 내보내기·초기화·provisioning UI를 제공하지 않는다. 잘못된 저장 상태를 발견해도 새 키/SEQ를 생성해 계속 보내지 않는다. 앱을 삭제해도 Keychain 항목이 남을 수 있으므로 삭제/재설치를 네트워크 초기화 수단으로 사용하지 않는다.

## 동작과 제한

- Core Location 표준 업데이트, When In Use 권한, `location` / `bluetooth-central` background modes. 공유 중만 백그라운드 위치를 활성화한다.
- Mesh 송신은 새 유효 위치 최대 1회/초. 나이 0~5초, 정확도 0~50m. 위치 이벤트 기반이며 1Hz 취득/타이머 실행을 보장하지 않는다. 화면은 별도 송신 없이도 수신값을 갱신한다.
- 앱의 대기 슬롯은 SDK 처리 중 1개 + 최신 위치 1개. SDK 내부의 Mesh segmentation/GATT 큐·재전송은 별개다. 무선 도착 시각/실시간 전달을 보장하지 않는다.
- 중지/위치 권한 철회 시 대기 위치를 버리고 새 제출을 중단한다. 이미 SDK가 받은 메시지 하나는 늦게 도착할 수 있다.
- 지정 Proxy만 재연결한다. 실패 후 실행 기회가 있을 때 1, 2, 4, 8, 16, 최대 30초 backoff를 적용한다. OS가 백그라운드 타이머를 정확히 깨운다는 뜻은 아니다.
- TEST는 **전면 전용**이며 현재 시각과 TEST flag로 고정 좌표를 생성한다. 앱이 배경으로 가면 TEST를 중지한다. 잠금 검증은 LIVE로 한다.
- `SDK 송신 완료`는 SDK completion 성공 수이다. 세 보드 수신율은 각 `GPS_RX`의 session/seq로 따로 측정한다. 앱에는 GPS 수신 ACK가 없다.
- 화면 잠금은 시험 대상이다. 강제 종료·재부팅/OS 종료 후 자동 공유 재개를 보장하지 않는다. 앱 재실행 뒤 사용자가 다시 시작한다.

## 보안 저장/SEQ

Nordic 4.8.0은 네트워크별 UserDefaults에 SEQ/IV 정보를 저장한다. 앱은 이에만 의존하지 않는다.

- SDK 네트워크 DB(키 포함)와 로컬 identity, SEQ high-water를 **하나의 Keychain 항목**으로 저장한다. `AfterFirstUnlockThisDeviceOnly`, iCloud Keychain 동기화 없음. 별도 키 JSON 파일을 생성하지 않는다.
- 모든 outgoing PDU 경로에 FencedTransmitter를 둔다. SDK가 할당한 다음 SEQ를 검사하고, 필요한 경우 다음 256개 범위를 Keychain에 먼저 저장한 뒤 GATT로 전달한다. GPS 외 Proxy configuration/segment ACK에도 적용된다.
- 앱/연결 재시작은 저장된 high-water부터 시작하므로 미사용 번호는 건너뛸 수 있다. 가져온 nRF provisioner의 주소/SEQ를 재사용하지 않는다.
- 저장 실패·SEQ 감소/고갈은 fail-closed. v1은 IV Update로 SDK SEQ가 0으로 초기화되는 경우에도 보수적으로 송신을 멈춘다. 앱 재실행 후 저장된 높은 SEQ로 복원한다. 장기 운용용 IV 전환/SEQ 고갈 절차는 후속 작업이다.
- 정책의 호스트 테스트는 통과했지만 **기기 Keychain 실패 주입, 강제 종료 후 SEQ 복원, 실제 IV 전환**은 아직 미검증이다.
- SDK logger는 연결하지 않는다(원시 키/패킷 로그 방지). 사용자 오류 표시에는 원시 JSON decoder 오류를 노출하지 않는다.

Vendor CID 0x02E5 / models 0x1000, 0x1001은 Espressif 예제 기반 실험용 namespace다. 사용자에게 할당된 회사 ID가 아니며 제품 배포에 그대로 쓰지 않는다.

## 15분 잠금/오프라인 시험

디버거 분리 후 실외 이동 상태에서 LIVE 시작 → Wi-Fi·셀룰러 데이터 끄기(Bluetooth 유지) → 화면 잠금 15분. 세 보드 로그에서 동일 session/seq/값을 대조한다. 유효 송신 대비 각 노드 95% 이상 수신, 정상 위치 공급/연결 중 원인 불명 10초 초과 공백 없음을 초기 목표로 측정한다.

지정 Proxy 전원을 30초 끊었다 복구하고 잠금 해제 없이 재개되는지 확인한다. 광고 복귀 후 60초 내 재개를 초기 목표로 측정하되 OS 제한으로 미달하면 그대로 기록한다. GPS 자체가 갱신되지 않은 구간과 통신 누락을 구분한다. 세 노드 수신만으로 Relay 경유를 증명하지 않는다.

설계/바이트 계약: `../../docs/superpowers/specs/2026-08-28-iphone-gps-mesh-design.md`.
