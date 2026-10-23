/**
 * @file bsp_can.c
 * @brief CAN1 板级驱动：引脚、位定时、过滤器、中断、协议钩子
 *
 * 接线：PA11=CAN_RX，PA12=CAN_TX → TJA1050 → 总线（两端各 120Ω）
 */
#include "bsp_can.h"
#include "can_gw.h"
#include <string.h>
#include <stdio.h>

CAN_HandleTypeDef hcan;

/* ---------------- 内部 ---------------- */
static void can_gpio_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* RX：浮空输入 */
    g.Pin  = GPIO_PIN_11;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    /* TX：复用推挽 */
    g.Pin   = GPIO_PIN_12;
    g.Mode  = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
}

static bool can_filter_init(void)
{
    CAN_FilterTypeDef f = {0};

    /* 全通：ID 与掩码都为 0，接收所有标准帧到 FIFO0 */
    f.FilterBank           = 0;
    f.FilterMode           = CAN_FILTERMODE_IDMASK;
    f.FilterScale          = CAN_FILTERSCALE_32BIT;
    f.FilterIdHigh         = 0x0000;
    f.FilterIdLow          = 0x0000;
    f.FilterMaskIdHigh     = 0x0000;
    f.FilterMaskIdLow      = 0x0000;
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterActivation     = ENABLE;
    f.SlaveStartFilterBank = 14;

    return HAL_CAN_ConfigFilter(&hcan, &f) == HAL_OK;
}

/* HAL 会在 HAL_CAN_Init 里回调它 */
void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan_ptr)
{
    (void)hcan_ptr;

    __HAL_RCC_CAN1_CLK_ENABLE();
    can_gpio_init();

    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}

/* ---------------- 对外接口 ---------------- */
bool bsp_can_init(void)
{
    hcan.Instance = CAN1;

    hcan.Init.Prescaler         = BSP_CAN_PRESCALER;
    hcan.Init.Mode              = BSP_CAN_MODE;
    hcan.Init.SyncJumpWidth     = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1          = BSP_CAN_BS1;
    hcan.Init.TimeSeg2          = BSP_CAN_BS2;
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff        = DISABLE;  /* 自己管理 Bus-Off，方便统计恢复时间 */
    hcan.Init.AutoWakeUp        = DISABLE;
    hcan.Init.AutoRetransmission= ENABLE;
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan) != HAL_OK) {
        return false;
    }
    if (!can_filter_init()) {
        return false;
    }
    if (HAL_CAN_Start(&hcan) != HAL_OK) {
        return false;
    }
    if (HAL_CAN_ActivateNotification(&hcan,
            CAN_IT_RX_FIFO0_MSG_PENDING |
            CAN_IT_ERROR |
            CAN_IT_BUSOFF) != HAL_OK) {
        return false;
    }
    return true;
}

void bsp_can_get_errors(uint8_t *tec, uint8_t *rec)
{
    if (tec != 0) {
        *tec = (uint8_t)(hcan.Instance->ESR >> 16);
    }
    if (rec != 0) {
        *rec = (uint8_t)(hcan.Instance->ESR >> 24);
    }
}

bool bsp_can_is_busoff(void)
{
    return (hcan.Instance->ESR & CAN_ESR_BOFF) != 0u;
}

bool bsp_can_recover(void)
{
    (void)HAL_CAN_Stop(&hcan);
    return bsp_can_init();
}

/* ---------------- 协议钩子 ---------------- */
bool gwg_hw_send(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    CAN_TxHeaderTypeDef hdr;
    uint32_t mailbox;
    uint8_t buf[8] = {0};

    if (data == 0 || dlc == 0u || dlc > 8u) {
        return false;
    }
    memcpy(buf, data, dlc);

    hdr.StdId = id & 0x7FFu;
    hdr.IDE   = CAN_ID_STD;
    hdr.RTR   = CAN_RTR_DATA;
    hdr.DLC   = dlc;
    hdr.TransmitGlobalTime = DISABLE;

    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0u) {
        return false;       /* 邮箱满，由协议层稍后重试 */
    }
    return HAL_CAN_AddTxMessage(&hcan, &hdr, buf, &mailbox) == HAL_OK;
}

/* ---------------- 中断 ---------------- */
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

/* 协议层交付上来的一包完整数据（已通过 CRC32） */
void gwg_on_payload(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    printf("[RX] %u bytes:", (unsigned)len);
    for (i = 0; i < len && i < 16u; i++) {
        printf(" %02X", data[i]);
    }
    if (len > 16u) {
        printf(" ...");
    }
    printf("\r\n");
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}

void CAN1_RX1_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}

void CAN1_SCE_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}
