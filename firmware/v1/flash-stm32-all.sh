#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE="${SCRIPT_DIR}/stm32/build/Release/nostos_stm32.bin"
FLASH_ADDRESS="0x08000000"
EXPECTED_BYTES="137632"
EXPECTED_SHA256="d76589510f81d40f09ac5a31a373481dca448e919d495a7a267f6620eaaf91b0"

STLINK_SERIALS=(
  "066DFF485277504867161930"
  "066DFF505567494867071811"
  "066EFF3134584B3043121635"
)

DRY_RUN=0
VERIFY_ONLY=0
case "${1:-}" in
  --dry-run)
    DRY_RUN=1
    shift
    ;;
  --verify-only)
    VERIFY_ONLY=1
    shift
    ;;
esac
if [[ "$#" -ne 0 ]]; then
  echo "사용법: $0 [--dry-run|--verify-only]" >&2
  exit 2
fi

if [[ -x "/opt/homebrew/bin/st-flash" ]]; then
  ST_FLASH="/opt/homebrew/bin/st-flash"
elif command -v st-flash >/dev/null 2>&1; then
  ST_FLASH="$(command -v st-flash)"
else
  echo "오류: st-flash를 찾을 수 없습니다. macOS에서는 'brew install stlink'를 먼저 실행하세요." >&2
  exit 1
fi

sha256_file() {
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | awk '{print $1}'
  elif command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    echo "오류: shasum 또는 sha256sum이 필요합니다." >&2
    return 1
  fi
}

if [[ ! -f "$IMAGE" ]]; then
  echo "오류: STM32 v1 이미지가 없습니다: $IMAGE" >&2
  echo "먼저 실행: bash ${SCRIPT_DIR}/build.sh stm32" >&2
  exit 1
fi

ACTUAL_BYTES="$(wc -c < "$IMAGE" | tr -d '[:space:]')"
ACTUAL_SHA256="$(sha256_file "$IMAGE")"
if [[ "$ACTUAL_BYTES" != "$EXPECTED_BYTES" || "$ACTUAL_SHA256" != "$EXPECTED_SHA256" ]]; then
  echo "오류: 이미지가 검증된 v1 빌드와 다릅니다." >&2
  echo "  기대: ${EXPECTED_BYTES} bytes, ${EXPECTED_SHA256}" >&2
  echo "  현재: ${ACTUAL_BYTES} bytes, ${ACTUAL_SHA256}" >&2
  exit 1
fi

echo "STM32 v1 이미지 확인 완료"
echo "  파일: $IMAGE"
echo "  크기: ${ACTUAL_BYTES} bytes"
echo "  SHA-256: $ACTUAL_SHA256"
echo "  대상: ST-LINK 3대 (병렬)"

if [[ "$DRY_RUN" -eq 1 ]]; then
  for serial in "${STLINK_SERIALS[@]}"; do
    echo "[DRY-RUN] $serial: write -> read-back -> compare -> reset"
  done
  exit 0
fi

SUDO=()
if [[ "$(uname -s)" == "Darwin" && "$(id -u)" -ne 0 ]]; then
  echo
  echo "macOS USB 장치 접근을 위해 관리자 암호를 한 번 요청합니다. 입력 문자는 표시되지 않습니다."
  sudo -v
  SUDO=(sudo -n)
fi

TMP_ROOT="${TMPDIR:-/tmp}"
WORK_DIR="$(mktemp -d "${TMP_ROOT%/}/nostos-stm32-flash.XXXXXX")"
cleanup() {
  case "$WORK_DIR" in
    "${TMP_ROOT%/}"/nostos-stm32-flash.*)
      "${SUDO[@]}" find "$WORK_DIR" -type f -delete 2>/dev/null || true
      rmdir "$WORK_DIR" 2>/dev/null || true
      ;;
  esac
}
trap cleanup EXIT INT TERM

flash_one() {
  local serial="$1"
  local log_file="${WORK_DIR}/${serial}.log"
  local readback_file="${WORK_DIR}/${serial}.bin"

  (
    exec >"$log_file" 2>&1
    if [[ "$VERIFY_ONLY" -eq 1 ]]; then
      echo "[$serial] read-back 검증 시작"
    else
      echo "[$serial] Flash 시작"
    fi

    if [[ "$VERIFY_ONLY" -eq 0 ]]; then
      if ! "${SUDO[@]}" "$ST_FLASH" --serial "$serial" --reset write "$IMAGE" "$FLASH_ADDRESS"; then
        echo "[$serial] 오류: write 실패"
        exit 1
      fi
    fi

    if ! "${SUDO[@]}" "$ST_FLASH" --serial "$serial" read "$readback_file" "$FLASH_ADDRESS" "0x219A0"; then
      echo "[$serial] 오류: read-back 실패"
      "${SUDO[@]}" "$ST_FLASH" --serial "$serial" reset || true
      exit 1
    fi

    if ! "${SUDO[@]}" chown "$(id -u):$(id -g)" "$readback_file"; then
      echo "[$serial] 오류: read-back 파일 소유권 변경 실패"
      "${SUDO[@]}" "$ST_FLASH" --serial "$serial" reset || true
      exit 1
    fi

    if ! readback_sha256="$(sha256_file "$readback_file")"; then
      echo "[$serial] 오류: read-back SHA-256 계산 실패"
      "${SUDO[@]}" "$ST_FLASH" --serial "$serial" reset || true
      exit 1
    fi
    if [[ "$readback_sha256" != "$EXPECTED_SHA256" ]] || ! cmp -s "$IMAGE" "$readback_file"; then
      echo "[$serial] 오류: read-back 불일치 ($readback_sha256)"
      "${SUDO[@]}" "$ST_FLASH" --serial "$serial" reset || true
      exit 1
    fi

    if ! "${SUDO[@]}" "$ST_FLASH" --serial "$serial" reset; then
      echo "[$serial] 오류: 검증 후 reset 실패"
      exit 1
    fi

    if [[ "$VERIFY_ONLY" -eq 1 ]]; then
      echo "[$serial] 성공: read-back SHA-256 일치"
    else
      echo "[$serial] 성공: write/read-back SHA-256 일치"
    fi
  )
}

PIDS=()
for serial in "${STLINK_SERIALS[@]}"; do
  flash_one "$serial" &
  PIDS+=("$!")
done

FAILED=0
for index in "${!PIDS[@]}"; do
  serial="${STLINK_SERIALS[$index]}"
  if wait "${PIDS[$index]}"; then
    cat "${WORK_DIR}/${serial}.log"
  else
    FAILED=1
    cat "${WORK_DIR}/${serial}.log" >&2
  fi
done

if [[ "$FAILED" -ne 0 ]]; then
  echo >&2
  echo "실패: 위 로그의 보드를 확인하세요. 실패한 보드는 자동 재시도하지 않았습니다." >&2
  exit 1
fi

echo
if [[ "$VERIFY_ONLY" -eq 1 ]]; then
  echo "완료: STM32 3대 모두 v1 byte-for-byte read-back 검증과 reset에 성공했습니다."
else
  echo "완료: STM32 3대 모두 v1 Flash 및 byte-for-byte read-back 검증에 성공했습니다."
fi
