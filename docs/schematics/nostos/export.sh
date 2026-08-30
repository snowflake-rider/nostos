#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SCHEMATIC="$PROJECT_DIR/nostos-wiring.kicad_sch"
EXPORT_DIR="$PROJECT_DIR/exports"
REPORT_DIR="$PROJECT_DIR/reports"

if [[ -n "${KICAD_CLI:-}" && -x "$KICAD_CLI" ]]; then
  KICAD_CLI_BIN="$KICAD_CLI"
elif command -v kicad-cli >/dev/null 2>&1; then
  KICAD_CLI_BIN="$(command -v kicad-cli)"
elif [[ -x "${HOME}/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli" ]]; then
  KICAD_CLI_BIN="${HOME}/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
elif [[ -x "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli" ]]; then
  KICAD_CLI_BIN="/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
else
  echo "kicad-cli를 찾을 수 없습니다." >&2
  exit 1
fi

mkdir -p "$EXPORT_DIR" "$REPORT_DIR"

"$KICAD_CLI_BIN" sch erc \
  --output "$REPORT_DIR/erc.rpt" \
  --severity-all \
  --exit-code-violations \
  "$SCHEMATIC"

"$KICAD_CLI_BIN" sch export netlist \
  --output "$REPORT_DIR/nostos-wiring.net" \
  "$SCHEMATIC"

"$KICAD_CLI_BIN" sch export svg \
  --output "$EXPORT_DIR" \
  "$SCHEMATIC"

"$KICAD_CLI_BIN" sch export pdf \
  --output "$EXPORT_DIR/nostos-wiring.pdf" \
  "$SCHEMATIC"

if command -v pdftocairo >/dev/null 2>&1; then
  pdftocairo -png -singlefile -r 200 \
    "$EXPORT_DIR/nostos-wiring.pdf" \
    "$EXPORT_DIR/nostos-wiring"
elif command -v sips >/dev/null 2>&1; then
  sips -s format png \
    "$EXPORT_DIR/nostos-wiring.svg" \
    --out "$EXPORT_DIR/nostos-wiring.png" >/dev/null
else
  echo "PNG 변환 도구가 없어 SVG/PDF까지만 생성했습니다." >&2
fi

printf 'KiCad CLI: %s\n' "$("$KICAD_CLI_BIN" version)"
printf 'ERC: %s\n' "$REPORT_DIR/erc.rpt"
printf 'Exports: %s\n' "$EXPORT_DIR"
