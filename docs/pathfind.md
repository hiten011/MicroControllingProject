# Pathfind

How the AMU drives the two corridors while staying clear of the storage wall and the ledge. Terms match [`GLOSSARY.md`](../GLOSSARY.md). The full diagram is [`amu_fsm.drawio`](amu_fsm.drawio) (preview: [`amu_fsm_preview.png`](amu_fsm_preview.png)).

Pathfind runs once per corridor (`corridor = 1`, then `corridor = 2` after Corridor turn). Layout A has the storage wall on the right and the ledge on the left; Layout B is the mirror image.

```mermaid
flowchart TB
    orient(["Orient"])
    fault(["Fault"])
    storage(["Storage Turn"])

    subgraph pathfind["Pathfind: runs once per corridor"]
        direction TB
        drive["<b>Drive</b><br/>100 mm/s forward"]
        turn["<b>Corridor turn</b><br/>A +90° / B −90°"]
        stop["<b>Corridor stop</b><br/>stop once"]
        reverse["<b>Reverse</b><br/>back 50 mm"]
        rotate["<b>Rotate</b><br/>A +10° / B −10°"]
    end

    orient -->|"rotation complete<br/>corridor = 1"| drive
    drive -->|"1: corridor length, corridor 1"| turn
    turn -->|"rotation complete, corridor = 2"| drive
    drive -->|"1: corridor length, corridor 2"| stop
    stop -->|"ε"| storage
    drive -->|"2: storage-wall bumper<br/>3: ledge cliff"| reverse
    reverse -->|"distance complete"| rotate
    rotate -->|"rotation complete"| drive
    pathfind -.->|"B1 or wheeldrop"| fault

    classDef red fill:#f8cecc,stroke:#b85450,color:#000
    class fault red
    style pathfind fill:#f7f7f7,stroke:#666,stroke-width:1.5px
    linkStyle 8 stroke:#b85450,color:#b85450
```

## States

| State | Role |
| --- | --- |
| **Drive** | Drive forward along the corridor heading |
| **Reverse** | Back a fixed distance after a wall bumper or ledge cliff |
| **Rotate** | Fixed layout nudge after Reverse (A +10°, B −10°) |
| **Corridor turn** | End of corridor 1: 90° onto corridor 2, latch new heading |
| **Corridor stop** | End of corridor 2: stop, then leave Pathfind into Storage Turn |

There are no Wall hit, Ledge hit, or Realign states. Wall and ledge events both enter **Reverse**.

## Check order

Each Pathfind case in `Src/main.c` checks conditions in priority order and takes at most one transition per Kobuki packet. **B1 or wheeldrop → Fault** is first in every Pathfind state.

| State | Checks, in order |
| --- | --- |
| Drive | fault → corridor length (corridor 1) → corridor length (corridor 2) → storage-wall bumper → ledge cliff → drive forward |
| Reverse | fault → distance complete → reverse |
| Rotate | fault → rotation complete → rotate |
| Corridor turn | fault → rotation complete → rotate |
| Corridor stop | fault → ε to Storage Turn |

## Drive

- **Do:** `Kobuki_Drive(PATHFIND_DRIVE_SPEED)` (100 mm/s).
- **Corridor length:** signed wheel travel from `corridor_start_distance` reaches the Figure 2 centreline length for this layout and corridor (A: 1200 then 1400 mm; B: 1400 then 1200 mm). Latched at Orient → Drive and Corridor turn → Drive; not re-latched on Rotate → Drive. Reverse subtracts from the total.
- **Storage-wall bumper:** Layout A right (`0x01`), Layout B left (`0x04`). Ledge-side bumper is ignored.
- **Ledge cliff:** Layout A left (`0x04`), Layout B right (`0x01`). Storage-wall-side cliff is ignored.
- **Ignored in Drive:** centre cliff, ledge-side bumper, storage-wall-side cliff, centre bumper.

## Reverse

Entered from Drive when the storage-wall bumper or ledge cliff fires.

- **Entry:** distance setpoint `−PATHFIND_REVERSE_MM` (−50 mm).
- **Do:** `Kobuki_Drive_Distance` until `distance_complete`.
- **Then:** always enter **Rotate**.
- **Sensors:** bumper and cliff are ignored here. Only B1 or wheeldrop → Fault.

## Rotate

Entered from Reverse when the back-off finishes.

- **Entry:** fixed angle `PATHFIND_ROTATE_A_DEG` (+10) or `PATHFIND_ROTATE_B_DEG` (−10). Does not use `robot.heading`.
- **Do:** `Kobuki_Rotate_Angle` until `rotation_complete`.
- **Then:** return to **Drive**. There is no matching +50 mm forward resume after Rotate.
- **Sensors:** same as Reverse: bumper and cliff are ignored; B1 or wheeldrop → Fault.

If Drive still sees the wall bumper or ledge cliff on the next packet after Rotate, it enters Reverse again (no “sensors must be clear” gate).

## Corridor turn / stop

- **Turn (corridor 1):** Layout A +90° (anticlockwise), Layout B −90° (clockwise). On leaving it latches `heading`, sets `corridor = 2`, and latches `corridor_start_distance`.
- **Stop (corridor 2):** stop once on entry, then straight into Storage Turn on the next packet.

## Constants

| Macro | Value | Used by |
| --- | --- | --- |
| `PATHFIND_DRIVE_SPEED` | 100 | Drive |
| `PATHFIND_CORRIDOR_1_A_MM` / `_2_A_MM` | 1200 / 1400 | Drive (Layout A) |
| `PATHFIND_CORRIDOR_1_B_MM` / `_2_B_MM` | 1400 / 1200 | Drive (Layout B) |
| `PATHFIND_REVERSE_MM` | 50 | Reverse |
| `PATHFIND_ROTATE_A_DEG` / `_B_DEG` | +10 / −10 | Rotate |
| `PATHFIND_CORNER_A_DEG` / `_B_DEG` | +90 / −90 | Corridor turn |

Angles are in degrees, + = anticlockwise (`Kobuki_Rotate`: +ve is CCW). All values are tunable in `Src/Helper/helper.h`.

## Faults

One red edge for the whole Pathfind region: **B1 or wheeldrop → Fault**. No other Pathfind-specific faults. Fault stops the motors and returns to Idle on the next packet.

## Code

Pathfind lives in the `ST_PATHFIND_*` cases of `Src/main.c`. Tunables and `robot` fields are in `Src/Helper/helper.h`. Entry setpoints run in `change_to()` for that state.
