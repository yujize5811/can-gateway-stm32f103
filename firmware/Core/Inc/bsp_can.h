/**
 * @file bsp_can.h
 * @brief 板级 CAN 初始化（STM32F103C8T6 + TJA1050）
 *
 * 默认使用回环模式，方便单板自测；双板联调时把 BSP_CAN_MODE 改成 CAN_MODE_NORMAL。
 */
#ifndef BSP_CAN_H
#define BSP_CAN_H

#include <stdbool.h>
#include "stm32f1xx_hal.h"

/* 500 kbps：APB1=36 MHz，Prescaler=8，tq=1+6+2=9 → 36e6/(8*9)=500 kHz */
#define BSP_CAN_PRESCALER   8u
#define BSP_CAN_BS1         CAN_BS1_6TQ
#define BSP_CAN_BS2         CAN_BS2_2TQ

/* 单板调试用 CAN_MODE_LOOPBACK，双板联调改 CAN_MODE_NORMAL */
#define BSP_CAN_MODE        CAN_MODE_LOOPBACK

extern CAN_HandleTypeDef hcan;

/** 初始化 CAN（含 GPIO / 过滤器 / 中断），成功返回 true */
bool bsp_can_init(void);

/** 读取错误计数器（TEC / REC），用于统计 */
void bsp_can_get_errors(uint8_t *tec, uint8_t *rec);

/** 当前是否处于 Bus-Off */
bool bsp_can_is_busoff(void);

/** 从 Bus-Off 恢复（重新初始化） */
bool bsp_can_recover(void);

#endif /* BSP_CAN_H */
