# NOSTOS Prototype Test 요구사항

## 1. 적용 범위

이 문서는 `BUTTON_OUTPUT_TEST`가 아닌 실제 **Prototype Test**의 기본 동작을 정의한다.

- 현수 STM32의 버튼 입력은 로컬 RGB와 로컬 오디오를 동작시킨다.
- 버튼 1~3 메시지는 USART1을 통해 현수 ESP32로 전달되고, Mesh를 거쳐 세민·성록 ESP32와 STM32에 전달된다.
- 세민·성록 STM32는 수신한 버튼 메시지에 맞는 오디오를 재생한다.
- 버튼 입력은 액티브 버저를 절대 울리지 않는다.
- 액티브 버저는 낙상이 확정된 `FALL_DETECTED` 상황에서만 울린다.

## 2. 기본 버튼 매핑

| 버튼 | 기능 | 메시지 | 값 | 현수 로컬 RGB | 로컬 동작 |
| --- | --- | --- | --- | --- | --- |
| 버튼 1 | SPEED UP | `MSG_SPEED_UP_REQUEST` | `0x11` | 초록 | RGB + 오디오 + USART1 송신 |
| 버튼 2 | SPEED DOWN | `MSG_SPEED_DOWN_REQUEST` | `0x10` | 노랑 | RGB + 오디오 + USART1 송신 |
| 버튼 3 | STOP | `MSG_STOP_REQUEST` | `0x13` | 빨강 | RGB + 오디오 + USART1 송신 |
| 버튼 4 | CALIBRATION 시퀀스 완료 | 로컬 요청 | 메시지 ID 없음 | 변경 없음 | BTN1→BTN2→BTN3 다음 입력일 때만 시작 |

### 캘리브레이션 시퀀스 규칙

- BTN1 → BTN2 → BTN3 → BTN4를 5초 안에 정확히 입력해야 한다.
- BTN1~BTN3은 시퀀스 중에도 각 RGB·오디오·USART1 송신 동작을 그대로 수행한다.
- 버튼 4를 단독으로 누르거나 순서가 틀리거나 5초를 초과하면 시작하지 않는다.
- 버튼 4 자체는 메시지·RGB·오디오·버저 출력을 만들지 않는다.
- 현재 공통 프로토콜에는 CALIBRATION 메시지 ID가 없으므로 USART1이나 Mesh로 전송하지 않는다.
- 캘리브레이션 성공 시 `calibration_completed.mp3`를 한 번 재생한다.
- 센서 실패·불안정 timeout·시작 거절에서는 완료 음성을 재생하지 않는다.

## 3. 출력 규칙

### RGB

- 버튼 1: 초록 (`R=OFF, G=ON, B=OFF`)
- 버튼 2: 노랑 (`R=ON, G=ON, B=OFF`)
- 버튼 3: 빨강 (`R=ON, G=OFF, B=OFF`)
- 버튼 4: RGB 변경 없음

### 오디오

- 버튼 1: `speed_up_request` 오디오
- 버튼 2: `speed_down_request` 오디오
- 버튼 3: `stop_request` 오디오
- 현수 STM32는 버튼 입력 직후 로컬 오디오를 재생한다.
- 세민·성록 STM32는 Mesh와 USART1을 통해 받은 동일 메시지의 오디오를 재생한다.
- MPU6050 캘리브레이션이 `READY`로 완료되면 `calibration_completed` 오디오를 한 번 재생한다.

### 버저

- 버튼 1, 2, 3, 4: 버저 OFF
- 후방 안전·후방 경고: 버저 OFF
- 낙상 의심·카운트다운: 버저 OFF
- SOS: 버저 OFF
- 낙상 확정 `FALL_DETECTED`: 버저 ON

## 4. 온습도 화면

- SSD1306은 I2C1 `PB8=SCL`, `PB9=SDA`, 7-bit 주소 `0x3C`를 사용한다.
- DHT11은 `PA1`에서 온도·습도를 측정한다.
- 부팅 직후 유효한 센서 값이 없으면 `DHT WAIT`를 표시한다.
- 측정 성공 시 온도·습도와 `DHT OK`, 실패 시 `DHT ERROR n`을 표시한다.
- 화면·DHT11 동작은 버튼 RGB·오디오·USART1·낙상 전용 버저 정책을 변경하지 않는다.

