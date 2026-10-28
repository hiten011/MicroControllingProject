# Stop on wheel drop

If the AMU is lifted (wheel drop) after start, the firmware must stop the motors quickly and remain a scored safety pass — the same STOP / FAULT path as B1 for wheeldrop.

## Sub-features

- `lift-after-start` lifts the robot a few seconds after B0.
- `lift-scored-stop` requires the sim's wheel-drop safety check.

## How to get to it (user POV)

- On the real Kobuki: lift the base so a wheel drop fires.
- In the simulator: pass `--lift-at <seconds>` (optional `--lift-for`).

## Driving it with control-amu

Preconditions:

- `control-amu doctor` reports READY.
- A launch evidence directory exists.

- **Lift during run.** Run `control-amu sim --feature wheel-drop-stop --layout A --deposit 2 --heading 187 --seed 1 --lift-at 6 --timeout 30`. Stdout shows `button B0 pressed (start)` then `lift robot lifted (wheel drop)`.
- **Proof.** In `…/wheel-drop-stop/sim.stdout.txt`, require `[PASS] Safety: stops on wheel drop (lifted)`, `[PASS] Never exceed 150 mm/s`, and `[PASS] Stayed on the platform`, plus the HTML replay.

## Gotchas

- Falling off the ledge also sets wheeldrop-like outcomes but fails `Stayed on the platform`; that is not this feature.
- `--lift-for` defaults to 2 s; do not treat put-down as a restart proof unless a feature explicitly asks for `safety_restart`.
