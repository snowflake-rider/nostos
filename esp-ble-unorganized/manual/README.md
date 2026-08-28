# Manuals

실물 보드와 앱을 사용해 작업을 재현하는 절차를 모아 둔 디렉터리다.

## Bluetooth Mesh

- [nRF Mesh로 세 ESP32-S3 Mesh network 만들기](how-to-make-mesh-network.md)

이 가이드는 다음 순서를 포함한다.

1. 세 보드에 Layer 7 firmware 설치
2. iPhone nRF Mesh에서 network 생성
3. 세 Node Provisioning
4. 하나의 AppKey를 세 Node에 Add
5. Generic OnOff Server와 Client를 같은 AppKey에 Bind
6. Server Subscription과 Client Publication을 group `0xC000`으로 설정
7. Group OnOff와 Proxy 연결 확인
8. Relay OFF/ON 통제 실험

현재 세 Node의 Provisioning, Model configuration, group OnOff는 확인했다. 실내 거리에서는 직접 경로가 계속 살아 있어 controlled Relay OFF/ON 비교는 보류했으며, 더 넓은 장소에서 같은 위치 조건으로 다시 검증한다.
