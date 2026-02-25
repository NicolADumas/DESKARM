#include "manipulator.h"
#include "usart.h"

// --- MANIPULATOR GLOBAL VARIABLES ---
// --- PID PARAMETERS ---
// Homing - Joint 1
#define PID_KP_HOMING_1 3.7f
#define PID_KI_HOMING_1 0.01f
#define PID_KD_HOMING_1 0.3f
#define PID_FF_HOMING_1 0.0f
#define PID_ACC_FF_HOMING_1 0.0f

// Homing - Joint 2
#define PID_KP_HOMING_2 4.7f
#define PID_KI_HOMING_2 0.01f
#define PID_KD_HOMING_2 0.3f
#define PID_FF_HOMING_2 0.0f
#define PID_ACC_FF_HOMING_2 0.0f

// Tracking - Joint 1
#define PID_KP_TRACKING_1 8.0f
#define PID_KI_TRACKING_1 0.0f
#define PID_KD_TRACKING_1 0.2f
#define PID_FF_TRACKING_1 1.01f
#define PID_ACC_FF_TRACKING_1 0.12f

// Tracking - Joint 2
#define PID_KP_TRACKING_2 8.0f
#define PID_KI_TRACKING_2 0.0f
#define PID_KD_TRACKING_2 0.2f
#define PID_FF_TRACKING_2 1.04f
#define PID_ACC_FF_TRACKING_2 0.12f

float global_degs1, global_degs2;
int8_t global_dir1, global_dir2;

float global_dq0, global_dq1;
float global_ddq0, global_ddq1;

// --- PEN SERVO CONFIGURATION ---
// TIM11 Running at 1MHz (1us tick). 50Hz PWM (20000 ticks period).
// Servo Range: 1000us (1ms) to 2000us (2ms) usually.
// Adjust these values based on mechanical calibration.
#define PEN_PWM_UP 1800   // Lifted position
#define PEN_PWM_DOWN 1000 // Drawing position


uint32_t global_size;
float homing_last_error0, homing_last_error1;
uint16_t homing_counter = 0;
float globa_setpint_q0, globa_setpint_q1; 

// --- INITIALIZATION ---
void manipulator_init(manipulator_t *manipulator, encoder_t *encoder_1, encoder_t *encoder_2, TIM_HandleTypeDef *motor1, TIM_HandleTypeDef *motor2, TIM_HandleTypeDef *htim, TIM_HandleTypeDef *pen_timer){
    // Hardware linking
    manipulator->encoder_1 = *encoder_1;
    manipulator->encoder_2 = *encoder_2;
    manipulator->motor_1 = *motor1;
    manipulator->motor_2 = *motor2;
    manipulator->pen_timer = *pen_timer;


    clear_manipulator_buffers(manipulator);
    manipulator->calibration_triggered = 0;
    manipulator->homed = 0;
    manipulator->target_reached_start_tick = 0;
    manipulator->telemetry_ready = 0;
    manipulator->feedforward_scale_1 = PID_FF_TRACKING_1;
    manipulator->feedforward_scale_2 = PID_FF_TRACKING_2;
    manipulator->feedforward_acc_scale_1 = PID_ACC_FF_TRACKING_1;
    manipulator->feedforward_acc_scale_2 = PID_ACC_FF_TRACKING_2;

    // Default PID Parameters
    pid_controller_t pc1, pc2;
    pc1.Kp = PID_KP_TRACKING_1; pc1.Ki = PID_KI_TRACKING_1; pc1.Kd = PID_KD_TRACKING_1;
    pc1.previous_error = 0.0f; pc1.integral_error = 0.0f;

    pc2.Kp = PID_KP_TRACKING_2; pc2.Ki = PID_KI_TRACKING_2; pc2.Kd = PID_KD_TRACKING_2;
    pc2.previous_error = 0.0f; pc2.integral_error = 0.0f;

    // 3. Copy initialized structures to manipulator structure
    manipulator->position_controller_1 = pc1;
    manipulator->position_controller_2 = pc2;

    // Calculate period with ARR, PSC and PCLK1 frequency
    uint32_t pclk1_freq = HAL_RCC_GetPCLK1Freq();
    uint32_t timer_clock = pclk1_freq * 2; // TIMxCLK
    uint32_t arr = htim->Instance->ARR;
    uint32_t psc = htim->Instance->PSC;
    manipulator->sensor_dt = (float)(arr + 1) * (float)(psc + 1) / (float)timer_clock;
}

