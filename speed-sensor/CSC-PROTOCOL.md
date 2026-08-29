# 표준 CSC 패킷과 속도 계산

이 문서는 실물 센서에서 Cycling Speed and Cadence Service `0x1816`과 CSC Measurement `0x2A5B`가 확인된 경우에만 적용한다.

## CSC Measurement 구조

모든 다중 바이트 값은 little-endian이다.

| 순서 | 크기 | 필드 | 조건 |
| --- | ---: | --- | --- |
| 1 | 1 byte | Flags | 항상 존재 |
| 2 | 4 bytes | Cumulative Wheel Revolutions | Flags bit 0이 1일 때 |
| 3 | 2 bytes | Last Wheel Event Time | Flags bit 0이 1일 때, 단위 `1/1024 s` |
| 4 | 2 bytes | Cumulative Crank Revolutions | Flags bit 1이 1일 때 |
| 5 | 2 bytes | Last Crank Event Time | Flags bit 1이 1일 때, 단위 `1/1024 s` |

속도 센서 연동에는 wheel revolution data가 필요하다. cadence-only notification을 속도로 해석하지 않는다.

## 계산식

연속된 유효 샘플 두 개에서 다음을 구한다.

```text
delta_revolutions = current_wheel_revolutions - previous_wheel_revolutions
delta_ticks       = current_event_time - previous_event_time
delta_seconds     = delta_ticks / 1024
distance_m        = delta_revolutions × wheel_circumference_mm / 1000
speed_kmh         = distance_m / delta_seconds × 3.6
kmh_x10           = round(speed_kmh × 10)
```

16-bit event time과 32-bit cumulative revolutions는 unsigned modular subtraction으로 wraparound를 처리한다.

## 예시

휠 둘레가 `2105 mm`이고 다음 두 패킷을 받았다고 가정한다.

```text
첫 패킷: 01 10 00 00 00 00 04
둘째   : 01 11 00 00 00 00 08
```

- Flags `0x01`: wheel data 있음
- 누적 회전수: `16 → 17`, 차이 `1회전`
- event time: `0x0400 → 0x0800`, 차이 `1024 tick = 1초`
- 속도: `2.105 m/s × 3.6 = 7.578 km/h`
- NOSTOS 값: 반올림해 `kmh_x10 = 76`

첫 패킷만으로는 속도를 계산하지 않고 기준값만 저장한다.

## 반드시 처리할 경계 조건

- payload 길이와 Flags 조합이 맞지 않으면 폐기한다.
- `delta_ticks == 0`이면 나눗셈하지 않고 해당 샘플을 속도 갱신에 사용하지 않는다.
- 회전수가 줄어든 것처럼 보여도 먼저 32-bit wraparound 가능성을 처리한다.
- 휠이 멈추면 센서가 새 notification을 보내지 않을 수 있다. 마지막 wheel event가 바뀌지 않거나 설정된 timeout이 지나면 속도를 `0`으로 만든다.
- 연결 끊김, stale, malformed packet은 `0 km/h`와 같은 의미가 아니다. NOSTOS `valid=false`로 전달할 조건을 별도로 정의한다.
- 비현실적인 급증 필터의 상한은 제품 요구사항과 실물 로그를 보고 정한다. 임의의 고정값을 코드에 넣지 않는다.
- `wheel_circumference_mm`는 센서 값이 아니라 자전거별 설정이다. 타이어 표기만 쓰기보다 실제 1회전 이동거리를 측정하는 편이 정확하다.

## NOSTOS 변환

현재 공통 메시지 계약은 다음과 같다.

```c
typedef struct {
    bool valid;
    uint16_t kmh_x10;
} nostos_speed_t;
```

표준 CSC의 누적 회전수와 event time 원문을 Mesh에 그대로 보내는 계약은 현재 없다. ESP32에서 계산한 속도를 `kmh_x10`으로 변환하되, source/session/sequence 소유권과 로컬 producer 경로는 [IMPLEMENTATION.md](IMPLEMENTATION.md)에서 별도로 결정한다.

## 기준 문서

- [Bluetooth SIG Cycling Speed and Cadence Service 1.0](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/CSCS_v1.0/out/en/index-en.html)

