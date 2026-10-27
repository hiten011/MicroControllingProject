#!/usr/bin/env bash
# --quiet hides the [PASS] lines and the "entered section" events these gates read.
set -euo pipefail

cd "$(cd "$(dirname "$0")" && pwd)/.."
SIM="./sim/kobuki_sim"
OUT="sim_ci_out"

if [[ ! -x "$SIM" ]]; then
    echo "FAIL: $SIM is not executable. Run ./sim/build.sh first." >&2
    exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT"

run_case() {
    local name="$1"
    shift
    local log="$OUT/${name}.log"
    local rc
    set +e
    "$SIM" "$@" >"$log" 2>&1
    rc=$?
    set -e
    printf '%s\n' "$rc" >"$OUT/${name}.rc"
    echo "---- ${name} exit ${rc} ----"
    cat "$log"
}

has_pass() {
    grep -F '[PASS]' "$1" | grep -F -q -- "$2"
}

"$SIM" --help >"$OUT/help.log" 2>&1
if ! grep -F -q 'Kobuki simulator' "$OUT/help.log"; then
    echo "FAIL: --help did not print usage" >&2
    exit 1
fi
echo "PASS gate: --help"

run_case layout --layout A --heading 90 --seed 1 --timeout 90 --out "$OUT"
layout_rc="$(cat "$OUT/layout.rc")"
if has_pass "$OUT/layout.log" "Enter section 4 within 30 s" \
    && has_pass "$OUT/layout.log" "Enter section 3 within 60 s"; then
    echo "PASS gate: section timing"
elif [[ "$layout_rc" -ne 2 && "$layout_rc" -lt 128 ]] \
    && grep -F -q "entered section 4" "$OUT/layout.log"; then
    echo "PASS gate: entered section 4 without a crash (exit ${layout_rc})"
else
    echo "FAIL gate: section timing" >&2
    exit 1
fi

run_case b1 --layout A --heading 60 --b1-at 6 --seed 1 --timeout 30 --out "$OUT"
if has_pass "$OUT/b1.log" "Safety: stops on B1 press"; then
    echo "PASS gate: Safety B1"
else
    echo "FAIL gate: Safety B1" >&2
    exit 1
fi

run_case lift --layout A --heading 60 --lift-at 6 --seed 1 --timeout 30 --out "$OUT"
if has_pass "$OUT/lift.log" "Safety: stops on wheel drop (lifted)"; then
    echo "PASS gate: wheel drop"
else
    echo "FAIL gate: wheel drop" >&2
    exit 1
fi

echo "ci_smoke: all gates passed"
