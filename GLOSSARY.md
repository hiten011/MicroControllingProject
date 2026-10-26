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
The outer wall of the storage unit that runs alongside the corridors. In Layout A it is on the AMU's left and the ledge on its right; Layout B is the mirror image.
_Avoid_: unit wall, side wall

**Entrance**:
The diagonal opening in the storage unit that the AMU turns to face before driving in.
_Avoid_: door, gap

**Deposit location**:
One of the two spots inside the storage unit (1 or 2) where the parcel must be delivered, with the centre of the AMU facing it. It is independent of the Layout: all four combinations of bollards and deposit location can occur.
_Avoid_: drop point, bay

### Finding the layout

**Search**:
The phase in which the AMU looks for a bollard so that it can learn the Layout. It ends when a Layout is learned or when the AMU gives up.
_Avoid_: Scan

**Detect**:
One ultrasonic reading taken while the AMU is stationary, recording whether an object is within detection range.
_Avoid_: Ping, sample

**Detection**:
A Detect reading in which an object is within detection range. The first Detection ends the Search and fixes the Layout.
_Avoid_: Hit

**Spin**:
Turning the AMU on the spot by one Step between two Detect readings.
_Avoid_: Move (already means driving a distance), Turn (already used by the storage and pathfind turns)

**Step**:
The fixed angle the AMU turns in one Spin.
_Avoid_: Increment

**Sweep**:
One full 360 degree revolution of Steps without a Detection. The Search gives up after a fixed number of Sweeps.
_Avoid_: Lap, rotation

### Aligning

**Centre heading**:
The heading at which the AMU faces the middle of the blocking bollard (A1 in Layout A, B1 in Layout B).
_Avoid_: target heading, centreline, theta target

**First-detection heading**:
The heading at which SEARCH stops, with the blocking bollard just inside the edge of the sensor's beam.
_Avoid_: stop heading, entry edge

**Alignment offset**:
The fixed angle, anticlockwise, from the first-detection heading to the centre heading.
_Avoid_: correction angle

### Driving to the storage unit

**Corridor**:
One of the two straight legs of the open route. The AMU drives the first corridor, turns 90° at its end, then drives the second.
_Avoid_: path, lane, leg

**Corridor heading**:
The direction the AMU holds while driving a corridor. It is set afresh at the start of each corridor.
_Avoid_: bearing, heading (alone, which also names the Centre heading)

**Pathfind**:
The phase in which the AMU drives the first corridor and then the second, keeping clear of the storage wall and the ledge.
_Avoid_: navigate, route-follow, PATHFIND 1 / PATHFIND 2 (it is one phase run twice)

**Storage** (phase):
The phase in which the AMU turns to face the entrance and drives into the storage unit.
_Avoid_: STORAGE when the storage unit itself is meant

### Running the FSM

**State**:
One box of the FSM diagram, named in capitals (SEARCH, DETECT, SPIN). It holds an ordered list of Actions, and one phase such as Search can span several States.
_Avoid_: Mode, Event

**Action**:
A condition paired with what to do when it holds: set a value the AMU remembers, or move to another State. A State lists its Actions in priority order.
_Avoid_: Event, Transition, Rule, Trigger

**Tick**:
One pass of the FSM, in which the current State's Actions are checked once. There is one Tick per received Kobuki packet.
_Avoid_: Step (already the Spin angle), Cycle, Loop
