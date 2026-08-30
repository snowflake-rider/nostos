# NOSTOS KiCad 배선 회로도

NOSTOS의 현재 핀 정의를 한 장으로 정리한 **배선 문서용 KiCad 회로도**다. MCU는
STM32 NUCLEO-F411RE, 로직 전압은 3.3V, 모든 모듈은 공통 GND를 기준으로 한다.

## 파일

- `nostos-wiring.kicad_pro`: KiCad 프로젝트
- `nostos-wiring.kicad_sch`: 편집 가능한 최종 회로도
- `Nostos.kicad_sym`: 프로젝트 전용 심볼 라이브러리
- `exports/nostos-wiring.svg`: 문서용 벡터 이미지
- `exports/nostos-wiring.pdf`: 인쇄용 PDF
- `exports/nostos-wiring.png`: 미리보기용 고해상도 PNG
- `reports/erc.rpt`: KiCad ERC 결과
- `reports/nostos-wiring.net`: 연결 검증용 KiCad 넷리스트
- `reports/analysis.json`: 보조 회로 분석 결과

핀의 기준은 [`PINS.md`](../../../PINS.md)와
[`nostos_stm32.ioc`](../../../firmware/stm32/nostos_stm32.ioc)다.

## 현재 센서 구성

| 구분 | 회로도에 표시된 장치 |
|---|---|
| 공통 | ESP32-S3 UART 브리지, SSD1306, VS1003B, 버튼 4개, RGB LED, 부저 |
| Rider Mid 활성 | DHT11 ON (`PA1/A1`) |
| Rider Tail 활성 | MPU6050 ON (`I2C1`, 주소 `0x68` 또는 `0x69`) |

회로도는 두 역할의 센서를 ON 상태로 표시한다. 현재 `firmware/stm32/CMakeLists.txt`의
기본 옵션은 `DHT11_SENSOR=OFF`, `MPU6050_SENSOR=OFF`이며 펌웨어 활성화는 별도 작업이다.

Button 4는 로컬 전용이며 현재 `MSG_NONE`이다. Buzzer Off 동작은 목표로만 표시했고
현재 펌웨어 구현으로 간주하지 않는다.

## 열기와 재생성

KiCad에서 `nostos-wiring.kicad_pro`를 열면 된다. 터미널에서는 다음 명령으로 ERC와
모든 출력물을 다시 만든다.

```bash
cd docs/schematics/nostos
./export.sh
```

스크립트는 `KICAD_CLI` 환경 변수, PATH, 사용자 Applications, 시스템 Applications
순서로 `kicad-cli`를 찾는다. ERC 위반이 하나라도 있으면 실패한다.

보조 분석 보고서의 MPN·footprint·datasheet 관련 항목은 PCB 제작 전 BOM 기준의
게이트다. 이 프로젝트는 breakout 배선 문서이므로 해당 항목을 ERC 오류로 해석하지 않는다.
I2C 외부 Pull-up 지적도 실제 OLED/센서 모듈의 내장 Pull-up 확인 후 판단한다.

## 전기적 주의

- 버튼은 내부 Pull-up을 사용하며 누르면 GND로 연결되는 Active-Low 구성이다.
- RGB LED는 공통 캐소드와 채널별 330 ohm 저항 기준이다. 실제 부품 극성을 확인한다.
- I2C Pull-up은 사용 중인 OLED/센서 보드의 내장 저항을 확인하고 중복으로 너무 낮아지지 않게 한다.
- DHT11 모듈에 Pull-up이 없을 때만 외부 10 kohm Pull-up을 사용한다.
- VS1003B와 부저의 전원 전압/전류는 실제 breakout 사양을 확인해야 한다. 고전류 부저는
  GPIO 직결 대신 트랜지스터 드라이버가 필요하다.
- 이 자료는 배선 문서이며 PCB 제작용 설계나 실제 하드웨어 동작 검증 결과가 아니다.
