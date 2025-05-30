#ifndef AI_H
#define AI_H

#include "bsp_usart.h"

#define AI_RECV_HEADER 0xA7 // AI接收数据头标识符
#define AI_SEND_HEADER 0xA8 // AI发送数据头标识符
#define AI_SEND_TAIL 0xA9   // AI发送数据尾标识符
#define AI_RECV_SIZE 3u     // AI接收数据大小
#define AI_SEND_SIZE 9u     // AI发送数据大小

typedef struct
{
    USART_Init_Config_s usart_config; // USART初始化配置
} AI_Init_Config_s;

typedef struct
{
    uint8_t head;       // 数据头标识符
    uint8_t tile_type;  // 牌类型 (万/条/筒/字)
    uint8_t tile_value; // 牌面值
} AI_Receive_s;

typedef struct
{
    uint8_t head;          // 数据头标识符
    uint8_t current_phase; // 牌局进行阶段
    uint8_t player;        // 玩家编号
    uint8_t action;        // 玩家操作类型
    uint8_t tile_type;     // 牌类型 (万/条/筒/字)
    uint8_t tile_value;    // 牌面值
    uint16_t check_sum;    // 校验和
    uint8_t tail;          // 数据尾标识符
} AI_Send_s;

typedef struct
{
    USART_Instance *usart; // AI模块对应的USART实例
    AI_Receive_s ai_recv;  // AI接收数据
    AI_Send_s ai_send;     // AI发送数据
} AI_Instance;

AI_Instance *AIInit(AI_Init_Config_s *init_config);
void AICallback(USART_Instance *_usart_instance);
void AISendData(AI_Instance *ai_instance, AI_Send_s *data_to_send);

#endif // !AI_H