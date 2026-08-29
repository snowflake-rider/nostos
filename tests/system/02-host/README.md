# 02. 코드 회귀

```sh
bash tests/system/02-host/run.sh
```

폴더 안에서는 `bash run.sh`. `--json`은 최종 JSON만 출력합니다.

기존 C/Python 회귀와 메시지 프로토콜/출력 모의 검사를 실행합니다. Debug·Release·ASan/UBSan 구성, 큐·코덱·파서·검사 도구 실패 경로를 확인합니다.

필요: Python 3, C 컴파일러, CMake, Make 또는 Ninja. PASS는 보드 없는 모의 검사 결과이며 실제 UART·센서·무선 성공이 아닙니다.

화면에는 요약만, 상세 로그·JSON은 실행마다 새 `build/test-results/` 폴더에 보존합니다.
[전체 검사 안내](../../README.md)

