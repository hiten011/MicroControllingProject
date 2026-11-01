#include <stdint.h>
#include <stdbool.h>

#define STM32G474xx

#include "stm32g474xx.h"
#include "stm32g4xx.h"
#include "uart.h"
#include "helper.h"


// --------------------------------------------------
// TUNING (all distances in mm, angles in 0.01 degree "ticks" unless stated)
// --------------------------------------------------

// Ultrasonic
#define SENSOR_OFFSET_MM      0     // sensor face ahead of the Kobuki centre (measure it, then set it)
#define BOLLARD_DETECT_MM     700   // range that counts as "a bollard" while searching at the start
#define DEPOSIT_DETECT_MM     450   // range that counts as "the marker" inside the storage unit

// Searching / scanning (wheel speeds, mm/s)
#define SEARCH_SPIN_SPEED     120   // fast anticlockwise spin to find the bollard (about 60 deg/s)
#define SCAN_SPIN_SPEED       50    // slow spin across the bollard edges (about 25 deg/s)
#define SEARCH_MAX_TICKS      72000L // give up after two full turns
#define SCAN_MAX_TICKS        12000L // give up if an edge is not found within 120 degrees
#define DETECT_CONFIRM        3     // consecutive readings that confirm a detection
#define LOST_CONFIRM          6     // consecutive readings that confirm the bollard has gone

// Orientation: from the bollard centre line to the corridor.
// The 48 / 64 ticks remove the 5 mm sideways offset of A1 / B1 from the start position.
#define ORIENT_A_TICKS        (-9048)  // Layout A: clockwise 90 deg to the bottom corridor
#define ORIENT_B_TICKS        (9064)   // Layout B: anticlockwise 90 deg to the left corridor

// Pathfinding
#define DRIVE_SPEED           130   // mm/s, below the 150 mm/s limit
#define NUDGE_TICKS           150   // 1.5 deg steer away from a wall / ledge after a touch
#define CORNER_A_TICKS        9000  // Layout A: anticlockwise onto the second corridor
#define CORNER_B_TICKS        (-9000)
#define STORAGE_TURN_A_TICKS  13500 // 135 deg to face the entrance
#define STORAGE_TURN_B_TICKS  (-13500)
#define STORAGE_IN_MM         850   // drive through the entrance to the middle of the storage unit

// Deposit: stop with the robot edge 100 mm from the marker surface
#define ROBOT_RADIUS_MM       177
#define DEPOSIT_GAP_MM        100
#define DEPOSIT_TRIM_MM       0     // calibration: + stops further away, - closer
#define DEPOSIT_MIN_VALID_MM  360   // below this the sensor may be at its 300 mm minimum
#define DEPOSIT_BACKUP_MM     100   // reverse this far to get a valid reading
#define DEPOSIT_SETTLE_TICKS  12    // let the ultrasonic reading catch up (20 ms each)
#define DEPOSIT_SAMPLES       10

// Heading control
#define ROTATE_TOL_TICKS      30    // finish a turn within 0.3 degrees
#define ROTATE_MIN_SPEED      30
#define ROTATE_GAIN           3     // mm/s per degree of error
#define ROTATE_MAX_SPEED      140
#define HEADING_GAIN          2.0   // rad/s of turn per rad of heading error
#define HEADING_MAX_RATE      0.25  // rad/s
#define HALF_WHEEL_BASE       115.0 // mm


// --------------------------------------------------
// FSM STATES
// --------------------------------------------------

typedef enum {
    ST_IDLE,
    ST_SEARCH,                 // fast spin until the ultrasonic sees something
    ST_SCAN_CW,                // slow spin clockwise to the near edge
    ST_SCAN_CCW,               // slow spin anticlockwise across to the far edge
    ST_ALIGN,                  // face the centre of the object
    ST_ORIENT,                 // turn from the bollard onto the corridor
    ST_PATHFIND_DRIVE,
    ST_PATHFIND_REVERSE,
    ST_PATHFIND_ROTATE,
    ST_PATHFIND_CORRIDOR_TURN,
    ST_PATHFIND_CORRIDOR_STOP,
    ST_STORAGE_TURN,
    ST_STORAGE_DRIVE_IN,
    ST_STORAGE_DEPOSIT,        // measure the distance to the marker
    ST_STORAGE_MOVE,           // reverse for a valid reading, or drive up to the marker
    ST_FAULT
} State;

static State state = ST_IDLE;


// --------------------------------------------------
// SHARED VARIABLES
// --------------------------------------------------

