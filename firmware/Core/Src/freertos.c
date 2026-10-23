/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "bsp_uart.h"
#include "bsp_can.h"
#include "can_gw.h"
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
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  uint32_t last_tx = 0;
  uint32_t last_stat = 0;
  char csv[80];
  uint8_t payload[8] = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x18};

  bsp_uart_init(115200);
  printf("\r\n=====================================\r\n");
  printf(" CAN-GW gateway  (node 1)\r\n");
  printf(" mode: %s\r\n", (BSP_CAN_MODE == CAN_MODE_LOOPBACK) ? "LOOPBACK" : "NORMAL");
  printf(" 500 kbps, PA11/PA12, USART1 115200\r\n");
  printf("=====================================\r\n");

  if (!bsp_can_init()) {
      printf("[ERR] CAN init failed\r\n");
  }
  gwg_init(1);

  /* Infinite loop */
  for(;;)
  {
      uint32_t now = HAL_GetTick();
      uint8_t tec = 0;
      uint8_t rec = 0;

      gwg_poll(now);

      /* 每 1 s 发一包 8 字节数据（会走 FF/CF 分片） */
      if (((now - last_tx) >= 1000u) && (!gwg_busy())) {
          last_tx = now;
          if (gwg_send(payload, sizeof(payload), now)) {
              printf("[TX] 8 bytes\r\n");
          }
      }

      /* 每秒输出一行统计，格式与 tools/monitor.py 一致 */
      if ((now - last_stat) >= 1000u) {
          last_stat = now;
          bsp_can_get_errors(&tec, &rec);
          gwg_stats_t s = *gwg_stats();
          s.tec = tec;
          s.rec = rec;
          gwg_stats_csv(csv, sizeof(csv), now);
          printf("%s\r\n", csv);
      }

      if (bsp_can_is_busoff()) {
          printf("[WARN] Bus-Off detected, recovering...\r\n");
          (void)bsp_can_recover();
      }

      osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

