/*
 * encoder.h
 *
 *  Created on: Dec 14, 2025
 *      Author: santal
 */

 #include "stm32f4xx_hal.h"

#ifndef INC_ENCODER_H_
#define INC_ENCODER_H_

#define CPR 200.0
#define STEPS_PER_ANGLE 1.80
#define ENCODER_RES 4


typedef struct {
	TIM_HandleTypeDef *htim_encoder;
	int32_t CNT_OFFSET;
} encoder_t;


void encoder_init(TIM_HandleTypeDef *htim_encoder, encoder_t *encoder);
void encoder_start(encoder_t *encoder);
void encoder_read(encoder_t *encoder, float *degs, int8_t *dir);

#endif /* INC_ENCODER_H_ */
