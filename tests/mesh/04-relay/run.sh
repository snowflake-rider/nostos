#!/usr/bin/env bash
set -euo pipefail
export MESH_REPEAT_HUMAN=1

if [[ $# -ne 1 || "${1:-}" != '--send' ]]; then
  printf '%s\n' '사용법: bash run.sh --send  (D6 → B6 중계 → 76, OFF/ON/OFF 각 20회)' \
    'README의 배치 조건을 먼저 확인하세요. Relay는 직접 변경하고 매 구간 확인합니다.'
  [[ $# -eq 1 && "$1" == '--help' ]] && exit 0
  exit 2
fi

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
RUNNER="$ROOT_DIR/tools/hardware/run_mesh_repeat.sh"
trap 'printf "\n중단: 추가 송신과 설정 자동 복원은 하지 않습니다.\n"; exit 130' INT
trap 'printf "\n종료: 추가 송신과 설정 자동 복원은 하지 않습니다.\n"; exit 143' TERM

confirm() {
  local answer
  printf '%s\n' "$1"
  printf '직접 확인했으면 yes, 취소하려면 그 외 입력: '
  if ! IFS= read -r answer || [[ "$answer" != 'yes' ]]; then
    printf '%s\n' '취소: 추가 송신하지 않습니다. 설정은 자동 복원하지 않습니다.'
    exit 2
  fi
}

# Only GETs before the operator confirms topology and the first Relay state.
bash "$RUNNER" check --source D6 --peers 76 --relay B6
printf '%s\n' 'D6↔76 직접 경로 제한, B6는 양쪽 도달, D6·76/다른 Relay OFF, 다른 송신 중지.'
printf '고정 배치/거리/출력/TTL 메모 (키·개인 위치 제외): '
if ! IFS= read -r conditions || [[ -z "${conditions//[[:space:]]/}" ]]; then
  printf '%s\n' '배치 메모가 없어 중단합니다.'
  exit 2
fi
confirm '위 배치 조건, 76의 C000 Server Bind/Subscription, 세 USB 연결을 확인했나요?'

mkdir -p "$ROOT_DIR/build/hardware-results"
EVIDENCE_DIR="$(mktemp -d "$ROOT_DIR/build/hardware-results/mesh-relay-XXXXXX")"
printf '결과 폴더: %s\n' "$EVIDENCE_DIR"

for phase in off-1 on off-2; do
  relay_phase=relay-off
  relay_label=OFF
  if [[ "$phase" == 'on' ]]; then
    relay_phase=relay-on
    relay_label='ON (3 transmissions / 20 ms)'
  fi
  confirm "[$phase] nRF Mesh에서 B6 Relay를 $relay_label 로 설정·재조회하고, 앱 조작을 멈췄나요? 배치는 그대로 유지하세요."
  bash "$RUNNER" run --send --source D6 --peers 76 --relay B6 \
    --phase "$relay_phase" --confirm-isolated-topology --conditions "$conditions" \
    --count 20 --interval 5 --window 3 --out "$EVIDENCE_DIR/$phase"
  # The engine can return 0 for Ctrl+C; do not advance an incomplete run.
  if ! python3 -B -c 'import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    r = json.load(f)
sys.exit(0 if r.get("stop") == "COMPLETED" and r.get("completed") == 20
         and r.get("ambiguous") == 0 and not r.get("error") else 2)' "$EVIDENCE_DIR/$phase/summary.json"; then
    printf '%s\n' '구간이 완전히 끝나지 않아 중단합니다. 기존 결과를 보존하며 다음 구간은 송신하지 않습니다.'
    exit 2
  fi
done

result=0
bash "$RUNNER" compare "$EVIDENCE_DIR/off-1/summary.json" \
  "$EVIDENCE_DIR/on/summary.json" "$EVIDENCE_DIR/off-2/summary.json" \
  > "$EVIDENCE_DIR/comparison.json" || result=$?
python3 -B -c 'import json, sys
with open(sys.argv[1], encoding="utf-8") as f: r = json.load(f)
print(r["verdict"] + (" — Relay 효과 관찰; 정확한 경로 증명 아님" if r["verdict"] == "RELAY_EFFECT_OBSERVED" else " — 결론 보류; 비교 조건/누락 사유 확인"))
for issue in r.get("issues", []): print("  확인: " + issue)' "$EVIDENCE_DIR/comparison.json"
printf '비교 결과: %s/comparison.json\nRelay/ON-OFF 상태는 자동 복원하지 않습니다.\n' "$EVIDENCE_DIR"
exit "$result"