void clear_manipulator_buffers(manipulator_t *manipulator){
    rbclear(&manipulator->q0);
    rbclear(&manipulator->q1);
    rbclear(&manipulator->dq0);
    rbclear(&manipulator->dq1);
    rbclear(&manipulator->ddq0);
    rbclear(&manipulator->ddq1);
}

void manipulator_start(manipulator_t *manipulator){
    encoder_start(&manipulator->encoder_1);
    encoder_start(&manipulator->encoder_2);
    HAL_TIM_PWM_Start(&manipulator->motor_1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&manipulator->motor_2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&manipulator->pen_timer, TIM_CHANNEL_1);
    
    // Default Pen Up
    control_pen(manipulator, PEN_UP);
}

// --- STATUS READ AND SENSORS ---
void manipulator_read_status(manipulator_t *manipulator){
    float degs1, degs2;
    int8_t dir1, dir2;

    // Read Encoders
    encoder_read(&manipulator->encoder_1, &degs1, &dir1);
    encoder_read(&manipulator->encoder_2, &degs2, &dir2);

    degs1 = degs1; // wrap-around already handled by encoder
    degs2 = -1*degs2; // Invert for kinematic convention (encoder counts opposite to joint convention)
                       // IMPORTANT: due to this inversion AND dir_inverted=1 in controller.c,
                       // calibration_stage2 must use +0.7f (not -0.7f) to move toward endstop 2.

    global_degs1 = degs1;
    global_degs2 = degs2;
    global_dir1 = dir1;
    global_dir2 = dir2;

    // Convert Degrees -> Radians and Push to Buffer
    float q0 = degs1 * (M_PI / 180.0f); 
    float q1 = degs2 * (M_PI / 180.0f);

    rbpush(&manipulator->q0, q0);
    rbpush(&manipulator->q1, q1);

    // Estimate Velocity and Acceleration (Numerical Differentiation)
    float dq0 = calculate_slope(&manipulator->q0, NUM_POINTS_FOR_VEL, manipulator->sensor_dt);
    float dq1 = calculate_slope(&manipulator->q1, NUM_POINTS_FOR_VEL, manipulator->sensor_dt);

    global_dq0 = dq0;
    global_dq1 = dq1;
    
    rbpush(&manipulator->dq0, dq0);
    rbpush(&manipulator->dq1, dq1);

    float ddq0 = calculate_slope(&manipulator->dq0, NUM_POINTS_FOR_ACC, manipulator->sensor_dt);
    float ddq1 = calculate_slope(&manipulator->dq1, NUM_POINTS_FOR_ACC, manipulator->sensor_dt);

    global_ddq0 = ddq0;
    global_ddq1 = ddq1;

    rbpush(&manipulator->ddq0, ddq0);
    rbpush(&manipulator->ddq1, ddq1);

    /* Signal Telemetry Ready (Decimated 1:5) */
    static uint8_t telemetry_div = 0;
    if (++telemetry_div >= 5) {
        manipulator->telemetry_ready = 1;
        telemetry_div = 0;
    }
}

// --- CALIBRATION AND HOMING ---
void calibration_start(manipulator_t *manipulator){
    manipulator->calibration_triggered = 1; // State 1: Finding Limit Switch 1
    manipulator->homed = 0;

    homing_last_error0 = 0;
    homing_last_error1 = 0;
    // Move slowly towards endstops (-0.7 rad/s based on previous firmware)
    apply_velocity_input(manipulator, (float[2]){-0.7f, 0.0f});
}

void calibration_stage2(manipulator_t *manipulator){
    manipulator->calibration_triggered = 2; // State 2: Finding Limit Switch 2
    calibration_encoder(manipulator, &manipulator->encoder_1, CALIBRATION_1);
    // Move second joint towards endstop (+0.7 rad/s - positive because encoder 2 is inverted in manipulator_read_status)
    apply_velocity_input(manipulator, (float[2]){0.0f, +0.7f});
}

void calibration_stop(manipulator_t *manipulator){
    manipulator->calibration_triggered = 0; // State 0: Done
    clear_manipulator_buffers(manipulator);
    apply_velocity_input(manipulator, (float[2]){0.0f, 0.0f});
    
    // Read the exact position we just calibrated to
    float current_q0, current_q1;
    manipulator_read_status(manipulator); // Ensure buffers have at least one value
    rbpeek(&manipulator->q0, &current_q0);
    rbpeek(&manipulator->q1, &current_q1);

    // Set setpoints to CURRENT position so PID doesn't jump
    manipulator_set_setpoints(manipulator, current_q0, current_q1);
}

