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

**Deposit marker**:
The blue, bollard-sized (90 mm) marker that stands at the Deposit location. The AMU finds it with the ultrasonic, faces its centre and stops a set gap short of it. Only one of the two Deposit locations has one.
_Avoid_: Bollard (that is an orange blocker), target, spot, deposit bollard

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
The phase in which the AMU faces the entrance and drives into the storage unit. It spans Turn and Drive-in, and ends when Drive-in enters the Deposit phase.
_Avoid_: STORAGE when the storage unit itself is meant

**Turn** (Storage):
The State in which the AMU turns on the spot from the second-corridor heading to face the Entrance.
_Avoid_: face-entrance, Rotate (Pathfind)

**Drive-in**:
The State in which the AMU drives a fixed distance through the Entrance into the storage unit. Any bumper ends it early. Either way it enters Deposit search.
_Avoid_: enter, DRIVE_IN (code id), Drive (already the Pathfind State)

### Depositing

**Deposit** (phase):
The phase in which the AMU finds the Deposit marker, faces its centre and moves to the Deposit gap from it. It spans Deposit search, Deposit track, Deposit edge scan, Deposit align, Measure, Deposit backoff and Deposit drive, and ends in Idle. It never gives up, and only B1 or a wheel drop sends it to Fault.
_Avoid_: DEPOSIT (code id), drop-off (the place is Deposit location)

**Deposit search**:
The State in which the AMU turns anticlockwise on the spot, reading the ultrasonic every Tick, until a reading is within the Deposit detection range (500 mm). It never gives up.
_Avoid_: Search (that is the layout-finding State), Scan

**Deposit detection range**:
The range in millimetres (500) within which a reading counts as the Deposit marker. It is tighter than the Detection range (750) that Search uses.
_Avoid_: Detection (that is the 750 mm Search term), threshold

**Lost angle**:
The turn in degrees (10) the AMU makes past the last reading of the Deposit marker before it decides it has gone past it. Deposit track and Deposit edge scan both use it.
_Avoid_: margin, overshoot

**Edge band**:
How far beyond the closest reading, in millimetres (60), a reading may be and still count as an edge of the Deposit marker in Deposit edge scan.
_Avoid_: tolerance, window

**Deposit track**:
The State in which the AMU keeps turning anticlockwise over the Deposit marker, recording the closest reading, until the marker has been lost for the lost angle.
_Avoid_: Follow

**Deposit edge scan**:
The State in which the AMU turns slowly clockwise back across the Deposit marker. Readings within the edge band of the closest reading mark where it starts and ends, and the middle of those two is the Centre heading.
_Avoid_: Sweep, Detect

**Deposit align**:
The State in which the AMU turns from where Deposit edge scan stopped to the Centre heading.
_Avoid_: centre-up, correct heading

**Centre heading**:
The heading at which the AMU faces the middle of the Deposit marker.
_Avoid_: target heading, bearing, heading (alone)

**Measure**:
The State in which the AMU stops on the Centre heading and takes the median of five ultrasonic readings as its distance to the Deposit marker. It sends the AMU to Deposit backoff, Deposit drive or Idle.
_Avoid_: Ping, sample, Detect

**Blind zone**:
The range inside which the ultrasonic cannot say how far an object is, because it reads about 300 mm for anything nearer. A reading under the blind limit (320 mm) means the Deposit marker is too close to know its distance.
_Avoid_: dead zone, minimum range

**Deposit backoff**:
The State in which the AMU reverses a fixed 150 mm straight back along the Centre heading, then returns to Measure, so the Deposit marker can leave the Blind zone. It runs at most five times; after that a Blind zone reading in Measure ends in Idle.
_Avoid_: Reverse (the storage-wall and ledge recovery), Corridor reverse, back-off

**Deposit drive**:
The State in which the AMU drives the signed distance Measure worked out, forward or in reverse, and ends in Idle. A bumper ends a forward drive early.
_Avoid_: Drive (the Pathfind State), Drive-in, DEPOSIT_DRIVE (code id)

**Deposit gap**:
The distance, 100 mm, between the front of the AMU and the surface of the Deposit marker when the AMU stops. The brief asks for "100 mm away" and gives no tolerance.
_Avoid_: stand-off, clearance, distance (alone)

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
