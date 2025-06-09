#include "crc.h"

/**
 * @brief   更新单个字节的CRC16值
 * @param   crc: 当前CRC值
 * @param   data: 输入字节
 * @param   polynomial: CRC多项式
 * @return  更新后的CRC16值
 */
uint16_t CRC16_Update(uint16_t crc, uint8_t data, uint16_t polynomial)
{
    crc ^= (uint16_t)data << 8;
    for (int i = 0; i < 8; i++)
    {
        if (crc & 0x8000)
            crc = (crc << 1) ^ polynomial;
        else
            crc <<= 1;
    }
    return crc;
}

/**
 * @brief   计算CCITT-CRC16值 (X^16 + X^12 + X^5 + 1)
 * @param   data: 数据指针
 * @param   length: 数据长度
 * @return  计算得到的CRC16值
 */
uint16_t CRC16_CCITT(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;

    // 使用CCITT-CRC16算法计算
    for (size_t i = 0; i < length; i++)
    {
        crc = CRC16_Update(crc, data[i], CRC16_CCITT_POLY);
    }

    return crc;
}

/**
 * @brief   计算MODBUS-CRC16值
 * @param   data: 数据指针
 * @param   length: 数据长度
 * @return  计算得到的CRC16值
 */
uint16_t CRC16_MODBUS(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= CRC16_MODBUS_POLY;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}
