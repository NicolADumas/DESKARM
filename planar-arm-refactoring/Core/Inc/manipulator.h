/*
 * encoder.h
 *
 *  Created on: Dec 14, 2025
 *      Author: umby
 */


#ifndef INC_MANIPULATOR_H_
#define INC_MANIPULATOR_H_

#include "manipulator_types.h"
#include "protocol.h"
#include "controller.h"
#include "tim.h"
#include "ring_buffer.h"
#include "encoder.h"
#include <math.h>
#include "utils.h"

#define PEN_UP 1
#define PEN_DOWN 0

// Note: Macros and Structs moved to manipulator_types.h

extern manipulator_t manipulator;

// This function should init and start the encoders
// REMOVE THIS FUNCTIONS FROM main.c, IT SHOULD ONLY CALL manipulator_init 
void manipulator_init(manipulator_t *manipulator,
                      encoder_t *encoder_1,
                      encoder_t *encoder_2,
                      TIM_HandleTypeDef *motor1,
                      TIM_HandleTypeDef *motor2,
                      TIM_HandleTypeDef *htim,
					  TIM_HandleTypeDef *pen_timer);



void manipulator_start(manipulator_t *manipulator);
void manipulator_read_status(manipulator_t *manipulator);
void manipulator_set_setpoints(manipulator_t *manipulator, float q0_setpoint_rad, float q1_setpoint_rad);

void clear_manipulator_buffers(manipulator_t *manipulator);
void calibration_start(manipulator_t *manipulator);
void calibration_stage2(manipulator_t *manipulator);
void calibration_stop(manipulator_t *manipulator);
uint8_t calibration_check(manipulator_t *manipulator);
void calibration_encoder(manipulator_t *manipulator, encoder_t *encoder, uint32_t calibration_value);

int manipulator_process_motion_queue(manipulator_t *manipulator);

void homing(manipulator_t *manipulator);
uint8_t homing_check(manipulator_t *manipulator);
uint8_t manipulator_error_check(manipulator_t *manipulator, float error_threshold1, float error_threshold2);
uint8_t manipulator_check_target_reached(manipulator_t *manipulator, float pos_tolerance, float vel_tolerance, uint32_t min_stable_time_ms);
void control_pen(manipulator_t *manipulator, uint8_t pen_state);

#endif
