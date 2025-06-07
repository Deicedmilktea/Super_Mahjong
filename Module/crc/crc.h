#ifndef __CRC_H
#define __CRC_H

#include <stdint.h>
#include <stddef.h>

/* CRC16 多项式选择 */
#define CRC16_CCITT_POLY 0x1021  // x^16 + x^12 + x^5 + 1
#define CRC16_IBM_POLY 0x8005    // x^16 + x^15 + x^2 + 1
#define CRC16_MODBUS_POLY 0x8005 // Modbus 使用的多项式

/* 函数声明 */
uint16_t CRC16_Calculate(const uint8_t *data, size_t length, uint16_t polynomial);
uint16_t CRC16_CCITT(const uint8_t *data, size_t length);
uint16_t CRC16_MODBUS(const uint8_t *data, size_t length);
uint16_t CRC16_Update(uint16_t crc, uint8_t data, uint16_t polynomial);

/* AI通信协议专用CRC计算函数 */
uint16_t Calculate_Frame_CRC16(const void *data, size_t length);

#endif /* __CRC_H */
