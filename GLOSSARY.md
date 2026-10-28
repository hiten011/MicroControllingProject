# AMU parcel run

The Autonomous Mobile Unit (AMU) that carries a parcel through the test area to the storage unit. This glossary holds the project's own terms, so the FSM diagram, the report and the code use the same words.

## Language

**AMU**:
The Autonomous Mobile Unit: the Kobuki robot, driven by the Nucleo board, that carries a parcel from the collection location to the storage unit.
_Avoid_: AMR, robot

**Bollard**:
A marker placed by the humans to block one end of a path. The AMU must never drive into one.

**Layout**:
One of the two bollard arrangements in the test area, A or B. A has bollards A1/A2 blocking the left/top route, B has bollards B1/B2 blocking the bottom/right route. B is the mirror image of A.
_Avoid_: Case, scenario, path combination

**Ledge**:
The drainage drop-off surrounding the warehouse. The AMU must never drive off it.
_Avoid_: edge, drop

### The storage unit

**Storage unit**:
The enclosed bay the parcel is delivered into, reached through a diagonal entrance. The AMU drives round it and then into it.
_Avoid_: the unit, freezer, storage (when the unit itself is meant)

**Storage wall**:
The outer wall of the storage unit that runs alongside the corridors. With Orient A = −90° / B = +90°, Layout A has it on the AMU's left and the ledge on its right; Layout B is the mirror image.
_Avoid_: unit wall, side wall

**Entrance**:
The diagonal opening in the storage unit that the AMU turns to face before driving in.
_Avoid_: door, gap

**Deposit location**:
One of the two spots inside the storage unit (1 or 2) where the parcel must be delivered, with the centre of the AMU facing it. It is independent of the Layout: all four combinations of bollards and deposit location can occur.
_Avoid_: drop point, bay

### Finding the layout

**Search** (phase):
The phase in which the AMU turns on the spot to find a bollard and learn the Layout. It spans Clear, Search, Track, Edge scan and Detect, and ends when Detect stores Layout A or Layout B and enters Align. If the scan finds nothing, or never loses the object, it gives up into Fault with LED 1 green.
_Avoid_: Scan (alone, which also names the turning in Edge scan)

**Scan angle**:
The net turn in degrees since B0, anticlockwise positive, summed from gyro changes so it does not wrap at 180°. Every scan bearing (first seen, last seen, edges, centre) is a Scan angle.
_Avoid_: bearing, heading (alone, which also names the Centre heading)

