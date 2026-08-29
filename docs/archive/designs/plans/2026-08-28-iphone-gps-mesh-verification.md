> 이관 원문: `docs/superpowers/plans/2026-08-28-iphone-gps-mesh-verification.md`. 현재 실행 경로는 [팀원 시작 안내](../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# GPS Mesh 구현 검증 기록

날짜: 2026-08-28. 실제 경로: `/Users/kafka/Workspace_AI/esp-ble`.

## 수행/결과

| 항목 | 결과 |
| --- | --- |
| Swift wire contract | 24바이트 golden vector, 음수/0/좌표 경계, 반올림, 잘못된 시각/정확도/flags/길이, Data slice 통과 |
| Swift 공유 정책 | 1초 throttle, 최신값 하나, stale/future 폐기, 연결 복구, 중지, 이전 세션 completion 무시, backoff 상한/초기화 통과 |
| SEQ fence 정책 | 영속화 선행, 재시작 high-water, 저장 실패 시 예약 미갱신, rollback/고갈 거부 통과 |
| C | golden vector, 정렬되지 않은 입력, 음수/경계값, NULL/길이/필드 거부, source 필터, 중복/직전 세션, stale, 설정 명령/CRLF/overflow/NUL, reset 명령 비활성 통과 |
| C 동적 검사 | ASan / UBSan, `-Wall -Wextra -Werror`, CTest 1/1 통과 |
| ESP32-S3 | ESP-IDF 5.5.5 독립 빌드 성공. 앱 binary 0xDD170 bytes, 앱 partition 41% 여유 |
| iOS 앱 | Xcode 26.6 (17F113), iPhoneOS SDK 26.5, arm64 unsigned `.app` 생성 및 validation 성공 |
| plist/project | plutil lint 통과. 생성 앱의 UIBackgroundModes = location, bluetooth-central 확인 |
| 의존성 | Nordic 4.8.0 및 CryptoSwift 1.8.5 Package.resolved 고정 |
| 파티션 비교 | 새 partition-table.bin과 기존 layer-7 빌드의 바이너리 cmp 일치 |
| 기존 소스 보호 | 아래 소스 집합의 전후 SHA-256 일치 |
| 기기·무선 | 설치/플래시/키 가져오기/모델 설정 변경/무선 송수신 모두 수행하지 않음 |

최종 Swift Testing 실행은 테스트 7개를 포함한다. 출력 앞부분의 XCTest `Executed 0 tests`는 Swift Testing 결과가 아니다. 뒤의 `Test run with 7 tests`를 확인한다.

## 빌드 환경 이슈와 우회

`-scheme GPSMesh -destination 'generic/platform=iOS'`는 `iOS 26.5 is not installed`로 실패했다. Simulator 목록도 비어 있다. SDK 자체는 `/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS26.5.sdk`에 존재한다.

`-target GPSMesh -sdk iphoneos CODE_SIGNING_ALLOWED=NO`로 실제 arm64 앱 컴파일/링크/번들 validation까지 성공했다. UI 실행과 기기 설치를 대신하지 않는다. 경고는 manual target order deprecated, AppIntents 미사용에 따른 metadata extraction skip이다.

초기 SwiftPM workspace-local build DB에서 disk I/O 오류가 한 번 관찰됐다. 여유 공간은 142GiB였다. 새 `/tmp` scratch path에서 모든 테스트를 통과했고 기존 캐시를 삭제하지 않았다.

## 이번 실행의 임시 산출물

- iOS: `/tmp/gps-mesh-sdk.zpiU71/target-build/Debug-iphoneos/GPSMesh.app`
- ESP32: `/tmp/gps-mesh-sdk.zpiU71/esp-build/gps_mesh_node.bin`
- iOS build log: `/tmp/gps-mesh-sdk.zpiU71/ios-target-build.log`
- ESP build log: `/tmp/gps-mesh-sdk.zpiU71/esp-final-build.log`
- 테스트 디렉터리: `/tmp/gps-mesh-sdk.zpiU71/swift-tests`, `/tmp/gps-mesh-sdk.zpiU71/c-tests`

임시 파일은 OS가 지울 수 있다. 재현 명령은 각 README에 있고, 소스/프로젝트는 workspace에 있다. unsigned 앱은 그대로 설치할 수 없다.

## 보호 경계

검사 명령:

```sh
rg --files -0 layers/layer-7/main communication-module stm32-project -g '!**/build/**' -g '!**/.git/**' -g '!**/logs/**' | xargs -0 shasum -a 256 | LC_ALL=C sort | shasum -a 256
```

전후 모두 `00f8c23d78c1db59e7bc56eb974313fe96531f56636960dca0a87c7c5be2429c`.

Git repository가 아닌 폴더이므로 커밋/스테이징/원격 푸시하지 않았다. 신규 `apps/ios-gps-mesh`, `examples/esp32s3/gps-mesh-node`, 문서만 작성했다.

## 반드시 남겨 둔 실기 검증

1. iPhone 실제 OS/페어링/Developer Mode/Team 서명 및 앱 UI 실행.
2. 한 ESP32 적용 후 기존 OnOff, provisioning/NVS 유지, 실제 새 Composition Data 확인.
3. 세 Vendor Model의 AppKey binding/subscription과 최신 JSON import, 새 송신 주소 설정.
4. TEST 30개 같은 session/seq/값을 세 보드에서 비교.
5. LIVE 실외 이동, debugger 분리 15분 잠금, Wi-Fi/셀룰러 없이 전달.
6. Proxy 전원 30초 차단/복구, 앱 전면 복귀 없이 재연결.
7. 중지/권한 철회, 앱 강제 종료 후 Keychain/SEQ 복원, 저장 실패 주입.
8. IV 전환 시 fail-closed 정책 확인. v1은 장기 무중단 운용을 보장하지 않는다.

사용자 기기 조작과 비밀 네트워크 설정이 필요한 항목이다. 성공했다고 표시하지 않는다. 앱 수치, Proxy 전달, 개별 보드 수신, Relay 경로 증거는 구분한다.

## 후속 Simulator 설치·실행 확인 (2026-08-28 12:20 KST)

- 사용자 설치 후 runtime: iOS 26.5. 실행 기기: iPhone 17, UDID `C3D40D1A-6128-45C4-9337-2393319E533D`.
- 최초 Simulator scheme 빌드는 앱의 x86_64 빌드가 arm64만 생성된 GPSCore/NordicMesh 모듈을 읽으면서 실패했다. 로그에 타깃 불일치를 확인했다.
- 일회성 arm64/ONLY_ACTIVE_ARCH 인자로 성공한 뒤, 프로젝트 **Debug에만 `ONLY_ACTIVE_ARCH=YES`**를 추가했다. Release나 앱 로직/펌웨어는 변경하지 않았다.
- 회귀 확인: 같은 Simulator의 Build iOS Apps `build_run_sim({})`를 추가 인자 없이 실행 → 빌드/설치/실행 성공. 최종 PID 36809, bundle ID `dev.kafkalab.GPSMesh`.
- UI 확인: GPS Mesh 메인 화면의 공유 중지·위치 대기·그룹 0xC000 표시, 설정 화면 진입과 메인 복귀. 최종 재실행 뒤 메인 화면 재확인. JSON/실제 키는 가져오지 않았다.
- 네트워크/Proxy 미설정으로 공유 시작 버튼이 비활성이다. Simulator 전용 모의 Mesh 전송은 구현하지 않았으며 GPS 공유/무선 수신이 검증됐다는 뜻이 아니다.
- 최종 빌드 로그: `/Users/kafka/Library/Developer/XcodeBuildMCP/workspaces/esp-ble-faeb82342431/logs/build_run_sim_2026-08-28T03-20-13-884Z_pid87836_3c0afcbe.log`.
- 최종 Simulator 앱: `/tmp/gps-mesh-sdk.zpiU71/simulator-build/Build/Products/Debug-iphonesimulator/GPSMesh.app`.

위의 초기 runtime 미설치 기록은 초기 빌드 시점의 기록이다. 현재는 Simulator 설치/앱 실행 확인까지 진행한 상태다. 실제 iPhone·ESP32 검증 항목은 여전히 미수행이다.

## 후속 위치 권한 흐름 확인 (2026-08-28 12:24 KST)

- 원인: 위치 권한 요청이 공유 시작에만 연결돼 있었고, Mesh/Proxy 미설정 상태에서는 시작 버튼이 비활성이라 권한 요청을 할 수 없었다.
- 수정: 메인 화면에 독립적인 위치 권한 요청/상태/정밀 위치 표시와 앱 설정 진입을 추가했다. 권한 콜백은 공유 중이 아니어도 UI에 반영하고, 앱 전면 복귀 때 상태를 갱신한다. 독립 요청은 위치 업데이트나 Mesh 공유를 시작하지 않는다.
- 같은 iPhone 17 / iOS 26.5 Simulator에서 추가 빌드 인자 없이 빌드·설치·실행 성공. GPSCore 기존 테스트 7개 재통과. 새 권한 흐름은 UI로 확인했으며 권한 전용 단위 테스트를 추가한 것은 아니다.
- 시스템의 `Allow “GPS Mesh” to use your location?` 창을 관찰했다. 이후 iOS 설정에서 GPS Mesh의 Location = While Using을 확인했다. 앱 재실행(PID 37930) 후 런타임 UI에서 `허용 상태, 앱 사용 중 허용`, `정밀 위치, 켬`, `공유 중지됨`, `위치 대기`를 확인했다.
- 빌드 로그: `/Users/kafka/Library/Developer/XcodeBuildMCP/workspaces/esp-ble-faeb82342431/logs/build_run_sim_2026-08-28T03-23-28-991Z_pid87836_3b36553a.log`.
- 권한 거부/철회 분기, 실제 GPS 샘플, 실기 잠금 중 수집 및 ESP32 수신은 이번 확인 범위가 아니다. Mesh 키 가져오기나 펌웨어 변경/플래시는 수행하지 않았다.

## 후속 독립 GPS·지도 확인 (2026-08-28 12:32 KST)

사용자는 지도 배경의 인터넷 사용과 Mesh 없이 GPS 화면을 표시하는 범위를 승인했다. 직전 권한-only 버전과 달리, 이제 권한이 있으면 전면에서 자동으로 화면용 위치를 받는다. Mesh 전송은 여전히 명시적인 시작이 필요하다.

- `LocationSource.updateDemand(viewing:sharing:)`로 전면 표시와 LIVE 공유의 수집 요구를 구분했다. 공유하지 않을 때 배경 진입은 위치 업데이트 중지, 전면 복귀는 재개한다. LIVE 공유 중에만 background location을 요청한다.
- `AppModel`에서 화면용 최신 위치 갱신과 Mesh 제출을 분리했다. TEST 패킷 생성은 화면 위치를 바꾸지 않는다. 화면은 정확도가 50m를 넘는 유효 좌표도 표시하지만 wire contract/ShareSession 송신 필터는 그대로다.
- `LocationMapView`를 추가하고 Xcode target에 연결했다. 지도와 숫자는 같은 샘플을 사용한다. 수동 지도 조작 후 위치 버튼으로 다시 중심을 맞출 수 있다. 수신값이 오래되면 경과 시간과 대기 상태를 표시한다.
- Simulator iPhone 17 / iOS 26.5에서 빌드·설치·실행 성공. GPSCore 기존 테스트 7개 통과, Info.plist/project plutil lint 통과. 수신 값/빈 상태의 SwiftUI Preview 코드는 빌드되었지만 Xcode Canvas 렌더링은 별도로 실행하지 않았다.
- 모의 좌표 A `37.5665000, 126.9780000`, B `37.5700000, 126.9820000`를 simctl location으로 주입했다. 각각 UI 숫자, `± 5.0 m`, 서울 지도 타일과 최근 GPS 핀을 확인했다. 현재 위치로 이동 버튼을 눌러 B 중심으로 복귀하는 화면을 확인했다.
- 5초 넘게 새 샘플을 넣지 않으면 `새 위치 대기`와 경과 초가 표시됐다. 고정 좌표를 반복해서 새 측정으로 만드는 타이머는 추가하지 않았다.
- 홈 이동 → 모의 좌표 변경 → 홈의 GPS Mesh 아이콘으로 복귀 후 좌표 갱신을 확인했다. PID 40890이 유지되어 같은 앱 프로세스의 복귀임을 확인했다. 이 관찰은 실기 잠금 중 수집/배터리 검증을 대신하지 않는다.
- Mesh 미설정·공유 중지 상태에서 `SDK 제출 0`, `SDK 송신 완료 0`, `Session —`, `마지막 sample_seq —`를 UI로 확인했다.
- 최종 빌드 로그: `/Users/kafka/Library/Developer/XcodeBuildMCP/workspaces/esp-ble-faeb82342431/logs/build_run_sim_2026-08-28T03-31-23-482Z_pid87836_5da46649.log`.
- 이번 검증은 모의 Core Location → 앱 화면/지도까지다. 실제 iPhone GPS, 권한 철회/대략적 위치 분기의 UI 시험, LIVE/TEST Mesh 실전 송수신 및 15분 잠금 시험은 미수행이다. 기존 ESP32/STM32 소스·펌웨어·네트워크 키는 변경하지 않았다.

## 후속 실제 iPhone 서명·설치 확인 (2026-08-28 13:07 KST)

- 기기에서 iPhone 15 Pro Max / iOS 26.6.1 (23G83), 페어링 완료, Developer Mode 활성 상태를 확인했다. 사용자 잠금 해제 후 개발용 DDI 서비스도 준비 완료됐다. 연결은 localNetwork tunnel로 보고됐다.
- Xcode에 실제로 표시되는 `Hyun Kim (Personal Team)` 사용을 사용자에게 확인받았다. 프로젝트 Debug/Release는 해당 Team의 Automatic signing을 사용한다.
- `xcodebuild -project apps/ios-gps-mesh/GPSMesh.xcodeproj -scheme GPSMesh -configuration Debug -destination 'generic/platform=iOS' -derivedDataPath /tmp/gps-mesh-sdk.zpiU71/device-build build`가 exit 0, `BUILD SUCCEEDED`로 완료됐다. Mac 키체인 서명 승인은 사용자가 직접 진행했다.
- 실기용 arm64 산출물: `/tmp/gps-mesh-sdk.zpiU71/device-build/Build/Products/Debug-iphoneos/GPSMesh.app`. `codesign --verify --deep --strict --verbose=2` 통과, Apple Development 서명 및 앱 식별자 `dev.kafkalab.GPSMesh`를 확인했다.
- `devicectl device install app`은 CoreDeviceError 3002 / IXRemoteErrorDomain 6 / remote connection closed로 실패를 반환했다. 중복 설치 전에 `devicectl device info apps --bundle-id dev.kafkalab.GPSMesh`로 실제 기기 상태를 조회했고 `GPS Mesh`, 버전 `0.1.0`, build `1`이 설치된 것을 확인했다. 설치 명령의 정상 종료와 앱 설치 확인은 구분한다.
- 이어서 `devicectl device process launch`를 요청했으나 CoreDeviceError 10002 / FBSOpenApplicationErrorDomain 7 / `Locked`로 거부됐다. 오류는 기기가 잠겨 있어 앱을 실행할 수 없다고 명시한다. 따라서 실기 앱 실행 성공이나 실제 화면 확인으로 기록하지 않는다.
- GPSCore 회귀 테스트 7개가 다시 통과했다. 이번 단계에서 앱 로직·ESP32 펌웨어·Mesh 키는 변경하지 않았다. 지도 모양 앱 아이콘은 사용자 요청에 따라 나중으로 미뤘다.
- 다음 확인: iPhone 잠금 해제 후 GPS Mesh 실행, 실기 위치 권한 허용, 실제 GPS 숫자와 지도 확인. 실기 Bluetooth 송수신, 세 보드 수신, 15분 잠금 공유, 재연결은 여전히 미검증이다.

위의 초기 미설치/미서명 기록은 각 시점의 기록이다. 현재는 서명과 실제 설치까지만 확인됐고, 실기 UI 실행은 잠금 상태로 막혀 있다.
