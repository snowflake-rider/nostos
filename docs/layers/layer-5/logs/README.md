> 이관 원문: `layers/layer-5/logs/README.md`. 현재 실행 경로는 [팀원 시작 안내](../../../00-team/START.md)를 따른다. 아래의 과거 명칭·명령·검증 범위는 기록 당시 내용이다.

# Layer 5 logs

`bootload.sh`와 `bootload-pair.sh`가 생성하는 실행 기록을 저장한다.

- one-board: `esp32s3-layer-5-packet-node-YYYYMMDDTHHMMSS-KST.log`
- pair: `esp32s3-layer-5-packet-node-pair-YYYYMMDDTHHMMSS-KST.log`
- optional Mac scan: `esp32s3-layer-5-over-air-adv-YYYYMMDDTHHMMSS-KST.log`

Build, Flash, local runtime, `PACKET_TX`, 실제 상대 `PACKET_RX_NEW`는 서로
다른 검증 단계다. 한 보드 로그는 양방향 packet 수신 증거가 아니다.
