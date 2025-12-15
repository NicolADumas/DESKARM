#include "manipulator.h"

float global_degs1, global_degs2;
int8_t global_dir1, global_dir2;

float global_dq0, global_dq1;
float global_ddq0, global_ddq1;



void manipulator_init(manipulator_t *manipulator, encoder_t *encoder_1, encoder_t *encoder_2, TIM_HandleTypeDef *htim){
    manipulator->encoder_1 = *encoder_1;
    manipulator->encoder_2 = *encoder_2;

    rbclear(&manipulator->q0);
    rbclear(&manipulator->q1);
    rbclear(&manipulator->dq0);
    rbclear(&manipulator->dq1);
    rbclear(&manipulator->ddq0);
    rbclear(&manipulator->ddq1);

    // Get the period of the timer in seconds
    manipulator->dt = (float)(htim->Init.Period + 1) / HAL_RCC_GetPCLK1Freq();
}

void manipulator_start(manipulator_t *manipulator){
    encoder_start(&manipulator->encoder_1);
    encoder_start(&manipulator->encoder_2);
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

    float prev_q0, prev_q1;
    rblast(&manipulator->q0, &prev_q0);
    rblast(&manipulator->q1, &prev_q1);

    float dq0 = (q0 - prev_q0);\
    dq0 = dq0 / manipulator->dt;
    float dq1 = (q1 - prev_q1);
    dq1 = dq1 / manipulator->dt;

    global_dq0 = dq0;
    global_dq1 = dq1;
    
    float prev_dq0, prev_dq1;
    rblast(&manipulator->dq0, &prev_dq0);
    rblast(&manipulator->dq1, &prev_dq1);

    float ddq0 = dq0 - prev_dq0;
    ddq0 = ddq0 / manipulator->dt;
    float ddq1 = dq1 - prev_dq1;
    ddq1 = ddq1 / manipulator->dt;

    global_ddq0 = ddq0;
    global_ddq1 = ddq1;

    rbpush(&manipulator->q0, q0);
    rbpush(&manipulator->q1, q1);
    rbpush(&manipulator->dq0, dq0);
    rbpush(&manipulator->dq1, dq1);
    rbpush(&manipulator->ddq0, ddq0);
    rbpush(&manipulator->ddq1, ddq1);
}
