# AMU hardware test document

The checklist for the group member running the AMU on the platform. It covers everything the assignment documents and the existing code could not settle. Each item is either a **value to try** (Section 3) or a **check** (Section 4). Fill the blank Result cells and tables in the format in Section 1. Vocabulary follows [`GLOSSARY.md`](../GLOSSARY.md). This is work item 2 of the AMU FSM spec (#61), ticket #63.

## 1. How to report results

Every Result cell and table is blank until you test. Use one format everywhere so results can be pasted straight back:

| Kind of result | Write it as | Example |
| --- | --- | --- |
| Measured number | `value unit (n=trials, min–max)` | `98 mm/s (n=3, 96–101)` |
| Byte read from the AMU | hexadecimal | `0x04` |
| Yes / no question | `yes` or `no`, plus the evidence | `no (cliff byte stayed 0x00 for 3 laps)` |
| Something that did not happen | say so explicitly, never leave it blank | `not reached`, `not observed` |

Copy this header above each session's results:

```
Session: <date> · Tester: <name> · Battery: <full / half / low> · Firmware commit: <hash, or "bench program">
```

Return the filled-in copy of this file (commit it on a branch, or paste the tables into #61).

## 2. Before you start

### 2.1 Conventions

- **Angles.** Positive is anticlockwise seen from above. This matches the gyro reading (`kobuki.angle`, hundredths of a degree, wrapping at ±180°) and the rotate setpoints (whole degrees).
- **Bearing.** Used to place objects and to say where the sensor detected something. 0° is east (right-hand side of Figure 2) and bearings increase anticlockwise, so 90° is north. Seen from the start circle: A1 is at 90°, B1 at 0°, the storage unit lies in the north-east quadrant (0° to 90°), and the Figure 2 external bollards sit south-west (**Estimate**: roughly 200° to 220°, read off Figure 2).
- **Left and right** are as seen from behind the AMU, facing its direction of travel.
- **Range** is `range_mm` from `Ultrasonic_Detect`. A detection is a range below 700 mm (`ULTRASONIC_DETECT_RANGE_MM`).
- **Stop sample.** The range sample on which SEARCH first sees a detection and stops. The bollard has just entered the outer limit of the beam, not the centre line, so the stop-sample range may read longer than the on-axis distance to the bollard.
- **Estimates.** Every number marked **Estimate** is derived or expected, not measured, and says how it was derived. Measured values go in Result.
- **Sensor forward offset `s`.** How far the sensor face sits ahead of the AMU centre, in mm. Measure it once with the tape measure and write it here: `s = ____ mm`. Several estimates below are written in terms of `s`.

### 2.2 Equipment

- Tape measure (at least 2 m), stopwatch (a phone is fine), masking tape for floor marks.
- Bollards (90 mm diameter): at least three, ideally all four plus two for the external bollards.
- Protractor or a printed angle sheet.
- A second person to catch the AMU at the ledge.
- A fully charged battery. Speeds and turn rates change as it drains, so note the battery state in the session header.
- The debugger Watch window (VS Code).

### 2.3 Test programs

Two kinds of program are used:

- **Bench program** (tag *Bench*). Temporarily replace `Src/main.c`, as the existing ultrasonic test does (`ultrasonic_test_every_2_seconds.txt`). Each loop calls `Kobuki_Read(&kobuki)`, which blocks until the next packet and so paces the loop. Add `Ultrasonic_Detect(&object)` where a check needs the sensor. Then call only the driver functions the check needs (`Kobuki_Drive`, `Kobuki_Rotate`, `Kobuki_Rotate_Angle_Setpoint` with `Kobuki_Rotate_Angle`, `Kobuki_Drive_Distance_Setpoint` with `Kobuki_Drive_Distance`). Do not use the `Robot_*` wrappers: they stop on any bumper. Read results from Watch variables with the program paused; there is no UART logging because the UART carries the Kobuki link.
- **Firmware** (tag *Firmware*). The FSM from work item 4. Build it with the first candidate for the constant under test, change one constant per run, and rebuild.

### 2.4 Safety

- Keep a hand near B1 for every run.
- For anything beside the ledge, have the second person ready to catch the AMU. This includes the start circle, which is only about 25 mm from the ledge (**Estimate**: the start circle is Ø350 mm in a 400 mm-wide lane, so (400 − 350) ÷ 2 = 25 mm each side, from Figure 2).
- Never drive towards the ledge faster than the search speed (75 mm/s) in a bench program.

### 2.5 Suggested order

| Phase | Where | Do |
| --- | --- | --- |
| 1 | Flat floor, off the platform | C1 (bumper bits, buttons, wheel drop), C2, C8, C9 |
| 2 | Start circle, bench program | C3, C4, C5, C6, C7a, then V4–V6 from the C4 data; C1 cliff-sensor part at the ledge |
| 3 | Corridors and storage unit, firmware | V1, V2, V3, V7, C7b, C10, C11 |

## 3. Values to try

One row per value that could not be verified from the assignment documents. Change one value at a time. Result: your chosen value, the evidence (what you observed at each candidate) and the number of trials.

| ID | Value | What it controls | Why it could not be verified | Candidates | How to measure | Result |
| --- | --- | --- | --- | --- | --- | --- |
| V1 | Path speed | The speed driven along both corridors, so the section times and how far the AMU overruns after the front cliff. | The brief only caps speed at 150 mm/s (the driver saturates at exactly 150, so the constant must be below it). The best value trades the 30 s and 60 s section targets against overrun and correction margin in a corridor only slightly wider than the AMU. | 80, 100, 120 mm/s.<br>**Estimate:** the path from the start circle to section 3 is about 2 m (about 1.9 m in Layout A and 2.0 m in Layout B, scaled off Figure 2, ±10 %), so driving alone takes about 25 s at 80, 20 s at 100 and 17 s at 120 mm/s, before the sweep (about 10 s, see C3). | *Bench:* mark 1000 mm on the floor, call `Kobuki_Drive(candidate)` straight, time the AMU between the marks, 3 runs per candidate; speed = 1000 mm ÷ time. *Firmware:* then record the overrun after the front cliff (C10) and the times (C11). | |
| V2 | Storage wall correction angle | How far the AMU turns away from the storage wall after a bumper hit on that side, before it returns to the corridor heading. | No document gives it. It depends on the bumper geometry and on the corridor being only about 50 mm wider than the AMU (400 mm lane, Ø350 mm AMU). | 5°, 10°, 15°, 20°. **Estimate:** the 5° to 20° range is a starting range from the spec, not derived from any document; start at 10°. | *Firmware, first corridor:* place the AMU at the start of the corridor, pointed slightly towards the storage wall so it hits within about a second. Per run record: does the bumper bit clear by the end of the turn; the heading error after REALIGN (gyro, degrees); how many corrections the corridor needed; whether it reached the front cliff. 5 runs per candidate, in both a Layout A and a Layout B corridor (mirror images, but the hardware is not symmetric). | |
| V3 | Ledge correction angle | How far the AMU turns away from the ledge after the cliff sensor on that side fires, before it returns to the corridor heading. | No document gives it. Too small and the AMU may not clear the ledge; too large and it swings into the storage wall. | 5°, 10°, 15°, 20°. **Estimate:** as V2, a starting range from the spec; start at 10°. | *Firmware, first corridor, second person at the ledge:* place the AMU pointed slightly towards the ledge. Per run record: does the cliff bit clear by the end of the turn; did the AMU stay on the platform; did the correction then hit the storage wall (bumper bit set); the heading error after REALIGN. 5 runs per candidate, both layouts. | |
| V4 | Layout split range | The range that separates a near blocking bollard (Layout B) from a far one (Layout A) at the SEARCH stop. | Depends on the sensor's forward offset `s` and on the beam pattern at the stop sample, neither of which is in the documents. | **Estimate:** about 475 mm − `s`. Derived from the midpoint of B1 (about 400 mm) and A1 (about 555 mm) surface distances from the start centre, (400 + 555) ÷ 2 = 477.5 mm, rounded to about 475 mm, where each is the Figure 2 centre-to-centre distance (about 445 and 600 mm) less the 45 mm bollard radius; the sensor measures from its own face, so subtract `s`. Try 450 − `s`, 475 − `s`, 500 − `s`. | Do C4 first. Final value: midway between the highest B1 stop-sample range and the lowest A1 stop-sample range. Check every C4 stop then classifies correctly. | |
| V5 | Layout minimum range | The lowest stop-sample range still accepted as Layout B. Anything below is "none", which stops the run. | Depends on the sensor's minimum reportable range, the beam pattern and whether the nearest storage wall corner is seen (C5); none are in the documents. | Lowest B1 stop-sample range from C4 minus 25, 50 or 75 mm. **Estimate** for illustration only: with `s` = 0 the B1 range is about 400 mm, giving 375, 350 or 325 mm. | After C5, decide whether the nearest storage wall corner (**Estimate:** about 283 mm, from the spec) should fall below this minimum (and so stop the run). Check no C4 stop on B1 falls below it. | |
| V6 | Layout maximum range | The highest stop-sample range still accepted as Layout A. Anything above, up to the 700 mm detect range, is "none". | As V5, from the far side. | Highest A1 stop-sample range from C4 plus 25, 50 or 75 mm, never above 700 mm. **Estimate** for illustration only: with `s` = 0 the A1 range is about 555 mm, giving 580, 605 or 630 mm. | Check no C4 stop on A1 exceeds it. Check what C6 gives for the external bollards at 400–500 mm: they may fall inside a band. | |
| V7 | Storage entry distance | How far the AMU drives into the storage unit after the STORAGE turn. | Depends on where the AMU stops and how it faces the entrance; Figure 2 only gives the geometry. | 600, 700, 800 mm. **Estimate:** about 740 mm, derived from about 560 mm from the stop point to the entrance line (read off Figure 2) plus the AMU radius of about 175 mm (the start circle is Ø350 mm); 560 + 175 ≈ 735. | *Firmware:* run the STORAGE turn then the drive-in, 3 runs per candidate in each layout. Per run: is the entire AMU inside the storage unit (tape from the entrance line to the AMU's rear; 2 % of the marks depend on it); any bumper bit set; the range to D1 and D2 from the final position, using `Ultrasonic_Detect` (both should read below 700 mm, which DEPOSIT needs). | |

## 4. Checks

Each check says what to do and what to record. Tag shows the program needed (Section 2.3).

### C1. Bumper, cliff and wheel drop bits (Bench)

**Question.** Which bit does each sensor set in `kobuki.bumper`, `kobuki.cliff` and `kobuki.wheeldrop`, and does `kobuki.button` match the practical?

**Estimate (expected, not measured).** Right 0x01, centre 0x02, left 0x04 for both the bumper and cliff bits. Source: the spec, to be confirmed. Buttons: B0 0x01, B1 0x02, B2 0x04 (the practical's button codes).

**Procedure.** Watch `kobuki.bumper`, `kobuki.cliff`, `kobuki.wheeldrop` and `kobuki.button`.

1. Press each bumper alone (right, centre, left), then two together. Confirm "right" is the right-hand side seen from behind the AMU, since Layout A and Layout B are mirror images.
2. Cliff sensors, at the ledge on the platform with the second person holding the AMU: with the motors idle, ease the AMU towards the ledge so only one cliff sensor at a time passes over it (right, then centre, then left).
3. Front cliff signature: drive straight at the ledge with `Kobuki_Drive(SEARCH_SPEED)` and stop on the first non-zero cliff byte. Record the cliff byte on that loop and the next 5 loops (is it the centre bit alone, or others too?). This defines what "front cliff" means in the firmware.
4. Lift the AMU clear of the floor; then lift one side at a time. Any non-zero `wheeldrop` must stop the AMU.
5. Press B0, B1 and B2.

**Record.**

| Test | Byte read (hex) | Matches estimate? |
| --- | --- | --- |
| Bumper right | | |
| Bumper centre | | |
| Bumper left | | |
| Bumpers two together | | |
| Cliff sensor, right | | |
| Cliff sensor, centre | | |
| Cliff sensor, left | | |
| Front cliff signature (loop 0 to 5) | | |
| Wheel drop, lifted clear | | |
| Wheel drop, one side lifted | | |
| B0 / B1 / B2 | | |

### C2. Loop period (Bench)

**Question.** How long is one loop?

**Estimate (assumption).** About 20 ms. Derived from the Kobuki sending one feedback packet about every 20 ms; the documents do not state the rate. `Kobuki_Read` blocks until a packet arrives, so the loop period is the packet period (plus any work done in the loop).

**Procedure.** Increment a loop counter each loop. Start the stopwatch and the program together, run exactly 60 s, pause, read the counter. Period = 60 s ÷ count. Repeat with the same loop work as the final program (`Ultrasonic_Detect` and the LED update included).

**Record.**

| Loop work | Loops in 60 s | Period (ms) |
| --- | --- | --- |
| Read only | | |
| Read + ultrasonic + LEDs | | |

### C3. Time for one full sweep at the search speed (Bench)

**Question.** How long does one 360° sweep take at `SEARCH_SPEED`, and does the gyro agree with a physical full turn?

**Estimate.** About 10 s. Derived from spinning on the spot at the search speed: if the commanded 75 mm/s is each wheel's speed and the wheel spacing is about 230 mm (Kobuki datasheet, not in the assignment documents), the AMU turns at 75 ÷ 115 = 0.65 rad/s ≈ 37°/s, so 360° takes about 9.6 s.

**Procedure.** Put a floor mark in line with a fixed point on the AMU body. Call `Kobuki_Rotate(SEARCH_SPEED)` (anticlockwise, as SEARCH does) for three laps, then one lap clockwise. Time each lap on the stopwatch. Each loop add the shortest signed difference between consecutive gyro readings to a swept-angle total.

**Record.** Per lap: the stopwatch time, and the swept-angle total at the moment the mark lines up again. It should read 360°; if not, the two-sweep limit is off by that proportion. Also record the typical gyro change per loop in hundredths of a degree (**Estimate:** about 75, since 37°/s × 20 ms = 0.75°). A change under 1° per loop means a whole-degree helper would lose the angle each loop.

| Lap | Direction | Time (s) | Swept-angle total at the mark (°) | Gyro change per loop (hundredths of a degree) |
| --- | --- | --- | --- | --- |
| 1 | anticlockwise | | | |
| 2 | anticlockwise | | | |
| 3 | anticlockwise | | | |
| 4 | clockwise | | | |

### C4. Stop-sample range spread at A1 and B1 (Bench)

**Question.** How much does the stop-sample range vary when the AMU starts from random headings, and does it stay below the point where it could cross the split range?

**Estimate (threshold).** About 75 mm. Derived as half the roughly 155 mm gap between the A1 (about 555 mm) and B1 (about 400 mm) surface distances.

**Procedure.** Set up Layout A (A1, A2) and note the A1 range data; then Layout B (B1, B2). Leave the storage walls in place; they are part of the platform. Put no external bollards out yet (C6 adds them). With the bench program: spin anticlockwise at the search speed until `Ultrasonic_Detect` first reports a detection, then `Kobuki_Drive(STOP)` at once and save that stop-sample range. Wait 1 s and save the settled range. Measure the angle between the AMU's forward axis and the centre of the object it ended up facing (positive anticlockwise). Do at least 10 trials per bollard, starting from headings spread around the full circle, including headings that put the blocking bollard just ahead of and just behind the beam.

**Record.** For each trial note which object the stop faced: blocking bollard, storage wall or other. This measures the known risk that the first detection may not be the blocking bollard.

| Layout | Trial | Start heading (°) | Stop-sample range (mm) | Settled range 1 s later (mm) | Object centre offset from forward axis (°) | Object faced |
| --- | --- | --- | --- | --- | --- | --- |
| A (A1) | 1–10, one row each | | | | | |
| B (B1) | 1–10, one row each | | | | | |

Then work out, over the trials that faced the blocking bollard: spread = highest − lowest stop-sample range, separately for A1 and B1, and the same for the settled ranges.

| | A1 | B1 |
| --- | --- | --- |
| Stop-sample spread (mm) | | |
| Settled spread (mm) | | |
| Spread above about 75 mm? If so, add a short settle-and-average step after the stop | | |

### C5. Does the sensor see the storage walls? (Bench)

**Question.** Does the ultrasonic sensor detect the storage walls from the start circle, and from which bearings?

**Estimate.** The nearest storage wall corner is about 283 mm from the start centre at about 45°, and the storage walls are inside the detect range for bearings of about 0° to 90°, if the sensor sees low storage walls. Source: the spec, scaled off Figure 2.

**Procedure.** Remove every bollard. Put the AMU on the start circle facing east (0°) and note the gyro reading `g0`; bearing = gyro − `g0`, wrapped to ±180°. Spin anticlockwise at the search speed for two laps. Each time `detected` goes from 0 to 1 and back, record the bearing, and note the lowest range during each detection.

**Record.**

| Detection | Bearing in (°) | Bearing out (°) | Lowest range (mm) |
| --- | --- | --- | --- |
| 1 | | | |
| 2 | | | |
| 3 | | | |
| No detection at any bearing? | | | |

If the range is pinned to one value over a span of bearings (for example 300 mm), note it; it is probably the sensor's minimum reportable range.

### C6. Are the external bollards detected, and at what range? (Bench)

**Question.** Does the sensor see an external bollard, at what range, and what would the layout rule do with it?

**Estimate.** The Figure 2 examples sit about 400 to 500 mm from the start centre, south-west (bearings of about 200° to 220°), which is the same range as B1. Source: the spec, scaled off Figure 2.

**Procedure.**

1. Remove the other bollards. Stand one bollard beyond the ledge at bearing 210°, at 300, 400, 450, 500, 600 and 700 mm from the start centre. Repeat at bearings 180° and 240°. For each, spin as in C4 and record whether it is detected and the stop-sample range.
2. Put the Layout A blocking bollards back, and place both external bollards as in Figure 2. Repeat the C4 procedure with at least 10 random start headings. Record how often the stop faced an external bollard and the classification the current candidate bands would give.

**Record.**

| Distance (mm) | Bearing (°) | Detected? | Stop-sample range (mm) | Class under current bands (A / B / none) |
| --- | --- | --- | --- | --- |
| 300 / 400 / 450 / 500 / 600 / 700, one row per distance and bearing | | | | |

| With both external bollards placed | Count out of 10 |
| --- | --- |
| Stop faced an external bollard | |
| Stop faced the blocking bollard | |
| Stop faced something else | |

### C7. Do cliff flags fire while spinning or turning near the ledge? (Bench, then Firmware)

**Question.** Do the cliff sensors fire while the AMU spins on the start circle, and while it turns beside the ledge?

**Procedure.**

**a. Spin on the start circle (Bench).** Second person ready. Spin at the search speed for three laps anticlockwise and three clockwise, watching `kobuki.cliff` and `kobuki.bumper`. Optionally repeat with the AMU placed 20 mm off-centre towards the nearest ledge, since the real placement will not be perfect.

**b. Turns beside the ledge (Firmware).** At the end of the first corridor, after the front cliff stop, run the 90° turn (anticlockwise in Layout A, clockwise in Layout B). At the end of the second corridor, run the 135° STORAGE turn. Also run the V2 and V3 correction turns. Record the cliff and bumper bytes during each turn.

**Record.**

| Test | Any non-zero cliff byte? | Which bit(s), bearing/heading | Any non-zero bumper byte? |
| --- | --- | --- | --- |
| a. Start circle, anticlockwise | | | |
| a. Start circle, clockwise | | | |
| a. Start circle, off-centre (optional) | | | |
| b. First corridor end, 90° turn | | | |
| b. STORAGE turn, 135° | | | |
| b. Storage wall correction turn | | | |
| b. Ledge correction turn | | | |

### C8. Is rotation complete ever reached, and how long does it take? (Bench)

**Question.** The driver sets rotation complete only when the whole-degree error is exactly zero. Does that happen, and how long does it take?

**Procedure.** Call `Kobuki_Rotate_Angle_Setpoint(kobuki.angle, X)` once, then `Kobuki_Rotate_Angle(kobuki.angle)` every loop, for X = +90, −90, +135, −135 and +10. The completion flag in the snapshot is only visible from the next loop. Confirm the AMU turns anticlockwise for a positive X. Do 5 trials per X, from varied headings, including at least one start with the gyro reading within 20° of the ±180° wrap (the driver's own comment doubts its wrap handling). Measure the physical turn with the protractor or floor marks.

**Record.** If the flag is not reached within 5 s, write `not reached`, and record the whole-degree error over those 5 s (does it oscillate around zero?).

| X (°) | Trial | Start gyro (°) | Flag reached? | Time to flag (s) | Physical angle turned (°) | If not reached: error range over 5 s |
| --- | --- | --- | --- | --- | --- | --- |
| +90, −90, +135, −135, +10, 5 rows each | | | | | | |

### C9. Coasting angle after a stop at the search speed (Bench)

**Question.** How far does the AMU keep turning after the stop command?

**Estimate.** A few degrees. Not derived from any document: at about 37°/s each loop of delay adds about 0.75°, plus whatever the AMU takes to decelerate. Measure it.

**Procedure.** Spin anticlockwise with `Kobuki_Rotate(SEARCH_SPEED)` for at least 3 s so it is turning steadily. Call `Kobuki_Drive(STOP)` and save the gyro reading on that loop. After 2 s, save the gyro again. Coasting angle = the shortest signed difference. Do 10 trials.

**Record.**

| Trial | Gyro at stop command (hundredths of a degree) | Gyro 2 s later | Coasting angle (°) |
| --- | --- | --- | --- |
| 1–10, one row each | | | |

Summary: mean ___°, min ___°, max ___°.

### C10. Back-off experiment after the front cliff (Firmware)

**Question.** The diagram has no reverse step after the front cliff. Is one needed before the corner turns, and how far should it be?

**Procedure.** At the end of each corridor, after the front cliff stop, back off `Kobuki_Drive_Distance_Setpoint(kobuki.distance, −d)` for d = 0, 25 and 50 mm, then run the corner turn (90° at the end of the first corridor, 135° STORAGE turn at the end of the second). Five trials per distance, in both layouts, with the second person at the ledge. Also record the overrun: how far the AMU travelled from the first non-zero front cliff reading to standstill (tape on the floor), at each path speed from V1 you are considering.

**Record.**

| Layout | Corner | Back-off d (mm) | Cliff / bumper bytes during the turn | Cliff byte clear after the back-off? | Turn completed? | Hit a storage wall in the next 300 mm? | Overrun (mm) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A and B; 90° and 135°; d = 0, 25, 50 (5 trials each) | | | | | | | |

### C11. State durations against the section targets (Firmware)

**Question.** Does each layout meet the time targets, and how long does each state take?

**Targets.** Layout A: enter section 4 within 30 s and section 3 within 60 s. Layout B: enter section 2 within 30 s and section 3 within 60 s. Time from the B0 press. The target time is when the AMU centre crosses into the section; this is the safe (later) figure if the tutor counts the leading part.

**Procedure.** Run each layout at least 3 times back to back with the same program, as the final test does. For each run, record the loop count at entry to each state and multiply by the C2 period, or time them with the stopwatch and LED changes. ALIGN and DEPOSIT are stubs until their tasks are done; record them as `stub`. Whatever remains of the 30 s target after SEARCH, ORIENT and the drive to the section boundary is the time budget ALIGN can use.

**Record.**

| Layout | Run | SEARCH (s) | ALIGN (s) | ORIENT (s) | PATHFIND 1 (s) | PATHFIND 2 (s) | STORAGE (s) | DEPOSIT (s) | Section 4 or 2 entered at (s) | Section 3 entered at (s) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A | 1–3 | | stub | | | | | stub | | |
| B | 1–3 | | stub | | | | | stub | | |

## 5. What to do with the results

- V1–V7 become the constants in work item 3 (missing constants and helpers).
- C1 settles the bumper and cliff bit constants and the meaning of "front cliff".
- C4, C5 and C6 decide whether a settle-and-average step is needed after the SEARCH stop, and whether the layout bands can tell a blocking bollard from the other objects the sensor may see first.
- C8 decides whether rotation complete is usable as a transition condition or the firmware must accept a tolerance.
- C3 and C9 feed the sweep limit and the ALIGN contract.
- C10 decides whether a back-off step is added after the front cliff.
