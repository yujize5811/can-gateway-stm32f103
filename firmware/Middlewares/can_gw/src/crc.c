/**
 * @file crc.c
 * @brief CRC8 / CRC32 实现（逐位算法，代码小，速度够用）
 */
#include "crc.h"

uint8_t crc8_calc(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00u;

    if (data == 0) {
        return 0u;
    }

    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8u; i++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

uint32_t crc32_calc(const uint8_t *data, uint16_t len)
{
    uint32_t crc = 0xFFFFFFFFu;

    if (data == 0) {
        return 0u;
    }

    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8u; i++) {
            if (crc & 1u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}
