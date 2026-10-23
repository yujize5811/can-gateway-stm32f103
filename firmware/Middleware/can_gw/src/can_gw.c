/**
 * @file can_gw.c
 * @brief CAN-GW 协议核心：初始化、发送状态机（停等）、接收分发、统计
 */
#include "can_gw.h"
#include "can_gw_internal.h"
#include "proto_rx.h"
#include "crc.h"
#include <stdio.h>
#include <string.h>

/* ---------------- 内部状态 ---------------- */
static uint8_t s_node = 1u;
gwg_stats_t g_stats;

typedef enum {
    TX_IDLE = 0,
    TX_WAIT_ACK
} tx_state_t;

static struct {
    uint8_t     buf[GWG_MAX_PAYLOAD + 4u];
    uint16_t    total;          /* 用户数据 + 4 字节 CRC32 */
    uint16_t    offset;         /* 已被确认的字节数 */
    uint8_t     seq;            /* 当前在途帧的序号 */
    uint8_t     retry;
    uint32_t    deadline;
    tx_state_t  st;
    bool        in_use;
} s_tx;

/* 最近一帧的副本，用于超时重发 */
static uint8_t  s_last[GWG_FRAME_DLC];
static uint32_t s_last_id;
static uint8_t  s_last_dlc;
static uint16_t s_last_advance;

/* ---------------- 小工具 ---------------- */
uint8_t gwg_node(void)
{
    return s_node;
}

void gwg_send_ack(uint8_t sn, uint8_t status)
{
    uint8_t f[3];

    f[0] = (uint8_t)((GWG_T_ACK & 0xF0u) | (sn & 0x0Fu));
    f[1] = status;
    f[2] = crc8_calc(f, 2u);

    if (gwg_hw_send(GWG_ID_ACK | (uint32_t)s_node, f, 3u)) {
        g_stats.tx_frames++;
    }
}

bool gwg_busy(void)
{
    return s_tx.in_use;
}

const gwg_stats_t *gwg_stats(void)
{
    return &g_stats;
}

static void tx_finish(void)
{
    s_tx.in_use = false;
    s_tx.st = TX_IDLE;
}

/* ---------------- 发送状态机 ---------------- */
static void tx_emit(uint32_t now_ms)
{
    uint16_t remain = (uint16_t)(s_tx.total - s_tx.offset);
    uint8_t  f[GWG_FRAME_DLC];
    uint16_t adv;

    if (remain == 0u) {
        tx_finish();
        return;
    }

    memset(f, 0, sizeof(f));

    if (s_tx.offset == 0u && remain > 3u) {
        /* 首帧 FF：[0x1F][lenH][lenL][SN=0][d0][d1][d2][CRC8] */
        uint8_t n = 3u;
        f[0] = GWG_T_FF;
        f[1] = (uint8_t)(s_tx.total >> 8);
        f[2] = (uint8_t)(s_tx.total & 0xFFu);
        f[3] = 0u;
        memcpy(&f[4], &s_tx.buf[0], n);
        f[4 + n] = crc8_calc(f, (uint16_t)(4u + n));
        s_last_dlc = (uint8_t)(5u + n);
        adv = n;
    } else if (s_tx.offset == 0u) {
        /* 总长 <= 3：仍然用 FF 承载，dlc 相应缩短 */
        uint8_t n = (uint8_t)remain;
        f[0] = GWG_T_FF;
        f[1] = (uint8_t)(s_tx.total >> 8);
        f[2] = (uint8_t)(s_tx.total & 0xFFu);
        f[3] = 0u;
        memcpy(&f[4], &s_tx.buf[0], n);
        f[4 + n] = crc8_calc(f, (uint16_t)(4u + n));
        s_last_dlc = (uint8_t)(5u + n);
        adv = n;
    } else {
        /* 续帧 CF：[0x2C][SN][d0..d4][CRC8] */
        uint8_t n = (remain > 5u) ? 5u : (uint8_t)remain;
        f[0] = (uint8_t)(GWG_T_CF & 0xF0u);
        f[0] = (uint8_t)(f[0] | (s_tx.seq & 0x0Fu));
        memcpy(&f[2], &s_tx.buf[s_tx.offset], n);
        f[2 + n] = crc8_calc(f, (uint16_t)(2u + n));
        s_last_dlc = (uint8_t)(3u + n);
        adv = n;
    }

    s_last_id = GWG_ID_XFER | (uint32_t)s_node;
    adv = (uint16_t)adv;
    memcpy(s_last, f, s_last_dlc);

    if (gwg_hw_send(s_last_id, s_last, s_last_dlc)) {
        g_stats.tx_frames++;
        s_last_advance = adv;
        s_tx.st = TX_WAIT_ACK;
        s_tx.deadline = now_ms + GWG_ACK_TIMEOUT_MS;
    } else {
        /* 邮箱满：稍后重试，不算重传 */
        s_tx.deadline = now_ms + 2u;
        s_tx.st = TX_WAIT_ACK;
    }
}

