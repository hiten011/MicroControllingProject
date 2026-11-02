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
The phase in which the AMU turns on the spot to find a bollard and learn the Layout. It spans Search and Identify, and ends when Identify stores Layout A or Layout B and enters Orient. If the AMU turns a full circle and finds nothing, it gives up into Fault with LED 1 green.
_Avoid_: Scan

**Search** (State):
The State in which the AMU turns anticlockwise, reading the ultrasonic every Tick, until a reading is within detection range. It gives up after a full turn.
_Avoid_: Spin, Scan

**Detection**:
A reading within detection range (750 mm). Search acts on Detections.
_Avoid_: Hit

**Identify**:
The State in which the AMU stops on the first Detection and compares that reading with the Layout split: under it the Layout is B (the nearer bollard B1), otherwise A (A1). It lights LED 1 red for A or yellow for B, then enters Orient. There is no centring on the bollard.
_Avoid_: Detect, Classify, Ping, sample

**Layout split**:
The range in millimetres (`LAYOUT_B_MAX_MM`) that separates Layout B from Layout A, compared with the first Detection. It is a calibration value, measured with the real sensor in place; it needs re-measuring if the sensor moves.
_Avoid_: threshold, LAYOUT_SPLIT, band

### Facing the corridor

**Orient**:
The State in which the AMU turns from wherever Search stopped to face the first corridor.
_Avoid_: turn-to-corridor, face-corridor

### Driving to the storage unit

**Corridor**:
One of the two straight legs of the open route. The AMU drives the first corridor, turns 90° at its end, then drives the second.
_Avoid_: path, lane, leg

**Corridor heading**:
The direction the AMU holds while driving a corridor. It is set afresh at the start of each corridor.
_Avoid_: bearing, heading (alone)

**Pathfind**:
The phase in which the AMU drives the first corridor and then the second, backs off a storage-wall bumper, and recovers from a ledge cliff. It spans Drive, Reverse, Rotate, Corridor turn, Corridor reverse and Corridor stop. Each corridor ends on the front cliff.
_Avoid_: navigate, route-follow, PATHFIND 1 / PATHFIND 2 (it is one phase run twice)

**Front cliff**:
The cliff sensor at the front centre of the AMU seeing the ledge ahead. It ends a corridor: the first in Corridor reverse, the second in Corridor stop.
_Avoid_: centre cliff, edge ahead

**Drive**:
The State in which the AMU drives forward along the current corridor until the front cliff. A storage-wall bumper or a ledge cliff sends it into Reverse.
_Avoid_: cruise

**Reverse**:
The State in which the AMU backs a fixed distance clear of the storage wall or the ledge after that side's bumper or cliff fires during Drive, then always enters Rotate.
_Avoid_: Realign, back-off

**Rotate**:
The State in which the AMU turns the nudge latched in Drive (`pathfind_recover_deg`), away from the storage wall or the ledge, then returns to Drive with no forward resume. It does not restore the corridor heading.
_Avoid_: Realign, Turn (Storage)

**Corridor reverse**:
The State at the end of the first corridor: the AMU backs the fixed Reverse distance off the front cliff, then enters Corridor turn.
_Avoid_: Reverse (alone, that is the storage-wall and ledge recovery), back-off

**Corridor turn**:
The State after Corridor reverse: the AMU turns on the spot onto the second corridor and latches a new corridor heading.
_Avoid_: Front-cliff turn, corner turn, mid-route turn

**Corridor stop**:
The State at the end of the second corridor: the AMU stops once, ending Pathfind, then moves straight into Storage Turn.
_Avoid_: Front-cliff stop, pathfind done, arrival

**Storage** (phase):
<<<<<<< Updated upstream
The phase in which the AMU faces the entrance, drives into the storage unit, and finishes at the Deposit State. It spans Turn, Drive-in and Deposit.
=======
The phase in which the AMU faces the entrance, drives into the storage unit, scans for the closest target, refines its heading, and adjusts to the configured ultrasonic range (currently 410 mm). It spans Turn, Drive-in, Deposit, Deposit align, Deposit refine turn/sample, Deposit distance, Deposit reverse and Deposit approach.
>>>>>>> Stashed changes
_Avoid_: STORAGE when the storage unit itself is meant

**Turn** (Storage):
The State in which the AMU turns on the spot from the second-corridor heading to face the Entrance.
_Avoid_: face-entrance, Rotate (Pathfind)

**Drive-in**:
The State in which the AMU drives a fixed distance through the Entrance into the storage unit. Any bumper ends it early, in Deposit.
_Avoid_: enter, DRIVE_IN (code id), Drive (already the Pathfind State)

**Deposit** (State):
The State after Drive-in in which the AMU stops inside the storage unit. Deposit-location sensing is not part of this State yet.
_Avoid_: DEPOSIT (code id), drop-off (the place is Deposit location)

<<<<<<< Updated upstream
=======
**Deposit align**:
The State that turns by the shortest route to the target heading. After Deposit it returns to the coarse heading within 1° and starts the local refinement. After refinement it returns to the centre of the nearest echo group, stops when the heading error is within 0.5°, and enters Deposit distance. B1 or wheeldrop stops it through Fault.

**Deposit refine turn/sample**:
The States that scan ±25° around the coarse heading in 2° steps. Turn uses 35 mm/s wheel speed; Sample stops for about 300 ms and takes the median of three readings spaced about 100 ms apart. After the sweep, the centre of the widest contiguous group within 10 mm of the local minimum becomes the refined heading. Tied groups prefer the coarse heading. With no valid echo, the AMU stops through Fault. B1 or wheeldrop stops either State immediately.

**Deposit distance**:
The stopped State after alignment or a distance correction. It waits about 300 ms for the ultrasonic output to catch up, then checks the live range. Below target minus 10 mm it enters Deposit reverse; above target plus 10 mm it enters Deposit approach; within that band it lights LED 1 green and enters Idle. The current target is 410 mm from the sensor face to the rod.

**Deposit reverse**:
The State that backs away at 50 mm/s until the ultrasonic reads at least target plus 100 mm, then stops in Deposit distance before approaching forward. The extra 100 mm provides room to finish the correction moving forward.

**Deposit approach**:
The State that moves forward at 35 mm/s until the ultrasonic reads at most target plus 10 mm, then stops in Deposit distance to check the settled reading. B1, wheeldrop, any bumper or cliff, or a zero/no-echo reading stops any Deposit distance-adjustment State through Fault.

>>>>>>> Stashed changes
### Running the FSM

**Idle**:
The State in which the AMU holds still until B0 (start) is pressed. It is the first State, and the State every run returns to. Leaving it forgets the last Search and starts a new one in Search.
_Avoid_: Standby, Ready, IDLE (code id)

**Fault**:
The State the AMU stops in when it must not go on: B1 pressed, a wheel dropped, or an unexpected bumper or cliff (which ones depends on the State). It stops the motors, then returns to Idle on the next Tick. The diagram calls it STOP / FAULT.
_Avoid_: Error, Stop, E-stop

**State**:
One box of the FSM diagram, named in capitals (SEARCH, IDENTIFY, ORIENT). In the firmware it is one arm of the switch in Src/main.c. One phase such as Search can span several States.
_Avoid_: Mode, Event

**Action**:
A condition paired with what to do when it holds: set a value the AMU remembers, or move to another State. A State lists its Actions in priority order.
_Avoid_: Event, Transition, Rule, Trigger

**Tick**:
One pass of the FSM, in which the current State's Actions are checked once. There is one Tick per received Kobuki packet.
_Avoid_: Step, Cycle, Loop
