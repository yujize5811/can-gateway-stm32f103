/**
 * @file proto_rx.h
 * @brief 接收侧：分片重组状态机
 */
#ifndef CAN_GW_PROTO_RX_H
#define CAN_GW_PROTO_RX_H

#ifdef __cplusplus
extern "C" {
#endif

/** 复位接收状态机（gwg_init 时调用） */
void proto_rx_init(void);

#ifdef __cplusplus
}
#endif
#endif /* CAN_GW_PROTO_RX_H */
