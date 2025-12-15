#include "manipulator.h"

float global_degs1, global_degs2;
int8_t global_dir1, global_dir2;

float global_dq0, global_dq1;
float global_ddq0, global_ddq1;



void manipulator_init(manipulator_t *manipulator, encoder_t *encoder_1, encoder_t *encoder_2, TIM_HandleTypeDef *motor1, TIM_HandleTypeDef *motor2, TIM_HandleTypeDef *htim){
    manipulator->encoder_1 = *encoder_1;
    manipulator->encoder_2 = *encoder_2;
    manipulator->motor_1 = *motor1;
    manipulator->motor_2 = *motor2;

    clear_manipulator_buffers(manipulator);
    manipulator->calibration_triggered = 0;

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


