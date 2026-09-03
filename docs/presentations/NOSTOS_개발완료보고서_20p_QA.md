# NOSTOS 개발완료보고서 20P QA

검수 시각: 2026-09-03 11:58 KST<br>
Authoritative artifact: [NOSTOS - 제출용](https://docs.google.com/presentation/d/1BzE6PMaCnnMmrZA7b4I_Thv9GezRC08fBhCpag5Z-Gc/edit)<br>
Google Slides revision: `8m98I3vgy9P-Pw`<br>
Google Drive last modified: `2026-09-03 11:57:54 KST`<br>
Rollback checkpoint: `Before 20P outline redesign`

## 결과 요약

| 검사 | 결과 |
| --- | --- |
| Google Slides page count | PASS — 20 pages |
| 왼쪽 footer AI 표기 | PASS — `Provenance`, `Verification`, `Status`, host/physical check 문구 모두 제거 |
| 공식 목차 배분 | PASS — 3/10/5/2 |
| PDF page count / size | PASS — 20 pages, 720×405 pt |
| 페이지 번호 | PASS — `01/20`…`20/20`, leading digit clipping 없음 |
| 전체 contact sheet | PASS — 겹침·잘림·빈 slide 없음 |
| PPTX overflow test | PASS — `slides_test.py` reported no overflow |
| N03 hyperlinks | PASS — GitHub repository, ride-signals demo |
| N20 hyperlinks | PASS — GitHub repository, YouTube preliminary demo |
| N07 architecture | PASS — Rider A/B, Mesh, STATE/STOP/ACK lane 식별 가능 |
| N09 OLED | PASS — 2:1 frame, speed/distance/temp/humidity/message/link state 표시 |
| N11 STOP/ACK | PASS — coral request가 peer STM32 acceptance gate로 들어가고 navy ACK 반환 |
| Font export | PASS with approved fallback — Arial/Roboto Mono + Google Korean fallback; glyph 깨짐 없음 |
| Physical 3-node E2E claim | PASS — footer 표기는 제거했지만 본문과 evidence snapshot에 미수행 경계를 유지 |

## Evidence snapshot

- Git commit: `fbd06fea1fd918543cdc28c0b0b96b308d54abf8`
- Snapshot time: `2026-09-03 11:41:54 KST`
- Hardware flash/provisioning/physical test: 수행하지 않음

| Slide/claim | Verification token | Source / exact command | Result artifact |
| --- | --- | --- | --- |
| N06, N08, N09, N10, N15 — STM32 service/algorithm host behavior | `HOST CHECK VERIFIED` | `bash firmware/tools/fw check stm32` | 12/12 PASS; `firmware/out/host-tests/stm32/fast`; `HARDWARE_AND_BICYCLE=NOT_TESTED` |
| N06, N07, N11, N12, N16 — ESP32 runtime/queue policy | `HOST CHECK VERIFIED` | `bash firmware/tools/fw check esp32` | policy PASS, 3/3 PASS; `firmware/out/host-tests/esp32/fast` |
| N04, N06, N07, N11, N16 — shared application protocol | `HOST CHECK VERIFIED` | `bash firmware/tools/fw check protocol` | 1/1 PASS; `firmware/out/host-tests/protocol/fast` |
| N07, N11, N18 — three-node STOP path | `PHYSICAL E2E PENDING` | not run | physical sensor/RF/output chain remains unverified |

## 구조 및 링크 검수

Google Slides API readback에서 다음 20개 slide object가 순서대로 존재하며, 기존 slide 20개는 rollback checkpoint 후 제거했다.

1. `g3fb1d285c14_1_5`
2. `g3fb1d285c14_1_39`
3. `g3fb1d285c14_1_70`
4. `g3fb1d285c14_1_103`
5. `g3fb1d285c14_1_136`
6. `g3fb1d285c14_1_167`
7. `g3fb1d285c14_1_209`
8. `g3fb1d285c14_1_263`
9. `g3fb1d285c14_1_297`
10. `g3fb1d285c14_1_335`
11. `g3fb1d285c14_1_371`
12. `g3fb1d285c14_1_404`
13. `g3fb1d285c14_1_444`
14. `g3fb1d285c14_1_477`
15. `g3fb1d285c14_1_506`
16. `g3fb1d285c14_1_532`
17. `g3fb1d285c14_1_569`
18. `g3fb1d285c14_1_596`
19. `g3fb1d285c14_1_630`
20. `g3fb1d285c14_1_670`

Verified hyperlink targets:

- `https://github.com/snowflake-rider/nostos`
- `https://github.com/snowflake-rider/nostos/blob/main/docs/media/nostos-essence/public/video/nostos-ride-signals.mp4`
- `https://www.youtube.com/watch?v=r-7fjGOm1hw&feature=youtu.be`

## 배포 산출물

- Google Slides: `https://docs.google.com/presentation/d/1BzE6PMaCnnMmrZA7b4I_Thv9GezRC08fBhCpag5Z-Gc/edit`
- PDF: `docs/presentations/NOSTOS_개발완료보고서_최종_표지포함20p.pdf`
- Reproducible source: `docs/presentations/build_nostos_20p.mjs`
- Import artifact: `docs/presentations/NOSTOS_개발완료보고서_20p_import-20260903-codex.pptx`
- PDF SHA-256: `93ee9352ea5ba8e1362585cd539e1acd292932014cba58fdffe48c5d17714484`

## 남은 증거 경계

- STM32/ESP32/protocol host checks는 실제 자전거 장착, RF 전달, peer output 완료를 증명하지 않는다.
- 3-node physical STOP E2E가 새로 수행되기 전까지 N07/N11/N18의 `PENDING` 표기를 유지한다.
- 확정 팀원명이 제공되면 N20의 세 역할 범주에 이름을 추가할 수 있으나, 이번 deck에서는 이름을 추정하지 않았다.
