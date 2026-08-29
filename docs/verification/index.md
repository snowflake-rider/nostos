# 검증 안내

[문서 목록](../README.md)

**지금 ESP32 세 대를 시험하려면 [01 → 04 단계별 안내](../../tests/mesh/README.md)부터 시작하세요.**

## 현재 저장소 검사

- `bash tools/test-host.sh`: 보드 없는 C·Python·링크 검사.
- [Mesh Console](../../apps/mesh-console/README.md): fake serial 백엔드 검사와 UI 검사.
- [iPhone 앱](../../apps/ios-gps-mesh/README.md): Swift GPSCore 테스트와 iOS 빌드.
- [이번 경로 이관 결과](../decisions/0001-repository-layout.md).
- [구조 변경 후 펌웨어 전체 빌드](firmware-build-2026-08-28.md): 기본·예제·패치 복사본 9개 조합의 MCU 빌드 결과.

## 실물 관찰과 기존 증거

- [보존된 실물 시험](README.md)
- [팀 기준 STM32 버튼·UART 시험](stm32-button-uart.md)
- [수입 원본의 후속 Layer 8 관찰](layer8-latest-import.md)
- [메시지 전달 관찰 도구](message-broadcast.md)
- [반복 송수신·Relay OFF/ON/OFF 검사](mesh-repeat.md)
- [오디오 관찰 도구](audio-testing.md)

후속 STM32 시험 문서의 명령은 [미통합 출력 시험 패치](../../experiments/stm32-output-test/README.md)가 적용된 펌웨어를 전제로 할 수 있습니다. 호스트 테스트·USB 전달·Mesh API 수락·상대 수신·실제 출력 성공은 각각 별도로 확인합니다. 원본에 포함되지 않은 증거 파일은 그 사실을 표시했으며 성공 증거로 복원하거나 대체하지 않았습니다.