**Clear**:
The State in which the AMU turns clockwise until the ultrasonic reads nothing within detection range, so Search never starts on the bollard. It gives up after one full turn.
_Avoid_: Clear start, CLEAR_START (friend's code id)

**Search** (State):
The State in which the AMU turns anticlockwise, reading the ultrasonic every Tick, until a reading is within detection range. It gives up after a full turn plus the give-up angle.
_Avoid_: Spin, Scan

**Track**:
The State in which the AMU keeps turning anticlockwise over the object, recording the closest reading and the Scan angle it was last seen at. It ends once the object has been lost for the lost angle, which means the AMU is just past it.
_Avoid_: Follow, Spin

**Edge scan**:
The State in which the AMU turns slowly clockwise back across the bollard. Readings within the edge band of the closest reading mark where the bollard starts and ends, and the middle of those two Scan angles is the Centre heading's Scan angle. It gives up if it passes back beyond where the object was first seen.
_Avoid_: Detailed scan, Sweep

**Detection**:
A reading within detection range (750 mm). Search, Track and Edge scan act on Detections.
_Avoid_: Hit

**Detect**:
The State in which the closest reading of the Edge scan is compared with the Layout split: under it the Layout is B (the nearer bollard B1), otherwise A (A1). It lights LED 1 red for A or yellow for B, then enters Align.
_Avoid_: Classify, Ping, sample

**Layout split**:
The range in millimetres (`LAYOUT_B_MAX_MM`) that separates Layout B from Layout A. It is a calibration value, measured with the real sensor in place; it needs re-measuring if the sensor moves.
_Avoid_: threshold, LAYOUT_SPLIT, band

### Aligning

**Align**:
The State in which the AMU turns from where Edge scan stopped to the centre heading.
_Avoid_: centre-up, correct heading

**Centre heading**:
The heading at which the AMU faces the middle of the blocking bollard (A1 in Layout A, B1 in Layout B).
_Avoid_: target heading, centreline, theta target

**Orient**:
The State in which the AMU turns from the centre heading to face the first corridor.
_Avoid_: turn-to-corridor, face-corridor

### Driving to the storage unit

**Corridor**:
One of the two straight legs of the open route. The AMU drives the first corridor, turns 90° at its end, then drives the second.
_Avoid_: path, lane, leg

**Corridor heading**:
The direction the AMU holds while driving a corridor. It is set afresh at the start of each corridor.
_Avoid_: bearing, heading (alone, which also names the Centre heading)

**Pathfind**:
The phase in which the AMU drives the first corridor and then the second for the corridor length, and backs off a storage-wall bumper. It spans Drive, Reverse, Rotate, Corridor turn and Corridor stop. Pathfind does not read the cliff sensors.
_Avoid_: navigate, route-follow, PATHFIND 1 / PATHFIND 2 (it is one phase run twice)

**Front cliff**:
The cliff sensor at the front centre of the AMU seeing the ledge ahead. Pathfind Drive does not read it, and it does not end a corridor.
_Avoid_: centre cliff, edge ahead

**Corridor length**:
The left-wheel travel that ends a corridor, from the tick latched by `Corridor_Begin`. Layout A is 1200 mm then 1400 mm. Layout B is 1400 mm then 1200 mm. Reverse and Rotate stay inside that travel because returning to Drive does not call `Corridor_Begin` again.
_Avoid_: remaining distance, front-cliff end

**Drive**:
The State in which the AMU drives forward along the current corridor until the corridor length is reached. A storage-wall bumper sends it into Reverse. The front cliff and the side cliff sensors do not end Drive and do not send it into Reverse.
_Avoid_: PATHFIND_DRIVE (code id), cruise

**Reverse**:
The State in which the AMU backs a fixed distance clear of the storage wall after the storage-wall bumper fires during Drive, then always enters Rotate.
_Avoid_: Realign, back-off, PATHFIND_REVERSE (code id)

**Rotate**:
The State in which the AMU turns the nudge latched at Reverse entry (`pathfind_recover_deg`), away from the storage wall, then returns to Drive with no forward resume and without starting the corridor length again. It does not restore the corridor heading.
_Avoid_: Realign, Turn (Storage), PATHFIND_ROTATE (code id)

**Corridor turn**:
The State at the end of the first corridor length: the AMU turns 90° onto the second corridor, latches a new corridor heading, and calls `Corridor_Begin(2)`.
_Avoid_: Front-cliff turn, corner turn, mid-route turn

**Corridor stop**:
The State at the end of the second corridor length: the AMU stops once, ending Pathfind, then moves straight into Storage Turn.
_Avoid_: Front-cliff stop, pathfind done, arrival

**Storage** (phase):
The phase in which the AMU faces the entrance, drives into the storage unit, and finishes at the Deposit State. It spans Turn, Drive-in and Deposit.
_Avoid_: STORAGE when the storage unit itself is meant

**Turn** (Storage):
The State in which the AMU turns on the spot from the second-corridor heading to face the Entrance.
_Avoid_: face-entrance, Rotate (Pathfind), STORAGE_TURN (code id)

**Drive-in**:
The State in which the AMU drives a fixed distance through the Entrance into the storage unit. Any bumper ends it early, in Deposit.
_Avoid_: enter, DRIVE_IN (code id), Drive (already the Pathfind State)

**Deposit** (State):
The State after Drive-in in which the AMU stops inside the storage unit. Deposit-location sensing is not part of this State yet.
_Avoid_: DEPOSIT (code id), drop-off (the place is Deposit location)

### Running the FSM

**Idle**:
The State in which the AMU holds still until B0 (start) is pressed. It is the first State, and the State every run returns to. Leaving it forgets the last Search and starts a new one from Clear.
_Avoid_: Standby, Ready, IDLE (code id)

**Fault**:
The State the AMU stops in when it must not go on: B1 pressed, a wheel dropped, or an unexpected bumper or cliff (which ones depends on the State). It stops the motors, then returns to Idle on the next Tick. The diagram calls it STOP / FAULT.
_Avoid_: Error, Stop, E-stop

**State**:
One box of the FSM diagram, named in capitals (SEARCH, TRACK, EDGE SCAN). In the firmware it is one arm of the switch in Src/main.c. One phase such as Search can span several States.
_Avoid_: Mode, Event

**Action**:
A condition paired with what to do when it holds: set a value the AMU remembers, or move to another State. A State lists its Actions in priority order.
_Avoid_: Event, Transition, Rule, Trigger

**Tick**:
One pass of the FSM, in which the current State's Actions are checked once. There is one Tick per received Kobuki packet.
_Avoid_: Step, Cycle, Loop
