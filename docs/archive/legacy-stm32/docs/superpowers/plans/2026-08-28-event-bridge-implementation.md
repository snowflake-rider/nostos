> 이관 원문: `stm32-project/docs/superpowers/plans/2026-08-28-event-bridge-implementation.md`. 현재 실행 경로는 [팀원 시작 안내](../../../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# 1차 이벤트 bridge 구현 계획

사용자가 상세 설계를 승인했다. 구현 기준은 [설계](../specs/2026-08-28-stm32-mesh-event-bridge-design.md)다.

1. 공통 codec → 방향별 FIFO/만료/전송 정책을 공개 API의 red/green 테스트로 추가한다.
2. ESP32-S3에 UART RX Task, Bridge Task, Vendor Model과 콘솔 상태 조회를 추가한다.
3. 호스트 Debug/Release 및 sanitizer, ESP-IDF v5.5.5, STM32 Debug/Release × 센서 설정 4조합을 빌드한다.
4. 사용법·배선·설정과 관찰 가능한 검증 단계를 기록한다.

보호 대상: `integration/stm32/` 전체, 원본 `esp-ble/layers/layer-7/`, 다른 로컬 STM32 저장소. Flash, NVS 삭제, 원격 push는 하지 않는다.

승인된 테스트 경계: 8종 ID codec, UART/Mesh 입력과 출력, self 제외, 반복 ID, 32건 용량, 1000ms 만료, 준비 상태, 전송 실패. 시간과 외부 전송만 주입한다.

런타임 구현 세부: 호스트와 ESP32가 같은 고정 FIFO 코드를 사용한다. ESP32에서는 짧은 `portMUX` critical section으로 큐·통계 접근을 보호하고 Task notification으로 대기한다. 전송/로그/드라이버 호출은 critical section 밖이다. 생산자의 거부 통계도 같은 보호를 사용한다.
