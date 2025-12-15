/*
 * encoder.h
 *
 *  Created on: Dec 14, 2025
 *      Author: umby
 */


#ifndef INC_MANIPULATOR_H_
#define INC_MANIPULATOR_H_

#include "stm32f4xx_hal.h"
#include "ring_buffer.h"
#include "encoder.h"
#include <math.h>

typedef struct {
    encoder_t encoder_1;
    encoder_t encoder_2;
    float current_position; // wrt end-effector
    float current_velocity; // wrt end-effector
    float dt;

    ringbuffer_t q0;
    ringbuffer_t q1;
    ringbuffer_t dq0;
    ringbuffer_t dq1;
    ringbuffer_t ddq0;
    ringbuffer_t ddq1;
    
} manipulator_t;

// This function should init and start the encoders
// REMOVE THIS FUNCTIONS FROM main.c, IT SHOULD ONLY CALL manipulator_init 
void manipulator_init(manipulator_t *manipulator,
                      encoder_t *encoder_1,
                      encoder_t *encoder_2,
                      TIM_HandleTypeDef *htim);
void manipulator_start(manipulator_t *manipulator);
void manipulator_read_status(manipulator_t *manipulator);


#endif 