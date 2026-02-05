/*
 * manipulator_types.h
 *
 *  Created on: Dec 14, 2025
 *      Author: umby
 */

#ifndef INC_MANIPULATOR_TYPES_H_
#define INC_MANIPULATOR_TYPES_H_

#include "tim.h"
#include "ring_buffer.h"
#include "encoder.h"
#include "pid.h"
#include "packet.h"

/* Definitions copied from manipulator.h */
#define NUM_POINTS_FOR_VEL 50
#define NUM_POINTS_FOR_ACC 20

/* RESOLUTION OF THE STEPPER MOTOR (in rads) */
#define RESOLUTION 0.0314159 // 2*PI/200
/* REDUCTION OF THE MOTOR 1*/
#define REDUCTION_1 10.0f
/* REDUCTION OF THE MOTOR 2*/
#define REDUCTION_2 5.0f
/* MICROSTEPS MOTOR 1*/
#define MICROSTEPS_1 32
/* MICROSTEPS MOTOR 2*/
#define MICROSTEPS_2 32

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

#define MOTION_BUFFER_SIZE 32 // Ensure this is defined if it was in packet.h or similar, but here we see it used in manipulator.h. Assuming it IS in packet.h or needs to be here. 
// Checking context, MOTION_BUFFER_SIZE was used in manipulator.h but not defined there, likely in packet.h or ring_buffer.h. 
// Given I cannot see packet.h, I will assume it's available via packet.h include.

typedef enum {
    MOTOR_1,
    MOTOR_2
} motor_id_t;

typedef struct {
    Packet_t motion_buffer[MOTION_BUFFER_SIZE];
    volatile uint8_t mb_head;
    volatile uint8_t mb_tail;
    volatile uint8_t mb_count;
    encoder_t encoder_1;
    encoder_t encoder_2;
    TIM_HandleTypeDef motor_1;
    TIM_HandleTypeDef motor_2;
    TIM_HandleTypeDef pen_timer;
    float current_position; // wrt end-effector
    float current_velocity; // wrt end-effector
    float sensor_dt;

    Packet_t current_setpoint;

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
    
    volatile uint8_t telemetry_ready;
    float feedforward_scale_1;
    float feedforward_scale_2;
    float feedforward_acc_scale_1;
    float feedforward_acc_scale_2;
} manipulator_t;

#endif /* INC_MANIPULATOR_TYPES_H_ */
