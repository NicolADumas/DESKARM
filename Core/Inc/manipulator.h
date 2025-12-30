/*
 * encoder.h
 *
 *  Created on: Dec 14, 2025
 *      Author: umby
 */


#ifndef INC_MANIPULATOR_H_
#define INC_MANIPULATOR_H_

#include "tim.h"
#include "ring_buffer.h"
#include "encoder.h"
#include <math.h>
#include "utils.h"
#include "pid.h"

#define NUM_POINTS_FOR_VEL 50
#define NUM_POINTS_FOR_ACC 20
/* RESOLUTION OF THE STEPPER MOTOR (in rads) */
#define RESOLUTION 0.0314159 // 2*PI/200
/* REDUCTION OF THE MOTOR 1*/
#define REDUCTION_1 10.0f
/* REDUCTION OF THE MOTOR 2*/
#define REDUCTION_2 5.0f
/* MICROSTEPS MOTOR 1*/
#define MICROSTEPS_1 16
/* MICROSTEPS MOTOR 2*/
#define MICROSTEPS_2 16

/* CALIBRATION MOTOR 1*/ 
#define CALIBRATION_1 6549 // CNT value at calibration position

/* CALIBRATION MOTOR 2*/
#define CALIBRATION_2 2464 // CNT value at calibration position

#define STEPS_PER_REVOLUTION 200.0f
#define TWO_PI 6.28318530718f

#define RATE_HOMING_CONTROL_MS 10

/* SOFTWARE ENDSTOPS */
#define Q0_MIN_DEG -105.0f
#define Q1_MAX_DEG 125.0f
#define Q0_MIN_RAD (-1.83259571f) // -105.0f * PI / 180.0f
#define Q1_MAX_RAD (2.18166156f)  //  125.0f * PI / 180.0f

typedef enum {
    MOTOR_1,
    MOTOR_2
} motor_id_t;



typedef struct {
    encoder_t encoder_1;
    encoder_t encoder_2;
    TIM_HandleTypeDef motor_1;
    TIM_HandleTypeDef motor_2;
    float current_position; // wrt end-effector
    float current_velocity; // wrt end-effector
    float sensor_dt;

    ringbuffer_t q0;
    ringbuffer_t q1;
    ringbuffer_t dq0;
    ringbuffer_t dq1;
    ringbuffer_t ddq0;
    ringbuffer_t ddq1;

    pid_controller_t position_controller_1;
    pid_controller_t position_controller_2;

    float q0_setpoint;
    float q1_setpoint;

    float integral_error_q0;
    float integral_error_q1;

    float B[4]; // Matrice di Inerzia B(q)
    float C[4]; // Matrice di Coriolis C(q, dq)

    uint32_t target_reached_start_tick;

    uint8_t calibration_triggered;
    uint8_t homed;
} manipulator_t;




// This function should init and start the encoders
// REMOVE THIS FUNCTIONS FROM main.c, IT SHOULD ONLY CALL manipulator_init 
void manipulator_init(manipulator_t *manipulator,
                      encoder_t *encoder_1,
                      encoder_t *encoder_2,
                      TIM_HandleTypeDef *motor1,
                      TIM_HandleTypeDef *motor2,
                      TIM_HandleTypeDef *htim);

void manipulator_start(manipulator_t *manipulator);
void manipulator_read_status(manipulator_t *manipulator);
void apply_velocity_input(manipulator_t *manipulator, float *u);
void manipulator_set_motor_velocity(manipulator_t *manipulator, motor_id_t motor, float speed_rad_s);

void clear_manipulator_buffers(manipulator_t *manipulator);
void calibration_start(manipulator_t *manipulator);
void calibration_stop(manipulator_t *manipulator);
uint8_t calibration_check(manipulator_t *manipulator);
void calibration_encoder(manipulator_t *manipulator, encoder_t *encoder, uint32_t calibration_value);
void manipulator_update_position_controller(manipulator_t *manipulator);
void manipulator_update_inverse_dynamics_controller(manipulator_t *manipulator);
void manipulator_reset_pid_controllers(manipulator_t *manipulator);
void manipulator_set_setpoints(manipulator_t *manipulator, float q0_setpoint_rad, float q1_setpoint_rad);


void homing(manipulator_t *manipulator);
uint8_t homing_check(manipulator_t *manipulator);
uint8_t manipulator_error_check(manipulator_t *manipulator, float error_threshold1, float error_threshold2);
uint8_t manipulator_check_target_reached(manipulator_t *manipulator, float pos_tolerance, float vel_tolerance, uint32_t min_stable_time_ms);

#endif