static bool     deposit_phase;        // false: looking for a bollard, true: looking for the marker
static int32_t  swept_ticks;          // angle spun in the current search / scan pass
static int16_t  last_angle;
static int16_t  edge_cw;              // gyro angle where the clockwise edge was lost
static int16_t  edge_ccw;             // gyro angle where the anticlockwise edge was lost
static int16_t  target_angle;         // gyro angle of the object centre
static uint16_t range_min;            // closest reading while the object was in view
static uint8_t  detect_run;           // consecutive readings with the object in view
static uint8_t  lost_run;             // consecutive readings without it
static int16_t  lost_angle;           // gyro angle at the first reading without it
static bool     scan_seen;
static int16_t  heading;              // gyro heading to hold while driving
static int16_t  nudge_ticks;
static int16_t  move_mm;
static bool     backed_up;
static uint8_t  dep_tick;
static uint8_t  dep_count;
static uint32_t dep_sum;


// --------------------------------------------------
// ANGLE HELPERS
// --------------------------------------------------

// Shortest signed angle from one gyro reading to another (+ = anticlockwise).
static int32_t angle_diff(int16_t from, int16_t to)
{
    int32_t d = (int32_t)to - (int32_t)from;

    while (d > 18000) {
        d -= 36000;
    }
    while (d <= -18000) {
        d += 36000;
    }
    return d;
}

static int16_t angle_add(int16_t angle, int32_t delta)
{
    int32_t a = (int32_t)angle + delta;

    while (a >= 18000) {
        a -= 36000;
    }
    while (a < -18000) {
        a += 36000;
    }
    return (int16_t)a;
}

// Turn on the spot to an exact gyro angle. Returns true once within 0.3 degrees.
static bool rotate_to(int16_t target)
{
    int32_t error = angle_diff(robot.kobuki.angle, target);
    int32_t speed;

    if (error <= ROTATE_TOL_TICKS && error >= -ROTATE_TOL_TICKS) {
        Kobuki_Rotate(0);
        return true;
    }

    speed = ((error >= 0) ? ROTATE_MIN_SPEED : -ROTATE_MIN_SPEED) + (ROTATE_GAIN * error) / 100;

    if (speed > ROTATE_MAX_SPEED) {
        speed = ROTATE_MAX_SPEED;
    } else if (speed < -ROTATE_MAX_SPEED) {
        speed = -ROTATE_MAX_SPEED;
    }

    Kobuki_Rotate((int16_t)speed);
    return false;
}

// Drive forward holding a gyro heading by steering a very gentle arc.
// The faster wheel never goes above "speed", so the 150 mm/s limit cannot be broken.
static void drive_heading(int16_t speed, int16_t target)
{
    double error_rad = (double)angle_diff(robot.kobuki.angle, target) * 0.01 * 0.0174533;
    double rate = HEADING_GAIN * error_rad;     // wanted turn rate, + = anticlockwise
    double radius;
    double abs_radius;
    int16_t radius_mm = 0;
    uint8_t payload[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x00};

    if (rate > HEADING_MAX_RATE) {
        rate = HEADING_MAX_RATE;
    } else if (rate < -HEADING_MAX_RATE) {
        rate = -HEADING_MAX_RATE;
    }

    if (rate > 0.002 || rate < -0.002) {
        radius = (double)speed / rate;          // + = turn left
        if (radius > 32000.0) {
            radius = 32000.0;
        } else if (radius < -32000.0) {
            radius = -32000.0;
        }
        abs_radius = (radius < 0.0) ? -radius : radius;
        speed = (int16_t)((double)speed / (1.0 + HALF_WHEEL_BASE / abs_radius));
        radius_mm = (int16_t)radius;
    }

    if (speed > SPEED_LIMIT) {
        speed = SPEED_LIMIT;
    }

    payload[2] = (uint8_t)speed;
    payload[3] = (uint8_t)(speed >> 8);
    payload[4] = (uint8_t)radius_mm;
    payload[5] = (uint8_t)(radius_mm >> 8);
    Kobuki_Tx(payload, sizeof(payload));
}


// --------------------------------------------------
// ULTRASONIC (read once per packet)
// --------------------------------------------------

