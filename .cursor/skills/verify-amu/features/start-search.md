# Start Search from Idle

From Idle, pressing B0 must start the mission: the sim records the start button and the firmware leaves the Idle hold-still behaviour to begin Search.

## Sub-features

- `b0-start-event` observes the B0 press at t=0 after placement.
- `run-begins` observes that the trial proceeds past start (LEDs/events) rather than idling forever with no B0.

## How to get to it (user POV)

- Press B0 on the Kobuki after power-up / Idle.
- Any normal `kobuki_sim` trial presses B0 automatically at the start of scoring.

## Driving it with control-amu

Preconditions:

- `control-amu doctor` reports READY.
- Use a short timeout; this feature does not require deposit success.

- **Begin a trial.** Run `control-amu sim --feature start-search --layout A --deposit 2 --heading 187 --seed 1 --timeout 15`. Stdout contains `button B0 pressed (start)` near t=0.
- **Proof.** Transcript shows B0 and at least one post-start event or motion (LED line, sensor event, or non-zero max wheel in the result block). Save stdout + HTML under the feature evidence folder. Do **not** require deposit `[PASS]` lines here.

## Gotchas

- Without B0 the firmware stays in Idle (`ST_IDLE` in `main.c`); the sim always injects B0 for scored trials — proving Idle-without-B0 needs a different harness and is not mapped yet.
- A FAIL on deposit checks does not fail this feature; only a missing B0 start event does.
