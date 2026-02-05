#include "controller.h"
#include <math.h>
#include "main.h" // for HAL_RCC_GetPCLK1Freq, GPIO definitions etc.

// Global debug variables
float global_v_calc1, global_v_calc2;


void manipulator_set_motor_velocity(manipulator_t *manipulator, motor_id_t motor, float speed_rad_s) {
    // --- COMMON SETTINGS ---
    const uint32_t TIMER_INPUT_FREQ = HAL_RCC_GetPCLK1Freq() * 2;
    const uint32_t PRESCALER = 99;
    const uint32_t TIMER_COUNT_FREQ = TIMER_INPUT_FREQ / (PRESCALER + 1); // Should be 1,000,000 Hz

    TIM_HandleTypeDef *motor_timer;
    GPIO_TypeDef* dir_port;
    uint16_t dir_pin;
    float reduction;
    float microsteps;
    uint8_t dir_inverted;

    if (motor == MOTOR_1) {
        motor_timer = &manipulator->motor_1;
        dir_port = DIR_1_GPIO_Port;
        dir_pin = DIR_1_Pin;
        reduction = REDUCTION_1;
        microsteps = MICROSTEPS_1;
        dir_inverted = 0;
    } else { // MOTOR_2
        motor_timer = &manipulator->motor_2;
        dir_port = DIR_2_GPIO_Port;
        dir_pin = DIR_2_Pin;
        reduction = REDUCTION_2;
        microsteps = MICROSTEPS_2;
        dir_inverted = 1;
    }

    // --- SET DIRECTION ---
    GPIO_PinState dir_state = (speed_rad_s < 0) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    if (dir_inverted) {
        dir_state = (dir_state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }
    HAL_GPIO_WritePin(dir_port, dir_pin, dir_state);

    // --- CALCULATE FREQUENCY AND ARR ---
    float abs_speed = fabsf(speed_rad_s);
    uint32_t arr;

    if (abs_speed < 0.001f) {
        arr = 0; // Stop motor
    } else {
        float motor_speed_rad_s = abs_speed * reduction;
        float step_freq = motor_speed_rad_s * (STEPS_PER_REVOLUTION / TWO_PI) * microsteps;

        if (step_freq > 0) {
            arr = (uint32_t)(TIMER_COUNT_FREQ / step_freq);
            if (arr < 10) arr = 10; // Limit to avoid excessively high frequencies
        } else {
            arr = 0;
        }
    }

    // --- APPLY VALUES TO TIMER ---
    __HAL_TIM_SET_PRESCALER(motor_timer, PRESCALER);
    __HAL_TIM_SET_AUTORELOAD(motor_timer, arr);
    __HAL_TIM_SET_COMPARE(motor_timer, TIM_CHANNEL_1, arr > 0 ? arr / 2 : 0); // Duty 50% or 0
    motor_timer->Instance->EGR = TIM_EGR_UG;
}


void apply_velocity_input(manipulator_t *manipulator, float *u){
    manipulator_set_motor_velocity(manipulator, MOTOR_1, u[0]);
    manipulator_set_motor_velocity(manipulator, MOTOR_2, u[1]);
}

void manipulator_update_position_controller(manipulator_t *manipulator) {
    // Limits for integral anti-windup and maximum velocity
    const float INTEGRAL_MAX = 10.0f;
    const float VELOCITY_MAX = 2.0f; // rad/s
    const float DT = 0.01f; // 10 ms

    // --- JOINT 0 CONTROLLER ---
    float current_q0;
    rbpeek(&manipulator->q0, &current_q0); // Read latest position without removing it

    float error_q0 = manipulator->q0_setpoint - current_q0;

    // Proportional Term
    float p_term_q0 = manipulator->position_controller_1.Kp * error_q0;

    // Integral Term (with anti-windup)
    manipulator->position_controller_1.integral_error += error_q0 * DT;
    if (manipulator->position_controller_1.integral_error > INTEGRAL_MAX) manipulator->position_controller_1.integral_error = INTEGRAL_MAX;
    if (manipulator->position_controller_1.integral_error < -INTEGRAL_MAX) manipulator->position_controller_1.integral_error = -INTEGRAL_MAX;
    float i_term_q0 = manipulator->position_controller_1.Ki * manipulator->position_controller_1.integral_error;

    // Derivative Term
    float derivative_error_q0 = (error_q0 - manipulator->position_controller_1.previous_error) / DT;
    float d_term_q0 = manipulator->position_controller_1.Kd * derivative_error_q0;
    manipulator->position_controller_1.previous_error = error_q0;

    // --- JOINT 1 CONTROLLER ---
    float current_q1;
    rbpeek(&manipulator->q1, &current_q1);

    float error_q1 = manipulator->q1_setpoint - current_q1;
    float p_term_q1 = manipulator->position_controller_2.Kp * error_q1;
    manipulator->position_controller_2.integral_error += error_q1 * DT;
    if (manipulator->position_controller_2.integral_error > INTEGRAL_MAX) manipulator->position_controller_2.integral_error = INTEGRAL_MAX;
    if (manipulator->position_controller_2.integral_error < -INTEGRAL_MAX) manipulator->position_controller_2.integral_error = -INTEGRAL_MAX;
    float i_term_q1 = manipulator->position_controller_2.Ki * manipulator->position_controller_2.integral_error;
    float derivative_error_q1 = (error_q1 - manipulator->position_controller_2.previous_error) / DT;
    float d_term_q1 = manipulator->position_controller_2.Kd * derivative_error_q1;
    manipulator->position_controller_2.previous_error = error_q1;

    // Calculate control output (desired velocity)
    float u1 = p_term_q0 + i_term_q0 + d_term_q0 + manipulator->current_setpoint.dq0 * manipulator->feedforward_scale_1; // Use u0 for Joint 0
    float u2 = p_term_q1 + i_term_q1 + d_term_q1 + manipulator->current_setpoint.dq1 * manipulator->feedforward_scale_2; // Use Joint 1 calculation for Joint 1

    // --- SOFTWARE ENDSTOPS ---
	// Check limit for motor 1 (q0)
	if ((current_q0 <= Q0_MIN_RAD && u1 < 0.0f)) {
		u1 = 0.0f;
	}

	// Check limit for motor 2 (q1)
	if ((current_q1 >= Q1_MAX_RAD && u2 > 0.0f)) {
		u2 = 0.0f;
	}

    // Velocity Saturation (clamping)
    if (u1 > VELOCITY_MAX) u1 = VELOCITY_MAX;
    if (u1 < -VELOCITY_MAX) u1 = -VELOCITY_MAX;
    if (u2 > VELOCITY_MAX) u2 = VELOCITY_MAX;
    if (u2 < -VELOCITY_MAX) u2 = -VELOCITY_MAX;

    global_v_calc1 = u1;
    global_v_calc2 = u2;
    
    // Apply calculated velocities to motors
    apply_velocity_input(manipulator, (float[]){u1, u2});
}

void manipulator_update_inverse_dynamics_controller(manipulator_t *manipulator) {
    // External PID Controller Gains
    const float Kp0 = 120.0f; // Proportional Gain
    const float Ki0 = 0.0f;  // Integral Gain
    const float Kd0 = 17.0f;  // Derivative Gain

    const float Kp1 = 125.0f; 
    const float Ki1 = 0.0f;  
    const float Kd1 = 14.0f;  

    // Limits
    const float VELOCITY_MAX = 2.0f; // rad/s
    const float INTEGRAL_MAX = 10.0f;
    const float DT = 0.01f; // 10 ms

    // Read current states (q, dq)
    float q0, q1, dq0, dq1;
    rbgetoffset(&manipulator->q0, 0, &q0);
    rbgetoffset(&manipulator->q1, 0, &q1);
    rbgetoffset(&manipulator->dq0, 0, &dq0);
    rbgetoffset(&manipulator->dq1, 0, &dq1);

    // Calculate dynamic matrices B(q) and C(q, dq)
    manipulator_calc_B(manipulator);
    manipulator_calc_C(manipulator);

    // --- JOINT 0 CONTROL ---
    float err_q0 = manipulator->q0_setpoint - q0;
    manipulator->integral_error_q0 += err_q0 * DT;  
    // Anti-windup
    if (manipulator->integral_error_q0 > INTEGRAL_MAX) manipulator->integral_error_q0 = INTEGRAL_MAX;
    if (manipulator->integral_error_q0 < -INTEGRAL_MAX) manipulator->integral_error_q0 = -INTEGRAL_MAX;
    float err_dq0 = manipulator->current_setpoint.dq0 - dq0; // Velocity Error

    // --- JOINT 1 CONTROL ---
    float err_q1 = manipulator->q1_setpoint - q1;
    manipulator->integral_error_q1 += err_q1 * DT;
    // Anti-windup
    if (manipulator->integral_error_q1 > INTEGRAL_MAX) manipulator->integral_error_q1 = INTEGRAL_MAX;
    if (manipulator->integral_error_q1 < -INTEGRAL_MAX) manipulator->integral_error_q1 = -INTEGRAL_MAX;
    float err_dq1 = manipulator->current_setpoint.dq1 - dq1;

    // Control law for desired acceleration (ddq_ref)
    float ddq_ref0 = manipulator->current_setpoint.ddq0 + Kp0 * err_q0 + Ki0 * manipulator->integral_error_q0 + Kd0 * err_dq0;
    float ddq_ref1 = manipulator->current_setpoint.ddq1 + Kp1 * err_q1 + Ki1 * manipulator->integral_error_q1 + Kd1 * err_dq1;

    // Inverse Dynamics Control Law: u = B(q)*ddq_ref + C(q,dq)*dq
    // Calculate C*dq
    float C_dq0 = manipulator->C[0] * dq0 + manipulator->C[1] * dq1;
    float C_dq1 = manipulator->C[2] * dq0 + manipulator->C[3] * dq1;

    // Calculate B*ddq_ref
    float B_ddq0 = manipulator->B[0] * ddq_ref0 + manipulator->B[1] * ddq_ref1;
    float B_ddq1 = manipulator->B[2] * ddq_ref0 + manipulator->B[3] * ddq_ref1;

    // Final Command (Torque/Velocity)
    float u1 = B_ddq0 + C_dq0;
    float u2 = B_ddq1 + C_dq1;

    // Velocity Saturation (clamping)
    if (u1 > VELOCITY_MAX) u1 = VELOCITY_MAX;
    if (u1 < -VELOCITY_MAX) u1 = -VELOCITY_MAX;
    if (u2 > VELOCITY_MAX) u2 = VELOCITY_MAX;
    if (u2 < -VELOCITY_MAX) u2 = -VELOCITY_MAX;

    global_v_calc1 = u1;
    global_v_calc2 = u2;

    // Apply calculated velocities to motors
    apply_velocity_input(manipulator, (float[]){u1, u2});
}

void manipulator_reset_pid_controllers(manipulator_t *manipulator) {
    manipulator->position_controller_1.integral_error = 0.0f;
    manipulator->position_controller_1.previous_error = 0.0f;
    manipulator->position_controller_2.integral_error = 0.0f;
    manipulator->position_controller_2.previous_error = 0.0f;
    manipulator->integral_error_q0 = 0.0f;
    manipulator->integral_error_q1 = 0.0f;
}

void manipulator_calc_B(manipulator_t *manipulator){
    float q1, q2;
    rbgetoffset(&manipulator->q0, 0, &q1);
    rbgetoffset(&manipulator->q1, 0, &q2);

    manipulator->B[0] = (float) (0.0047413*cos(q1 + 2*q2) + 0.028554*cos(q1 + q2) + 0.078463*cos(q1) + 0.014224*cos(q2) + 0.045182);
    manipulator->B[1] = (float) (0.0023706*cos(q1 + 2*q2) + 0.023453*cos(q1 + q2) + 0.039491*cos(q1) + 0.0094825*cos(q2) + 0.01103);
    manipulator->B[2] = manipulator->B[1]; // Matrix is symmetric
    manipulator->B[3] = (float) (0.018351*cos(q1 + q2) + 0.039491*cos(q1) + 0.0047413*cos(q2) + 0.011032);
}

void manipulator_calc_C(manipulator_t *manipulator){
    float q1, q2, dq1, dq2;
    rbgetoffset(&manipulator->q0, 0, &q1);
    rbgetoffset(&manipulator->q1, 0, &q2);
    rbgetoffset(&manipulator->dq0, 0, &dq1);
    rbgetoffset(&manipulator->dq1, 0, &dq2);

    manipulator->C[0] = (float) ( - 0.5*dq2*(0.0047413*sin(q1 + 2*q2) + 0.010203*sin(q1 + q2) + 0.0094825*sin(q2)));
    manipulator->C[1] = (float) ( - 0.000030008*(dq1 + dq2)*(79.0*sin(q1 + 2*q2) + 170*sin(q1 + q2) + 158*sin(q2)));
    manipulator->C[2] = (float) (   dq1*(0.0023706*sin(q1 + 2*q2) + 0.0051014*sin(q1 + q2) + 0.0047413*sin(q2)));
    manipulator->C[3] = (float) 0.0;
}
