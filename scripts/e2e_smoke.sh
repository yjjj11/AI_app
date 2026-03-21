#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:19091}"
SECONDS="${SECONDS:-2}"

curl -fsS "$BASE_URL/health" >/dev/null
HTTP_BASE_URL="$BASE_URL" SSE_SECONDS="$SECONDS" ./bin/sse_hold_smoketest >/dev/null

echo "ok=true"

