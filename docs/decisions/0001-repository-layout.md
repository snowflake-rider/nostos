# 0001 — 제품 중심 저장소 구조

날짜: 2026-08-28. 상태: 사용자 승인 후 적용.

## 결정

- `firmware/`: STM32·ESP32 독립 배포 프로젝트. ESP32의 현재 기준은 Layer 8 하나.
- `apps/`: Mesh Console과 iPhone 앱.
- `libs/protocol/`: 실제 공통 메시지 계약. 복사본 없이 참조.
- `tools/`, `tests/integration/`: 공용 도구와 구성 요소 간 검사.
- `experiments/`: 미통합 모듈·참고 예제·추가 STM32 변경 패치.
- `docs/`: 시작 안내·아키텍처·하드웨어·검증·결정·보존 자료.

프로젝트 README·빌드 설정·단위 테스트는 코드 옆에 둡니다. SDK가 정한 STM32 `Core/Drivers/MyApp`, ESP-IDF `main/` 구조는 유지합니다. Nx 등 별도 모노레포 관리 도구는 도입하지 않습니다.

## 이유와 한계

빌드·배포 경계를 첫 화면에서 찾을 수 있도록 `code/` 한 겹을 제거합니다. 과거 버전은 Git 이력으로 조회하고, 현재 구현을 여러 경로에서 유지하지 않습니다. 폴더 이동으로 펌웨어 기능·핀·센서 기본값·Mesh 설정을 바꾸지 않습니다.

원본의 추가 STM32 변경은 단순 경로 수정이 아니므로 [별도 패치](../../experiments/stm32-output-test/README.md)로 보존했습니다. 현재 펌웨어에 적용하는 판단은 후속 기능 작업입니다.

원본 소스의 파일별 처리와 검증 결과는 [이관 기록](../archive/restructure/README.md)을 봅니다. 기존 manifest와 관찰 로그의 당시 경로·해시는 덮어쓰지 않습니다.

## 참고한 공식 구조

- [Nx monorepo folder structure](https://nx.dev/docs/kb/folder-structure): 실행 프로젝트·라이브러리 및 책임 범위 구분.
- [ESP-IDF build system](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/build-system.html): 프로젝트·컴포넌트와 `main`의 역할.
- [Zephyr example application](https://github.com/zephyrproject-rtos/example-application): 앱·라이브러리·도구·검사 경계의 참고 사례. Zephyr로 전환하는 결정은 아닙니다.
