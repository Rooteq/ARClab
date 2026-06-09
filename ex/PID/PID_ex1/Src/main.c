/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// --> include all necessary headers for
// printf() redirection
// FreeRTOS related headers
#include "pid.h"

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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define RX_QUEUE_SIZE 64
#define LINE_BUFF_SIZE 64

#define SET_VAL_DT 500
int _write(int file, char *ptr, int len) {
        HAL_UART_Transmit(&huart2, (uint8_t*) ptr, len, 50);
        return len;
}

SemaphoreHandle_t set_val_mutex;
QueueHandle_t measured_queue;
QueueHandle_t desired_queue;
QueueHandle_t control_queue;

typedef struct
{
  float kp, ki, kd;
  float e, e_prev, e_sum;
} pid_t;

QueueHandle_t rx_queue;
uint8_t rx_byte;

uint32_t convert_to_mv(uint16_t control)
{
  return (uint32_t)(1000 * 3.3 * control / 4095);
}

uint16_t calculate_control(pid_t* pid, uint16_t measured, uint16_t desired)
{
  pid->e = (float)desired - (float)measured;
  float de = pid->e - pid->e_prev;
  pid->e_sum = pid->e_sum + pid->e;
  pid->e_prev = pid->e;

  float control = pid->kp * pid->e + pid->ki * pid->e_sum + pid->kd * de;

  if(control < 0.0f)
    control = 0.0f;
  if(control > 4095.0f)
    control = 4095.0f;

  return (uint16_t)control;
} 

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  if(hadc->Instance == ADC1) // Check which ADC triggered it
  {
    uint16_t adc_value = HAL_ADC_GetValue(hadc);
    
    BaseType_t HPTW = pdFALSE;

    xQueueSendFromISR(measured_queue, &adc_value, &HPTW);

    portYIELD_FROM_ISR(HPTW);
  }

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if(huart->Instance != USART2) return;

  BaseType_t HPTW = pdFALSE;
  xQueueSendFromISR(rx_queue, &rx_byte, &HPTW);

  HAL_UART_Receive_IT(huart, &rx_byte, 1);

  portYIELD_FROM_ISR(HPTW);
}

void measureTask(void *args) {
	TickType_t xLastWakeTime;

	xLastWakeTime = xTaskGetTickCount();

  uint16_t measured_val = 0;

	for (;;) {
    HAL_ADC_Start_IT(&hadc1);
    if (xQueueReceive(measured_queue, &measured_val, portMAX_DELAY) == pdPASS)
    {
      // printf("%u\r\n", measured_val);
    }
 
	}
}

void controlTask(void *args) {
	TickType_t xLastWakeTime;


  pid_t pid;
  uint32_t period_ms = 20;

	pid.kp = 4.0f;
	pid.ki = 0.3f;
	pid.kd = 0.1f;

	pid.e = 0;
	pid.e_prev = 0;
	pid.e_sum = 0;

  uint16_t measured;
  uint16_t desired;
  uint16_t control_val;


	for (;;) {
    xLastWakeTime = xTaskGetTickCount();

		if (xQueuePeek(measured_queue, &measured, 100) == pdPASS) {

		}

		if (xQueuePeek(desired_queue, &desired, 100) != pdPASS) {
			desired = 0;
		}

    control_val = calculate_control(&pid, measured, desired);
		HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, control_val);

    if(xQueueOverwrite(control_queue, &control_val) != pdPASS)
    {
      // save error?
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(period_ms));
	}
}

void commTask(void *args) {
	TickType_t xLastWakeTime;
	uint32_t period_ms = 100;

	uint16_t measured = 0;
	uint16_t desired = 0;
	uint16_t control_val = 0;

	for (;;) {
    xLastWakeTime = xTaskGetTickCount();
		if (xQueuePeek(measured_queue, &measured, 100) == pdPASS) {

		}

		if (xQueuePeek(desired_queue, &desired, 100) == pdPASS) {

		}

		if (xQueuePeek(control_queue, &control_val, 100) == pdPASS) {

		}

    int16_t conv_des = convert_to_mv(desired);
    int16_t conv_mes = convert_to_mv(measured);

		// printf("mv: %ld, dv: %ld, cs: %ld\r\n", measured, desired, control_val);
		printf("%4d;%4d;%4d;%4d\r\n", convert_to_mv(measured), convert_to_mv(desired), convert_to_mv(control_val), (conv_des-conv_mes));

		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(period_ms));
	}
}

void userTask(void *args) {
	TickType_t xLastWakeTime;

	xLastWakeTime = xTaskGetTickCount();

  uint8_t byte;

	for (;;) {
    if(xQueueReceive(rx_queue, &byte, portMAX_DELAY) != pdTRUE)
      continue;

    int num = byte - '0';
    if(num < 0 || num > 9)
    {
      continue;
    }

    uint16_t set_value = num * SET_VAL_DT;
    xQueueOverwrite(desired_queue, &set_value);
	}
}

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
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM6_Init();
  MX_DAC1_Init();

  HAL_ADC_Start(&hadc1);
  /* USER CODE BEGIN 2 */
  rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(uint8_t));

	// mutex = xSemaphoreCreateMutex();

	measured_queue = xQueueCreate(1, sizeof(uint16_t));
	control_queue = xQueueCreate(1, sizeof(uint16_t));
	desired_queue = xQueueCreate(1, sizeof(uint16_t));

  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
	// enable UART receive in interrupt mode

	// --> create all necessary synchronization mechanisms
	// --> create all necessary tasks
	printf("Starting!\r\n");

  xTaskCreate(userTask, "user_task", 256, NULL, 1, NULL);
  xTaskCreate(measureTask, "measure_task", 256, NULL, 1, NULL);
  xTaskCreate(controlTask, "control_task", 256, NULL, 1, NULL);
  xTaskCreate(commTask, "comm_task", 256, NULL, 1, NULL);
	// --> start FreeRTOS scheduler
	vTaskStartScheduler();

	// --> start FreeRTOS scheduler

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */

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
	 tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
