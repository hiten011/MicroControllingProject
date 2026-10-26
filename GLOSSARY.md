# AMU navigation

The Autonomous Mobile Unit (AMU, a Kobuki robot) carries a parcel from the start circle through the storage unit. This glossary names the concepts its state machine uses.

## Language

**Centre heading**:
The heading at which the AMU faces the middle of the blocking bollard (A1 in Layout A, B1 in Layout B).
_Avoid_: target heading, centreline, theta target

**First-detection heading**:
The heading at which SEARCH stops, with the blocking bollard just inside the edge of the sensor's beam.
_Avoid_: stop heading, entry edge

**Alignment offset**:
The fixed angle, anticlockwise, from the first-detection heading to the centre heading.
_Avoid_: correction angle
