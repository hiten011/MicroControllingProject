# Stop on B1

After the operator starts the AMU with B0, pressing the stop button B1 must cut the motors within a short window and leave the robot on the platform without a speed violation.

## Sub-features

- `b1-after-start` presses B1 a few seconds after B0 during an otherwise normal Layout A run.
- `b1-scored-stop` requires the sim's safety check that motors are zero shortly after B1.

## How to get to it (user POV)

- On the real Kobuki: press B0 to start, then B1 to stop.
- In the simulator: pass `--b1-at <seconds>` so the sim presses B1 that many seconds after B0.

## Driving it with control-amu

Preconditions:

- `control-amu doctor` reports READY.
- A launch evidence directory exists.
- No need for a completed deposit; this journey only scores the safety checks.

- **Start and stop.** Run a Layout A / deposit 2 trial that presses B1 at 6 s. Run `control-amu sim --feature b1-stop --layout A --deposit 2 --heading 187 --seed 1 --b1-at 6 --timeout 30`. Stdout shows `button B0 pressed (start)` then `button B1 pressed (stop)`.
- **Proof.** In `verify-artifacts/amu/<run-id>/b1-stop/sim.stdout.txt`, require `[PASS] Safety: stops on B1 press`, `[PASS] Never exceed 150 mm/s`, and `[PASS] Stayed on the platform`. Keep the HTML replay written beside that log.

## Gotchas

- Omitting `--b1-at` runs a full deposit attempt; that is a different feature (`deposit-run`) and currently may FAIL Pathfind checks.
- A stop that happens only because the robot fell off the ledge is not a B1 proof.
- Rebuild `sim/kobuki_sim.exe` with `sim\build.bat` after changing fault Actions in `Src/FSM_state/` if you are on Windows; Linux Wine runs the committed binary.
