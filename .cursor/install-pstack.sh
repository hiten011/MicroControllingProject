#!/usr/bin/env bash
# Idempotent Cloud Agent install for the dev-pstack environment.
# Enables the pstack project plugin and seeds the model-role rule.
# (Plugins are not apt packages — Cursor loads them from .cursor/settings.json.)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mkdir -p .cursor "$HOME/.cursor/rules"

SETTINGS=".cursor/settings.json"
if [[ ! -f "$SETTINGS" ]]; then
  cat >"$SETTINGS" <<'EOF'
{
  "plugins": {
    "pstack": {
      "enabled": true
    }
  }
}
EOF
else
  python3 - <<'PY'
import json
from pathlib import Path
path = Path(".cursor/settings.json")
data = json.loads(path.read_text())
plugins = data.setdefault("plugins", {})
pstack = plugins.setdefault("pstack", {})
pstack["enabled"] = True
path.write_text(json.dumps(data, indent=2) + "\n")
PY
fi

MODEL_SRC=".cursor/pstack-models.mdc"
MODEL_DST="$HOME/.cursor/rules/pstack-models.mdc"
if [[ -f "$MODEL_SRC" ]]; then
  cp "$MODEL_SRC" "$MODEL_DST"
fi

python3 - <<'PY'
import json
from pathlib import Path
s = json.loads(Path(".cursor/settings.json").read_text())
assert s.get("plugins", {}).get("pstack", {}).get("enabled") is True, s
print("pstack: enabled in .cursor/settings.json")
PY
test -f "$HOME/.cursor/rules/pstack-models.mdc"
echo "pstack: model rule at $HOME/.cursor/rules/pstack-models.mdc"
echo "pstack: install OK"
