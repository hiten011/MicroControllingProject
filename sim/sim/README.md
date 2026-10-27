# Kobuki simulator

Runs the firmware in `../Src` **unchanged** on the PC against a simulated
Kobuki, MB1043 ultrasonic sensor and the project arena (brief, Figure 2),
then scores each run against Table 1 and writes an HTML replay.

## Build

Needs a PC `gcc` (MSYS2 UCRT64: `C:\msys64\ucrt64\bin` on PATH).

```
sim\build.bat          -> sim\kobuki_sim.exe      (normal)
sim\build.bat shim     -> sim\kobuki_sim_dbg.exe  (for gdb, see below)
```

Every `.c` under `Src\` is compiled (except the ARM-only `syscall.c` /
`sysmem.c`). Every folder under `Src\` is on the include path, so new files and
folders are picked up automatically. Rebuild after every firmware change.

## Run

Run from the project folder:

```
sim\kobuki_sim.exe --layout A --deposit 2 --heading 135
sim\kobuki_sim.exe --layout B                 (D1, random heading)
sim\kobuki_sim.exe --trials A2,B1@200         final test: two layouts back-to-back, no reset
sim\kobuki_sim.exe --b1-at 6                  safety: press B1 6 s after start
sim\kobuki_sim.exe --lift-at 6                safety: lift the robot (wheel drop)
sim\kobuki_sim.exe --suite                    4 layout/deposit combos x 8 headings + safety
sim\kobuki_sim.exe --help                     all options
```

Headings are in the world frame: 0 = facing right (+x), 90 = facing up the
page, CCW positive. Each run prints its events and checks. It also writes
`sim_out\<run>.html`: open it in a browser to scrub through the run (Space plays
or pauses, the arrow keys step, add `#t=31.5` to the URL to open at a time).
`--suite` also writes `sim_out\index.html`, which links every replay.

## Calibrate it to your robot first

| Option | Default | What to set |
|---|---|---|
| `--sensor-offset MM` | 0 | Distance from the Kobuki centre to the ultrasonic's face, along the heading (0 = sensor at the centre, facing forward). **This changes every range reading.** |
| `--beam DEG` | 10 | Half-angle at which a 90 mm bollard is still detected. Measure it by turning past a bollard. |
| `--us-latency MS` | 100 | How late a reading appears on the analogue pin. |
| `--gap-tol`, `--align-tol` | 25 mm, 5 deg | Pass bands for the deposit checks (the brief doesn't give any). |

## How it works

- **Registers.** The real `stm32g474xx.h` is used, so `USART3`, `ADC1`, `RCC`
  and `GPIOx` point at their real addresses. The simulator maps memory there.
  The USART3 and ADC1 pages are no-access, so each access by `uart.c` / `adc.c`
  traps (access violation, then single-step) into a model that behaves like the
  hardware: `TXE`/`RXNE`, `RDR`/`TDR`, `ADCAL` clearing, `ADRDY`, `EOC`, and
  write-1-to-clear `ISR`. The sim checks the clocks, PC10/PC11 AF7, the baud
  rate (from `BRR` and the clock source), 8N1, PA0 analogue mode, ADC
  calibration and so on, and warns about anything that would fail on hardware
  (`--strict` stops instead).
- **Time.** Time advances 20 ms each time the firmware waits on `RXNE` with
  nothing left to read. Each time, one 50 Hz Kobuki feedback packet is sent.
  This is how the real loop is paced, so one firmware loop = one packet.
- **Kobuki.** The sim parses `AA 55` packets (base control 0x01, LEDs 0x0C;
  bad checksums are reported). It sends basic sensor data, docking IR,
  inertial, cliff, current and GPI sub-payloads at the same byte offsets as the
  real robot. It models differential drive (230 mm wheelbase) with an
  acceleration limit. 16-bit encoders start at random values to exercise
  wrap-around, and the gyro is zeroed at an arbitrary heading. When the robot
  pushes a wall, the wheels slip: the encoders keep counting but the robot
  doesn't move.
- **Bumpers** (left/centre/right, front half) fire on walls and bollards.
  **Cliff sensors** (−50°, 0°, +50°, 165 mm out) fire past the platform edge.
  **Wheel drop** fires when the robot is lifted or a wheel goes over the
  ledge, which counts as falling off.
- **MB1043.** Ranges at 10 Hz and reports the nearest echo in the cone: round
  objects anywhere in the beam, flat walls only where they're square to the
  sensor or at a corner. The storage unit's walls are below the sensor, so
  only the bumpers feel them; the room walls beyond the ledge do echo. Output is clamped to 300–5000 mm (no target = 5000),
  then converted to volts at Vcc/5120 per mm and to a 12-bit ADC code.

## Debugging with gdb

The normal build uses CPU exceptions, which confuse debuggers. Build `shim`
instead: it swaps only `uart.c` / `adc.c` for direct calls (no register
checks) and compiles at `-O0`. Then:

```
gdb --args sim\kobuki_sim_dbg.exe --layout B --heading 0 --seed 116
(gdb) break Classify_Path
(gdb) run
(gdb) print range_mm
(gdb) print rb            # the simulator's robot state (pose, sensors, time)
```

## Limits

This is a lite simulator. Wheels don't slip on the floor (only against walls),
the gyro is perfect unless you set `--gyro-drift`, the ultrasonic model is
geometric (no multipath or soft targets), and the MCU is infinitely fast. Use
it to find logic bugs and edge cases; tune the final numbers on the real robot.
