#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ! -x .venv/bin/python ]]; then
  python3 -m venv .venv
  .venv/bin/python -m pip install -r requirements-lock.txt
fi
if [[ ! -d node_modules ]]; then npm ci; fi
npm run build
printf '\nMesh Console: http://127.0.0.1:8787\nUSB 포트는 연결 버튼을 누를 때만 엽니다. 종료: Ctrl+C\n\n'
exec .venv/bin/python -m uvicorn server.app:app --host 127.0.0.1 --port 8787 --no-access-log
