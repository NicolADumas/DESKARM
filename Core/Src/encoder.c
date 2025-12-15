/*
 * encoder.c
 *
 *  Created on: Dec 14, 2025
 *      Author: santal
 */

#include "encoder.h"

void encoder_init(TIM_HandleTypeDef *htim_encoder, encoder_t *encoder) {
	encoder->htim_encoder = htim_encoder;
	encoder->CNT_OFFSET = htim_encoder->Instance->ARR / 2;
}

void encoder_start(encoder_t *encoder) {
	encoder->htim_encoder->Instance->CNT = encoder->htim_encoder->Instance->ARR / 2;
	HAL_TIM_Encoder_Start(encoder->htim_encoder, TIM_CHANNEL_ALL);
}


void encoder_read(encoder_t *encoder, float *degs, int8_t *dir) {
    int32_t counter;
    counter = (int32_t)(encoder->htim_encoder->Instance->CNT) - (int32_t)encoder->CNT_OFFSET;
    *degs = (float)counter / (float)(encoder->htim_encoder->Instance->ARR) * 360.0f;
    *dir = (__HAL_TIM_DIRECTION_STATUS(encoder->htim_encoder) * 2 - 1);
}

void encoder_set_count(encoder_t *encoder, uint32_t count){
    HAL_TIM_Encoder_Stop(encoder->htim_encoder, TIM_CHANNEL_ALL);
    encoder->htim_encoder->Instance->CNT = count;
    HAL_TIM_Encoder_Start(encoder->htim_encoder, TIM_CHANNEL_ALL);
}
