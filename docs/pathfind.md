# Pathfind

How the AMU drives the two corridors while staying clear of the storage wall and the ledge. Terms match [`GLOSSARY.md`](../GLOSSARY.md). The diagram is [`amu_fsm.drawio`](amu_fsm.drawio) (preview: [`amu_fsm_preview.png`](amu_fsm_preview.png)).

Pathfind runs once per corridor (`corridor = 1`, then `corridor = 2` after Corridor turn). Layout A has the storage wall on the left and the ledge on the right; Layout B is the mirror image.

## States

| State | Role |
| --- | --- |
| **Drive** | Cruise forward along the corridor heading |
| **Reverse** | Back a fixed distance after a wall bumper or ledge cliff |
| **Rotate** | Turn the shortest way back to the corridor heading |
| **Corridor turn** | End of corridor 1: 90° onto corridor 2, latch new heading |
| **Corridor stop** | End of corridor 2: stop, then leave Pathfind into Storage Turn |

There are no Wall hit, Ledge hit, or Realign states. Wall and ledge events both enter **Reverse**.

## Flow

```
Drive
  ├─ corridor length (≥ 1200 mm net), corridor 1  → Corridor turn → Drive (corridor 2)
  ├─ corridor length (≥ 1200 mm net), corridor 2  → Corridor stop → Storage Turn
  ├─ storage-wall bumper                         → Reverse → Rotate → Drive
  └─ ledge cliff                                 → Reverse → Rotate → Drive
```

Drive checks those triggers in that order (corridor length, then wall bumper, then ledge cliff).

## Drive

- **Do:** `Kobuki_Drive(FSM_PATHFIND_DRIVE_SPEED)` (100 mm/s).
- **Corridor length:** signed wheel travel from `corridor_start_distance` ≥ `FSM_PATHFIND_CORRIDOR_MM` (1200 mm). Latched at Orient → Drive and Corridor turn → Drive; not re-latched on Rotate → Drive. Reverse subtracts from the total.
- **Storage-wall bumper:** Layout A left (`0x04`), Layout B right (`0x01`). Ledge-side bumper is ignored.
- **Ledge cliff:** Layout A right (`0x01`), Layout B left (`0x04`). Storage-wall-side cliff is ignored.
- **Ignored in Drive:** centre cliff, ledge-side bumper, storage-wall-side cliff, centre bumper.

## Reverse

Entered from Drive when the storage-wall bumper or ledge cliff fires.

- **Entry:** distance setpoint `−FSM_PATHFIND_REVERSE_MM` (−50 mm).
- **Do:** `Kobuki_Drive_Distance` until `distance_complete`.
- **Then:** always enter **Rotate** (even if the heading delta is already ~0°).
- **Sensors:** bumper and cliff are ignored here. Only B1 or wheeldrop → Fault.

## Rotate

Entered from Reverse when the back-off finishes.

- **Entry:** shortest signed angle from `kobuki.angle` to `fsm_data.heading` (corridor heading latched by Orient / Corridor turn), via `Angle_Delta_Deg` (+ = anticlockwise, wraps at ±180°).
- **Do:** `Kobuki_Rotate_Angle` until `rotation_complete`.
- **Then:** return to **Drive**. There is no matching +50 mm forward resume after Rotate.
- **Sensors:** same as Reverse — bumper/cliff ignored; B1 or wheeldrop → Fault.

If Drive still sees the wall bumper or ledge cliff on the next tick after Rotate, it enters Reverse again (no “sensors must be clear” gate).

## Corridor turn / stop

- **Turn (corridor 1):** Layout A +90°, Layout B −90°; on exit, latch `heading`, set `corridor = 2`, latch `corridor_start_distance`, then Drive.
- **Stop (corridor 2):** stop once, then Storage Turn.

## Constants

| Macro | Value | Used by |
| --- | --- | --- |
| `FSM_PATHFIND_DRIVE_SPEED` | 100 | Drive |
| `FSM_PATHFIND_CORRIDOR_MM` | 1200 | Drive (end of corridor) |
| `FSM_PATHFIND_REVERSE_MM` | 50 | Reverse |
| `FSM_PATHFIND_CORNER_A_DEG` / `_B_DEG` | +90 / −90 | Corridor turn |

## Faults

One red edge for the whole Pathfind region: **B1 or wheeldrop → Fault**. No other Pathfind-specific faults.

## Code

| State | File |
| --- | --- |
| Drive | `Src/FSM_state/states/state_pathfind_drive.c` |
| Reverse | `Src/FSM_state/states/state_pathfind_reverse.c` |
| Rotate | `Src/FSM_state/states/state_pathfind_rotate.c` |
| Corridor turn | `Src/FSM_state/states/state_pathfind_corridor_turn.c` |
| Corridor stop | `Src/FSM_state/states/state_pathfind_corridor_stop.c` |
| Ids / macros | `Src/FSM_state/fsm_states.h` |
