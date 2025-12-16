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

    pid_controller_t pc1, pc2;
    pc1.Kp = 4.5f;
    pc1.Ki = 0.5f;
    pc1.Kd = 0.0f;
    pc1.previous_error = 0.0f;
    pc1.integral_error = 0.0f;

    pc2.Kp = 4.5f;
    pc2.Ki = 0.5f;
    pc2.Kd = 0.0f;
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
    manipulator->dt = (float)(arr + 1) * (float)(psc + 1) / (float)timer_clock;
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

    float dq0 = calculate_slope(&manipulator->q0, NUM_POINTS_FOR_VEL, manipulator->dt);
    float dq1 = calculate_slope(&manipulator->q1, NUM_POINTS_FOR_VEL, manipulator->dt);

    global_dq0 = dq0;
    global_dq1 = dq1;
    
    rbpush(&manipulator->dq0, dq0);
    rbpush(&manipulator->dq1, dq1);

    float ddq0 = calculate_slope(&manipulator->dq0, NUM_POINTS_FOR_ACC, manipulator->dt);
    float ddq1 = calculate_slope(&manipulator->dq1, NUM_POINTS_FOR_ACC, manipulator->dt);

    global_ddq0 = ddq0;
    global_ddq1 = ddq1;

    rbpush(&manipulator->ddq0, ddq0);
    rbpush(&manipulator->ddq1, ddq1);
}



void apply_velocity_input(manipulator_t *manipulator, float *u){
    int8_t dir1, dir2;
    uint32_t f;
    uint32_t ARR, CCR;
    uint32_t prescaler1, prescaler2;

    dir1 = u[0] < 0 ?  GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(DIR_1_GPIO_Port, DIR_1_Pin, dir1);

    dir2 = u[1] > 0 ?  GPIO_PIN_SET : GPIO_PIN_RESET; /* the second motor is upside-down */

    HAL_GPIO_WritePin(DIR_2_GPIO_Port, DIR_2_Pin, dir2);


	prescaler1 = (uint16_t)  5200; // 8400;//12000 ;//8400;
	f = HAL_RCC_GetPCLK1Freq()*2;
	ARR = (uint32_t)(fabsf(u[0]) < 0.0001 ? 0:(uint32_t)  (RESOLUTION*f/(fabsf(u[0])*REDUCTION_1*MICROSTEPS_1*prescaler1)));
	CCR = ARR /2;
    __HAL_TIM_SET_PRESCALER(&manipulator->motor_1, prescaler1); //2625
    __HAL_TIM_SET_AUTORELOAD(&manipulator->motor_1, ARR);
    __HAL_TIM_SET_COMPARE(&manipulator->motor_1, TIM_CHANNEL_1, CCR);
    manipulator->motor_1.Instance->EGR = TIM_EGR_UG;

   	prescaler2 = (uint16_t)  8400; //12000 ;//8400;
    f = HAL_RCC_GetPCLK1Freq()*2;
    ARR = (uint32_t)(fabsf(u[1]) < 0.0001 ? 0:(uint32_t)  (RESOLUTION*f/(fabsf(u[1])*REDUCTION_2*MICROSTEPS_2*prescaler2)));
    CCR = ARR /2;
   	__HAL_TIM_SET_PRESCALER(&manipulator->motor_2, prescaler2); //2625
    __HAL_TIM_SET_AUTORELOAD(&manipulator->motor_2, ARR);
    __HAL_TIM_SET_COMPARE(&manipulator->motor_2, TIM_CHANNEL_1, CCR);
    manipulator->motor_2.Instance->EGR = TIM_EGR_UG;
    return;
}


void calibration_start(manipulator_t *manipulator){
    manipulator->calibration_triggered = 1;
    apply_velocity_input(manipulator, (float[2]){-0.5, 0.0});
}

void calibration_stop(manipulator_t *manipulator){
    manipulator->calibration_triggered = 0;
    clear_manipulator_buffers(manipulator);
    apply_velocity_input(manipulator, (float[2]){0, 0});
}

uint8_t calibration_check(manipulator_t *manipulator){
    return manipulator->calibration_triggered;
}

void calibration_encoder(manipulator_t *manipulator, encoder_t *encoder, uint32_t calibration_value){
    apply_velocity_input(manipulator, (float[2]){0.0, 0.0});
    encoder_set_count(encoder, calibration_value);
}





void manipulator_update_position_controller(manipulator_t *manipulator, float target_q0_rad, float target_q1_rad) {
    // Limiti per l'anti-windup dell'integrale e per la velocità massima
    const float INTEGRAL_MAX = 10.0f;
    const float VELOCITY_MAX = 2.0f; // rad/s
    const float DT = 0.01f; // 10 ms

    // --- CONTROLLORE GIUNTO 0 ---
    float current_q0;
    rbpeek(&manipulator->q0, &current_q0); // Legge la posizione più recente senza rimuoverla

    float error_q0 = target_q0_rad - current_q0;

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

    float error_q1 = target_q1_rad - current_q1;
    float p_term_q1 = manipulator->position_controller_2.Kp * error_q1;
    manipulator->position_controller_2.integral_error += error_q1 * DT;
    if (manipulator->position_controller_2.integral_error > INTEGRAL_MAX) manipulator->position_controller_2.integral_error = INTEGRAL_MAX;
    if (manipulator->position_controller_2.integral_error < -INTEGRAL_MAX) manipulator->position_controller_2.integral_error = -INTEGRAL_MAX;
    float i_term_q1 = manipulator->position_controller_2.Ki * manipulator->position_controller_2.integral_error;
    float derivative_error_q1 = (error_q1 - manipulator->position_controller_2.previous_error) / DT;
    float d_term_q1 = manipulator->position_controller_2.Kd * derivative_error_q1;
    manipulator->position_controller_2.previous_error = error_q1;
    float u1 = p_term_q1 + i_term_q1 + d_term_q1;

    // Saturazione della velocità di comando
    if (u0 > VELOCITY_MAX) u0 = VELOCITY_MAX;
    if (u0 < -VELOCITY_MAX) u0 = -VELOCITY_MAX;
    if (u1 > VELOCITY_MAX) u1 = VELOCITY_MAX;
    if (u1 < -VELOCITY_MAX) u1 = -VELOCITY_MAX;

    // Applica le velocità calcolate ai motori

    global_v_calc1 = u0;
    global_v_calc2 = u1;


    float u[2] = {u0, u1};
    apply_velocity_input(manipulator, u);
}
