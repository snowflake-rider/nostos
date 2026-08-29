# Nostos ESP32 Layer 8 RIDAR — RPLIDAR C1 확장

Layer 8 RIDAR는 [Layer 8](..)의 STM32 UART1 ↔ Bluetooth Mesh 기능을
유지하면서 RPLIDAR C1 진단용 UART2를 추가한 별도 ESP-IDF 프로젝트다.
기본 Layer 8 파일은 수정하지 않는다.

## 하드웨어 계약

대상은 ESP32-S3-N16R8, ESP-IDF v5.5.5다.

| RPLIDAR C1 | ESP32-S3 | 역할 |
| --- | --- | --- |
| 빨강 VCC | 5V | 센서 전원(허용 4.8–5.2V) |
| GND | GND | 공통 접지 |
| 노랑 TX | GPIO13 | UART2 RX |
| 초록 RX | GPIO14 | UART2 TX |
| 도면상 선 없는 다섯 번째 접점 | 연결하지 않음 | C1 문서는 신호명/전기 특성을 정의하지 않음 |

C1 공식 XH2.54-5P 도면에는 VCC/TX/RX/GND 네 신호와 선 없는 접점 하나가
그려져 있다. 커넥터 방향을 추측해 핀 번호만으로 연결하지 말고 케이블 색과
데이터시트 그림을 함께 확인한다. C1 UART는 3.3V TTL, 460800 baud,
8N1이다. GPIO에 5V를 인가하지 않는다.
센서 기동 전류 대표값은 800mA이므로 실제 센서 입력 전압은 별도로 측정해야
한다.

## 지원 범위

- `lidar-status`: UART 설정, 요청/응답/수신 바이트·이벤트/timeout/error 카운터,
  GPIO13 현재 논리 레벨과 마지막 결과
- `lidar-info`: SLAMTEC `GET_INFO` (`A5 50`)
- `lidar-health`: SLAMTEC `GET_HEALTH` (`A5 52`)
- `lidar-rx-test`: GPIO13에 약한 풀다운을 잠깐 적용해 C1 TX의 UART 유휴 high
  구동 여부를 읽고 즉시 풀업으로 복원한다. 데이터선에 출력 전압을 인가하지 않는다.
- `help`: 전체 콘솔 명령 표시

전원 인가만으로 명령을 보내지 않으며 스캔 시작·모터 구동 명령은 구현하지
않았다. 따라서 이 펌웨어 빌드만으로 거리 데이터가 생성되지는 않는다.

## 검사와 빌드

```sh
cd firmware/esp32/layer8-ridar
bash test-host.sh

export IDF_PATH=/path/to/esp-idf-v5.5.5
source "$IDF_PATH/export.sh"
bash build.sh
```

빌드는 보드를 변경하지 않는다. Flash 대상은 USB serial
`14:C1:9F:CE:F0:D4`인 D6이며, Flash 전에 NVS/Mesh 상태를 확인하고
백업한다. `erase-flash`와 factory reset은 사용하지 않는다.

Flash 후 USB 콘솔에서 다음 순서로 확인한다.

```text
status
lidar-info
lidar-health
lidar-rx-test
lidar-status
```

합격 조건은 기존 Mesh 주소·AppKey·C001 Publication/Subscription 유지,
C1 INFO 응답, HEALTH 응답, timeout/parse error 없음이다. 기존 Layer 8 경로는
STM32 버튼 1회 → D6 UART_RX/MESH_TX → 76·B6 MESH_RX로 별도 확인한다.

## 설계

- UART1(GPIO17/18, 115200/8N1): 기존 Layer 8 STM32 ↔ Mesh 전용
- UART2(GPIO14 TX/GPIO13 RX, 460800/8N1): C1 전용
- UART2 driver ring buffer → 전용 parser task → 고정 크기 상태 snapshot
- 응답 payload는 최대 64바이트로 제한하며 무한 대기 없이 1초 timeout 처리

프로토콜 근거는 [SLAMTEC RPLIDAR protocol v2.2](https://bucket-download.slamtec.com/f010c72be308cdc618e91746d643278185ed02b2/LR001_SLAMTEC_rplidar_protocol_v2.2_en.pdf),
전기 규격은 [C1 datasheet v1.0](https://d229kd5ey79jzj.cloudfront.net/3157/SLAMTEC_rplidar_datasheet_C1_v1.0_en.pdf)를 사용했다.
