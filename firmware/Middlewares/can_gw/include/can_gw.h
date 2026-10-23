/**
 * @file    can_gw.h
 * @brief   CAN-GW 应用层协议 —— 公共定义与接口
 *
 * 设计约束：
 *   1. 本模块不依赖 HAL，也不依赖任何具体芯片，移植时只需实现下面的 hw 钩子；
 *   2. 所有时间由调用者传入（ms），方便在裸机和 RTOS 下复用；
 *   3. 单帧数据（<= 6 字节）直接走 DATA 帧，可靠传输（> 6 字节）走 XFER 分片 + 停等重传。
 */
#ifndef CAN_GW_H
#define CAN_GW_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- 协议参数 ---------------- */
#define GWG_MAX_PAYLOAD      128u   /* 单次可靠传输的最大用户数据 */
#define GWG_ACK_TIMEOUT_MS   50u    /* 等 ACK 超时 */
#define GWG_MAX_RETRY        3u     /* 最大重传次数 */
#define GWG_FRAME_DLC        8u

/* 帧 ID 基址（11 位标准帧），实际 ID = 基址 | 节点号 */
#define GWG_ID_HEARTBEAT     0x100u
#define GWG_ID_DATA          0x200u
#define GWG_ID_CMD           0x300u
#define GWG_ID_XFER          0x400u
#define GWG_ID_ACK           0x500u

/* XFER 帧类型（byte0 高 nibble） */
#define GWG_T_FF             0x1Fu   /* 首帧：长度 + 前 3 字节数据 */
#define GWG_T_CF             0x2Cu   /* 续帧：SN + 5 字节数据 */
#define GWG_T_ACK            0xA0u   /* ACK：SN + 状态 */

/* ACK 状态码 */
#define GWG_ACK_OK           0x00u
#define GWG_ACK_CRCERR       0x01u
#define GWG_ACK_SEQERR       0x02u

/* ---------------- 硬件钩子（由用户实现） ---------------- */
/**
 * @brief  把一帧发到 CAN 控制器
 * @return true 表示已放入发送邮箱
 */
bool gwg_hw_send(uint32_t id, const uint8_t *data, uint8_t dlc);

/**
 * @brief  收到一帧用户数据后的回调（已通过 CRC32 校验）
 */
void gwg_on_payload(const uint8_t *data, uint16_t len);

/* ---------------- 统计 ---------------- */
typedef struct {
    uint32_t tx_frames;    /* 发出的帧数 */
    uint32_t rx_frames;    /* 收到的帧数 */
    uint32_t retrans;      /* 重传次数 */
    uint32_t err;          /* 协议错误（CRC/序号/超时） */
    uint32_t tec;          /* 发送错误计数（由上层写入） */
    uint32_t rec;          /* 接收错误计数（由上层写入） */
} gwg_stats_t;

/* ---------------- API ---------------- */
/** 初始化，own_node 为本节点的节点号（1 或 2） */
void gwg_init(uint8_t own_node);

/** 主循环里周期调用（建议 1~10 ms），now_ms 用同一个单调时钟 */
void gwg_poll(uint32_t now_ms);

/** 中断收到一帧时调用（只做搬运，不解析） */
void gwg_rx_frame(uint32_t id, const uint8_t *data, uint8_t dlc, uint32_t now_ms);

/** 发送用户数据（<= GWG_MAX_PAYLOAD），返回 false 表示忙或超长 */
bool gwg_send(const uint8_t *data, uint16_t len, uint32_t now_ms);

/** 是否正在传输 */
bool gwg_busy(void);

/** 取统计指针（只读） */
const gwg_stats_t *gwg_stats(void);

/** 生成一行 CSV：t,tx,rx,retrans,err,tec,rec（buf 至少 64 字节） */
void gwg_stats_csv(char *buf, uint32_t buf_len, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
#endif /* CAN_GW_H */
