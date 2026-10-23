/**
 * @file stm32f1_port_example.c
 * @brief HAL 移植示例（复制到 Core/Src 后按需修改）
 *
 * 演示：
 *   1. 实现 gwg_hw_send / gwg_on_payload
 *   2. 在 CAN RX 中断里调用 gwg_rx_frame
 *   3. 主循环驱动 gwg_poll
 */
#include "main.h"
#include "can_gw.h"

/* CubeMX 生成的句柄 */
extern CAN_HandleTypeDef hcan;

/* ---------------- 钩子 1：发一帧 ---------------- */
bool gwg_hw_send(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    CAN_TxHeaderTypeDef hdr;
    uint32_t mailbox;
    uint8_t  buf[8] = {0};

    if (data == 0 || dlc > 8u) {
        return false;
    }
    for (uint8_t i = 0; i < dlc; i++) {
        buf[i] = data[i];
    }

    hdr.StdId = id & 0x7FFu;
    hdr.IDE   = CAN_ID_STD;
    hdr.RTR   = CAN_RTR_DATA;
    hdr.DLC   = dlc;
    hdr.TransmitGlobalTime = DISABLE;

    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0u) {
        return false;   /* 邮箱满，交给上层稍后重试 */
    }
    return HAL_CAN_AddTxMessage(&hcan, &hdr, buf, &mailbox) == HAL_OK;
}

/* ---------------- 钩子 2：收到完整数据 ---------------- */
void gwg_on_payload(const uint8_t *data, uint16_t len)
{
    /* TODO: 交给你的业务层，例如打印或转发到串口 */
    (void)data;
    (void)len;
}

/* ---------------- 中断入口 ---------------- */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_ptr)
{
    CAN_RxHeaderTypeDef hdr;
    uint8_t data[8];

    if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO0, &hdr, data) != HAL_OK) {
        return;
    }
    if (hdr.IDE != CAN_ID_STD) {
        return;
    }
    gwg_rx_frame(hdr.StdId, data, (uint8_t)hdr.DLC, HAL_GetTick());
}

/* ---------------- 主循环示例 ---------------- */
void app_loop_example(void)
{
    static uint32_t last_tx = 0;
    uint8_t  payload[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

    gwg_init(1);        /* 节点 1 */

    while (1) {
        uint32_t now = HAL_GetTick();

        gwg_poll(now);

        /* 每 1 s 发一次 8 字节数据（会走 FF/CF 分片） */
        if (now - last_tx >= 1000u && !gwg_busy()) {
            last_tx = now;
            (void)gwg_send(payload, sizeof(payload), now);
        }
    }
}