bool gwg_send(const uint8_t *data, uint16_t len, uint32_t now_ms)
{
    uint32_t c;

    if (data == 0 || len == 0u || len > GWG_MAX_PAYLOAD) {
        return false;
    }
    if (s_tx.in_use) {
        return false;
    }

    memcpy(s_tx.buf, data, len);
    c = crc32_calc(data, len);
    s_tx.buf[len + 0u] = (uint8_t)(c & 0xFFu);
    s_tx.buf[len + 1u] = (uint8_t)((c >> 8) & 0xFFu);
    s_tx.buf[len + 2u] = (uint8_t)((c >> 16) & 0xFFu);
    s_tx.buf[len + 3u] = (uint8_t)((c >> 24) & 0xFFu);

    s_tx.total  = (uint16_t)(len + 4u);
    s_tx.offset = 0u;
    s_tx.seq    = 0u;
    s_tx.retry  = 0u;
    s_tx.in_use = true;
    s_tx.st     = TX_IDLE;

    tx_emit(now_ms);
    return true;
}

/* ---------------- ACK 处理（发送侧） ---------------- */
static void tx_on_ack(const uint8_t *d, uint8_t dlc, uint32_t now_ms)
{
    uint8_t sn;
    uint8_t status;

    if (dlc < 2u || (d[0] & 0xF0u) != (GWG_T_ACK & 0xF0u)) {
        return;
    }
    sn     = (uint8_t)(d[0] & 0x0Fu);
    status = d[1];

    if (!s_tx.in_use) {
        return;
    }
    if (sn != s_tx.seq) {
        return;         /* 迟到的旧 ACK，忽略 */
    }
    if (status != GWG_ACK_OK) {
        /* 对方要求重发 */
        g_stats.err++;
        s_tx.retry++;
        s_tx.st = TX_WAIT_ACK;
        s_tx.deadline = 0u;     /* 让 poll 立刻重发 */
        return;
    }

    s_tx.offset = (uint16_t)(s_tx.offset + s_last_advance);
    s_tx.seq    = (uint8_t)((s_tx.seq + 1u) & 0x0Fu);
    s_tx.retry  = 0u;

    if (s_tx.offset >= s_tx.total) {
        tx_finish();
    } else {
        tx_emit(now_ms);
    }
}

/* ---------------- 轮询 ---------------- */
void gwg_poll(uint32_t now_ms)
{
    if (!s_tx.in_use) {
        return;
    }
    if ((int32_t)(now_ms - s_tx.deadline) < 0) {
        return;     /* 还没超时 */
    }

    /* 超时或被要求重发 */
    if (s_tx.retry >= GWG_MAX_RETRY) {
        g_stats.err++;
        tx_finish();
        return;
    }

    s_tx.retry++;
    g_stats.retrans++;

    if (gwg_hw_send(s_last_id, s_last, s_last_dlc)) {
        g_stats.tx_frames++;
        s_tx.deadline = now_ms + GWG_ACK_TIMEOUT_MS;
    } else {
        s_tx.deadline = now_ms + 2u;
    }
}

/* ---------------- 接收入口 ---------------- */
void gwg_rx_frame(uint32_t id, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
    uint32_t base = id & 0x700u;

    if (data == 0 || dlc == 0u) {
        return;
    }
    g_stats.rx_frames++;

    switch (base) {
    case GWG_ID_ACK:
        tx_on_ack(data, dlc, now_ms);
        break;

    case GWG_ID_XFER:
        gwg_rx_feed(id, data, dlc, now_ms);
        break;

    case GWG_ID_DATA:
        /* 单帧数据：byte7 为 CRC8 */
        if (dlc == GWG_FRAME_DLC && crc8_calc(data, 7u) == data[7]) {
            gwg_on_payload(&data[1], 6u);
        } else {
            g_stats.err++;
        }
        break;

    default:
        break;      /* 心跳、命令等由应用层处理 */
    }
}

/* ---------------- 统计 ---------------- */
void gwg_stats_csv(char *buf, uint32_t buf_len, uint32_t now_ms)
{
    if (buf == 0 || buf_len == 0u) {
        return;
    }
    (void)snprintf(buf, (size_t)buf_len,
                   "%lu,%lu,%lu,%lu,%lu,%lu,%lu",
                   (unsigned long)(now_ms / 1000u),
                   (unsigned long)g_stats.tx_frames,
                   (unsigned long)g_stats.rx_frames,
                   (unsigned long)g_stats.retrans,
                   (unsigned long)g_stats.err,
                   (unsigned long)g_stats.tec,
                   (unsigned long)g_stats.rec);
}

void gwg_init(uint8_t own_node)
{
    s_node = (own_node == 2u) ? 2u : 1u;

    memset(&g_stats, 0, sizeof(g_stats));
    memset(&s_tx, 0, sizeof(s_tx));
    s_tx.st = TX_IDLE;

    proto_rx_init();
}
