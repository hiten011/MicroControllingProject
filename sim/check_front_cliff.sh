#!/usr/bin/env bash
# Layout B run: each corridor must end on the front cliff, without falling off the ledge.
set -euo pipefail

cd "$(cd "$(dirname "$0")" && pwd)/.."
SIM="./sim/kobuki_sim"
OUT="sim_front_cliff_out"

if [[ ! -x "$SIM" ]]; then
    echo "FAIL: $SIM is not executable. Run ./sim/build.sh first." >&2
    exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT"

log="$OUT/layout_b.log"
"$SIM" --layout B --heading 90 --seed 1 --timeout 90 -v --out "$OUT" >"$log" 2>&1 || true

centre_cliffs="$(grep -c 'centre cliff sensor sees the ledge' "$log" || true)"
if [[ "$centre_cliffs" -lt 2 ]]; then
    echo "FAIL: expected a front cliff at the end of both corridors, saw ${centre_cliffs}" >&2
    cat "$log" >&2
    exit 1
fi
echo "PASS: centre cliff seen at the end of both corridors"

for check in "Enter section 3 within 60 s" "Stayed on the platform"; do
    if ! grep -F '[PASS]' "$log" | grep -F -q -- "$check"; then
        echo "FAIL: $check" >&2
        cat "$log" >&2
        exit 1
    fi
    echo "PASS: $check"
done
