#ifndef FSM_CONFIG_H
#define FSM_CONFIG_H

// Kobuki button bits: B0 = start, B1 = stop.
#define FSM_BUTTON_B0 0x01U
#define FSM_BUTTON_B1 0x02U

// Search. The Spin Step has no value yet: the number below is a placeholder (TBD).
#define FSM_SPIN_STEP_DEG     15
#define FSM_SWEEP_DEG         360
#define FSM_SEARCH_MAX_SWEEPS 2

// Layout ranges in mm. Placeholders until measured on the hardware.
// Layout A: far bollard, SPLIT < range <= A_MAX. Layout B: near bollard, B_MIN <= range <= SPLIT.
#define FSM_LAYOUT_B_MIN_MM 300U
#define FSM_LAYOUT_SPLIT_MM 450U
#define FSM_LAYOUT_A_MAX_MM 650U

// Kobuki angle: one tick is 0.01 degree and the reading wraps at +/-180 degrees.
#define FSM_ANGLE_TICKS_PER_DEG   100
#define FSM_ANGLE_TICKS_HALF_TURN 18000
#define FSM_ANGLE_TICKS_FULL_TURN 36000

#endif
