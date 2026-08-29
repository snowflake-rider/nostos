> 이관 주의: 원본의 추가 STM32 시험·진단 명령은 미통합 출력 시험 패치를 전제로 할 수 있습니다. 현재 팀 펌웨어에 자동 반영된 기능으로 해석하지 않습니다. [검증 안내](index.md)를 먼저 확인하세요.

# 버튼·오디오 테스트 도구

진행/실물 판정은 [../archive/imported/testing/checklist.md](../archive/imported/testing/checklist.md)에 기록한다. 낙상·초음파 통합 시험은 별도다.
2026-08-28 최종 사용자 확인으로 버튼 1~4의 실제 MP3 청취·RGB·버저 동작은 모두 PASS다.
센서 통합과 반복 초기화 안정성은 별도 미확인 항목으로 남긴다.

## 호스트 회귀 테스트

```sh
sh /Users/kafka/Workspace_AI/esp-ble/tools/test-stm32-host.sh
```

cmake, ninja, C 컴파일러가 필요하다. ASan/UBSan을 켜서 기존 전체 호스트 테스트를 실행한다.
실제 GPIO/SPI 대신 모의 HAL을 사용하므로 빌드/호스트 PASS가 청취 PASS는 아니다.

## USB 관찰과 코덱 진단

pyserial이 있는 Python이 필요하다. 이 컴퓨터에서 검증한 실행기:
`/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python`.

```sh
# 기본은 읽기 전용 STM32 관찰. 버튼을 하나씩 누르고 최소 5초 간격을 둔다.
/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python /Users/kafka/Workspace_AI/esp-ble/tools/hardware/codec_diag.py --seconds 600

# 지금 코덱 레지스터 읽기. 보드/코덱 reset 없음.
/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python /Users/kafka/Workspace_AI/esp-ble/tools/hardware/codec_diag.py --command registers --seconds 5

# 코덱만 reset하고 SDI 명령 응답 검사. 마지막에 다시 초기화하며 소리는 내지 않는다.
/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python /Users/kafka/Workspace_AI/esp-ble/tools/hardware/codec_diag.py --command sdi --seconds 5

# 청취자가 준비된 후에만: 기존 VOL=5050에서 내장 시험음 1초, 종료 후 코덱 초기화.
/Users/kafka/.espressif/python_env/idf5.5_py3.14_env/bin/python /Users/kafka/Workspace_AI/esp-ble/tools/hardware/codec_diag.py --command sine --confirm-tone --seconds 5
```

`--command reset`은 코덱/진단 서비스만 초기화한다. MCU, USART1, ESP32, Mesh 키/설정은
초기화하지 않는다. `--mesh`는 D6/76도 함께 관찰하며 ESP32에 `status`만 질의한다.
지정한 보드가 USB 식별자로 확인되지 않으면 중단한다. USB 포트 이름으로 장치를 추정하지 않는다.
명령 전 STM32 진단 펌웨어 응답을 확인하며, 일반 펌웨어에 시험 명령을 보내지 않는다.

- DTR/RTS를 바꾸지 않는다. 스크립트 자체는 STM32 reset/flash를 하지 않는다.
- `r`/`t`/`s`는 현재 재생을 취소한다. 시험 중에는 버튼을 누르지 않는다.
- `s`는 1초 후 종료하며 stop 전송에 실패해도 XRST로 중단한다. 볼륨 자동 증가는 없다.
- 초기화가 실패하면 시험음을 시작하지 않는다.
- 새 `results/codec-.../`에 raw.jsonl, console.log, summary.json을 저장한다.
  `--out`은 **존재하지 않는 새 폴더**만 허용한다. 실행 중 해당 폴더에 `stop` 파일을 만들면 종료한다.
- 한 번에 한 관찰기만 실행한다. 종료 또는 예외 발생 시 열린 포트를 닫는다.
- 성공 판정은 출력된 `CODEC_INIT`/`SDI_TEST`/`SINE_START`/`SINE_STOP`의 상태를 읽는다.
  스크립트 exit=0은 기록 완료를 뜻한다. 실제 청취 결과는 자동 판정하지 않는다.

## 시험 순서 및 현재 상태

1. MODE=0800, 초기화 OK 확인.
2. `t` 검사에서 `SDI_TEST status=OK expected=0820 echo=0820` 확인.
3. `s`의 내장 시험음이 실제로 들리는지 확인.
4. 버튼 1→2→3→4의 색/버저/음성을 하나씩 확인하고 체크리스트 갱신.

위 순서는 문제 재발 시 사용할 진단 절차다. 최종 사용자 관찰에서는 네 버튼의 음성과
RGB가 모두 동작했고 버저 3·4번도 추가 확인됐다. 별도 SDI echo/사인 시험음은 통과로 간주하지 않으며, 진단 중 관찰된
간헐적 초기화 실패의 원인은 확정하지 않았다. 사용자 요청에 따라 추가 하드웨어 검사는 멈췄다.
SDI echo 검사는 MP3 바이트 수가 아니라 코덱이 받은 명령의 실행 결과를 비교한다.
[VS1003 공식 데이터시트 §9.8.1, §9.8.4](https://vlsi.fi/fileadmin/datasheets/vs1003.pdf)

## 빌드와 복구

현재 설치 이미지/소스 해시는 STM32 `build/codec-diagnosis._udglp4l/installed-test.elf`,
`installed-manifest.json`에 보존했다. 이전 시험 이미지와 전체 Flash 백업도 같은 폴더에 있다.
같은 수정 소스로 정상 기능을 복구할 때는 해당 폴더의 `normal/bike_swarm_guard.elf`를 사용한다.
기존 `build/Debug`는 이번 드라이버 수정 이전 이미지이므로 혼동하지 않는다.
**아직 정상 모드 복구는 실행하지 않았다.** Flash/부팅과 실제 출력 확인은 따로 기록해야 한다.
