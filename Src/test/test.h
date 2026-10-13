#ifndef TEST_H
#define TEST_H

#include <stdint.h>

// Result of one test. TEST_NOT_RUN means the test never finished,
// e.g. a blocking call hung because the Kobuki is off.
typedef enum { TEST_NOT_RUN = 0, TEST_PASS = 1, TEST_FAIL = 2 } Test_Result;

#define TEST_CHECK(condition) ((condition) ? TEST_PASS : TEST_FAIL)

// Values of test_stage
#define TEST_STAGE_ADC        1U
#define TEST_STAGE_ULTRASONIC 2U
#define TEST_STAGE_UART       3U
#define TEST_STAGE_KOBUKI     4U
#define TEST_STAGE_DONE       5U

// One Test_Result per library function, plus raw values to watch

typedef struct {
    Test_Result init;
    Test_Result read;
    uint16_t raw;                   // ADC_Read(ADC_CHANNEL_1)
} Test_ADC_Results;

typedef struct {
    Test_Result init;
    Test_Result read;
    Test_Result detect;
    uint16_t range_mm;              // Ultrasonic_Read()
    uint8_t detected;               // from Ultrasonic_Detect()
} Test_Ultrasonic_Results;

typedef struct {
    Test_Result init;
    Test_Result tx;
    Test_Result rx;
} Test_UART_Results;

typedef struct {
    Test_Result rx;
    Test_Result read;
    Test_Result tx;
    Test_Result drive;
    Test_Result rotate;
    Test_Result rotate_angle_setpoint;
    Test_Result rotate_angle;
    Test_Result drive_distance_setpoint;
    Test_Result drive_distance;
    Test_Result update_leds;
    int16_t tx_ticks;               // encoder change, raw forward packet
    int16_t drive_ticks;            // encoder change, Kobuki_Drive reverse
    int32_t rotate_ccw_ticks;       // angle change, 0.01 deg per tick
    int32_t rotate_cw_ticks;
    int32_t rotate_angle_ticks;     // expect about 9000 (90 deg)
    int16_t drive_distance_ticks;   // expect about 1170 (100 mm)
} Test_Kobuki_Results;

// Watch these in the debugger
extern volatile uint8_t test_stage;     // test currently running
extern volatile Test_Result test_all;   // overall verdict
extern volatile Test_ADC_Results test_adc;
extern volatile Test_Ultrasonic_Results test_ultrasonic;
extern volatile Test_UART_Results test_uart;
extern volatile Test_Kobuki_Results test_kobuki;

void Test_ADC(void);
void Test_Ultrasonic(void);
void Test_UART(void);
void Test_Kobuki(void);

// Runs every test, then loops forever showing the verdict on the Kobuki LEDs
void Test_RunAll(void);

#endif