uint8_t calibration_check(manipulator_t *manipulator){
    return manipulator->calibration_triggered > 0;
}

void calibration_encoder(manipulator_t *manipulator, encoder_t *encoder, uint32_t calibration_value){
    apply_velocity_input(manipulator, (float[2]){0.0, 0.0});
    encoder_set_count(encoder, calibration_value);
}

// --- HOMING LOGIC ---
uint8_t homing_check(manipulator_t *manipulator){
    return manipulator->homed;
}

void homing(manipulator_t *manipulator){
    // Ensure Pen is UP during homing
    control_pen(manipulator, PEN_UP);

    // Set Homing PID parameters
    manipulator->position_controller_1.Kp = PID_KP_HOMING_1;
    manipulator->position_controller_1.Ki = PID_KI_HOMING_1;
    manipulator->position_controller_1.Kd = PID_KD_HOMING_1;
    
    manipulator->position_controller_2.Kp = PID_KP_HOMING_2;
    manipulator->position_controller_2.Ki = PID_KI_HOMING_2;
    manipulator->position_controller_2.Kd = PID_KD_HOMING_2;

    manipulator->feedforward_scale_1 = PID_FF_HOMING_1;
    manipulator->feedforward_scale_2 = PID_FF_HOMING_2;
    manipulator->feedforward_acc_scale_1 = PID_ACC_FF_HOMING_1;
    manipulator->feedforward_acc_scale_2 = PID_ACC_FF_HOMING_2;

    manipulator_update_position_controller(manipulator);
    float current_q0, current_q1;
    rbpeek(&manipulator->q0, &current_q0);
    rbpeek(&manipulator->q1, &current_q1);

    // Incrementally move setpoints toward 0.0 to prevent jerking
    float step = 0.01f; // rad per 10ms (approx 1 rad/s)
    if (manipulator->q0_setpoint > step) manipulator->q0_setpoint -= step;
    else if (manipulator->q0_setpoint < -step) manipulator->q0_setpoint += step;
    else manipulator->q0_setpoint = 0.0f;

    if (manipulator->q1_setpoint > step) manipulator->q1_setpoint -= step;
    else if (manipulator->q1_setpoint < -step) manipulator->q1_setpoint += step;
    else manipulator->q1_setpoint = 0.0f;

    float error_q0 = fabsf(manipulator->q0_setpoint - current_q0);
    float error_q1 = fabsf(manipulator->q1_setpoint - current_q1);

    // Check error stability (when reaching 0 and stable, homing finished)
    if(manipulator->q0_setpoint == 0.0f && manipulator->q1_setpoint == 0.0f && fabsf(error_q0 - homing_last_error0) < 0.005f && fabsf(error_q1 - homing_last_error1) < 0.005f && error_q0 < 0.05f && error_q1 < 0.05f){
        homing_counter++;
    } else {
        homing_counter = 0; // Reset counter if it becomes unstable
    }

    homing_last_error0 = error_q0;
    homing_last_error1 = error_q1;
    if(homing_counter >= 50){ // Stable for 500ms (50 cycles of 10ms)
        manipulator->homed = 1;
        apply_velocity_input(manipulator, (float[2]){0.0, 0.0});

        // Restore Tracking PID parameters
        manipulator->position_controller_1.Kp = PID_KP_TRACKING_1;
        manipulator->position_controller_1.Ki = PID_KI_TRACKING_1;
        manipulator->position_controller_1.Kd = PID_KD_TRACKING_1;

        manipulator->position_controller_2.Kp = PID_KP_TRACKING_2;
        manipulator->position_controller_2.Ki = PID_KI_TRACKING_2;
        manipulator->position_controller_2.Kd = PID_KD_TRACKING_2;

        manipulator->feedforward_scale_1 = PID_FF_TRACKING_1;
        manipulator->feedforward_scale_2 = PID_FF_TRACKING_2;
        manipulator->feedforward_acc_scale_1 = PID_ACC_FF_TRACKING_1;
        manipulator->feedforward_acc_scale_2 = PID_ACC_FF_TRACKING_2;
    }
    
}

