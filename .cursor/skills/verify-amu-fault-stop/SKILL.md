---
name: verify-amu-fault-stop
description: "Formally check that AMU Active states stop the motors when B1 or wheeldrop is asserted (STOP/FAULT path). Use when changing action_fault*, state_fault, FSM_Tick ordering, or Idle vs Active fault wiring."
---

# Verify AMU fault stop (formal)

## Property

**Plain sentence:** If the stop button B1 is pressed or a wheel is dropped, the motors are not left Running.

**Harm if broken:** The AMU keeps driving while lifted or after the operator hits stop — collision, ledge fall, or dragging.

**Tool:** TLA+ / TLC (`tla2tools.jar` **v1.7.3**), because this is an ordering/priority question across FSM ticks and sensor samples. The model names States and Actions from this repo; it is not a line-by-line proof of C.

Read every TLC result with the pstack `/statechart` skill (shortest violation path in plain words when it fails; state chart when it passes). Do not copy `/statechart` CLI flags into this skill — they change between releases.

## When to run

A change touches any of:

- `Src/FSM_state/states/action_fault.c` / `action_fault.h`
- `Src/FSM_state/states/state_fault.c`
- `Src/FSM/fsm.c` (Tick / Action order)
- Any State that must (or must not) put `action_fault_b1_wheeldrop` / `action_fault_all` first in its Action list
- `Src/FSM_state/states/state_idle.c` (Idle must not be treated as Active for B1)

## Coverage record

| Model piece | Code |
| --- | --- |
| `Active` + stop → `Fault` + `Stopped` | `action_fault.c` conditions; first Action in Search/Detect/Spin/Orient/Pathfind/Storage States |
| `Fault` → `Idle` + `Stopped` | `state_fault.c` (`fault_init` → `Kobuki_Drive(STOP)`, then `fault_to_idle`) |
| One Action per Tick | `fsm.c` `State_Run` |
| Idle ignores B1→Fault | `state_idle.c` (only B0→Search and hold still) |

**Sync commit:** `d52bcd8d9ae63510fd024fc62cc7067724c8b815` (confirm the files above still match the model before trusting a pass; move this commit forward only after re-confirming).

## Bounds and assumptions

- Collapses every non-Idle non-Fault State that wires a B1/wheeldrop fault Action into one `Active` label.
- Bumper/cliff-only faults are abstracted as nondeterministic `Active`→`Fault` without requiring stop sensors.
- Does not model Kobuki packet timing, UART, or Pathfind geometry.
- A pass covers this model, not the whole firmware or the Windows sim.

## Run

```bash
.cursor/skills/verify-amu-fault-stop/helpers/run-check.sh
```

Needs Java 11+. Downloads pinned `tla2tools.jar` v1.7.3 into `helpers/` if missing.

Expect: TLC succeeds with `SafeMotors` holding.

## Broken variant (must fail)

```bash
.cursor/skills/verify-amu-fault-stop/helpers/run-check.sh --broken
```

Uses `formal/RobotFaultStopBroken.tla` (Fault entered but motors left `Running`). Expect: `Invariant SafeMotors is violated`. If that run passes, the check is useless — fix the model or the script.

## Interpreting a real failure

Counterexample in code words:

1. AMU is in an Active State (Search, Pathfind, …).
2. A Tick samples B1 and/or wheeldrop asserted.
3. Motors remain Running (missing `Kobuki_Drive(STOP)` or fault Action not first / not wired).

Distinguish **modeling error** (Idle treated as Active, or Env updated after actuators without a Tick) from **product bug** (C no longer matches the table above). Do not weaken `SafeMotors` to make a red check green. Do not change product code unless the user asks.

## Update rule

After an intentional change to the fault path, update the model and the sync commit in this skill only once TLC on the good spec and `--broken` both behave as above and `/statechart` has been used to read the results.
