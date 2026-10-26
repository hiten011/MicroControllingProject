# AMU parcel run

The Autonomous Mobile Unit (AMU) that carries a parcel through the test area to the storage unit. This glossary holds the project's own terms, so the FSM diagram, the report and the code use the same words.

## Language

**AMU**:
The Autonomous Mobile Unit: the Kobuki robot, driven by the Nucleo board, that carries a parcel from the collection location to the storage unit.
_Avoid_: AMR, robot

**Bollard**:
A marker placed by the humans to block one end of a path. The AMU must never drive into one.

**Layout**:
One of the two bollard arrangements in the test area, A or B. A has bollards A1/A2 blocking the left/top route, B has bollards B1/B2 blocking the bottom/right route.
_Avoid_: Case, scenario

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
