/*
 * encoder.h
 *
 *  Created on: Dec 14, 2025
 *      Author: santal
 */


#ifndef INC_PID_H_
#define INC_PID_H_

typedef struct {
    float Kp;
    float Ki;
    float Kd;
    float dt;

    float previous_error;
    float integral_error;
} pid_controller_t;

#endif /* INC_PID_H_ */