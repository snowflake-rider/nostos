# 별도 실험과 참고 구현

현재 팀 펌웨어는 `firmware/`에 있습니다. 이 폴더의 코드는 자동으로 제품에 통합되지 않습니다.

- [Communication Module](communication-module/README.md): 이벤트·주기 처리 C API와 독립 호스트 검사.
- [ESP32-S3 GPS 노드](examples/esp32s3/gps-mesh-node/README.md): iPhone GPS 앱의 참고 노드.
- [ESP32-C3 예제](examples/esp32c3/generic-onoff-node/README.md): 하드웨어별 참고 구현.
- [STM32 출력 시험 변경](stm32-output-test/README.md): 원본의 미통합 변경을 패치로 보존.

학습용 Layer 0–7이나 이전 ESP32 배포본을 이곳에 다시 복사하지 않습니다. 필요하면 Git 이력에서 확인하고 현재 프로젝트에 필요한 기능만 추가합니다.