static void sample_ultrasonic(void)
{
    uint16_t limit = deposit_phase ? DEPOSIT_DETECT_MM : BOLLARD_DETECT_MM;
    bool in_view;

    Robot_ReadUltrasonic();

    in_view = ((uint32_t)robot.object.range_mm + SENSOR_OFFSET_MM) < limit;
    robot.object.detected = in_view ? 1U : 0U;

    if (in_view) {
        if (detect_run < 255) {
            detect_run++;
        }
        lost_run = 0;

        if (robot.object.range_mm < range_min) {
            range_min = robot.object.range_mm;
        }
    } else {
        if (lost_run == 0) {
            lost_angle = robot.kobuki.angle;   // where the object was first missed
        }
        if (lost_run < 255) {
            lost_run++;
        }
        detect_run = 0;
    }
}


// --------------------------------------------------
// FAULT DETECTION
// --------------------------------------------------

static bool b1_or_wheeldrop(void)
{
    return (robot.kobuki.button & BUTTON_B1) != 0 ||
           robot.kobuki.wheeldrop != 0;
}

static bool fault_all(void)
{
    return b1_or_wheeldrop() ||
           robot.kobuki.bumper != 0 ||
           robot.kobuki.cliff != 0;
}

static bool fault_ignore_cliff(void)
{
    return b1_or_wheeldrop() ||
           robot.kobuki.bumper != 0;
}


// --------------------------------------------------
// CORRIDOR LENGTH
// --------------------------------------------------

static int16_t corridor_length_mm(void)
{
    if (robot.layout == LAYOUT_A) {

        return (robot.corridor == 1)
            ? PATHFIND_CORRIDOR_1_A_MM
            : PATHFIND_CORRIDOR_2_A_MM;
    }

    return (robot.corridor == 1)
        ? PATHFIND_CORRIDOR_1_B_MM
        : PATHFIND_CORRIDOR_2_B_MM;
}


// --------------------------------------------------
// STATE TRANSITIONS (what happens once, on entering a state)
// --------------------------------------------------

static void change_to(State next)
{
    state = next;

    switch (next) {

        case ST_SEARCH:
            swept_ticks = 0;
            last_angle = robot.kobuki.angle;
            range_min = 0xFFFFU;
            break;

        case ST_SCAN_CW:
            swept_ticks = 0;
            last_angle = robot.kobuki.angle;
            break;

        case ST_SCAN_CCW:
            swept_ticks = 0;
            last_angle = robot.kobuki.angle;
            scan_seen = false;
            break;

        case ST_ORIENT:
            heading = angle_add(target_angle,
                                (robot.layout == LAYOUT_A) ? ORIENT_A_TICKS : ORIENT_B_TICKS);
            break;

        case ST_PATHFIND_REVERSE:
            Robot_Move(-PATHFIND_REVERSE_MM, 0);
            break;

        case ST_PATHFIND_ROTATE:
            heading = angle_add(heading, nudge_ticks);
            break;

        case ST_PATHFIND_CORRIDOR_TURN:
            heading = angle_add(heading,
                                (robot.layout == LAYOUT_A) ? CORNER_A_TICKS : CORNER_B_TICKS);
            break;

        case ST_PATHFIND_CORRIDOR_STOP:
            Robot_Stop();
            break;

        case ST_STORAGE_TURN:
            heading = angle_add(heading,
                                (robot.layout == LAYOUT_A) ? STORAGE_TURN_A_TICKS : STORAGE_TURN_B_TICKS);
            break;

        case ST_STORAGE_DRIVE_IN:
            robot.corridor_start_distance = robot.kobuki.distance;
            break;

        case ST_STORAGE_DEPOSIT:
            Robot_Stop();
            dep_tick = 0;
            dep_count = 0;
            dep_sum = 0;
            break;

        case ST_STORAGE_MOVE:
            Robot_Move(move_mm, 0);
            break;

        case ST_FAULT:
            Robot_Stop();
            break;

        case ST_IDLE:
        case ST_ALIGN:
        case ST_PATHFIND_DRIVE:
            break;
    }
}

// Start looking for the deposit marker from the middle of the storage unit.
static void start_deposit_search(void)
{
    deposit_phase = true;
    backed_up = false;
    change_to(ST_SEARCH);
}


// --------------------------------------------------
// MAIN
// --------------------------------------------------

