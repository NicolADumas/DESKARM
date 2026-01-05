/*
 * controller.h
 *
 *  Created on: Jan 5, 2026
 *      Author: umby
 */

#ifndef INC_CONTROLLER_H_
#define INC_CONTROLLER_H_

#include "manipulator_types.h"

void manipulator_update_position_controller(manipulator_t *manipulator);
void manipulator_update_inverse_dynamics_controller(manipulator_t *manipulator);
void manipulator_set_motor_velocity(manipulator_t *manipulator, motor_id_t motor, float speed_rad_s);
void apply_velocity_input(manipulator_t *manipulator, float *u);
void manipulator_calc_B(manipulator_t *manipulator);
void manipulator_calc_C(manipulator_t *manipulator);
void manipulator_reset_pid_controllers(manipulator_t *manipulator);

#endif /* INC_CONTROLLER_H_ */
