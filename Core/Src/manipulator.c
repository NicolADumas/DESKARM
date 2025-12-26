#include "manipulator.h"

float global_degs1, global_degs2;
int8_t global_dir1, global_dir2;

float global_dq0, global_dq1;
float global_ddq0, global_ddq1;
float global_v_calc1, global_v_calc2;



void manipulator_init(manipulator_t *manipulator, encoder_t *encoder_1, encoder_t *encoder_2, TIM_HandleTypeDef *motor1, TIM_HandleTypeDef *motor2, TIM_HandleTypeDef *htim){
    manipulator->encoder_1 = *encoder_1;
    manipulator->encoder_2 = *encoder_2;
    manipulator->motor_1 = *motor1;
    manipulator->motor_2 = *motor2;

    clear_manipulator_buffers(manipulator);
    manipulator->calibration_triggered = 0;
    manipulator->homed = 0;

    pid_controller_t pc1, pc2;
    pc1.Kp = 3.7f;
    pc1.Ki = 0.01f;
    pc1.Kd = 0.3f;
    pc1.previous_error = 0.0f;
    pc1.integral_error = 0.0f;

    pc2.Kp = 4.7f;
    pc2.Ki = 0.01f;
    pc2.Kd = 0.3f;
    pc2.previous_error = 0.0f;
    pc2.integral_error = 0.0f;

    // 3. Copia le strutture inizializzate nella struttura del manipolatore
    manipulator->position_controller_1 = pc1;
    manipulator->position_controller_2 = pc2;

    // calculate period with arr e psc and pclk1 frequency
    uint32_t pclk1_freq = HAL_RCC_GetPCLK1Freq();
    uint32_t timer_clock = pclk1_freq * 2; // TIMxCLK
    uint32_t arr = htim->Instance->ARR;
    uint32_t psc = htim->Instance->PSC;
    manipulator->sensor_dt = (float)(arr + 1) * (float)(psc + 1) / (float)timer_clock;
}

void clear_manipulator_buffers(manipulator_t *manipulator){
    rbclear(&manipulator->q0);
    rbclear(&manipulator->q1);
    rbclear(&manipulator->dq0);
    rbclear(&manipulator->dq1);
    rbclear(&manipulator->ddq0);
    rbclear(&manipulator->ddq1);
}

void manipulator_start(manipulator_t *manipulator){
    encoder_start(&manipulator->encoder_1);
    encoder_start(&manipulator->encoder_2);
    HAL_TIM_PWM_Start(&manipulator->motor_1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&manipulator->motor_2, TIM_CHANNEL_1);
}

void manipulator_read_status(manipulator_t *manipulator){
    float degs1, degs2;
    int8_t dir1, dir2;

    encoder_read(&manipulator->encoder_1, &degs1, &dir1);
    encoder_read(&manipulator->encoder_2, &degs2, &dir2);

    degs1 = degs1; // wrap-around already handled by encoder
    degs2 = -1*degs2;

    global_degs1 = degs1;
    global_degs2 = degs2;
    global_dir1 = dir1;
    global_dir2 = dir2;

    float q0 = degs1 * (M_PI / 180.0f); // Convert degrees to radians
    float q1 = degs2 * (M_PI / 180.0f);

    rbpush(&manipulator->q0, q0);
    rbpush(&manipulator->q1, q1);

    float dq0 = calculate_slope(&manipulator->q0, NUM_POINTS_FOR_VEL, manipulator->sensor_dt);
    float dq1 = calculate_slope(&manipulator->q1, NUM_POINTS_FOR_VEL, manipulator->sensor_dt);

    global_dq0 = dq0;
    global_dq1 = dq1;
    
    rbpush(&manipulator->dq0, dq0);
    rbpush(&manipulator->dq1, dq1);

    float ddq0 = calculate_slope(&manipulator->dq0, NUM_POINTS_FOR_ACC, manipulator->sensor_dt);
    float ddq1 = calculate_slope(&manipulator->dq1, NUM_POINTS_FOR_ACC, manipulator->sensor_dt);

    global_ddq0 = ddq0;
    global_ddq1 = ddq1;

    rbpush(&manipulator->ddq0, ddq0);
    rbpush(&manipulator->ddq1, ddq1);
}



