# STM32 버튼 출력·오디오 시험 변경 (미적용)

원본 `esp-ble-unorganized/stm32-project/integration/stm32`에는 팀 기준보다 추가된 버튼 출력 시험·USB 진단·오디오 드라이버 변경과 테스트가 있었습니다. 폴더 정리 중 현재 펌웨어 동작을 바꾸지 않도록 [changes.patch](changes.patch)에 15개 파일의 차이를 보존했습니다. 나머지 공통 파일은 `firmware/stm32/`를 사용합니다.

## 포함 내용

- `BUTTON_OUTPUT_TEST` 빌드 옵션과 `output_test.c/.h`.
- 버튼·오디오 및 HAL fake 변경, `test_output_test.c`, `test_vs1003b.c`.
- 원본 코드의 기능 변경은 유지하되 프로젝트명은 `nostos_stm32`, 공통 경로는 `libs/protocol`로 맞췄습니다.
- 패치는 이번 이관 직후의 `firmware/stm32`를 기준으로 합니다. 이후 코드가 변경되면 수동 병합이 필요할 수 있습니다.

## 확인과 적용

저장소 루트에서 **변경 없이 적용 가능 여부만 검사**:

```sh
git apply --check experiments/stm32-output-test/changes.patch
```

실제 적용은 별도 기능 작업에서 결정합니다. 위 검사는 펌웨어를 수정하거나 보드에 연결하지 않습니다. 폴더 정리에서는 임시 복사본에 패치를 적용해 호스트 테스트만 수행하며, 제품 폴더에는 적용하지 않습니다.

## 해당 원본의 시험 기록

- [버튼 출력 시험](../../docs/archive/imported/stm32-project/integration/stm32/BUTTON_OUTPUT_TEST.md)
- [후속 버튼·UART 시험](../../docs/archive/imported/stm32-project/integration/stm32/BUTTON_UART_TEST.md)
- [오디오 시험](../../docs/archive/imported/stm32-project/integration/stm32/VS1003B_AUDIO_TEST.md)

이 기록의 실물 성공은 당시 원본 소스·보드의 결과입니다. 현재 팀 펌웨어나 이번 패치 복사본의 실물 검증을 뜻하지 않습니다.