// --- TARGET REACHED CHECK ---
uint8_t manipulator_error_check(manipulator_t *manipulator, float error_threshold1, float error_threshold2){
    float current_q0, current_q1;
    rbpeek(&manipulator->q0, &current_q0);
    rbpeek(&manipulator->q1, &current_q1);

    float error_q0 = fabsf(manipulator->q0_setpoint - current_q0);
    float error_q1 = fabsf(manipulator->q1_setpoint - current_q1);

    return (error_q0 < error_threshold1) && (error_q1 < error_threshold2);
}

uint8_t manipulator_check_target_reached(manipulator_t *manipulator, float pos_tolerance, float vel_tolerance, uint32_t min_stable_time_ms) {
    float current_q0, current_q1;
    float current_dq0, current_dq1;

    rbpeek(&manipulator->q0, &current_q0);
    rbpeek(&manipulator->q1, &current_q1);
    rbpeek(&manipulator->dq0, &current_dq0);
    rbpeek(&manipulator->dq1, &current_dq1);

    float error_q0 = fabsf(manipulator->q0_setpoint - current_q0);
    float error_q1 = fabsf(manipulator->q1_setpoint - current_q1);
    float vel_q0 = fabsf(current_dq0);
    float vel_q1 = fabsf(current_dq1);

    if (error_q0 <= pos_tolerance && error_q1 <= pos_tolerance &&
        vel_q0 <= vel_tolerance && vel_q1 <= vel_tolerance) {
        
        if (manipulator->target_reached_start_tick == 0) {
            manipulator->target_reached_start_tick = HAL_GetTick();
        }

        if ((HAL_GetTick() - manipulator->target_reached_start_tick) >= min_stable_time_ms) {
            return 1;
        }
    } else {
        manipulator->target_reached_start_tick = 0;
    }

    return 0;
}

void manipulator_set_setpoints(manipulator_t *manipulator, float q0_setpoint_rad, float q1_setpoint_rad){
    manipulator->q0_setpoint = q0_setpoint_rad;
    manipulator->q1_setpoint = q1_setpoint_rad;
    manipulator->target_reached_start_tick = 0;
}

// --- MOTION QUEUE PROCESSING ---
int manipulator_process_motion_queue(manipulator_t *manipulator) {
    Packet_t packet;
    if (pb_pop(manipulator, &packet)) {
        manipulator->current_setpoint = packet;
        manipulator->q0_setpoint = packet.q0;
        manipulator->q1_setpoint = packet.q1;
        globa_setpint_q0 = packet.q0;
        globa_setpint_q1 = packet.q1;

        // Update Pen State
        control_pen(manipulator, packet.pen_up);

        return 1;
    } else{
        // Buffer empty: maintain last position setpoint, but zero out velocity/acceleration feedforward
        manipulator->current_setpoint.dq0 = 0.0f;
        manipulator->current_setpoint.dq1 = 0.0f;
        manipulator->current_setpoint.ddq0 = 0.0f;
        manipulator->current_setpoint.ddq1 = 0.0f;
        return 0;
    }
}

void control_pen(manipulator_t *manipulator, uint8_t pen_state){
    if(pen_state == PEN_UP){
        __HAL_TIM_SET_COMPARE(&manipulator->pen_timer, TIM_CHANNEL_1, PEN_PWM_UP);
    }else{
        __HAL_TIM_SET_COMPARE(&manipulator->pen_timer, TIM_CHANNEL_1, PEN_PWM_DOWN);
    }
}



// --- MELODY IMPLEMENTATION (JITTER METHOD) ---
// Pins: Motor 1 STEP (PA0), Motor 2 STEP (PA15)
// DIR Pins: Motor 1 DIR (PC10), Motor 2 DIR (PA1)

static void delay_us_approx(uint32_t us) {
    // Calibrated for ~100MHz/84MHz. approx 10-12 instructions per loop?
    // simple blocking loop.
    volatile uint32_t count = us * 8; 
    while (count--) {
        __NOP();
    }
}

