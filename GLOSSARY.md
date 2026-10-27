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
The outer wall of the storage unit that runs alongside the corridors. In Layout A it is on the AMU's right and the ledge on its left; Layout B is the mirror image.
_Avoid_: unit wall, side wall

**Entrance**:
The diagonal opening in the storage unit that the AMU turns to face before driving in.
_Avoid_: door, gap

**Deposit location**:
One of the two spots inside the storage unit (1 or 2) where the parcel must be delivered, with the centre of the AMU facing it. It is independent of the Layout: all four combinations of bollards and deposit location can occur.
_Avoid_: drop point, bay

### Finding the layout

**Search**:
The phase in which the AMU looks for a bollard so that it can learn the Layout. It ends when SEARCH stores Layout A or Layout B and enters Align, or when a Detection has no Layout and SEARCH enters Fault.
_Avoid_: Scan

**Detect**:
One ultrasonic reading taken while the AMU is stationary, recording whether an object is within detection range.
_Avoid_: Ping, sample

**Detection**:
A Detect reading in which an object is within detection range. SEARCH stores the Layout and enters Align when the class is A or B. When the class is LAYOUT_NONE, SEARCH enters Fault and leaves the stored Layout unchanged.
_Avoid_: Hit

**Spin**:
Turning the AMU on the spot by one Step between two Detect readings.
_Avoid_: Move (already means driving a distance), Turn (Storage), Rotate (Pathfind)

**Step**:
The fixed angle the AMU turns in one Spin.
_Avoid_: Increment

**Sweep**:
One full 360 degree revolution of Steps. The Search no longer counts Sweeps or gives up on a Sweep limit; DETECT and SPIN keep running until a Detection.
_Avoid_: Lap, rotation

### Aligning

**Align**:
The State in which the AMU turns from the first-detection heading to the centre heading.
_Avoid_: centre-up, correct heading

**Centre heading**:
The heading at which the AMU faces the middle of the blocking bollard (A1 in Layout A, B1 in Layout B).
_Avoid_: target heading, centreline, theta target

**First-detection heading**:
The heading at which SEARCH stops, with the blocking bollard just inside the edge of the sensor's beam.
_Avoid_: stop heading, entry edge

**Alignment offset**:
The fixed angle, anticlockwise, from the first-detection heading to the centre heading.
_Avoid_: correction angle

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
The phase in which the AMU drives the first corridor and then the second, keeping clear of the storage wall and the ledge. It spans Drive, Reverse, Rotate, Corridor turn and Corridor stop.
_Avoid_: navigate, route-follow, PATHFIND 1 / PATHFIND 2 (it is one phase run twice)

**Corridor length**:
The fixed distance the AMU must travel along each corridor before that corridor ends. Progress is net forward travel from the start of the corridor (reversals count against it).
_Avoid_: path length, segment length

**Drive**:
The State in which the AMU drives forward along the current corridor until the Corridor length is reached, or the storage wall or the ledge intervenes. A storage-wall bumper or ledge cliff sends it into Reverse; reaching the Corridor length ends the corridor.
_Avoid_: PATHFIND_DRIVE (code id), cruise

**Reverse**:
The State in which the AMU backs a fixed distance clear of the storage wall or the ledge after that side's bumper or cliff fires during Drive, then always enters Rotate.
_Avoid_: Realign, back-off, PATHFIND_REVERSE (code id)

**Rotate**:
The State in which the AMU turns a fixed angle after Reverse (Layout A anticlockwise, Layout B clockwise), then returns to Drive with no forward resume. It does not restore the corridor heading.
_Avoid_: Realign, Spin (Search), Turn (Storage), PATHFIND_ROTATE (code id)

**Corridor turn**:
The State at the end of the first corridor: the AMU turns 90° onto the second corridor and latches a new corridor heading.
_Avoid_: Front-cliff turn, corner turn, mid-route turn

**Corridor stop**:
The State at the end of the second corridor: the AMU stops once, ending Pathfind, then moves straight into Storage Turn.
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
The State in which the AMU holds still until B0 (start) is pressed. It is the first State, and the State every run returns to. Leaving it forgets the last Search.
_Avoid_: Standby, Ready, IDLE (code id)

**Fault**:
The State the AMU stops in when it must not go on: B1 pressed, a wheel dropped, an unexpected bumper or cliff (which ones depends on the State), or a Detection whose layout is LAYOUT_NONE. It stops the motors, then returns to Idle on the next Tick. The diagram calls it STOP / FAULT.
_Avoid_: Error, Stop, E-stop

**State**:
One box of the FSM diagram, named in capitals (SEARCH, DETECT, SPIN). It holds an ordered list of Actions, and one phase such as Search can span several States.
_Avoid_: Mode, Event

**Action**:
A condition paired with what to do when it holds: set a value the AMU remembers, or move to another State. A State lists its Actions in priority order.
_Avoid_: Event, Transition, Rule, Trigger

**Tick**:
One pass of the FSM, in which the current State's Actions are checked once. There is one Tick per received Kobuki packet.
_Avoid_: Step (already the Spin angle), Cycle, Loop
