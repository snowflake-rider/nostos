# XOSS 속도 센서 연동 체크리스트

## 장치 식별

- [ ] 센서 앞면·뒷면 사진 확보
- [ ] 제품명과 `S-26518` 표기의 위치 확인
- [ ] speed 모드로 설정
- [ ] 광고 이름 확인
- [ ] Advertised Service UUID 확인
- [ ] 다른 앱의 BLE 연결 해제

## GATT 캡처

- [ ] Services 화면 전체 캡처
- [ ] `0x1816` 존재 여부 확인
- [ ] `0x2A5B` Notify 속성 확인
- [ ] `0x2A5C` 값 읽기
- [ ] 회전 중 notification 3개 이상 저장
- [ ] 정지 후 notification/timeout 동작 기록
- [ ] disconnect 후 재연결 동작 기록

## 설정

- [ ] 실제 휠 둘레 `wheel_circumference_mm` 측정
- [ ] 정지 판정 timeout 합의
- [ ] stale/disconnect 시 `valid=false` 정책 합의
- [ ] 허용 가능한 최대 속도와 급증 처리 합의
- [ ] 연결할 센서 식별값 저장 방식 합의

## 코드와 회귀 검사

- [ ] CSC parser 경계 테스트
- [ ] wraparound와 reconnect baseline 테스트
- [ ] bounded queue와 backoff 확인
- [ ] NOSTOS source/session/sequence 소유권 확인
- [ ] 긴급 메시지 예약 슬롯 보존
- [ ] 호스트 Debug/Release/ASan/UBSan 통과
- [ ] ESP32-S3 타깃 빌드 통과
- [ ] 기존 Mesh/UART 회귀 없음

## 실물 검증

- [ ] Flash 전 대상 보드와 보존할 NVS/Mesh 상태 확인
- [ ] 사용자 승인 후에만 Flash
- [ ] 센서 검색·연결·notification 확인
- [ ] 저속/중속/정지 값 확인
- [ ] 범위 이탈·절전·재연결 확인
- [ ] GATT 연결 중 Mesh 송수신 확인
- [ ] 기준 속도계와 비교
- [ ] 결과와 미검증 항목을 Linear KAF-397에 기록