void Robot_Sing(uint16_t freq, uint16_t duration_ms) {
    if (freq == 0) {
        HAL_Delay(duration_ms);
        return;
    }

    // 1. Stop PWM limits to free the pins
    HAL_TIM_PWM_Stop(&manipulator.motor_1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&manipulator.motor_2, TIM_CHANNEL_1);

    // 2. Reconfigure Pins as Output
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PA0 (M1 STEP)
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA15 (M2 STEP)
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Get DIR pins ready
    // Motor 1 DIR: PC10
    // Motor 2 DIR: PA1
    
    // Calculate periods
    uint32_t period_us = 1000000 / freq;
    uint32_t half_period = period_us / 2;
    if(half_period < 10) half_period = 10; // safety


    uint32_t start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < duration_ms) {
        // --- PHASE A: DIR Forward ---
        HAL_GPIO_WritePin(GPIOC, DIR_1_Pin, GPIO_PIN_SET);   // M1 FWD
        HAL_GPIO_WritePin(GPIOA, DIR_2_Pin, GPIO_PIN_SET);   // M2 FWD
        
        // Pulse High
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
        delay_us_approx(5);
        // Pulse Low
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
        
        delay_us_approx(half_period - 5);

        // --- PHASE B: DIR Backward ---
        HAL_GPIO_WritePin(GPIOC, DIR_1_Pin, GPIO_PIN_RESET); // M1 BWD
        HAL_GPIO_WritePin(GPIOA, DIR_2_Pin, GPIO_PIN_RESET); // M2 BWD
        
        // Pulse High
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
        delay_us_approx(5);
        // Pulse Low
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
        
        delay_us_approx(half_period - 5);
    }

    // 3. Restore PWM Config (Alternate Function)
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM5; // TIM5_CH1
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2; // TIM2_CH1
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Restart PWM ch? No, manipulator_set_motor_velocity will restart it when needed.
    // Actually, manipulator_start called HAL_TIM_PWM_Start.
    // We should restart it with 0 duty cycle just in case.
    HAL_TIM_PWM_Start(&manipulator.motor_1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&manipulator.motor_2, TIM_CHANNEL_1);
}

// Melody Routines
void Robot_Sound_Boot(void) { // USB: D F# A C#
    Robot_Sing(NOTE_D4, 100);
    Robot_Sing(NOTE_Fsp4, 100);
    Robot_Sing(NOTE_A4, 100);
    Robot_Sing(NOTE_F5, 200); // Modified to F5 per user correction
}

void Robot_Sound_Success(void) { // Trajectory End: C# E G# B
    Robot_Sing(NOTE_Cs5, 100);
    Robot_Sing(NOTE_E5, 100);
    Robot_Sing(NOTE_Gs5, 100);
    Robot_Sing(NOTE_B5, 200);
}

void Robot_Sound_Error(void) { // Drawing: A C# E G#
    Robot_Sing(NOTE_A4, 100);
    Robot_Sing(NOTE_Cs5, 100);
    Robot_Sing(NOTE_E5, 100);
    Robot_Sing(NOTE_Gs5, 200);
}

void Robot_Sound_Thinking(void) { // Text: G# B D F#
    Robot_Sing(NOTE_Gs4, 100);
    Robot_Sing(NOTE_B4, 100);
    Robot_Sing(NOTE_D5, 100);
    Robot_Sing(NOTE_Fsp5, 200);
}

// Image: E G# B D
void Robot_Sound_PowerDown(void) { 
    Robot_Sing(NOTE_E5, 100);
    Robot_Sing(NOTE_Gs5, 100);
    Robot_Sing(NOTE_B5, 100);
    Robot_Sing(NOTE_D6, 200);
}

// High Freq Sound: E6 G6 B6 C7
void Robot_Sound_HighFreq(void) {
    Robot_Sing(NOTE_E6, 100);
    Robot_Sing(NOTE_G6, 100);
    Robot_Sing(NOTE_B6, 100);
    Robot_Sing(NOTE_C7, 150);
}

void manipulator_play_melody(manipulator_t *manipulator, uint8_t melody_id) {
    switch(melody_id) {
        case 1: Robot_Sound_Boot(); break;
        case 2: Robot_Sound_Error(); break; // Drawing
        case 3: Robot_Sound_Thinking(); break; // Text
        case 4: Robot_Sound_PowerDown(); break; // Image
        case 5: Robot_Sound_Success(); break; // End Trajectory
        case 6: // Motion Change (added extra)
            Robot_Sing(NOTE_C6, 50); 
            break;
        case 7: // Ghost Toggle (added extra)
             Robot_Sing(NOTE_A4, 50);
             break;
        case 8: // High Freq Sound
             Robot_Sound_HighFreq();
             break;
        default:
             // fallback beep
             Robot_Sing(NOTE_A4, 100); 
             break;
    }
}
