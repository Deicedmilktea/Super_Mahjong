#ifndef MAHJONG_H
#define MAHJONG_H

#include <stdint.h>

#define ELEVATOR_BG_ENCODER 0
#define ELEVATOR_B1_ENCODER 0
#define ELEVATOR_B2_ENCODER 0
#define PUSH_1_OUT_ENCODER 0
#define PUSH_1_BACK_ENCODER 0
#define PUSH_2_OUT_ENCODER 0
#define PUSH_2_BACK_ENCODER 0

#define TURNTABLE_START_PWM 1000  // 转盘启动PWM值
#define TURNTABLE_STOP_PWM 0      // 转盘停止PWM值
#define CONVEYOR_1_START_PWM 1000 // 传送带1启动PWM值
#define CONVEYOR_1_STOP_PWM 0     // 传送带1停止PWM值
#define CONVEYOR_2_START_PWM 1000 // 传送带2启动PWM值
#define CONVEYOR_2_STOP_PWM 0     // 传送带2停止PWM值

#define MOTOR_LID_OPEN 0x0A
#define MOTOR_LID_CLOSE 0x0B

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
    DEAL_PUSH_OUT_1_WAIT_COMPLETE,   // 第一次等待推牌电机向外推完成
    DEAL_TRAY_DOWN_B2_START,         // 托盘下降启动
    DEAL_TRAY_DOWN_B2_WAIT_COMPLETE, // 等待托盘下降完成
    DEAL_LAYER2_CONV_START,          // 第2层，传送带启动
    DEAL_LAYER2_CONV_WAIT_TILE,      // 第2层，等待传送带的牌
    DEAL_PUSH_OUT_2_WAIT_COMPLETE,   // 第二次等待推牌电机向外推完成
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
    JUMP_PUSH_OUT_WAIT_COMPLETE,     // 等待推牌电机向外推完成
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
    REFILL_PUSH_OUT_WAIT_COMPLETE,     // 等待推牌电机向外推完成
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

/* 电机ID定义 */
typedef enum
{
    MOTOR_PUSH_1,
    MOTOR_PUSH_2,
    MOTOR_ELEVATOR,
    MOTOR_TURNTABLE,
} MotorID;

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
    PlayerID ai;             // AI玩家的ID
    PlayerID current_player; // 当前行动的玩家ID
    uint8_t wall_tile_count; // 牌墙剩余牌数 (例如初始144张)
    uint16_t index;          // 当前操作数量索引

    // 牌局阶段
    GlobalPhase global_phase;
    DealingSubState dealing_sub_state;
    JumpingSubState jumping_sub_state;
    SingleRefillSubState single_refill_sub_state;
    OverSubState over_sub_state;
} GlobalGameState;

void mahjong_init();
void mahjong_task();
GlobalGameState *get_global_game_state();

#endif // !MAHJONG_H
