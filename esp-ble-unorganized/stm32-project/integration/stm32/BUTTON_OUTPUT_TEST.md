# 버튼·RGB·버저·VS1003B 통합 시험

2026-08-28. 현재 보드에는 **BUTTON_OUTPUT_TEST=ON 시험 이미지**가 설치되어 있다.
기준 핀: 워크스페이스 루트 `PIN_SETTINGS.md`. IOC/물리 핀 매핑은 변경하지 않았다.

## 최신 진단 상태

**2026-08-28 최종 사용자 확인: 버튼 1·2·3·4의 실제 MP3 청취·RGB·버저 동작 모두 PASS.**
버저 3·4번도 추가 사용자 확인을 받았다. 상세 판정은 워크스페이스
`testing/checklist.md`를 따른다. 사용자 확인 이후 추가 reset/재생/flash는 하지 않았다.

현재 설치본은 `build/codec-diagnosis._udglp4l/installed-test.elf`이다.
앞선 사용자 관찰: 버튼 1 RGB/버저/UP 음성 PASS, 버튼 2 RGB/버저 PASS·음성 안 들림.
이후 재부팅 관찰에서 버튼 1 전송이 2528바이트/DREQ LOW에서 멈추고 후속 요청이
BUSY_SKIPPED로 남는 결함을 확인했다. 실제 드라이버에 2초 타임아웃과 재초기화 시
스트림 취소를 추가했으며 ASan/UBSan 호스트 5/5, 기본/시험 MCU 빌드가 성공했다.
이번부터 기본 이미지도 드라이버 수정이 포함되므로 예전 바이너리와 동일하지 않다.

진단 중 MODE/STATUS=0000 초기화 실패와 정상 레지스터 응답이 모두 관찰됐다.
SDI echo 검사는 초기화 단계에서 중단했고 내장 사인 시험음은 실행하지 않았다.
이후 사용자의 최종 청취 확인으로 MP3 판정은 PASS로 갱신한다. 앞선 무음 원인은 미확정이다.
USART2 명령 `d`=레지스터, `r`=코덱 reset, `t`=SDI 응답 시험, `s`=내장 시험음 1초가
추가되었다. 핀/MP3/볼륨은 유지했다. 사용법과 실물 판정은 워크스페이스
`testing/README.md`, `testing/checklist.md`를 따른다.

아래 '확인된 단계'와 최초 빌드 경로는 **첫 시험 이미지의 이력**으로 보존한다.

## 시험 동작

| 버튼 | 실제 핀 | UART1 메시지 | RGB 2초 | 버저 | MP3 |
| --- | --- | --- | --- | --- | --- |
| 1 | D4/PB5 | UP `0x11` | 빨강 | 100ms ON, 100ms OFF, 100ms ON 후 OFF | speed_up_request |
| 2 | D6/PB10 | DOWN `0x10` | 초록 | 동일 | speed_down_request |
| 3 | D7/PA8 | SAFETY `0x12` | 파랑 | 동일 | cheer_up |
| 4 | D9/PC7 | STOP `0x13` | 흰색(R+G+B) | 동일 | stop_request |

색과 버저 패턴은 부품 시험용이며 제품의 메시지 의미/경고 규칙으로 변경한 것이 아니다.
현재 RGB/액티브 버저 드라이버는 HIGH=ON을 사용한다. 실제 색·소리는 사용자 관찰로 확인한다.
버튼을 하나씩 누르고 음성이 끝난 뒤 최소 5초 간격을 둔다. 기존 D10/PB6 보조 버튼도
STOP ID를 사용하므로 이번 시험에서는 누르지 않는다.

## 정상 기능과의 경계

- `BUTTON_OUTPUT_TEST` CMake 옵션 기본값은 OFF이다.
- 시험 모드에서만 MPU6050/초음파 자동 이벤트를 끄고 원격 수신은 출력하지 않고 버린다.
- 같은 버튼 드라이버(30ms), `message_router_publish_local`, UART1 1바이트 송신,
  MP3 서비스와 VS1003B/RGB/버저 드라이버를 사용한다.
- USART2/ST-LINK USB는 시험 텍스트 로그를 출력한다. 평소의 바이너리 송신 복사본과 혼합하지 않는다.
- 최초 시험 모드 추가 당시 기본 Debug Flash 바이너리는 변경 전과 바이트 단위 동일했다.
  최신 드라이버 수정본은 해당하지 않는다.
- 최초 시험에서는 MP3 드라이버를 수정하지 않았다. 최신본은 위 타임아웃/오류 처리가 추가됐다.

## 확인된 단계

- ASan/UBSan 호스트 테스트 **4/4 PASS**. 실제 버튼/RGB/버저 드라이버의
  디바운스, 색 매핑, 자동 종료, tick wrap과 진단 실패/재생 중/정체 표시를 확인했다.
  호스트의 audio/router는 모의 구현이므로 실제 MP3 전송 증거가 아니다.
- 기본/시험 MCU 빌드 성공. 기존 Flash 512KB 백업 후 시험 ELF 설치·verify·reset 성공.
- 새 부팅 USB: `OUTPUT_TEST_READY`, `STATUS audio=OK playing=0 pos=0 dreq=1`.
- 새 초기화 결과 RAM: MODE=`0x0800`, CLOCKF=`0x9800`, VOL=`0x5050`, status=OK.
  이전 시험의 MODE_MISMATCH와 달리 이번 초기화는 성공했다. 이전 실패 원인은 미확정이다.
- D6/76 Mesh 설정 유지 및 buffered=0 확인. STM32 설치/reset 구간의 D6 잡음·다른
  ID·noop/invalid 증가는 버튼 성공으로 세지 않는다. 이번에는 D6 reset을 하지 않았다.
- 최초 설치 시 사용자 관찰 대기 상태였다. 최신 버튼별 결과는 상단/체크리스트 참조.

`BUTTON ... rgb=...`는 출력 명령, `AUDIO_DATA_DONE`은 SPI 데이터 공급 완료다.
실제 발광·버저음·이어폰 음성 청취 성공을 대신하지 않는다. `BUSY_SKIPPED`는 새 음성
요청을 무시했다는 뜻이며, `BLOCKED`/`AUDIO_ERROR`/`AUDIO_STALLED`는 별도 조사 대상이다.

## 빌드·증거·복구

시험 빌드:

```sh
cmake --preset Debug -B build/button-output-test.v2v8jvp0/firmware -DBUTTON_OUTPUT_TEST=ON
cmake --build build/button-output-test.v2v8jvp0/firmware
```

원본: `build/button-output-test.v2v8jvp0/`.

- `before-flash.bin`: 설치 직전 전체 Flash 백업, 외부 공유/커밋하지 않는다.
- `before-build.elf`, `before.bin`, `normal.bin`: 기본 이미지 보존·비교.
- `installed-test.elf`, `installed-manifest.json`: 설치한 시험 이미지·소스 해시.
- `flash.log`, `runtime-init.json`: 설치 및 새 초기화 증거.
- `raw.jsonl`, `console.log`: STM32·D6·76 관찰 로그. 관찰기는 최대 600초 후 포트를 닫는다.

최신 드라이버 수정을 유지하면서 기본 동작으로 돌아갈 때는
`build/codec-diagnosis._udglp4l/normal/bike_swarm_guard.elf`를 설치한다.
`build/Debug`는 수정 전 이미지다. **현재는 기본 이미지로 복구하지 않은 시험 상태다.**
복구 reset 후 D6 수신 버퍼·Mesh 준비 상태도 다시 확인해야 한다.
