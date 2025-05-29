#ifndef RFID_H
#define RFID_H

#include "mahjong.h"
#include "bsp_usart.h"

#define RFID_BUFFER_SIZE 10 // RFID数据缓冲区大小

typedef struct
{
    int8_t head;                  // 队列头索引
    int8_t tail;                  // 队列尾索引
    int8_t count;                 // 当前队列中的元素数量
    Tile tiles[RFID_BUFFER_SIZE]; // 存储RFID数据的缓冲区
} RFIDQueue;

typedef struct
{
    USART_Instance *usart;  // USART实例
    RFIDQueue draw_tile;    // 摸牌
    RFIDQueue discard_tile; // 弃牌
} RFID_Instance;

#endif // !RFID_H