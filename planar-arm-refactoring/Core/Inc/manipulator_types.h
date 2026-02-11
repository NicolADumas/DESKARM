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

// MOTION_BUFFER_SIZE is defined in packet.h

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

/* --- MELODY DEFINITIONS (A=432Hz) --- */
#define NOTE_D4   288
#define NOTE_F4   343
#define NOTE_Fsp4 363 // F#4
#define NOTE_G4   384
#define NOTE_Gs4  408 // G#4
#define NOTE_A4   432
#define NOTE_B4   485
#define NOTE_C5   514
#define NOTE_Cs5  544 // C#5
#define NOTE_D5   576
#define NOTE_Ds5  610 // D#5
#define NOTE_E5   647
#define NOTE_F5   685
#define NOTE_Fsp5 726 // F#5
#define NOTE_G5   770
#define NOTE_Gs5  816 // G#5
#define NOTE_A5   864
#define NOTE_B5   970
#define NOTE_C6   1027
#define NOTE_D6   1153
#define NOTE_E6   1318
#define NOTE_F6   1396
#define NOTE_G6   1567
#define NOTE_A6   1760
#define NOTE_B6   1975
#define NOTE_C7   2093

void manipulator_play_melody(manipulator_t *manipulator, uint8_t melody_id);

#endif /* INC_MANIPULATOR_TYPES_H_ */
