/**
 * @file crc.h
 * @brief CRC8（单帧）与 CRC32（整包）计算
 */
#ifndef CAN_GW_CRC_H
#define CAN_GW_CRC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** CRC8，多项式 0x07，初值 0x00，不反转 */
uint8_t crc8_calc(const uint8_t *data, uint16_t len);

/** CRC32，IEEE 802.3（反射多项式 0xEDB88320），初值 0xFFFFFFFF，结果取反 */
uint32_t crc32_calc(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
#endif /* CAN_GW_CRC_H */
