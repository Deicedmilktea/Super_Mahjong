// 枚举定义状态
typedef enum
{
    ST_INIT,        // 初始化阶段
    ST_DEAL_PHASE,  // 发牌阶段
    ST_JUMP_PHASE,  // 跳牌阶段
    ST_NORMAL_PHASE // 正常抓牌阶段
} SystemState;

typedef enum
{
    DEAL_WAIT_FIRST_TWO,  // 等待发第一组两张牌
    DEAL_TURN_TRAY_DOWN,  // 托盘下降
    DEAL_WAIT_SECOND_TWO, // 等待发第二组两张牌
    DEAL_TURN_TRAY_UP,    // 托盘上升
    DEAL_NEXT_PLAYER      // 下一个玩家（未使用）
} DealSubState;

typedef enum
{
    JUMP_START,            // 跳牌开始
    JUMP_WAIT_TAKE_TWO,    // 等待取走两张牌
    JUMP_NEXT_SINGLE,      // 下一张单牌
    JUMP_WAIT_TAKE_SINGLE, // 等待取走单张牌
    JUMP_DONE              // 跳牌完成（未使用）
} JumpSubState;

typedef enum
{
    NORMAL_WAIT_TAKE, // 等待玩家抓牌
    NORMAL_TRAY_DOWN, // 托盘下降
    NORMAL_SEND_ONE,  // 发送一张牌
    NORMAL_TRAY_UP    // 托盘上升
} NormalSubState;

// 主状态变量
SystemState systemState = ST_INIT; // 系统主状态

// 各阶段子状态变量
DealSubState dealState = DEAL_WAIT_FIRST_TWO;  // 发牌子状态
JumpSubState jumpState = JUMP_START;           // 跳牌子状态
NormalSubState normalState = NORMAL_WAIT_TAKE; // 正常抓牌子状态

// 红外传感器变量
int irCountLeft = 0;  // 左侧红外计数
int irCountRight = 0; // 右侧红外计数
int lastIrLeft = 0;   // 上一次左侧红外计数
int lastIrRight = 0;  // 上一次右侧红外计数

// 计数器
int dealRounds = 0;    // 每人发牌次数
int jumpStep = 0;      // 跳牌阶段计数
bool trayBusy = false; // 托盘是否忙碌

void loop()
{
    // 红外更新
    updateIRCounts(); // 更新 irCountLeft/right，供下面比较判断

    switch (systemState)
    {
    case ST_INIT:                    // 初始化阶段
        initSystem();                // 初始化系统
        systemState = ST_DEAL_PHASE; // 进入发牌阶段
        break;

    case ST_DEAL_PHASE: // 发牌阶段
        switch (dealState)
        {
        case DEAL_WAIT_FIRST_TWO: // 等待发第一组两张牌
            if (irCountLeft > lastIrLeft && irCountRight > lastIrRight)
            {
                stopConveyors();                 // 停止传送带
                lowerTray();                     // 托盘下降
                trayBusy = true;                 // 托盘忙碌
                dealState = DEAL_TURN_TRAY_DOWN; // 进入托盘下降状态
            }
            break;
        case DEAL_TURN_TRAY_DOWN: // 托盘下降
            if (trayFinishedLowering())
            {
                startConveyors();                 // 启动传送带
                dealState = DEAL_WAIT_SECOND_TWO; // 等待发第二组两张牌
            }
            break;
        case DEAL_WAIT_SECOND_TWO: // 等待发第二组两张牌
            if (irCountLeft > lastIrLeft + 1 && irCountRight > lastIrRight + 1)
            {
                stopConveyors();               // 停止传送带
                raiseTray();                   // 托盘上升
                trayBusy = true;               // 托盘忙碌
                dealState = DEAL_TURN_TRAY_UP; // 进入托盘上升状态
            }
            break;
        case DEAL_TURN_TRAY_UP: // 托盘上升
            if (trayFinishedRaising())
            {
                lastIrLeft = irCountLeft; // 更新红外计数
                lastIrRight = irCountRight;
                dealRounds++; // 发牌轮数+1
                if (dealRounds >= 3)
                {
                    systemState = ST_JUMP_PHASE; // 进入跳牌阶段
                }
                else
                {
                    dealState = DEAL_WAIT_FIRST_TWO; // 继续发牌
                }
            }
            break;
        }
        break;

    case ST_JUMP_PHASE: // 跳牌阶段
        switch (jumpState)
        {
        case JUMP_START:                    // 跳牌开始
            raiseTrayTwoTiles();            // 托盘升起两张牌
            jumpState = JUMP_WAIT_TAKE_TWO; // 等待取走两张牌
            break;
        case JUMP_WAIT_TAKE_TWO: // 等待取走两张牌
            if (irCountLeft + irCountRight >= lastIrLeft + lastIrRight + 2)
            {
                jumpStep = 1;                 // 跳牌步数初始化
                jumpState = JUMP_NEXT_SINGLE; // 进入下一张单牌
            }
            break;
        case JUMP_NEXT_SINGLE:                 // 下一张单牌
            raiseTrayOneTile();                // 托盘升起一张牌
            jumpState = JUMP_WAIT_TAKE_SINGLE; // 等待取走单张牌
            break;
        case JUMP_WAIT_TAKE_SINGLE: // 等待取走单张牌
            if (irCountLeft + irCountRight >= lastIrLeft + lastIrRight + jumpStep + 2)
            {
                jumpStep++; // 跳牌步数+1
                if (jumpStep >= 4)
                {
                    systemState = ST_NORMAL_PHASE; // 进入正常抓牌阶段
                    normalState = NORMAL_WAIT_TAKE;
                    lastIrLeft = irCountLeft; // 更新红外计数
                    lastIrRight = irCountRight;
                }
                else
                {
                    jumpState = JUMP_NEXT_SINGLE; // 继续下一张单牌
                }
            }
            break;
        }
        break;

    case ST_NORMAL_PHASE: // 正常抓牌阶段
        switch (normalState)
        {
        case NORMAL_WAIT_TAKE: // 等待玩家抓牌
            if (irCountLeft + irCountRight > lastIrLeft + lastIrRight)
            {
                lowerTray();                    // 托盘下降
                trayBusy = true;                // 托盘忙碌
                normalState = NORMAL_TRAY_DOWN; // 进入托盘下降状态
            }
            break;
        case NORMAL_TRAY_DOWN: // 托盘下降
            if (trayFinishedLowering())
            {
                startConveyorOne();            // 启动单个传送带
                normalState = NORMAL_SEND_ONE; // 发送一张牌
            }
            break;
        case NORMAL_SEND_ONE: // 发送一张牌
            if (irCountLeft + irCountRight > lastIrLeft + lastIrRight + 1)
            {
                stopConveyor();               // 停止传送带
                raiseTray();                  // 托盘上升
                trayBusy = true;              // 托盘忙碌
                normalState = NORMAL_TRAY_UP; // 进入托盘上升状态
            }
            break;
        case NORMAL_TRAY_UP: // 托盘上升
            if (trayFinishedRaising())
            {
                lastIrLeft = irCountLeft; // 更新红外计数
                lastIrRight = irCountRight;
                normalState = NORMAL_WAIT_TAKE; // 继续等待玩家抓牌
            }
            break;
        }
        break;
    }
}
