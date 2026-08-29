# NOSTOS 소개 영상

NOSTOS의 현재 아키텍처를 약 35초로 설명하는 무음 Remotion 영상입니다.

## 내용 근거

- 저장소 루트 `README.md`
- `docs/architecture/message-protocol/04-board-roles.md`
- `docs/getting-started/README.md`
- `firmware/stm32/README.md`, `firmware/esp32/README.md`
- `apps/mesh-console/README.md`, `apps/ios-gps-mesh/README.md`

영상은 실물 검증을 새로 주장하지 않습니다. 화면·호스트 테스트와 실제 무선 수신을 구분한다는 프로젝트 원칙을 마지막 장면에 반영했습니다.

## 실행

```sh
npm install
npm run dev
npm run lint
npm run render
```

렌더 결과는 `out/nostos-intro.mp4`입니다.
