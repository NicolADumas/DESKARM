#include "manipulator.h"
#include "usart.h" 
#include <cstdint>

// --- MANIPULATOR GLOBAL VARIABLES ---
// --- PID PARAMETERS ---
// Homing - Joint 1
#define PID_KP_HOMING_1 3.7f
#define PID_KI_HOMING_1 0.01f
#define PID_KD_HOMING_1 0.3f
#define PID_FF_HOMING_1 0.0f

// Homing - Joint 2
#define PID_KP_HOMING_2 4.7f
#define PID_KI_HOMING_2 0.01f
#define PID_KD_HOMING_2 0.3f
#define PID_FF_HOMING_2 0.0f

// Tracking - Joint 1
#define PID_KP_TRACKING_1 0.0f
#define PID_KI_TRACKING_1 0.0f
#define PID_KD_TRACKING_1 0.0f
#define PID_FF_TRACKING_1 1.01f

// Tracking - Joint 2
#define PID_KP_TRACKING_2 0.0f
#define PID_KI_TRACKING_2 0.0f
#define PID_KD_TRACKING_2 0.0f
#define PID_FF_TRACKING_2 1.025f

float global_degs1, global_degs2;
int8_t global_dir1, global_dir2;

float global_dq0, global_dq1;
float global_ddq0, global_ddq1;

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
}

// --- STATUS READ AND SENSORS ---
void manipulator_read_status(manipulator_t *manipulator){
    float degs1, degs2;
    int8_t dir1, dir2;

    // Read Encoders
    encoder_read(&manipulator->encoder_1, &degs1, &dir1);
    encoder_read(&manipulator->encoder_2, &degs2, &dir2);

    degs1 = degs1; // wrap-around already handled by encoder
    degs2 = -1*degs2; // Invert for convention

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
    manipulator->calibration_triggered = 1;
    manipulator->homed = 0;

    homing_last_error0 = 0;
    homing_last_error1 = 0;
    homing_counter = 0;
    // Move slowly towards endstops
    apply_velocity_input(manipulator, (float[2]){-0.5, 0.0});
}

void calibration_stop(manipulator_t *manipulator){
    manipulator->calibration_triggered = 0;
    clear_manipulator_buffers(manipulator);
    apply_velocity_input(manipulator, (float[2]){0, 0});
    manipulator_set_setpoints(manipulator, 0.0f, 0.0f);
}

uint8_t calibration_check(manipulator_t *manipulator){
    return manipulator->calibration_triggered;
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
    // Set Homing PID parameters
    manipulator->position_controller_1.Kp = PID_KP_HOMING_1;
    manipulator->position_controller_1.Ki = PID_KI_HOMING_1;
    manipulator->position_controller_1.Kd = PID_KD_HOMING_1;
    
    manipulator->position_controller_2.Kp = PID_KP_HOMING_2;
    manipulator->position_controller_2.Ki = PID_KI_HOMING_2;
    manipulator->position_controller_2.Kd = PID_KD_HOMING_2;

    manipulator->feedforward_scale_1 = PID_FF_HOMING_1;
    manipulator->feedforward_scale_2 = PID_FF_HOMING_2;

    manipulator_update_position_controller(manipulator);
    float current_q0, current_q1;
    rbpeek(&manipulator->q0, &current_q0);
    rbpeek(&manipulator->q1, &current_q1);

    float error_q0 = fabsf(manipulator->q0_setpoint - current_q0);
    float error_q1 = fabsf(manipulator->q1_setpoint - current_q1);

    // Check error stability (when stable near 0, homing finished)
    if(error_q0 - homing_last_error0 ==0 && error_q1 - homing_last_error1 ==0 && error_q0 < 0.2f && error_q1 < 0.2f){
        homing_counter++;
    }

    homing_last_error0 = error_q0;
    homing_last_error1 = error_q1;
    if(homing_counter >= 10){ // Stable for 100ms (10 cycles of 10ms)
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
    // obtain ARR from manipulator->pen_timer
	const uint32_t max_arr = 1250;
    float dc = 0.5;
    // calculate pulse width
    uint32_t pulse = (uint32_t)(dc * (float)(max_arr + 1));
	if(pen_state == PEN_UP){
        __HAL_TIM_SET_COMPARE(&manipulator->pen_timer, TIM_CHANNEL_1, 500);
        HAL_Delay(1000);
        __HAL_TIM_SET_COMPARE(&manipulator->pen_timer, TIM_CHANNEL_1, 1500);
        HAL_Delay(1000);
        __HAL_TIM_SET_COMPARE(&manipulator->pen_timer, TIM_CHANNEL_1, 2500);

    }else{
        __HAL_TIM_SET_COMPARE(&manipulator->pen_timer, TIM_CHANNEL_1, pulse + 100);

	}

}