## 5. Prototype Test 체크리스트

### ⇧ 버튼 1 · SPEED UP (초록)

- [ ] **현수 로컬**: 현수 버튼 1 입력 → 현수 STM32 → 초록 RGB + 현수 오디오
- [ ] **현수 송신**: 현수 STM32 → USART1 TX → 현수 ESP32 → `button 1 message packet`
- [ ] **세민 수신**: 현수 ESP32 → `button 1 message packet` → 세민 ESP32 → USART1 RX → 세민 STM32 → 세민 오디오
- [ ] **성록 수신**: 현수 ESP32 → `button 1 message packet` → 성록 ESP32 → USART1 RX → 성록 STM32 → 성록 오디오
- [ ] **버저 확인**: 현수·세민·성록 버저가 모두 울리지 않음

### ⇩ 버튼 2 · SPEED DOWN (노랑)

- [ ] **현수 로컬**: 현수 버튼 2 입력 → 현수 STM32 → 노랑 RGB + 현수 오디오
- [ ] **현수 송신**: 현수 STM32 → USART1 TX → 현수 ESP32 → `button 2 message packet`
- [ ] **세민 수신**: 현수 ESP32 → `button 2 message packet` → 세민 ESP32 → USART1 RX → 세민 STM32 → 세민 오디오
- [ ] **성록 수신**: 현수 ESP32 → `button 2 message packet` → 성록 ESP32 → USART1 RX → 성록 STM32 → 성록 오디오
- [ ] **버저 확인**: 현수·세민·성록 버저가 모두 울리지 않음

### 🚨 버튼 3 · STOP (빨강)

- [ ] **현수 로컬**: 현수 버튼 3 입력 → 현수 STM32 → 빨강 RGB + 현수 오디오
- [ ] **현수 송신**: 현수 STM32 → USART1 TX → 현수 ESP32 → `button 3 message packet`
- [ ] **세민 수신**: 현수 ESP32 → `button 3 message packet` → 세민 ESP32 → USART1 RX → 세민 STM32 → 세민 오디오
- [ ] **성록 수신**: 현수 ESP32 → `button 3 message packet` → 성록 ESP32 → USART1 RX → 성록 STM32 → 성록 오디오
- [ ] **버저 확인**: 현수·세민·성록 버저가 모두 울리지 않음

### ⚙️ 버튼 1 → 2 → 3 → 4 · CALIBRATION

- [ ] **정확한 순서**: BTN1 → BTN2 → BTN3 → BTN4를 5초 안에 입력
- [ ] **일반 동작 유지**: BTN1~BTN3의 RGB + 오디오 + USART1 송신이 각각 실행됨
- [ ] **로컬 시작**: 마지막 BTN4 입력 → 현수 STM32 → MPU6050 캘리브레이션 시작
- [ ] **단독 입력 무시**: BTN4만 누르면 캘리브레이션과 출력이 모두 시작되지 않음
- [ ] **통신 차단**: USART1과 Mesh에 버튼 4 메시지를 보내지 않음
- [ ] **완료 음성**: 성공 `READY`에서 `calibration_completed.mp3`가 한 번 재생됨
- [ ] **실패 무음**: 센서 실패·불안정 timeout·시작 거절에서 완료 음성이 재생되지 않음
- [ ] **버저 차단**: 캘리브레이션 시작·진행·완료에서 버저가 동작하지 않음

## 6. 완료 판정

- SSD1306에 DHT11 온도·습도와 상태가 표시되어야 한다.
- 버튼 1~3의 RGB 색상, 로컬 오디오, USART1 송신, 세민·성록 원격 오디오가 모두 확인되어야 한다.
- 버튼 1~4 시험 중 어느 보드에서도 버저가 울리면 실패다.
- 낙상 확정 전 카운트다운에서 버저가 울리면 실패다.
- `FALL_DETECTED`에서만 버저가 울려야 한다.
- 캘리브레이션 성공에서만 완료 음성이 정확히 한 번 재생되어야 한다.
