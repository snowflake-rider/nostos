> 이관 원문: `docs/02-learning/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Firmware / SW Bring-up — 학습 순서

[전체 시작 메뉴](../records/esp-ble-original-index.md) · [프로젝트 개요](../project/OVERVIEW.md) · [진행 상태](../project/STATUS.md)

**01번부터 시작한다. 한 문서씩 이해를 확인하고 다음 단계로 넘어간다.**

시작 전: Mac에 ESP-IDF와 개발 도구가 설치되어 있어야 한다. 설치와 터미널 환경 활성화는 다른 작업이다.

## 01 → 02 → 03 순서로 읽기

1. **[프로젝트 시작과 첫 빌드](01-project-start.md)**

   - 환경 활성화 → 프로젝트 생성 → 칩 지정 → 빌드

2. **[Hardware Bring-up — 보드에서 첫 실행 확인](02-hardware-bringup.md)**

   - 로그 코드 작성 → 빌드 → 보드·포트 확인 → Flash → 부팅·로그 확인

3. **[BLE Advertising — 스마트폰에서 보드 발견하기](03-ble-advertising.md)**

   - BLE 준비 → Advertising → 빌드·Flash → 시작 로그 → 스마트폰 실제 수신

**02번은 기초 Bring-up, 03번은 BLE 광고 수신 확인이다. 빌드나 보드 로그만으로 무선 수신까지 검증한 것은 아니다.**

## BLE 참고와 이후 학습

- [블루투스 기본 용어](../../architecture/glossary.md)
- [BLE 학습 Layer 순서](LAYER-ROADMAP.md)

## 참고

- [Bring-up 개요와 빌드 파일·sdkconfig 설명](../reference/BUILD-AND-BOOT.md)
- [이전 설명과 이해 확인 기록](../records/BRINGUP-NOTES.md)
- [내가 이해한 BLE 용어](../records/MY_UNDERSTANDING.md)

이 목차는 학습 순서다. 문서가 있다는 것과 명령 실행·실물 검증을 마쳤다는 것은 다르다.
