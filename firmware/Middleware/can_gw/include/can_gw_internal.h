/**
 * @file can_gw_internal.h
 * @brief 模块内部共享声明（不对外暴露）
 */
#ifndef CAN_GW_INTERNAL_H
#define CAN_GW_INTERNAL_H

#include "can_gw.h"

extern gwg_stats_t g_stats;

/** 当前节点号（1 或 2） */
uint8_t gwg_node(void);

/** 发送 ACK 帧：sn 为被确认的序号，status 见 GWG_ACK_* */
void gwg_send_ack(uint8_t sn, uint8_t status);

/** 接收侧：喂入一帧 CMD/DATA/XFER 数据（由 gwg_rx_frame 调用） */
void gwg_rx_feed(uint32_t id, const uint8_t *data, uint8_t dlc, uint32_t now_ms);

/** 接收侧重置（超时或新传输开始） */
void gwg_rx_reset(void);

#endif /* CAN_GW_INTERNAL_H */
