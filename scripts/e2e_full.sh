#!/usr/bin/env bash
set -euo pipefail

PORT="${HTTP_PORT:-19092}"
BASE="http://127.0.0.1:${PORT}"

ENABLE_DEBUG_ROUTES=1 HTTP_PORT="${PORT}" ./bin/app &
APP_PID=$!
trap "kill $APP_PID || true" EXIT

for i in {1..30}; do
  if curl -fsS "$BASE/health" >/dev/null; then
    break
  fi
  sleep 0.5
done

curl -fsS "$BASE/health" >/dev/null
HTTP_BASE_URL="$BASE" SSE_SECONDS=2 ./bin/sse_hold_smoketest >/dev/null

TOKEN="$(curl -si -X POST "$BASE/login" -H 'Content-Type: application/x-www-form-urlencoded' --data 'username=admin&password=123456' | awk -F'[=;]' '/^Set-Cookie: token=/{print $2}')"
if [[ -z "${TOKEN}" ]]; then
  echo "ok=false"
  echo "error=login_failed"
  exit 1
fi

HAS_MYSQL="${MYSQL_HOST:-}"
if [[ -z "${HAS_MYSQL}" ]]; then
  echo "ok=true"
  echo "note=mysql not configured, skipping chat/interview e2e"
  exit 0
fi

SID="$(curl -fsS "$BASE/chat/session/new" -H "Cookie: token=$TOKEN" -H 'Content-Type: application/json' -d '{"title":"e2e"}' | python3 -c 'import sys,json; print(json.load(sys.stdin).get("sessionId",""))')"
if [[ -z "${SID}" ]]; then
  echo "ok=false"
  echo "error=create_session_failed"
  exit 1
fi

curl -fsS "$BASE/interview/jd/parse" -H "Cookie: token=$TOKEN" -H 'Content-Type: application/json' -d '{"jd":"We need C++ backend.","direction":"C++后端"}' >/dev/null

HAS_LLM="$(python3 - <<'PY'
import json, os, sys
p = os.path.join("config", "llm_profiles.json")
pt = os.path.join("config", "llm_profiles.json.template")
for fp in (p, pt):
    try:
        with open(fp, "r", encoding="utf-8") as f:
            j = json.load(f)
        for prof in j.get("profiles", []):
            if prof.get("api_key") and prof.get("base_url"):
                print("1")
                sys.exit(0)
    except Exception:
        pass
print("0")
PY
)"
if [[ "$HAS_LLM" == "1" ]]; then
  curl -N "$BASE/chat/send" -H "Cookie: token=$TOKEN" -H 'Content-Type: application/json' -d "{\"sessionId\":\"$SID\",\"question\":\"ping\"}" --max-time 40 | grep -m 1 -q '^event: done'
  curl -N -X POST "$BASE/interview/turn" -H "Cookie: token=$TOKEN" -H 'Content-Type: application/json' -d "{\"sessionId\":\"$SID\",\"jd\":\"We need C++ backend.\",\"resume\":\"I have C++ and MySQL.\",\"direction\":\"C++后端\",\"answer\":\"\"}" --max-time 40 | grep -m 1 -q '^event: done'
  curl -fsS -X POST "$BASE/interview/review" -H "Cookie: token=$TOKEN" -H 'Content-Type: application/json' -d "{\"sessionId\":\"$SID\",\"direction\":\"C++后端\"}" | python3 -c 'import sys,json; j=json.load(sys.stdin); assert j.get("ok") is True'
else
  echo "note=llm not configured, skipping chat/interview streaming"
fi

if [[ -n "${QDRANT_URL:-}" ]]; then
  ./bin/memory_index_smoketest >/dev/null
fi

echo "ok=true"
