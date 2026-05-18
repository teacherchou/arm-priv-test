#!/usr/bin/env bash
# run_qemu.sh — CI-friendly QEMU runner for arm-priv-test
#
# C-side test_print_summary() (common/test_framework.c) emits a
# "RESULT: PASS|FAIL" line at the end of every run.
#
# This wrapper just bounds the run with a timeout, tees output, looks at
# that final line, and translates it into an exit code:
#   0 = PASS, 1 = FAIL, 2 = TIMEOUT or RESULT line missing.
#
# When the run times out before the C summary prints, we synthesize a
# fallback "RESULT: TIMEOUT" line so the top-level Makefile
# aggregator still has something to parse.
#
# Usage:
#   run_qemu.sh --timeout 30 --label sysreg -- qemu-system-aarch64 ...

set -uo pipefail

timeout_sec=30
label="run"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --timeout) timeout_sec="$2"; shift 2 ;;
        --label)   label="$2";       shift 2 ;;
        --)        shift;            break  ;;
        *) echo "run_qemu.sh: unknown arg: $1" >&2; exit 64 ;;
    esac
done

if [[ $# -eq 0 ]]; then
    echo "run_qemu.sh: missing QEMU command after --" >&2
    exit 64
fi

log=$(mktemp -t arm-priv-test-"${label}".XXXXXX) || exit 64
trap 'rm -f "$log"' EXIT

# Portable timeout (macOS lacks `timeout` by default).
run_with_timeout() {
    if   command -v timeout  >/dev/null 2>&1; then
        timeout  --foreground --signal=TERM --kill-after=2 "${timeout_sec}" "$@"
    elif command -v gtimeout >/dev/null 2>&1; then
        gtimeout --foreground --signal=TERM --kill-after=2 "${timeout_sec}" "$@"
    else
        "$@" &
        local pid=$!
        ( sleep "${timeout_sec}" && kill -TERM "$pid" 2>/dev/null \
            && sleep 2 && kill -KILL "$pid" 2>/dev/null ) &
        local watchdog=$!
        local rc=0
        wait "$pid" 2>/dev/null || rc=$?
        kill "$watchdog" 2>/dev/null || true
        wait "$watchdog" 2>/dev/null || true
        return $rc
    fi
}

set +e
run_with_timeout "$@" 2>&1 | tee "$log"
set -e

# Look for the C-printed summary line: "RESULT: PASS|FAIL"
verdict=$(grep -oE 'RESULT: (PASS|FAIL)' "$log" | tail -n1 | awk '{print $2}' || true)

case "${verdict}" in
    PASS) exit 0 ;;
    FAIL) exit 1 ;;
    *)
        # Synthesize a fallback line so the top-level aggregator can still
        # see this extension contributed to the result.
        printf '\nRESULT: TIMEOUT\n'
        exit 2
        ;;
esac
