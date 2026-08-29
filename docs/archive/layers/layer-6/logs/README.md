> 이관 원문: `layers/layer-6/logs/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../../getting-started/README.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Layer 6 workflow logs

`bootload.sh`, `bootload-pair.sh`, `bootload-triplet.sh`가 KST timestamp를 붙인
end-to-end log를 이 directory에 저장한다.

파일명:

- `esp32s3-layer-6-relay-node-single-YYYYMMDDTHHMMSS-KST.log`
- `esp32s3-layer-6-relay-node-pair-YYYYMMDDTHHMMSS-KST.log`
- `esp32s3-layer-6-relay-node-triplet-YYYYMMDDTHHMMSS-KST.log`

마지막 `RESULT`와 세부 evidence marker를 함께 확인한다. Pair PASS는 triplet
relay PASS를 뜻하지 않는다.
