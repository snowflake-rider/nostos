# 05. 검증 증거와 원본 파일

검증일: 2026-08-28 (KST). 아래 핵심 발췌는 원본 로그에서 확인하여 보존했다. `build/` 원본이 나중에 삭제되어도 이 문서의 결과·발췌·해시는 남지만, 전체 실행 로그가 함께 보존되는 것은 아니다.

## UART 단독 시험의 원본

원본 디렉터리: `stm32-project/integration/stm32/build/uart-periodic-test.WRLuTL/`.

| 파일 | 용도 |
| --- | --- |
| [observation-gpio18-after-d6-reset-02.log](../stm32-project/integration/stm32/build/uart-periodic-test.WRLuTL/observation-gpio18-after-d6-reset-02.log) | 성공 실행: STM32 10회와 D6 UART 10회 |
| [stm32-runtime-after-gpio18.log](../stm32-project/integration/stm32/build/uart-periodic-test.WRLuTL/stm32-runtime-after-gpio18.log) | MCU RAM의 시도·성공·실패 카운터 |
| [restore-after-gpio18.log](../stm32-project/integration/stm32/build/uart-periodic-test.WRLuTL/restore-after-gpio18.log) | 원래 512KB Flash 복구·검증 |
| [d6-final-uart-diag-02.log](../stm32-project/integration/stm32/build/uart-periodic-test.WRLuTL/d6-final-uart-diag-02.log) | 복구 후 D6 재부팅·최종 준비 상태 |
| [RESULT.md](../stm32-project/integration/stm32/build/uart-periodic-test.WRLuTL/RESULT.md) | 초기 실패와 성공까지의 누적 시험 기록 |

성공 구간 발췌:

```text
8.341s STM32: USB_TRACE hex=13
8.344s D6: I (27121) LAYER_8_UART: UART_RX id=0x13 result=queued
17.304s STM32: USB_TRACE hex=13
17.306s D6: I (36081) LAYER_8_UART: UART_RX id=0x13 result=queued
22.582s D6: I (41361) LAYER_8_UART: QUEUE pending=0 capacity=32; uart_rx valid=14 noop=103 invalid=27 hw_errors=0
```

위 발췌는 첫 번째와 마지막 수신이며, 원본 전체에서 `STM32: USB_TRACE hex=13`과 `UART_RX id=0x13 result=queued`를 각각 10건 확인했다. 누적 valid=14 중 4건은 첫 시험 바이트보다 앞서 발생했으므로 정상 송신 10건에 섞지 않았다.

## 버튼 → Mesh 시험의 원본

원본 디렉터리: `stm32-project/integration/stm32/build/button-mesh-test.eFmI1o/`.

| 파일 | 용도 |
| --- | --- |
| [raw.jsonl](../stm32-project/integration/stm32/build/button-mesh-test.eFmI1o/raw.jsonl) | 각 보드의 전체 관찰 로그·status·상대 시각 |
| [console.log](../stm32-project/integration/stm32/build/button-mesh-test.eFmI1o/console.log) | 대상 `0x13` 이벤트 발췌·집계 |
| [RESULT.md](../stm32-project/integration/stm32/build/button-mesh-test.eFmI1o/RESULT.md) | 사용자 세 번 누름 확인을 포함한 현장 결과 |

첫 이벤트 전체 경로와 상대 보드의 나머지 두 수신:

```text
16.228s STM32 USB_TRACE hex=13
16.232s D6 I (268981) LAYER_8_UART: MESH_TX id=0x13 source=0x0005 api=accepted age_ms=0
16.232s D6 I (268991) LAYER_8_UART: UART_RX id=0x13 result=queued
16.265s 76 I (737233) LAYER_8_UART: MESH_RX source=0x0005 id=0x13 result=queued
16.845s 76 I (737813) LAYER_8_UART: MESH_RX source=0x0005 id=0x13 result=queued
17.276s 76 I (738243) LAYER_8_UART: MESH_RX source=0x0005 id=0x13 result=queued
22.238s COUNTS={"76.MESH_RX.0x13": 3, "D6.MESH_TX.0x13": 3, "D6.UART_RX.0x13": 3, "STM32.0x13": 3}
22.241s PORTS_CLOSED
```

D6에서 `MESH_TX` 로그가 `UART_RX` 로그보다 먼저 출력된 것은 실제 로그 순서다. 출력 순서만으로 데이터 흐름이 역전됐다고 판단하지 않는다. 두 로그는 별도 작업 경로에서 출력된다.

버튼 시험 전후 D6 카운터:

```text
I (254841) LAYER_8_UART: QUEUE pending=0 capacity=32; uart_rx valid=0 noop=0 invalid=0 hw_errors=0
I (274401) LAYER_8_UART: QUEUE pending=0 capacity=32; uart_rx valid=3 noop=0 invalid=0 hw_errors=0
```

이 로그와 사용자 확인 “세번 눌렀어”를 함께 사용해 버튼 시험을 3/3으로 판정했다. 단순히 임의의 배경 Mesh 데이터 세 개를 센 것이 아니다.

## 원본 SHA-256

아래 해시는 문서 작성 시 원본 파일에서 계산했다. 나중에 해당 원본의 변경 여부를 비교하는 용도이며, ESP32에 설치된 펌웨어 이미지의 해시가 아니다.

```text
observation-gpio18-after-d6-reset-02.log
901dfd761f8496653f1d43b97f97dd1e7ee0c146c03b9358b8a861c7a4517447

stm32-runtime-after-gpio18.log
defee63c84bc10d6b0ba2b8e26ae3f9601b337dcdf8141ea2ed2961cdad81725

restore-after-gpio18.log
83998589d5d0ae28f3d197ffb94be1d35714e2f0f0ab4f45ec638efdec5969ec

raw.jsonl
20f953ce9a6b3df8a8f9aeff0a17b8127b32bf6db7d97e092c24a617759c56fd

console.log
0ec2c569921ad3600e4ad09403e199b3e0bcadb33c798104755334b5e81bb6b3
```

## 보존 범위

- 이 디렉터리에는 설명과 필요한 증거 발췌만 추가했다. 원본 로그·소스는 이동하지 않았다.
- Flash 백업, NVS 덤프, 비밀 키 값은 복사하지 않았다.
- ESP32 설치 이미지의 암호학적 식별값이나 전체 소스 스냅샷은 이번 기록에 없다. 미래의 다른 펌웨어가 동일한 결과를 낸다고 보장하지 않는다.
- 시험 당시 소스 경로는 STM32 `integration/stm32`와 ESP32 `layers/layer-8`이다. 별도 통합 복사본까지 이번 결과로 자동 검증 처리하지 않는다.
