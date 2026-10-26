# AMU Navigation

The route an Autonomous Mobile Unit takes from its start position, through the test area, to the storage unit where it delivers a package.

## Language

**AMU**:
The Autonomous Mobile Unit: the Kobuki robot that carries the package.
_Avoid_: AMR, robot

**Bollard**:
A marker placed by humans (A1, A2, B1, B2) to block one route. The AMU must never drive into one.
_Avoid_: obstacle, cone

**Layout**:
Which pair of bollards is placed: A (A1/A2, blocking the left/top route) or B (B1/B2, blocking the bottom/right route). B is the mirror image of A.
_Avoid_: path combination

**Ledge**:
The drainage drop-off surrounding the warehouse. The AMU must never drive off it.
_Avoid_: edge, drop

**Corridor**:
One of the two straight legs of the open route. The AMU drives the first corridor, turns 90° at its end, then drives the second.
_Avoid_: path, lane, leg

**Heading**:
The direction the AMU holds while driving a corridor. It is set afresh at the start of each corridor.
_Avoid_: bearing, direction

**Storage unit**:
The enclosed bay the package is delivered into, reached through a diagonal entrance. The AMU drives round it and then into it.
_Avoid_: the unit, freezer, storage (when the unit itself is meant)

**Storage wall**:
The outer wall of the storage unit that runs alongside the corridors. In Layout A it is on the AMU's left and the ledge on its right; Layout B is the mirror image.
_Avoid_: unit wall, side wall

**Entrance**:
The diagonal opening in the storage unit that the AMU turns to face before driving in.
_Avoid_: door, gap

**Deposit location**:
One of the two spots inside the storage unit (1 or 2) where the package must be delivered, with the centre of the AMU facing it. It is independent of the Layout: all four combinations of bollards and deposit location can occur.
_Avoid_: drop point, bay

## Phases

**Pathfind**:
The phase in which the AMU drives the first corridor and then the second, keeping clear of the storage wall and the ledge.
_Avoid_: navigate, route-follow, PATHFIND 1 / PATHFIND 2 (it is one phase run twice)

**Storage** (phase):
The phase in which the AMU turns to face the entrance and drives into the storage unit.
_Avoid_: STORAGE when the storage unit itself is meant
