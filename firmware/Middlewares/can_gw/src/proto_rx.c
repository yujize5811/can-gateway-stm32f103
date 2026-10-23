/**
 * @file proto_rx.c
 * @brief 接收侧分片重组：FF/CF 状态机，逐帧回 ACK
 *
 * 帧格式
 *   FF: [0x1F][lenH][lenL][SN=0][d0..d(3)][CRC8]
 *   CF: [0x2C|SN][d0..d5][CRC8]
 * 负载末尾 4 字节为整包 CRC32，接收完成后校验。
 */
#include "proto_rx.h"
#include "can_gw.h"
#include "can_gw_internal.h"
#include "crc.h"
#include <string.h>

static struct {
    uint8_t  buf[GWG_MAX_PAYLOAD];
    uint16_t total;
    uint16_t got;
    uint8_t  next_sn;
    bool     active;
} s_rx;

void proto_rx_init(void)
{
    memset(&s_rx, 0, sizeof(s_rx));
}

void gwg_rx_reset(void)
{
    s_rx.active = false;
    s_rx.total = 0u;
    s_rx.got = 0u;
    s_rx.next_sn = 0u;
}

static void finish(void)
{
    if (s_rx.total >= 4u && s_rx.got >= s_rx.total) {
        uint16_t len = (uint16_t)(s_rx.total - 4u);
        uint32_t c;

        c  = (uint32_t)s_rx.buf[len + 0u];
        c |= (uint32_t)s_rx.buf[len + 1u] << 8;
        c |= (uint32_t)s_rx.buf[len + 2u] << 16;
        c |= (uint32_t)s_rx.buf[len + 3u] << 24;

        if (crc32_calc(s_rx.buf, len) == c) {
            gwg_on_payload(s_rx.buf, len);
        } else {
            g_stats.err++;
        }
    } else {
        g_stats.err++;
    }
    gwg_rx_reset();
}

void gwg_rx_feed(uint32_t id, const uint8_t *d, uint8_t dlc, uint32_t now_ms)
{
    uint8_t type;
    uint8_t sn;
    uint8_t n;

    (void)id;
    (void)now_ms;

    if (d == 0 || dlc < 3u) {
        return;
    }

    /* 帧尾 CRC8 */
    if (crc8_calc(d, (uint16_t)(dlc - 1u)) != d[dlc - 1u]) {
        g_stats.err++;
        return;
    }

    type = (uint8_t)(d[0] & 0xF0u);

    if (type == (uint8_t)(GWG_T_FF & 0xF0u)) {
        /* ---- 首帧 ---- */
        if (dlc < 5u) {
            return;
        }
        s_rx.total = (uint16_t)(((uint16_t)d[1] << 8) | d[2]);

        if (s_rx.total < 4u || s_rx.total > GWG_MAX_PAYLOAD) {
            g_stats.err++;
            gwg_rx_reset();
            gwg_send_ack(0u, GWG_ACK_SEQERR);
            return;
        }

        s_rx.got     = 0u;
        s_rx.next_sn = 1u;
        s_rx.active  = true;

        n = (uint8_t)(dlc - 5u);
        if (n > 3u) {
            n = 3u;
        }
        if ((uint16_t)(s_rx.got + n) > s_rx.total) {
            n = (uint8_t)(s_rx.total - s_rx.got);
        }
        memcpy(&s_rx.buf[s_rx.got], &d[4], n);
        s_rx.got = (uint16_t)(s_rx.got + n);

        gwg_send_ack(0u, GWG_ACK_OK);

        if (s_rx.got >= s_rx.total) {
            finish();
        }
        return;
    }

    if (type == (uint8_t)(GWG_T_CF & 0xF0u)) {
        /* ---- 续帧 ---- */
        if (!s_rx.active) {
            return;
        }
        sn = (uint8_t)(d[0] & 0x0Fu);

        if (sn != s_rx.next_sn) {
            g_stats.err++;
            gwg_send_ack(sn, GWG_ACK_SEQERR);
            return;
        }

        n = (uint8_t)(dlc - 3u);
        if (n > 5u) {
            n = 5u;
        }
        if ((uint16_t)(s_rx.got + n) > s_rx.total) {
            n = (uint8_t)(s_rx.total - s_rx.got);
        }
        memcpy(&s_rx.buf[s_rx.got], &d[2], n);
        s_rx.got = (uint16_t)(s_rx.got + n);

        gwg_send_ack(sn, GWG_ACK_OK);
        s_rx.next_sn = (uint8_t)((s_rx.next_sn + 1u) & 0x0Fu);

        if (s_rx.got >= s_rx.total) {
            finish();
        }
        return;
    }
}
