# AMU verification map

Maintained source for verifying user-visible AMU behaviour through the Kobuki PC simulator. Read this index, then the matching feature file.

## Baseline preconditions

- Run `control-amu launch` and keep the printed evidence directory.
- Doctor reports `verdict: READY` (`sim/kobuki_sim.exe` + Wine or native Windows).
- Drive only through `control-amu sim --feature <id> …` so evidence lands under the run id.
- Prefer fixed `--seed` and `--heading` from the feature file so runs are comparable.
- Never score a run that wrote into a shared default `sim_out/` you did not isolate.

## Driving conventions

- Start every recipe from a fresh `control-amu sim` process (firmware `main` starts once per process).
- Treat every flag and `[PASS]` check name as literal.
- World heading: `0` = facing right (+x), `90` = up the page, CCW positive (`sim/README.md`).
- Vocabulary matches [`GLOSSARY.md`](../../../../GLOSSARY.md): Layout, Search, Pathfind, Storage, Bollard, Ledge, Deposit location.

## Proof and skip reporting

- Capture stdout transcript and the HTML replay.
- Mutation / safety proof includes the inducing event line and the safety `[PASS]` line.
- Record the feature id in `feature-id.txt`.
- If the firmware cannot complete a mapped journey yet, report FAIL with the failing checks — do not mark it verified via a different path.

## Feature entry contract

Each feature file: H1, one paragraph, then exactly `Sub-features`, `How to get to it (user POV)`, `Driving it with control-amu`, `Gotchas`.

## Features

- [Stop on B1](./b1-stop.md) — stop button ends motion safely after start.
- [Stop on wheel drop](./wheel-drop-stop.md) — lift / wheeldrop ends motion safely.
- [Start Search from Idle](./start-search.md) — B0 leaves Idle and begins the run.
- [Deposit parcel run](./deposit-run.md) — full Layout + deposit delivery scoring (may FAIL while Pathfind is unfinished).
