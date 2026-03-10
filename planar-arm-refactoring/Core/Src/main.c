/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "encoder.h"
#include "manipulator.h"
// Note: manipulator.h includes protocol.h and controller.h now, so functions are available.
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// --- GLOBAL INSTANCES ---
manipulator_t manipulator;
encoder_t enc1;
encoder_t enc2;
uint32_t tick=0;
uint32_t switch_count1=0;
uint32_t switch_count2=0;

// Configurable Control Loop Period (default 10ms)
volatile uint32_t control_loop_period_ms = 10;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM2_Init();
  MX_TIM5_Init();
  MX_TIM10_Init();
  MX_TIM11_Init();
  /* USER CODE BEGIN 2 */

  // --- SYSTEM INITIALIZATION ---
  encoder_init(&htim3, &enc1);
  encoder_init(&htim4, &enc2);
  manipulator_init(&manipulator, &enc1, &enc2, &htim5, &htim2, &htim10, &htim11);
  manipulator_start(&manipulator);
  HAL_TIM_Base_Start_IT(&htim10);

  calibration_start(&manipulator);
  // Start UART DMA Reception
  HAL_UART_Receive_DMA(&huart2, uart_rx_buffer, UART_RX_BUFFER_SIZE);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  static uint8_t homing_completed_first_time = 0;
  static uint8_t target_reached_flag = 0;

  // --- MAIN LOOP ---
  while (1)
  {
    // Process UART packets
    manipulator_uart_process(&manipulator, &huart2);
    // Send Telemetry
    manipulator_handle_telemetry(&manipulator, &huart2);

    // If calibrating, skip the rest and handle limits via polling
    if(calibration_check(&manipulator)){
        if(manipulator.calibration_triggered == 1){
            // Polling Limit Switch 1
            if(HAL_GPIO_ReadPin(LIMIT_SWITCH_1_GPIO_Port, LIMIT_SWITCH_1_Pin) == GPIO_PIN_SET){
                calibration_stage2(&manipulator);
                tick = HAL_GetTick(); // Use tick as a debounce timer
            }
        } else if(manipulator.calibration_triggered == 2){
            // Polling Limit Switch 2 (with 500ms debounce from stage 2 start)
            if((HAL_GetTick() - tick) > 500){
                if(HAL_GPIO_ReadPin(LIMIT_SWITCH_2_GPIO_Port, LIMIT_SWITCH_2_Pin) == GPIO_PIN_SET){
                    calibration_encoder(&manipulator, &manipulator.encoder_2, CALIBRATION_2);
                    calibration_stop(&manipulator);
                }
            }
        }
        continue;
    }

    // If not homed, perform homing
	if(homing_check(&manipulator) == 0){ 
		if((HAL_GetTick() - tick) > 10){
			tick = HAL_GetTick();
			homing(&manipulator);
		}
		continue;
	}

    // --- CONTROL LOOP (approx 100Hz default, configurable) ---
    if((HAL_GetTick() - tick) > control_loop_period_ms){
        tick = HAL_GetTick();

        // Consume a point from the queue if available
        if (manipulator_process_motion_queue(&manipulator)) {
            target_reached_flag = 0; // If we have new points, reset arrival flag
        }

        if (target_reached_flag == 0) {
            // Check if target is reached (tolerance: 0.0025 rad position, 0.01 rad/s velocity, stable for 50ms (5 ticks))
            // Note: if executing a trajectory, the target changes continuously, so check_target_reached might not trigger until we stop.
            // However, if the buffer empties, process_motion_queue sets dq=0, so we might stop.
            
            if (manipulator_check_target_reached(&manipulator, 0.0025f, 0.01f, 5)) {
                target_reached_flag = 1;
                // Stop motors
                apply_velocity_input(&manipulator, (float[]){0.0f, 0.0f});
            } else {
                // Run position control
                manipulator_update_position_controller(&manipulator); 
            }
        }
    }


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/* USER CODE BEGIN 4 */
// --- INTERRUPT CALLBACKS ---
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	if(htim->Instance == TIM10){ /* check if it is the proper instance */
    manipulator_read_status(&manipulator);
	}
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
    // Limit switch calibration logic is now polled in the main loop to avoid missing edges.
    // We only keep the standalone stop logic when not calibrating.
    if(GPIO_Pin==LIMIT_SWITCH_1_Pin){
    	switch_count1+=1;
        if(manipulator.calibration_triggered == 0){
          manipulator_set_motor_velocity(&manipulator, MOTOR_1, 0.0f);
        }
    }

    if (GPIO_Pin==LIMIT_SWITCH_2_Pin){
    	switch_count2+=1;
        if(manipulator.calibration_triggered == 0){
          manipulator_set_motor_velocity(&manipulator, MOTOR_2, 0.0f);
        }
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
      ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
