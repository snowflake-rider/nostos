> 이관 원문: `stm32-project/PROJECT_STRUCTURE.md`. 현재 실행 경로는 [팀원 시작 안내](../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# 프로젝트 폴더 구조

## 기준 원칙

- `integration/stm32`가 세 팀원이 함께 사용하는 공용 STM32 펌웨어입니다.
- 핀과 주변장치 설정은 공용 `bike_swarm_guard.ioc`에서 관리합니다.
- `modules`는 개별 기능 실험이나 향후 통신 장치 연구 자료를 보관합니다.
- 빌드 결과물과 개인 IDE 설정은 Git에 포함하지 않습니다.

## 전체 구조

```text
stm32-project/
├─ README.md
├─ CUBEMX_SETUP.md
├─ PROJECT_STRUCTURE.md
├─ common/                       # 공통 규칙과 배선 문서
├─ modules/                      # 개별 기능 실험 공간
└─ integration/
   ├─ README.md                  # 통합 시험 기록
   └─ stm32/                     # 공용 CubeMX/CMake 프로젝트
      ├─ Core/
      ├─ Drivers/
      ├─ MyApp/
      ├─ cmake/
      ├─ tools/
      ├─ bike_swarm_guard.ioc
      ├─ CMakeLists.txt
      ├─ CMakePresets.json
      ├─ STM32F411xx_FLASH.ld
      └─ startup_stm32f411xe.s
```

## MyApp 계층

| 위치 | 역할 |
|---|---|
| `MyApp/ap` | 초기화와 super-loop 실행 |
| `MyApp/common` | 기능 플래그와 공통 메시지 ID |
| `MyApp/hw` | GPIO 및 하드웨어 드라이버 |
| `MyApp/service` | 안전 판단, 메시지 라우팅, 출력 정책 |
| `MyApp/audio` | MP3 원본과 펌웨어용 C 배열 |

센서와 버튼은 `message_type_t` 메시지를 생성합니다. 로컬 메시지는 자기 보드에서 처리한 뒤 USART로 한 번 전송하고, USART 수신 메시지는 자기 보드에서만 처리하여 무한 재전송을 방지합니다.

## Git에 포함하는 파일

- `.ioc`, `Core`, `Drivers`, `MyApp`
- CMake 설정, 시작 파일, 링커 스크립트
- 음원 원본과 변환된 C/H 파일
- 배선, 빌드 및 시험 문서

## Git에서 제외하는 파일

- `build`, `Debug`, `Release`
- `.elf`, `.hex`, `.bin`, `.map`
- `.settings`, `.vscode`, `.mxproject`
- 개인 임시 파일
