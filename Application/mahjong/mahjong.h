#ifndef MAHJONG_H
#define MAHJONG_H

#include <stdint.h>

#define MOTOR_CONVEYOR_1_START 0x01
#define MOTOR_CONVEYOR_1_STOP 0x02
#define MOTOR_CONVEYOR_2_START 0x03
#define MOTOR_CONVEYOR_2_STOP 0x04
#define MOTOR_TURNTABLE_START 0x05
#define MOTOR_TURNTABLE_STOP 0x06

#define MOTOR_ELEVATOR_BG 0x07
#define MOTOR_ELEVATOR_B1 0x08
#define MOTOR_ELEVATOR_B2 0x09

#define MOTOR_LID_OPEN 0x0A
#define MOTOR_LID_CLOSE 0x0B

#define WS2812_LED_NUM 30

#define L298N_MOTOR_PWM 500 // L298N电机PWM值

#define NUM_PLAYERS 4 // 玩家数量

// 阶段判断
typedef enum
{
    PHASE_IDLE,          // 空闲状态
    PHASE_DEALING,       // 发牌阶段
    PHASE_JUMPING,       // 跳牌阶段
    PHASE_SINGLE_REFILL, // 单张摸牌并补牌阶段
    PHASE_OVER,          // 阶段结束
    PHASE_ERROR          // 错误状态
} GlobalPhase;

// 发牌阶段
typedef enum
{
    DEAL_INIT,                       // 初始化
    DEAL_TRAY_DOWN_B1_START,         // 托盘下降启动
    DEAL_TRAY_DOWN_B1_WAIT_COMPLETE, // 等待托盘下降完成
    DEAL_LAYER1_CONV_START,          // 第1层，传送带启动
    DEAL_LAYER1_CONV_WAIT_TILE,      // 第1层，等待传送带的牌
    DEAL_TRAY_DOWN_B2_START,         // 托盘下降启动
    DEAL_TRAY_DOWN_B2_WAIT_COMPLETE, // 等待托盘下降完成
    DEAL_LAYER2_CONV_START,          // 第2层，传送带启动
    DEAL_LAYER2_CONV_WAIT_TILE,      // 第2层，等待传送带的牌
    DEAL_TRAY_UP_START,              // 托盘上升启动
    DEAL_TRAY_UP_WAIT_COMPLETE,      // 等待托盘上升完成
    DEAL_WAIT_TILE_CAUGHT,           // 等待牌被接走
} DealingSubState;

// 跳牌阶段
typedef enum
{
    JUMP_INIT,                       // 初始化
    JUMP_TRAY_DOWN_B1_START,         // 托盘下降启动
    JUMP_TRAY_DOWN_B1_WAIT_COMPLETE, // 等待托盘下降完成
    JUMP_CONV_START,                 // 第1层，传送带启动
    JUMP_CONV_WAIT_TILE,             // 第1层，等待传送带的牌
    JUMP_TRAY_UP_START,              // 托盘上升启动
    JUMP_TRAY_UP_WAIT_COMPLETE,      // 等待托盘上升完成
    JUMP_WAIT_TILE_CAUGHT,           // 等待牌被接走
} JumpingSubState;

// 单张摸牌并补牌阶段
typedef enum
{
    REFILL_INIT,                       // 初始化
    REFILL_TRAY_DOWN_B1_START,         // 托盘下降启动
    REFILL_TRAY_DOWN_B1_WAIT_COMPLETE, // 等待托盘下降完成
    REFILL_CONV_START,                 // 第1层，传送带启动
    REFILL_CONV_WAIT_TILE,             // 第1层，等待传送带的牌
    REFILL_TRAY_UP_START,              // 托盘上升启动
    REFILL_TRAY_UP_WAIT_COMPLETE,      // 等待托盘上升完成
    REFILL_WAIT_TILE_CAUGHT,           // 等待牌被接走
} SingleRefillSubState;

typedef enum
{
    OVER_INIT,                    // 初始化
    OVER_LID_OPEN_START,          // 托盘下降启动
    OVER_LID_OPEN_WAIT_COMPLETE,  // 等待托盘下降完成
    OVER_LID_CLOSE_START,         // 托盘上升启动
    OVER_LID_CLOSE_WAIT_COMPLETE, // 等待托盘上升完成
} OverSubState;

/* 对牌操作的定义 */
typedef enum
{
    ACTION_DRAW,    // 摸牌
    ACTION_DISCARD, // 打牌
    ACTION_PONG,    // 碰
    ACTION_KONG,    // 杠
    ACTION_WIN,     // 胡
    ACTION_CHOW,    // 吃
} PlayerAction;

/* 牌类型定义 */
typedef enum
{
    TILE_TYPE_WAN,  // 万
    TILE_TYPE_TIAO, // 条
    TILE_TYPE_TONG, // 筒
    TILE_TYPE_ZI    // 字
} TileType;

/* 牌定义 */
typedef struct
{
    TileType type; // 牌类型 (万/条/筒/字)
    uint8_t value; // 牌面值
} Tile;

/* 玩家信息 */
typedef struct
{
    Tile hand[14];         // 手牌 (最多14张)
    Tile discard[30];      // 弃牌堆 (最多30张)
    uint8_t hand_count;    // 当前手牌数
    uint8_t discard_count; // 弃牌数
} PlayerInfo;

/* 玩家代号 */
typedef enum
{
    PLAYER_ID_EAST,
    PLAYER_ID_SOUTH,
    PLAYER_ID_WEST,
    PLAYER_ID_NORTH
} PlayerID;

/* 全局牌局状态 */
typedef struct
{
    PlayerInfo players[NUM_PLAYERS];
    PlayerID current_dealer; // 当前庄家的玩家ID
    PlayerID current_player; // 当前行动的玩家ID
    uint8_t wall_tile_count; // 牌墙剩余牌数 (例如初始144张)
} GlobalGameState;

void mahjong_init();
void mahjong_task();

#endif // !MAHJONG_H