#ifndef RFID_H
#define RFID_H

#include "mahjong.h"
#include "bsp_usart.h"

#define RFID_BUFFER_SIZE 20 // RFID数据缓冲区大小
#define RFID_DATA_HEAD 0xA5 // RFID接收数据头标识符

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

typedef struct
{
    USART_Init_Config_s usart_config; // USART初始化配置
} RFID_Init_Config_s;

typedef struct
{
    uint8_t head;   // 队列头索引 0xA5
    uint8_t action; // 玩家操作类型
    uint16_t index; // 牌的索引 (例如摸牌或弃牌的索引)
    uint8_t type;   // 牌类型 (万/条/筒/字)
    uint8_t value;  // 牌面值
} RFID_Receive_Data_s;

RFID_Instance *RFIDInit(RFID_Init_Config_s *init_config);
void RFIDCallback(USART_Instance *_usart_instance);
uint8_t RFIDEnqueue(RFIDQueue *queue, Tile tile);
uint8_t RFIDDequeue(RFIDQueue *queue, Tile *tile);

#endif // !RFID_H