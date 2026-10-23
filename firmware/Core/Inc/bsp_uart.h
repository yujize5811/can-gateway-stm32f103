/**
 * @file bsp_uart.h
 * @brief USART1 调试串口（PA9/PA10，115200），并重定向 printf
 */
#ifndef BSP_UART_H
#define BSP_UART_H

#include "stm32f1xx_hal.h"

extern UART_HandleTypeDef huart1;

/** 初始化 USART1 并重定向 printf */
void bsp_uart_init(uint32_t baud);

#endif /* BSP_UART_H */
