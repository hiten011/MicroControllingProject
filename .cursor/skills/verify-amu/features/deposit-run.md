# Deposit parcel run

A full Layout + deposit trial should reach the storage unit, avoid bollards, stay on the platform, respect the 150 mm/s cap, and stop near the deposit marker within gap/align tolerances.

## Sub-features

- `layout-A-deposit-2` runs Layout A with deposit location 2.
- `layout-B-deposit-1` runs Layout B with deposit location 1 (mirror path).
- `scored-delivery` requires every sim check for that trial to PASS.

## How to get to it (user POV)

- Place the AMU on the test area for Layout A or B, press B0, and let it Search, Pathfind, and Storage through to the deposit location.
- Simulator: `--layout` / `--deposit` / `--heading` without `--b1-at` or `--lift-at`.

## Driving it with control-amu

Preconditions:

- `control-amu doctor` reports READY.
- Firmware Pathfind/Storage behaviour is believed complete enough to score; if not, expect FAIL and report it.

- **Layout A / D2.** Run `control-amu sim --feature deposit-run --layout A --deposit 2 --heading 187 --seed 1 --timeout 120`. Read every `[PASS]`/`[FAIL]` line in the result block.
- **Proof.** All listed checks PASS (section timing, inside storage, no bollard hit, gap/align, speed, platform). Keep stdout + HTML. If any check FAILs, the feature is not verified — record the FAIL lines; do not substitute a safety-only run as proof of deposit.

## Gotchas

- As of the sync used when this map was seeded, Pathfind can still drive off the ledge on some headings; treat FAIL as a real finding.
- `--suite` is a broader matrix; map one combo here before claiming the suite.
- Safety flags (`--b1-at`, `--lift-at`) change the scored check set — use `b1-stop` / `wheel-drop-stop` instead.
