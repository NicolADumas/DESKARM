/*
 * encoder.h
 *
 *  Created on: Dec 14, 2025
 *      Author: santal
 */

#include "ring_buffer.h"

#ifndef INC_UTILS_H_
#define INC_UTILS_H_


float calculate_slope(ringbuffer_t *buffer, uint8_t num_points, float dt);

#endif /* INC_UTILS_H_ */