void manipulator_set_motor_velocity(manipulator_t *manipulator, motor_id_t motor, float speed_rad_s) {
    // --- IMPOSTAZIONI COMUNI ---
    const uint32_t TIMER_INPUT_FREQ = HAL_RCC_GetPCLK1Freq() * 2;
    const uint32_t PRESCALER = 99;
    const uint32_t TIMER_COUNT_FREQ = TIMER_INPUT_FREQ / (PRESCALER + 1); // Should be 1,000,000 Hz

    TIM_HandleTypeDef *motor_timer;
    GPIO_TypeDef* dir_port;
    uint16_t dir_pin;
    float reduction;
    float microsteps;
    uint8_t dir_inverted;

    if (motor == MOTOR_1) {
        motor_timer = &manipulator->motor_1;
        dir_port = DIR_1_GPIO_Port;
        dir_pin = DIR_1_Pin;
        reduction = REDUCTION_1;
        microsteps = MICROSTEPS_1;
        dir_inverted = 0;
    } else { // MOTOR_2
        motor_timer = &manipulator->motor_2;
        dir_port = DIR_2_GPIO_Port;
        dir_pin = DIR_2_Pin;
        reduction = REDUCTION_2;
        microsteps = MICROSTEPS_2;
        dir_inverted = 1;
    }

    // --- IMPOSTA DIREZIONE ---
    GPIO_PinState dir_state = (speed_rad_s < 0) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    if (dir_inverted) {
        dir_state = (dir_state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }
    HAL_GPIO_WritePin(dir_port, dir_pin, dir_state);

    // --- CALCOLA FREQUENZA E ARR ---
    float abs_speed = fabsf(speed_rad_s);
    uint32_t arr;

    if (abs_speed < 0.001f) {
        arr = 0; // Ferma il motore
    } else {
        float motor_speed_rad_s = abs_speed * reduction;
        float step_freq = motor_speed_rad_s * (STEPS_PER_REVOLUTION / TWO_PI) * microsteps;

        if (step_freq > 0) {
            arr = (uint32_t)(TIMER_COUNT_FREQ / step_freq);
            if (arr < 10) arr = 10; // Limite per evitare frequenze troppo alte
        } else {
            arr = 0;
        }
    }

    // --- APPLICA VALORI AL TIMER ---
    __HAL_TIM_SET_PRESCALER(motor_timer, PRESCALER);
    __HAL_TIM_SET_AUTORELOAD(motor_timer, arr);
    __HAL_TIM_SET_COMPARE(motor_timer, TIM_CHANNEL_1, arr > 0 ? arr / 2 : 0); // Duty 50% o 0
    motor_timer->Instance->EGR = TIM_EGR_UG;
}


void apply_velocity_input(manipulator_t *manipulator, float *u){
    manipulator_set_motor_velocity(manipulator, MOTOR_1, u[0]);
    manipulator_set_motor_velocity(manipulator, MOTOR_2, u[1]);
}


void calibration_start(manipulator_t *manipulator){
    manipulator->calibration_triggered = 1;
    apply_velocity_input(manipulator, (float[2]){-0.5, 0.0});
}

void calibration_stop(manipulator_t *manipulator){
    manipulator->calibration_triggered = 0;
    clear_manipulator_buffers(manipulator);
    apply_velocity_input(manipulator, (float[2]){0, 0});
    manipulator_set_setpoints(manipulator, 0.0f, 0.0f);
}

uint8_t calibration_check(manipulator_t *manipulator){
    return manipulator->calibration_triggered;
}

void calibration_encoder(manipulator_t *manipulator, encoder_t *encoder, uint32_t calibration_value){
    apply_velocity_input(manipulator, (float[2]){0.0, 0.0});
    encoder_set_count(encoder, calibration_value);
}

uint8_t homing_check(manipulator_t *manipulator){
    return manipulator->homed;
}

void homing(manipulator_t *manipulator){
    static float last_error0, last_error1;
    static uint16_t counter = 0;

    manipulator_update_position_controller(manipulator);
    float current_q0, current_q1;
    rbpeek(&manipulator->q0, &current_q0);
    rbpeek(&manipulator->q1, &current_q1);

    float error_q0 = fabsf(manipulator->q0_setpoint - current_q0);
    float error_q1 = fabsf(manipulator->q1_setpoint - current_q1);

    if(error_q0 - last_error0 ==0 && error_q1 - last_error1 ==0 && error_q0 < 0.2f && error_q1 < 0.2f){
        counter++;
    }

    last_error0 = error_q0;
    last_error1 = error_q1;

    if(counter >= 10){ // 10 cycles of 10ms = 100ms stable
        manipulator->homed = 1;
        apply_velocity_input(manipulator, (float[2]){0.0, 0.0});
    }
    
}


uint8_t manipulator_error_check(manipulator_t *manipulator, float error_threshold1, float error_threshold2){
    float current_q0, current_q1;
    rbpeek(&manipulator->q0, &current_q0);
    rbpeek(&manipulator->q1, &current_q1);

    float error_q0 = fabsf(manipulator->q0_setpoint - current_q0);
    float error_q1 = fabsf(manipulator->q1_setpoint - current_q1);

    return (error_q0 < error_threshold1) && (error_q1 < error_threshold2);
}

void manipulator_set_setpoints(manipulator_t *manipulator, float q0_setpoint_rad, float q1_setpoint_rad){
    manipulator->q0_setpoint = q0_setpoint_rad;
    manipulator->q1_setpoint = q1_setpoint_rad;
}




void manipulator_update_position_controller(manipulator_t *manipulator) {
    // Limiti per l'anti-windup dell'integrale e per la velocità massima
    const float INTEGRAL_MAX = 10.0f;
    const float VELOCITY_MAX = 2.0f; // rad/s
    const float DT = 0.01f; // 10 ms

    // --- CONTROLLORE GIUNTO 0 ---
    float current_q0;
    rbpeek(&manipulator->q0, &current_q0); // Legge la posizione più recente senza rimuoverla

    float error_q0 = manipulator->q0_setpoint - current_q0;

    // Termine Proporzionale
    float p_term_q0 = manipulator->position_controller_1.Kp * error_q0;

    // Termine Integrale (con anti-windup)
    manipulator->position_controller_1.integral_error += error_q0 * DT;
    if (manipulator->position_controller_1.integral_error > INTEGRAL_MAX) manipulator->position_controller_1.integral_error = INTEGRAL_MAX;
    if (manipulator->position_controller_1.integral_error < -INTEGRAL_MAX) manipulator->position_controller_1.integral_error = -INTEGRAL_MAX;
    float i_term_q0 = manipulator->position_controller_1.Ki * manipulator->position_controller_1.integral_error;

    // Termine Derivativo
    float derivative_error_q0 = (error_q0 - manipulator->position_controller_1.previous_error) / DT;
    float d_term_q0 = manipulator->position_controller_1.Kd * derivative_error_q0;
    manipulator->position_controller_1.previous_error = error_q0;

    // Calcolo della velocità di comando (output del PID)
    float u0 = p_term_q0 + i_term_q0 + d_term_q0;

    // --- CONTROLLORE GIUNTO 1 ---
    float current_q1;
    rbpeek(&manipulator->q1, &current_q1);

    float error_q1 = manipulator->q1_setpoint - current_q1;
    float p_term_q1 = manipulator->position_controller_2.Kp * error_q1;
    manipulator->position_controller_2.integral_error += error_q1 * DT;
    if (manipulator->position_controller_2.integral_error > INTEGRAL_MAX) manipulator->position_controller_2.integral_error = INTEGRAL_MAX;
    if (manipulator->position_controller_2.integral_error < -INTEGRAL_MAX) manipulator->position_controller_2.integral_error = -INTEGRAL_MAX;
    float i_term_q1 = manipulator->position_controller_2.Ki * manipulator->position_controller_2.integral_error;
    float derivative_error_q1 = (error_q1 - manipulator->position_controller_2.previous_error) / DT;
    float d_term_q1 = manipulator->position_controller_2.Kd * derivative_error_q1;
    manipulator->position_controller_2.previous_error = error_q1;

    // Calcolo uscita di controllo (velocità desiderata)
    float u1 = p_term_q0 + i_term_q0 + d_term_q0;
    float u2 = p_term_q1 + i_term_q1 + d_term_q1;

    // --- SOFTWARE ENDSTOPS ---
	// Check limit for motor 1 (q0)
	if ((current_q0 <= Q0_MIN_RAD && u1 < 0.0f)) {
		u1 = 0.0f;
	}

	// Check limit for motor 2 (q1)
	if ((current_q1 >= Q1_MAX_RAD && u2 > 0.0f)) {
		u2 = 0.0f;
	}

    // Saturazione della velocità (clamping)
    if (u1 > VELOCITY_MAX) u1 = VELOCITY_MAX;
    if (u1 < -VELOCITY_MAX) u1 = -VELOCITY_MAX;
    if (u2 > VELOCITY_MAX) u2 = VELOCITY_MAX;
    if (u2 < -VELOCITY_MAX) u2 = -VELOCITY_MAX;

    global_v_calc1 = u1;
    global_v_calc2 = u2;
    

    // Applica le velocità calcolate ai motori
    apply_velocity_input(manipulator, (float[]){u1, u2});
}

void manipulator_update_inverse_dynamics_controller(manipulator_t *manipulator) {
    // Guadagni del controllore PID esterno
    const float Kp0 = 120.0f; // Guadagno proporzionale
    const float Ki0 = 1.0f;  // Guadagno integrale
    const float Kd0 = 17.0f;  // Guadagno derivativo

    const float Kp1 = 125.0f; // Guadagno proporzionale
    const float Ki1 = 1.0f;  // Guadagno integrale
    const float Kd1 = 14.0f;  // Guadagno derivativo

    // Limiti
    const float VELOCITY_MAX = 2.0f; // rad/s
    const float INTEGRAL_MAX = 10.0f;
    const float DT = 0.01f; // 10 ms

    // Leggi stati attuali (q, dq)
    float q0, q1, dq0, dq1;
    rbgetoffset(&manipulator->q0, 0, &q0);
    rbgetoffset(&manipulator->q1, 0, &q1);
    rbgetoffset(&manipulator->dq0, 0, &dq0);
    rbgetoffset(&manipulator->dq1, 0, &dq1);

    // Calcola matrici dinamiche B(q) e C(q, dq)
    manipulator_calc_B(manipulator);
    manipulator_calc_C(manipulator);

    // --- CONTROLLO GIUNTO 0 ---
    float err_q0 = manipulator->q0_setpoint - q0;
    manipulator->integral_error_q0 += err_q0 * DT;  
    // Anti-windup
    if (manipulator->integral_error_q0 > INTEGRAL_MAX) manipulator->integral_error_q0 = INTEGRAL_MAX;
    if (manipulator->integral_error_q0 < -INTEGRAL_MAX) manipulator->integral_error_q0 = -INTEGRAL_MAX;
    float err_dq0 = 0.0f - dq0; // Errore di velocità (setpoint di velocità è 0)

    // --- CONTROLLO GIUNTO 1 ---
    float err_q1 = manipulator->q1_setpoint - q1;
    manipulator->integral_error_q1 += err_q1 * DT;
    // Anti-windup
    if (manipulator->integral_error_q1 > INTEGRAL_MAX) manipulator->integral_error_q1 = INTEGRAL_MAX;
    if (manipulator->integral_error_q1 < -INTEGRAL_MAX) manipulator->integral_error_q1 = -INTEGRAL_MAX;
    float err_dq1 = 0.0f - dq1;

    // Legge di controllo PID per l'accelerazione di riferimento (ddq_ref)
    float ddq_ref0 = Kp0 * err_q0 + Ki0 * manipulator->integral_error_q0 + Kd0 * err_dq0;
    float ddq_ref1 = Kp1 * err_q1 + Ki1 * manipulator->integral_error_q1 + Kd1 * err_dq1;

    // Legge di controllo a dinamica inversa: u = B(q)*ddq_ref + C(q,dq)*dq
    // Calcolo di C*dq
    float C_dq0 = manipulator->C[0] * dq0 + manipulator->C[1] * dq1;
    float C_dq1 = manipulator->C[2] * dq0 + manipulator->C[3] * dq1;

    // Calcolo di B*ddq_ref
    float B_ddq0 = manipulator->B[0] * ddq_ref0 + manipulator->B[1] * ddq_ref1;
    float B_ddq1 = manipulator->B[2] * ddq_ref0 + manipulator->B[3] * ddq_ref1;

    // Comando finale (coppia/velocità)
    float u1 = B_ddq0 + C_dq0;
    float u2 = B_ddq1 + C_dq1;

    // Saturazione della velocità (clamping)
    if (u1 > VELOCITY_MAX) u1 = VELOCITY_MAX;
    if (u1 < -VELOCITY_MAX) u1 = -VELOCITY_MAX;
    if (u2 > VELOCITY_MAX) u2 = VELOCITY_MAX;
    if (u2 < -VELOCITY_MAX) u2 = -VELOCITY_MAX;

    global_v_calc1 = u1;
    global_v_calc2 = u2;

    // Applica le velocità calcolate ai motori
    apply_velocity_input(manipulator, (float[]){u1, u2});
}

void manipulator_reset_pid_controllers(manipulator_t *manipulator) {
    manipulator->position_controller_1.integral_error = 0.0f;
    manipulator->position_controller_1.previous_error = 0.0f;
    manipulator->position_controller_2.integral_error = 0.0f;
    manipulator->position_controller_2.previous_error = 0.0f;
    manipulator->integral_error_q0 = 0.0f;
    manipulator->integral_error_q1 = 0.0f;
}

void manipulator_calc_B(manipulator_t *manipulator){
    float q1, q2;
    rbgetoffset(&manipulator->q0, 0, &q1);
    rbgetoffset(&manipulator->q1, 0, &q2);

    manipulator->B[0] = (float) (0.0047413*cos(q1 + 2*q2) + 0.028554*cos(q1 + q2) + 0.078463*cos(q1) + 0.014224*cos(q2) + 0.045182);
    manipulator->B[1] = (float) (0.0023706*cos(q1 + 2*q2) + 0.023453*cos(q1 + q2) + 0.039491*cos(q1) + 0.0094825*cos(q2) + 0.01103);
    manipulator->B[2] = manipulator->B[1]; // La matrice è simmetrica
    manipulator->B[3] = (float) (0.018351*cos(q1 + q2) + 0.039491*cos(q1) + 0.0047413*cos(q2) + 0.011032);
}

void manipulator_calc_C(manipulator_t *manipulator){
    float q1, q2, dq1, dq2;
    rbgetoffset(&manipulator->q0, 0, &q1);
    rbgetoffset(&manipulator->q1, 0, &q2);
    rbgetoffset(&manipulator->dq0, 0, &dq1);
    rbgetoffset(&manipulator->dq1, 0, &dq2);

    manipulator->C[0] = (float) ( - 0.5*dq2*(0.0047413*sin(q1 + 2*q2) + 0.010203*sin(q1 + q2) + 0.0094825*sin(q2)));
    manipulator->C[1] = (float) ( - 0.000030008*(dq1 + dq2)*(79.0*sin(q1 + 2*q2) + 170*sin(q1 + q2) + 158*sin(q2)));
    manipulator->C[2] = (float) (   dq1*(0.0023706*sin(q1 + 2*q2) + 0.0051014*sin(q1 + q2) + 0.0047413*sin(q2)));
    manipulator->C[3] = (float) 0.0;
}