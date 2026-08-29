# 통합 경로의 모의 검사

이 폴더는 **개발자용**입니다. 실제 ESP32 시험은 [단계별 시험 폴더](../mesh/README.md)에서 시작하세요.

- `test_message_broadcast.py`: STM32 → ESP32 → Mesh 수신 로그 판정기.
- `test_mesh_repeat.py`: C000 반복 송수신·Relay 비교 판정기.
- `test_mesh_stages.py`: 단계별 실행 스크립트의 옵션·취소·진행 순서.

모두 합성 데이터/가짜 실행기를 사용합니다. 실제 보드나 Console에 접속하지 않습니다.

```sh
python3 -m unittest discover -s tests/integration -v
```

프로젝트별 단위 테스트는 `firmware/*/host-tests`, `libs/protocol/tests`, 각 앱과 실험 모듈 옆에 둡니다. 전체 호스트 회귀 검사는 `bash tools/test-host.sh`입니다.