int main(void)
{
    UART_Init();
    Robot_Init();

    while (1) {

        Robot_ReadPacket();
        sample_ultrasonic();

        switch (state) {

            // --------------------------------------
            // IDLE
            // --------------------------------------

            case ST_IDLE:

                if ((robot.kobuki.button & BUTTON_B0) != 0) {

                    robot.layout = LAYOUT_NONE;
                    robot.object.detected = 0;
                    robot.heading = 0;
                    robot.corridor = 0;
                    deposit_phase = false;
                    backed_up = false;

                    change_to(ST_SEARCH);
                    break;
                }

                Robot_Stop();
                break;


            // --------------------------------------
            // SEARCH: fast anticlockwise spin until something is in view
            // --------------------------------------

            case ST_SEARCH:

                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (detect_run >= DETECT_CONFIRM) {
                    change_to(ST_SCAN_CW);
                    break;
                }

                if (swept_ticks >= SEARCH_MAX_TICKS) {
                    change_to(ST_FAULT);
                    break;
                }

                swept_ticks += angle_diff(last_angle, robot.kobuki.angle);
                last_angle = robot.kobuki.angle;
                Kobuki_Rotate(SEARCH_SPIN_SPEED);
                break;


            // --------------------------------------
            // SCAN CW: slow clockwise spin until the object is lost (near edge)
            // --------------------------------------

            case ST_SCAN_CW:

                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (lost_run >= LOST_CONFIRM) {
                    edge_cw = lost_angle;
                    change_to(ST_SCAN_CCW);
                    break;
                }

                if (swept_ticks <= -SCAN_MAX_TICKS) {
                    change_to(ST_FAULT);
                    break;
                }

                swept_ticks += angle_diff(last_angle, robot.kobuki.angle);
                last_angle = robot.kobuki.angle;
                Kobuki_Rotate(-SCAN_SPIN_SPEED);
                break;


            // --------------------------------------
            // SCAN CCW: slow anticlockwise spin across the object to the far edge.
            // The delay in the sensor shifts each edge by the same amount in opposite
            // directions, so the middle of the two edges is the true centre.
            // --------------------------------------

            case ST_SCAN_CCW:

                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (detect_run >= DETECT_CONFIRM) {
                    scan_seen = true;
                }

                if (scan_seen && lost_run >= LOST_CONFIRM) {

                    edge_ccw = lost_angle;
                    target_angle = angle_add(edge_cw, angle_diff(edge_cw, edge_ccw) / 2);

                    if (!deposit_phase) {
                        robot.layout = Layout_From_Range((uint16_t)(range_min + SENSOR_OFFSET_MM));
                    }

                    change_to(ST_ALIGN);
                    break;
                }

                if (swept_ticks >= SCAN_MAX_TICKS) {
                    change_to(ST_FAULT);
                    break;
                }

                swept_ticks += angle_diff(last_angle, robot.kobuki.angle);
                last_angle = robot.kobuki.angle;
                Kobuki_Rotate(SCAN_SPIN_SPEED);
                break;


            // --------------------------------------
            // ALIGN: face the centre of the object
            // --------------------------------------

            case ST_ALIGN:

                if (fault_ignore_cliff()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (rotate_to(target_angle)) {
                    change_to(deposit_phase ? ST_STORAGE_DEPOSIT : ST_ORIENT);
                }

                break;


            // --------------------------------------
            // ORIENT: turn from the bollard onto the corridor
            // --------------------------------------

            case ST_ORIENT:

                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (rotate_to(heading)) {

                    robot.corridor = 1;
                    robot.corridor_start_distance = robot.kobuki.distance;

                    change_to(ST_PATHFIND_DRIVE);
                }

                break;


            // --------------------------------------
            // PATHFIND DRIVE
            // --------------------------------------

            case ST_PATHFIND_DRIVE: {

                bool corridor_done =
                    Kobuki_Distance_Delta_Mm(
                        robot.corridor_start_distance,
                        robot.kobuki.distance
                    ) >= corridor_length_mm();

                // Layout A: storage wall on the left, ledge on the right. Layout B: the reverse.
                uint8_t wall_mask  = (robot.layout == LAYOUT_A) ? SIDE_LEFT  : SIDE_RIGHT;
                uint8_t ledge_mask = (robot.layout == LAYOUT_A) ? SIDE_RIGHT : SIDE_LEFT;

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (corridor_done && robot.corridor == 1) {
                    change_to(ST_PATHFIND_CORRIDOR_TURN);
                    break;
                }

                if (corridor_done && robot.corridor == 2) {
                    change_to(ST_PATHFIND_CORRIDOR_STOP);
                    break;
                }

                // Anything other than the wall bumper or ledge cliff is a real obstruction.
                if ((robot.kobuki.bumper & (uint8_t)~wall_mask) != 0 ||
                    (robot.kobuki.cliff & (uint8_t)~ledge_mask) != 0) {
                    change_to(ST_FAULT);
                    break;
                }

                if ((robot.kobuki.bumper & wall_mask) != 0) {
                    // Touched the storage wall: steer away from it.
                    nudge_ticks = (robot.layout == LAYOUT_A) ? -NUDGE_TICKS : NUDGE_TICKS;
                    change_to(ST_PATHFIND_REVERSE);
                    break;
                }

                if ((robot.kobuki.cliff & ledge_mask) != 0) {
                    // Saw the ledge: steer away from it.
                    nudge_ticks = (robot.layout == LAYOUT_A) ? NUDGE_TICKS : -NUDGE_TICKS;
                    change_to(ST_PATHFIND_REVERSE);
                    break;
                }

                drive_heading(DRIVE_SPEED, heading);
                break;
            }


            // --------------------------------------
            // PATHFIND REVERSE
            // --------------------------------------

            case ST_PATHFIND_REVERSE:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                Robot_Update();

                if (Robot_MovementComplete()) {
                    change_to(ST_PATHFIND_ROTATE);
                }

                break;


            // --------------------------------------
            // PATHFIND ROTATE: small nudge away from the wall / ledge
            // --------------------------------------

            case ST_PATHFIND_ROTATE:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (rotate_to(heading)) {
                    change_to(ST_PATHFIND_DRIVE);
                }

                break;


            // --------------------------------------
            // CORRIDOR TURN
            // --------------------------------------

            case ST_PATHFIND_CORRIDOR_TURN:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (rotate_to(heading)) {

                    robot.corridor = 2;
                    robot.corridor_start_distance = robot.kobuki.distance;

                    change_to(ST_PATHFIND_DRIVE);
                }

                break;


            // --------------------------------------
            // CORRIDOR STOP
            // --------------------------------------

            case ST_PATHFIND_CORRIDOR_STOP:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                change_to(ST_STORAGE_TURN);
                break;


            // --------------------------------------
            // STORAGE TURN: face the entrance
            // --------------------------------------

            case ST_STORAGE_TURN:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (rotate_to(heading)) {
                    change_to(ST_STORAGE_DRIVE_IN);
                }

                break;


            // --------------------------------------
            // STORAGE DRIVE IN
            // --------------------------------------

            case ST_STORAGE_DRIVE_IN:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (Robot_BumperPressed()) {
                    Robot_Stop();
                    start_deposit_search();
                    break;
                }

                if (Kobuki_Distance_Delta_Mm(robot.corridor_start_distance,
                                             robot.kobuki.distance) >= STORAGE_IN_MM) {
                    start_deposit_search();
                    break;
                }

                drive_heading(DRIVE_SPEED, heading);
                break;


            // --------------------------------------
            // STORAGE DEPOSIT: measure the distance to the marker (robot is facing it)
            // --------------------------------------

            case ST_STORAGE_DEPOSIT:

                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }

                Robot_Stop();

                if (dep_tick < 255) {
                    dep_tick++;
                }

                if (dep_tick > DEPOSIT_SETTLE_TICKS) {
                    dep_sum += robot.object.range_mm;
                    dep_count++;
                }

                if (dep_count >= DEPOSIT_SAMPLES) {

                    uint16_t avg = (uint16_t)(dep_sum / dep_count);
                    int32_t dist = (int32_t)avg + SENSOR_OFFSET_MM;   // centre to marker surface

                    if (avg < DEPOSIT_MIN_VALID_MM && !backed_up) {
                        // Too close for a reliable reading: back away first.
                        backed_up = true;
                        move_mm = -DEPOSIT_BACKUP_MM;
                        change_to(ST_STORAGE_MOVE);
                        break;
                    }

                    if (dist > 600 || dist < 150) {
                        change_to(ST_FAULT);   // reading makes no sense
                        break;
                    }

                    move_mm = (int16_t)(dist - (ROBOT_RADIUS_MM + DEPOSIT_GAP_MM) + DEPOSIT_TRIM_MM);
                    change_to(ST_STORAGE_MOVE);
                }

                break;


            // --------------------------------------
            // STORAGE MOVE: reverse for a good reading, then drive up to the marker
            // --------------------------------------

            case ST_STORAGE_MOVE:

                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }

                Robot_Update();

                if (Robot_MovementComplete()) {
                    if (move_mm < 0) {
                        change_to(ST_STORAGE_DEPOSIT);    // measure again from further back
                    } else {
                        change_to(ST_IDLE);               // arrived: done
                    }
                }

                break;


            // --------------------------------------
            // FAULT
            // --------------------------------------

            case ST_FAULT:

                change_to(ST_IDLE);
                break;
        }
    }
}