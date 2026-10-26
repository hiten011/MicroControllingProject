# AMU Parcel Run

An Autonomous Mobile Unit (AMU, a Kobuki robot) carries a parcel from its start circle across a bounded platform to a storage unit, steering by bollards and avoiding the platform's ledge.

## Language

### The test area

**Platform**:
The raised floor the AMU drives on, bounded on every side by the ledge.
_Avoid_: Arena, track

**Ledge**:
The drop at the platform's outer edge, which the AMU must never drive off.
_Avoid_: Edge, cliff (the cliff is what the sensor reports; the ledge is the thing)

**Start circle**:
The marked spot where the AMU always begins a run, facing an arbitrary direction.
_Avoid_: Start point, home

**Section**:
One of four numbered quadrants of the platform: 1 bottom-left, 2 top-left, 3 top-right, 4 bottom-right. Reaching them within set times earns marks.

**Storage unit**:
The fenced area on the platform that the AMU must enter completely to deliver the parcel.
_Avoid_: Freezer, fence

**Storage wall**:
A wall of the storage unit. It bounds a corridor on the side opposite the ledge.
_Avoid_: Wall (unqualified; ambiguous with the room walls)

**Room wall**:
One of the two fixed walls of the room beyond the platform's top and right sides. The AMU never meets or senses them.

**Entrance**:
The diagonal opening in the storage unit's top-right corner, which the AMU drives through.

**Deposit location**:
One of two spots (D1 or D2) inside the storage unit where the parcel is delivered, marked by a bollard-sized marker.

### Routes

**Path**:
A complete route from the start circle to the storage unit, made of two corridors.
_Avoid_: Track, route

**Corridor**:
One straight lane of a path, between a storage wall and the ledge. It is 400 mm wide, only slightly wider than the AMU.

**First corridor** / **Second corridor**:
The two corridors of a path, driven in that order.

**Layout A**:
Blocking bollards A1 and A2 close the left and top route, so the path is the bottom corridor then the right corridor.
_Avoid_: Layout 1 (Table 2 uses 1 and 2 to number the two back-to-back test runs, not the layouts)

**Layout B**:
Blocking bollards B1 and B2 close the bottom and right route, so the path is the left corridor then the top corridor.
_Avoid_: Layout 2

### Objects

**Bollard**:
A 90 mm-diameter cylinder standing in for a person. The AMU must never hit one.

**Blocking bollard**:
A bollard (A1, A2, B1 or B2) that closes one end of the path not to be used. A1 or B1 is the one the AMU can see from the start circle.

**External bollard**:
One of two bollards placed outside the platform near the start circle to stand in for stray people. They are not part of either layout.
_Avoid_: Human, random bollard

### Sensing

**Front cliff**:
A ledge detected straight ahead of the AMU. It marks the end of a corridor.
