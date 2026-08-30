# NOSTOS Requirements

세 노드는 [공통 장치](DEVICES.md)와 [배선](PINS.md)을 사용하고 센서 역할만 다릅니다. 장비 serial과
Mesh 키는 로컬에서만 관리합니다.

## 역할과 공유 데이터

| 노드 | 센서 역할 | 다른 두 노드에 공유할 값 |
| --- | --- | --- |
| [A / Head](RIDER-HEAD.md) | XOSS 속도 센서 | 속도·휠 주행거리 |
| [B / Mid](RIDER-MID.md) | DHT11 | 온도·습도 |
| [C / Tail](RIDER-TAIL.md) | MPU6050 | 낙상 여부(Yes/No) |

세 ESP32-S3는 같은 Bluetooth Mesh에 참여하고 직접 통신이 어려울 때 모두 relay할 수 있어야 합니다.
공유 응용 데이터는 `[속도, 휠 주행거리]`, `[온도, 습도]`, `[버튼 1/2/3]`,
`[낙상 Yes/No]`로 제한합니다. 센서와 버튼 메시지는 공통 protocol로 전달하며 각 STM32의
dashboard에서 세 노드 데이터를 확인해야 합니다. MPU6050 원시 가속도·자이로 값은 낙상 판정에만
사용하고 Mesh로 공유하지 않습니다.

속도는 dashboard에서 5개의 원으로 표시합니다. 성인 자전거 443건의 자유 주행 관측값
`평균 20.62 km/h`, `표준편차 5.49 km/h`를 기준으로 0~42.6 km/h 범위를
`평균±표준편차` 구간으로 나눕니다. 0.1 km/h 단위 경계는 Level 1 `0.0~9.5`,
Level 2 `9.6~15.0`, Level 3 `15.1~26.0`, Level 4 `26.1~31.5`, Level 5 `31.6 이상`이며
유효하지 않은 값만 채운 원 없이 표시합니다. 42.6 km/h는 `평균+4σ`에 가까운 표시 범위의
참고 상단값이지 센서값 제한은 아닙니다. 기준값과 정상분포 적합성은
[FHWA의 13개 shared-use path 현장 연구](https://www.fhwa.dot.gov/publications/research/safety/pedbike/05137/chapter5.cfm)를
사용합니다.

## 버튼 broadcast

| 입력 | 메시지와 로컬 출력 |
| --- | --- |
| Button 1 | ACCELERATE → 초록 LED → `pace_up.mp3` |
| Button 2 | DECEL → 노랑 LED → `pace_down.mp3` |
| Button 3 | STOP → 빨강 LED → `stop.mp3` |

## 현재 구현 차이

- Mesh relay 기본값은 비활성화되어 있어 3노드 relay 요구를 아직 충족하지 않습니다.
- 기본 STM32 빌드는 DHT11·MPU6050을 끄며 XOSS 드라이버와 노드별 빌드 profile이 없습니다.
- ride/environment/fall protocol과 shared-state 기반은 있으나 센서 publish→Mesh→세 dashboard의 실물 검증은 남아 있습니다.
