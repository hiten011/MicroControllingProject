---
name: verify-amu
description: "Drive the AMU Kobuki firmware the way a run on the test area does — via the PC simulator (sim/kobuki_sim.exe). Use when proving FSM, Search, Pathfind, Storage, or safety (B1 / wheel drop) behaviour after a firmware change."
---

# Verify AMU

Project-local verification for the Autonomous Mobile Unit firmware in `Src/`. The user-facing surface in this repo is the **PC Kobuki simulator** (`sim/`), which runs the firmware unchanged against a simulated Kobuki, MB1043 ultrasonic, and the brief's arena, then scores the run. On the real NUCLEO-G474RE + Kobuki the same firmware path applies; this skill does not flash hardware.

## Launch

There is no long-lived server. Each drive is one `kobuki_sim` process.

```bash
.cursor/skills/verify-amu/helpers/control-amu launch
```

Ready means:

- `control-amu doctor` prints `verdict: READY`
- An evidence directory exists under `verify-artifacts/amu/<run-id>/` (printed by `launch`)

On Linux/macOS the helper runs the committed Windows binary with **Wine**. On native Windows it runs `sim\kobuki_sim.exe` directly. After changing `Src/`, rebuild the sim on Windows with `sim\build.bat` so the exe matches the firmware; Linux agents verify against the committed exe unless a fresh Windows build is supplied.

Teardown: `control-amu cleanup` (keeps evidence).

Stand-in note (Linux CI / cloud agents): Wine provides the Windows PE runner. Untested on the stand-in until covered on real Windows: `sim\build.bat` itself, and any behaviour that differs under Wine's SEH/MMIO trap path. `--help` and the safety trials below were proven under Wine.

## Doctor

```bash
.cursor/skills/verify-amu/helpers/control-amu doctor
```

Checks: `sim/kobuki_sim.exe` present; Wine (non-Windows) or native Windows available. Optional: notes whether `arm-none-eabi-gcc` + `cmake` exist for firmware rebuilds (`cmake --preset Debug && cmake --build --preset Debug` → `build/Debug/main.elf`).

Refuse to drive if doctor is not READY. Never attach to a sim process you did not start.

## Drive

Harness: `control-amu sim` → `kobuki_sim`. Prefer the feature map under `features/` over inventing flags.

```bash
.cursor/skills/verify-amu/helpers/control-amu sim --feature b1-stop \
  --layout A --deposit 2 --heading 187 --seed 1 --b1-at 6 --timeout 30
```

Stable handles (from `sim/README.md` and sim stdout):

| Handle | Meaning |
| --- | --- |
| `--layout A\|B` | Bollard Layout |
| `--deposit 1\|2` | Deposit location |
| `--heading DEG` | World-frame start heading (0 = +x / right, CCW) |
| `--seed N` | Repeatable randomness |
| `--b1-at SEC` | Press stop button B1 after B0 |
| `--lift-at SEC` | Wheel drop (lift) after B0 |
| `--timeout SEC` | Per-trial limit |
| stdout `[PASS]` / `[FAIL]` lines | Scored checks |
| `Replay: …html` | HTML scrubber for the run |

Isolation: each `launch` uses its own `verify-artifacts/amu/<run-id>/`. Do not point two agents at the same run id. The sim is one process per invocation; two instances may write the same default `sim_out/` if `--out` is omitted — always go through `control-amu` so `--out` is set.

## Evidence

Proof root: `verify-artifacts/amu/<run-id>/` (survives `cleanup`).

Per feature, require:

1. `sim.stdout.txt` — full transcript including events and `[PASS]`/`[FAIL]` lines
2. `sim.args.txt` — exact flags
3. `feature-id.txt` — map feature id
4. The HTML replay the sim wrote into that folder (e.g. `run_A2_h187_b1.html`)

Proof standards:

- Exercise the real firmware path inside the sim (B0 start, sensors, FSM ticks), not stubs that skip `FSM_Tick`
- Capture the inducing event (e.g. `button B1 pressed`) and the scored outcome, not only the final pose
- Side effects: motors stopped (`Safety: stops on … motors 0 after …`), platform stay, speed cap — as listed in the feature file
- Mocks: only the sim's own Kobuki/ultrasonic/arena models (production boundary for off-robot runs)

## Cleanup

```bash
.cursor/skills/verify-amu/helpers/control-amu cleanup
```

Kills nothing by process name (each `sim` invocation exits on its own). Clears only the `.current-run` pointer. **Never delete** `verify-artifacts/amu/`.

## Helpers

| Path | Invocation |
| --- | --- |
| `.cursor/skills/verify-amu/helpers/control-amu` | `control-amu doctor\|launch\|sim\|cleanup` (executable) |

## Feature map

See [`features/README.md`](features/README.md). Prefer mapped entry points; if a precondition is missing, fail that journey and record why in the feature's Gotchas — do not weaken the journey.
