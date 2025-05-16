#ifndef MAHJONG_H
#define MAHJONG_H

#define MOTOR_CONVEYOR_1_START 0x01
#define MOTOR_CONVEYOR_1_STOP 0x02
#define MOTOR_CONVEYOR_2_START 0x03
#define MOTOR_CONVEYOR_2_STOP 0x04
#define MOTOR_TURNTABLE_START 0x05
#define MOTOR_TURNTABLE_STOP 0x06

#define MOTOR_ELEVATOR_BG 0x07
#define MOTOR_ELEVATOR_B1 0x08
#define MOTOR_ELEVATOR_B2 0x09

// 阶段判断
typedef enum
{
    PHASE_IDLE,          // 空闲状态
    PHASE_DEALING,       // 发牌阶段
    PHASE_JUMPING,       // 跳牌阶段
    PHASE_SINGLE_REFILL, // 单张摸牌并补牌阶段
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
    DEAL_TRAY_RESET_START,           // 托盘复位启动
    DEAL_TRAY_RESET_WAIT_COMPLETE,   // 等待托盘复位完成
    DEAL_PHASE_COMPLETE              // 发牌阶段完成
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
    JUMP_TRAY_RESET_START,           // 托盘复位启动
    JUMP_TRAY_RESET_WAIT_COMPLETE,   // 等待托盘复位完成
    JUMP_PHASE_COMPLETE              // 跳牌阶段完成
} JumpingSubState;

// 单张摸牌并补牌阶段
typedef enum
{
    REFILL_INIT,                     // 初始化
    REFILL_CONV_START,               // 第1层，传送带启动
    REFILL_CONV_WAIT_TILE,           // 第1层，等待传送带的牌
    REFILL_TRAY_UP_START,            // 托盘上升启动
    REFILL_TRAY_UP_WAIT_COMPLETE,    // 等待托盘上升完成
    REFILL_WAIT_TILE_CAUGHT,         // 等待牌被接走
    REFILL_TRAY_RESET_START,         // 托盘复位启动
    REFILL_TRAY_RESET_WAIT_COMPLETE, // 等待托盘复位完成
    REFILL_PHASE_COMPLETE            // 单张摸牌并补牌阶段完成
} SingleRefillSubState;

void mahjong_init();
void mahjong_task();

#endif // !MAHJONG_H