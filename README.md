# MicroControllingProject

Firmware for the Autonomous Mobile Unit (AMU): a Kobuki base driven by a NUCLEO-G474RE that finds the test-area Layout, drives the open route past the storage wall and the ledge, and delivers a parcel into the storage unit.

- Hiten
- Safin
- Shaheer M

## How it runs

`DEPOSIT_TEST_MODE` in `Src/Helper/helper.h` is currently **1**: the AMU enters Deposit automatically after the first Kobuki feedback packet. It turns anticlockwise through a full 360° at 50 mm/s wheel speed, records the closest valid ultrasonic target, then turns by the shortest route to face that saved heading. Alignment finishes within 1° of the saved heading. It then adjusts the ultrasonic reading to **420 mm ±10 mm** before stopping in Idle. Press B0 to repeat the whole sequence. B1 or wheeldrop stops any part of Deposit.

If the reading is below 410 mm, the AMU reverses at 50 mm/s until it reads at least **520 mm**, then approaches at 35 mm/s. If it starts beyond 430 mm, it approaches directly. It pauses for about 300 ms after alignment and each stop so the 10 Hz ultrasonic output catches up, then checks the final distance again. A reading still below 410 mm repeats the reverse-then-approach correction. The target, tolerance, extra reverse distance and speeds are tunables in `Src/Helper/helper.h`.

LED 1 is yellow during distance adjustment, green when the settled reading is within tolerance, and red for a missing/invalid echo. Zero and maximum-range readings stop distance adjustment through Fault. Bumpers and cliffs also stop the distance states.

Inspect `robot.deposit_rod.detected`, `robot.deposit_rod.range_mm` and `robot.deposit_rod_angle` in the debugger for the scan result, and `robot.deposit_range_mm` for the live distance. The angle is the absolute Kobuki gyro heading in 0.01° ticks, wrapping at ±18000. All target distances are from the ultrasonic sensor face to the rod.

Set `DEPOSIT_TEST_MODE` to **0** to restore the full parcel run and B0 starting Search, including for the simulator's Pathfind recovery check. One control step runs per received Kobuki packet (`Kobuki_Read()`, then a `switch` on the current state in `Src/main.c`). The full run follows this path:

```
IDLE ──B0──▶ SEARCH ─▶ IDENTIFY ─▶ ORIENT ──▶ PATHFIND ──▶ STORAGE ──▶ IDLE

PATHFIND = Drive, Reverse, Rotate, Corridor turn / reverse / stop
STORAGE  = Turn, Drive-in, Deposit, Deposit align, Deposit distance / reverse / approach

Any State except IDLE ──B1 / wheeldrop──▶ STOP / FAULT ──▶ IDLE
```

![AMU FSM](docs/amu_fsm_preview.png)

Pathfind recovery is **Drive → Reverse (−50 mm) → Rotate (wall hit: A −5° / B +5°, ledge cliff: the opposite) → Drive**. There is no Realign, Wall hit, or Ledge hit state.

## Docs

| Doc | What it is |
| --- | --- |
| [`GLOSSARY.md`](GLOSSARY.md) | Shared vocabulary for the FSM, report, and code |
| [`docs/pathfind.md`](docs/pathfind.md) | Pathfind states and how Drive / Reverse / Rotate work |
| [`docs/amu_fsm.drawio`](docs/amu_fsm.drawio) | Full AMU FSM diagram (edit in diagrams.net) |
| [`docs/amu_fsm_preview.png`](docs/amu_fsm_preview.png) | PNG preview of that diagram (export at 200 % after editing the `.drawio`) |
| [`sim/README.md`](sim/README.md) | PC simulator that runs this firmware against a simulated Kobuki, ultrasonic sensor and arena |

## Source layout

| Path | What is in it |
| --- | --- |
| `Src/main.c` | Init, the Read loop, and the state `switch` |
| `Src/Helper/` | Shared `robot` data, tunables, fault checks, scan and Pathfind helpers |
| `Src/Kobuki_Drivers/` | Kobuki packet read / write, distance and angle setpoints |
| `Src/Ultrasonic/`, `Src/ADC/`, `Src/G474RE_Drivers/` | Ultrasonic reading, ADC and UART drivers |

## Build

```
cmake --preset Debug      # or Release
cmake --build --preset Debug
```

Needs `cmake`, `ninja` and the `arm-none-eabi` GCC toolchain. The ELF is `build/<preset>/main.elf`. CI builds both presets and checks formatting (`scripts/format.sh --check`, clang-format 18.1.8) and cppcheck.

`make -C sim check-deposit` runs the firmware loop against mocked gyro, motion and delayed ultrasonic readings. It covers starting too close, too far or within tolerance, reverse-then-approach correction, sensor noise, and movement stops. It needs Python 3 and a host GCC compiler; it does not use the arena simulator's memory mapping.
