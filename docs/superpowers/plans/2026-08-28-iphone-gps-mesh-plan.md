> 이관 원문: `docs/superpowers/plans/2026-08-28-iphone-gps-mesh-plan.md`. 현재 실행 경로는 [팀원 시작 안내](../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# GPS Mesh 구현 계획

승인된 설계: ../specs/2026-08-28-iphone-gps-mesh-design.md

상태: 신규 앱·펌웨어 소스 구현 및 호스트/빌드 검증 완료. 실기 설치·Mesh 설정·잠금 시험 미수행. 상세 증거는 `2026-08-28-iphone-gps-mesh-verification.md` 참고.

## 실행 순서

1. Build iOS Apps의 SwiftUI 패턴과 빌드 지침 적용. Nordic 4.8.0 API/SEQ 저장 흐름 확인.
2. Swift Package의 GPSCore: 공개 codec golden vector 테스트를 먼저 실패시키고 구현. 이어 경계값과 공유 정책(가짜 시각/위치/전송 이벤트)을 수직 단위로 추가.
3. C codec/receiver: golden vector, 잘못된 입력, 송신자 필터, 중복/세션/stale, 시리얼 주소 파싱 테스트. ASan/UBSan 실행.
4. 별도 gps-mesh-node에 기존 Layer 7 기반 모델을 유지하면서 Vendor Server 추가. 자동 NVS erase와 시리얼 factory-reset 제외. 독립 ESP-IDF 빌드.
5. iOS 앱: 안전한 신규 provisioner 할당, 잠금 후 접근 가능한 Keychain 저장, SEQ 선예약, 선택 Proxy 재연결, Core Location, SwiftUI 상태/설정 화면. SPM 의존성 고정 및 unsigned device build.
6. 전체 테스트 재실행, 기존 소스 해시 대조, 설치/모델 설정/15분 잠금/오프라인 시험 절차 기록.

## 검증 경계

- 호스트 테스트와 빌드는 실기 수신의 증거가 아니다.
- 현재 설치된 iOS Simulator 목록은 비어 있다. 임의로 Simulator runtime을 설치하지 않는다.
- 기기 플래시/앱 설치/네트워크 설정 변경은 하지 않는다. 정확한 포트, 서명, 네트워크 export 확인 후 별도 수행한다.
- 기존 layers, communication-module, stm32-project는 변경하지 않는다. 현재 폴더는 Git 저장소가 아니므로 커밋하지 않는다.
- UI에는 SDK 송신 수락만 표시하며 각 노드의 수신 여부를 추정하지 않는다.
