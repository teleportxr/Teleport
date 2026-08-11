#!/usr/bin/env bash
# Integration test for the teleport service + CLI split.
# Usage: control_integration.sh /path/to/teleportd /path/to/teleport_cli
#
# Drives the real control socket: ping, multi-connection management, CLI exit
# without disconnecting the service, and remote shutdown.

set -u

DAEMON="$1"
CLI="$2"
PORT=10599
# The XXXXXX suffix is required by GNU coreutils' mktemp; BSD/macOS tolerates its
# absence, which is why this went unnoticed until the test was run on Linux.
LOG="$(mktemp -t teleportd_integration.XXXXXX)"
FAILURES=0

check() {
	# check <description> <expected-substring> <actual-output>
	if printf '%s' "$3" | grep -qF -- "$2"; then
		echo "ok: $1"
	else
		echo "FAIL: $1 — expected to find '$2' in:"
		printf '%s\n' "$3"
		FAILURES=$((FAILURES + 1))
	fi
}

"$DAEMON" -p "$PORT" >"$LOG" 2>&1 &
DAEMON_PID=$!

cleanup() {
	kill "$DAEMON_PID" 2>/dev/null
	rm -f "$LOG"
}
trap cleanup EXIT

# Wait for the control socket to come up.
READY=0
for _ in $(seq 1 50); do
	if "$CLI" -p "$PORT" -e ping >/dev/null 2>&1; then
		READY=1
		break
	fi
	sleep 0.1
done
if [ "$READY" -ne 1 ]; then
	echo "FAIL: service did not start listening; log follows"
	cat "$LOG"
	exit 1
fi

check "ping" "pong" "$("$CLI" -p "$PORT" -e ping)"
check "no connections initially" "no connections" "$("$CLI" -p "$PORT" -e connections)"
check "unselected status is an error" "ERROR" "$("$CLI" -p "$PORT" -e status 2>&1)"

OUT="$("$CLI" -p "$PORT" -e "connect 127.0.0.1:9; connect 10.255.255.1:9; connections")"
check "first connection gets id 1" "id 1" "$OUT"
check "second connection gets id 2" "id 2" "$OUT"
check "both connections listed" "teleport://127.0.0.1:9" "$OUT"
check "latest connection auto-selected" "* 2" "$OUT"

OUT="$("$CLI" -p "$PORT" -e "use 1; disconnect; connections")"
check "select then disconnect" "disconnected 1" "$OUT"
check "one connection left" "teleport://10.255.255.1:9" "$OUT"

# The CLI exiting must not disturb the service.
check "service survives CLI exit" "pong" "$("$CLI" -p "$PORT" -e ping)"

# Piped stdin works as batch input.
check "piped stdin" "pong" "$(printf 'ping\n' | "$CLI" -p "$PORT")"

# JSON mode: -j sends "format json" first, so every body is a single compact object.
# Checked with grep rather than jq, which CI runners are not guaranteed to have.
OUT="$("$CLI" -p "$PORT" -j -e "ping; connections")"
check "json ping" '{"pong":true}' "$OUT"
check "json connections is an object" '"connections":' "$OUT"
check "json connections keeps the surviving id" '"id":2' "$OUT"
check "json bodies are one line each" "2" "$(printf '%s\n' "$OUT" | wc -l | tr -d ' ')"

# Errors carry the message in both the status line and the body.
OUT="$("$CLI" -p "$PORT" -j -e frobnicate 2>&1)"
check "json error body" '"error":"unknown command: frobnicate"' "$OUT"
check "json error status line" "ERROR unknown command: frobnicate" "$OUT"

# Text is still the default: a client that never says "format" sees what it always did.
check "text mode is unchanged" "pong" "$("$CLI" -p "$PORT" -e ping)"

# Unknown commands error out with a non-zero exit status.
"$CLI" -p "$PORT" -e frobnicate >/dev/null 2>&1
if [ $? -eq 1 ]; then
	echo "ok: unknown command exits 1"
else
	echo "FAIL: unknown command should exit 1"
	FAILURES=$((FAILURES + 1))
fi

check "shutdown accepted" "shutting down" "$("$CLI" -p "$PORT" -e shutdown)"
sleep 1
if kill -0 "$DAEMON_PID" 2>/dev/null; then
	echo "FAIL: daemon still running after shutdown"
	FAILURES=$((FAILURES + 1))
else
	echo "ok: daemon exited on shutdown"
fi

if [ "$FAILURES" -ne 0 ]; then
	echo "--- daemon log ---"
	cat "$LOG"
	exit 1
fi
echo "control_integration: all checks passed"
