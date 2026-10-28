#!/bin/bash
set -euo pipefail
B="$1"; N="$2"
fail=0
for f in "$B"/*.txt; do
  name=$(basename "$f")
  if [[ ! -f "$N/$name" ]]; then echo "MISSING $name"; fail=1; continue; fi
  if ! diff -u "$f" "$N/$name"; then
    echo "DIFFER $name"
    fail=1
  else
    echo "MATCH $name"
  fi
done
exit $fail
