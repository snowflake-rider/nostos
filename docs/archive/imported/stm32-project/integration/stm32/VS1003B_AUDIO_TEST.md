> 보존 원문: `esp-ble-unorganized/stm32-project/integration/stm32/VS1003B_AUDIO_TEST.md`. 당시 경로·명령·검증 결과이며 현재 실행 안내는 저장소 README를 따릅니다.

# 버튼 → VS1003B MP3 재생 시험

## 최신 상태: 버튼 1~4 실제 MP3 청취 PASS

2026-08-28 사용자 최종 확인: "이제 됏어 다 들려. 1,2,3,4 다 rgb 됐어."
네 버튼의 실제 음성 및 RGB 동작을 PASS로 기록했고 버저 3·4번도 추가 확인을 받았다.
버튼 1~4의 RGB·버저·MP3가 모두 PASS다. 로그의 바이트 공급 완료가 아니라
사용자 청취 결과에 따른 판정이다. `testing/checklist.md`에 반영했으며 추가 하드웨어 검사는
멈췄다. 앞선 실패 원인, 반복 초기화 안정성, 별도 SDI echo/사인 진단은 미확정/미완료로 유지한다.

## 앞선 진단 경과

통합 출력 첫 시험에서는 MODE=0800/CLOCKF=9800/VOL=5050으로 초기화됐고,
사용자가 버튼 1의 음성을 PASS로 확인했다. 버튼 2는 RGB/버저만 되고 음성은 안 들렸다.
이후 DREQ LOW 무한 BUSY 결함을 재현/수정하고 진단 로그를 추가했다.
설치 후 일부 재확인에서는 **MODE/STATUS 등 SCI 응답이 모두 0000**이었다.
코덱만 재초기화해도 MODE_MISMATCH여서 SDI/내장 시험음 단계로 진행하지 못했다.
이후 정상 SCI 값과 서비스 audio=OK도 관찰됐다. 원인은 미확정이며 당시에는
청취 성공으로 처리하지 않았다. 최종 사용자 PASS는 위 최신 상태를 따른다. 초기 실패 증거는
`build/codec-diagnosis._udglp4l/boot-stm32/`, `sdi-test/`, `sdi-recheck/`이다.

진행 기록은 [버튼·RGB·버저 통합 시험](BUTTON_OUTPUT_TEST.md)을 따른다.
아래 MODE_MISMATCH는 앞선 관찰 기록으로 보존하며, 실패의 원인이 확정된 것은 아니다.

## 앞선 판정 (2026-08-28)

**초기화 실패 확인. 실제 음성 재생은 미검증.**

기존 펌웨어에는 버튼 → 메시지 → 로컬 오디오 재생 경로가 이미 있다.
이번 단계에서는 소스·IOC·Flash를 변경하거나 reset을 요청하지 않고 상태를 읽었다.
실제 Flash 126280바이트를 읽어 현재 Debug ELF의 바이너리와 일치함을 확인한 뒤,
해당 ELF의 심볼 주소로 RAM을 해석했다.

| STM32에 저장된 디버그 항목 | 관찰값 |
| --- | --- |
| `vs1003b_debug_status` | 4 = `VS1003B_STATUS_MODE_MISMATCH` |
| `vs1003b_debug_mode` | `0x0000` |
| `vs1003b_debug_clockf` / `vs1003b_debug_volume` | `0x0000` / `0x0000` |
| `vs1003b_debug_audio_playing` / position | false / 0 |
| 드라이버 play_size / play_position | 0 / 0 |

위 MODE 값은 **부팅 초기화 때 읽어 저장한 값**이며, 이번에 VS1003B의 SCI를
직접 다시 읽은 결과가 아니다. CLOCKF/VOL의 0 역시 초기화가 중단된 뒤 남은
디버그 변수의 초기값이다. 실제 칩의 현재 레지스터값이라고 해석하지 않는다.

`vs1003b_init()`은 SCI_MODE=0x0800을 기대한다. 불일치하면
`message_service_play_audio()`와 `message_service_process()`의 OK 조건을
통과하지 못하므로 버튼 메시지가 생성되어도 MP3 전송은 시작되지 않는다.

GPIOC IDR=0x20B0에서 DREQ/PC4는 HIGH였다. 입력 핀 한 번의 판독만으로
VS1003B 연결이나 정상 응답을 입증할 수 없다. 전원·신호 배선·접점·연결 시점은
아직 사용자 확인이 필요하며, 초기화 실패의 물리 원인은 확정하지 않았다.

## 현재 코드의 연결

| VS1003B 신호 | STM32 핀 |
| --- | --- |
| SCK | PB13, SPI2 |
| SO / MISO | PB14, SPI2 |
| SI / MOSI | PB15, SPI2 |
| XCS / CS | PB12 |
| XDCS / DCS | PC5 |
| DREQ | PC4 |
| XRESET / RST | PB1 |

모듈 전원 입력 전압과 오디오 출력 회로는 모듈 실물 확인 후 판단한다.
칩의 GBUF는 일반 GND가 아니며 GND에 연결하면 안 된다.
[VS1003 공식 데이터시트 §6, §8.6.1](https://vlsi.fi/fileadmin/datasheets/vs1003.pdf)

## 음원·버튼 매핑 검사

| 버튼 | 메시지 | 파일 | C 배열 크기 | ffprobe 길이 |
| --- | --- | --- | ---: | ---: |
| 1 D4/PB5 | UP 0x11 | speed_up_request.mp3 | 19917 bytes | 2.350063 s |
| 2 D6/PB10 | DOWN 0x10 | speed_down_request.mp3 | 19341 bytes | 2.300063 s |
| 3 D7/PA8 | SAFETY 0x12 | cheer_up.mp3 | 20205 bytes | 2.400063 s |
| 4 D9/PC7 | STOP 0x13 | stop_request.mp3 | 16749 bytes | 1.950063 s |

네 원본 MP3는 각각 펌웨어 C 배열과 바이트 단위로 일치한다. 모두 ffprobe에서
MP3 / 16000 Hz / mono / 64 kbit/s로 식별됐고 ffmpeg 디코딩 검사 exit=0이다.
이는 PC 파일 검사이며 VS1003B의 실제 디코딩·이어폰 출력을 검증한 것은 아니다.
특히 버튼 3의 실제 음성 문구가 안전운전 알림에 맞는지는 청취로 확인해야 한다.

## 다음 실물 시험

1. 모듈 전원·7개 신호·공통 GND와 출력 단자 연결 확인.
2. 초기화 재시도 후 MODE/CLOCKF/VOL 읽기 성공 확인.
3. 재생 중인 음원이 끝난 뒤 버튼을 하나씩 눌러 데이터 진행·완료와 실제 음성을 대응.
   현재 서비스는 이미 재생 중인 경우 새 요청을 무시하므로 빠르게 연속 누르지 않는다.
4. 사용자 청취 확인을 받아 버튼별 성공 여부 기록.

원본 자료: `build/button-audio-test.u0a0lalg/`의 `runtime-before.log`,
`runtime-before.json`, `flash-readback.log`, `firmware-identity.txt`.
