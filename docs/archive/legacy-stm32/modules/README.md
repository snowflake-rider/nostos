> 이관 원문: `stm32-project/modules/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Modules

이 폴더는 개별 센서나 통신 방식을 독립적으로 시험하기 위한 공간입니다. 실제 공용 펌웨어는 `integration/stm32`에서 관리하며, 검증이 끝난 기능은 그 프로젝트의 `MyApp` 계층에 병합합니다.

- `01-sensor-module`: 센서 기능 실험 자료
- `02-sensor-module`: 추가 센서 기능 실험 자료
- `03-communication`: USART 및 향후 통신 장치 실험 자료

각 폴더의 코드는 공용 펌웨어의 복사본으로 사용하지 않습니다.
