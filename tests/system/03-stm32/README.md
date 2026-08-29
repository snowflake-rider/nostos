# 03. STM32 빌드

```sh
bash tests/system/03-stm32/run.sh
```

폴더 안에서는 `bash run.sh`. `--json`은 최종 JSON만 출력합니다.

STM32F411RE Debug와 Release를 각각 새 폴더에서 전체 컴파일·링크합니다. `-Wall -Werror` 적용, ELF와 메모리 사용량은 로그에 남습니다.

필요: CMake, Ninja, Arm GNU Toolchain. PATH 또는 설치된 `NOSTOS_TOOLCHAINS`를 사용합니다. Flash하지 않으며 센서 기본 플래그도 변경하지 않습니다. 링크 성공은 런타임 stack/heap 여유 보장이 아닙니다.

화면에는 요약만, 상세 로그·JSON은 실행마다 새 `build/test-results/` 폴더에 보존합니다.
[전체 검사 안내](../../README.md)

