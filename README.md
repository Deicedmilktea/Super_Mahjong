# 🀄Super Mahjong

## 📖项目简介

Super Mahjong 是一个基于 STM32F407 微控制器的智能麻将机器人系统，结合了机械自动化、RFID识别、人工智能和实时操作系统等多项技术，能够自动完成麻将的发牌、摸牌、识别和游戏逻辑处理。

## 💻硬件平台

- ⚡**主控芯片**: STM32F407VET6 (ARM Cortex-M4, 168MHz)
- 🔄**实时操作系统**: FreeRTOS
- 🛠️**开发环境**: STM32CubeMX + VS Code + EIDE + HAL库
- 🔧**编译工具**: ARM GCC工具链

## 🏗️系统架构

```
Super_Mahjong/
├── Application/       # 应用层
│   ├── mahjong/         # 麻将游戏逻辑
│   ├── robot/           # 机器人主控制
│   └── led/             # LED指示系统
├── Module/            # 功能模块
│   ├── ai/              # AI决策模块
│   ├── rfid/            # RFID识别模块
│   ├── driver/          # 电机驱动模块
│   ├── ws2812/          # RGB LED驱动
│   ├── l298n/           # 直流电机驱动
│   ├── algorithm/       # 控制校验算法
│   └── daemon/          # 守护进程
├── Bsp/               # 板级支持包 
│   ├── usart/           # 串口通信
│   ├── gpio/            # GPIO控制
│   └── dwt/             # 高精度计时
└── Core/                # STM32 HAL库核心
```

## 🎯核心功能模块

### 1. 麻将游戏引擎 (`Application/mahjong/`)

实现完整的麻将游戏逻辑，包括：

- 🎮 **游戏状态管理**: 空闲、发牌、跳牌、摸牌补牌、结束等阶段
- 👥 **玩家管理**: 支持4名玩家，包含AI玩家
- 🎯 **牌局控制**: 自动化的发牌序列和游戏流程
- ⚙️ **状态机设计**: 精细的子状态控制，确保每个动作的准确执行

```c
typedef enum {
    PHASE_IDLE,          // 空闲状态
    PHASE_DEALING,       // 发牌阶段
    PHASE_JUMPING,       // 跳牌阶段
    PHASE_SINGLE_REFILL, // 单张摸牌并补牌阶段
    PHASE_OVER,          // 阶段结束
    PHASE_ERROR          // 错误状态
} GlobalPhase;
```

### 2. RFID识别系统 (`Module/rfid/`)

- 🔍 **双通道识别**: 支持两路并行的牌面识别
- 🔄 **队列缓存**: 实现RFID数据的环形队列管理
- ⚡ **实时通信**: 基于UART的高速数据传输
- ✅ **校验机制**: CRC16校验确保数据完整性

```c
uint8_t RFIDEnqueue(RFIDQueue *queue, Tile tile)
{
    if (!queue || RFIDQueueIsFull(queue))
    {
        return 0; // Queue is full or invalid
    }
    queue->tiles[queue->tail] = tile;
    queue->tail = (queue->tail + 1) % RFID_BUFFER_SIZE;
    queue->count++;
    return 1;
}

uint8_t RFIDDequeue(RFIDQueue *queue, Tile *tile)
{
    if (!queue || RFIDQueueIsEmpty(queue) || !tile)
    {
        return 0; // Queue is empty or invalid parameters
    }
    *tile = queue->tiles[queue->head];
    queue->head = (queue->head + 1) % RFID_BUFFER_SIZE;
    queue->count--;
    return 1;
}
```

### 3. AI决策引擎 (`Module/ai/`)

- 📶 **串口通信**: 与外部AI算法模块通信
- 📦 **数据协议**: 结构化的数据包格式
- 🚀 **实时反馈**: 快速的AI决策响应

```c
typedef struct {
    uint8_t head;          // 数据头标识符
    uint16_t index;        // 操作数量索引
    uint8_t current_phase; // 牌局进行阶段
    uint8_t player;        // 玩家编号
    uint8_t action;        // 玩家操作类型
    uint8_t tile_type;     // 牌类型 (万/条/筒/字)
    uint8_t tile_value;    // 牌面值
    uint16_t check_sum;    // 校验和 crc16_ccitt
    uint8_t tail;          // 数据尾标识符
} AI_Send_s;

typedef struct
{
    uint8_t head;       // 数据头标识符
    uint8_t index;      // 操作数量索引
    uint8_t tile_type;  // 牌类型 (万/条/筒/字)
    uint8_t tile_value; // 牌面值
    uint16_t check_sum; // 校验和 crc16_ccitt
} AI_Receive_s;
```

### 4. 电机控制系统 (`Module/driver/`)

- 🔄 **多轴协调**: 同时控制推牌、升降、转盘等多个电机
- 🎯 **PID控制**: 精确的位置和速度控制
- 🛡️ **安全保护**: 电机状态监控和异常处理
- 📍 **位置反馈**: 编码器反馈的精确定位

```c
typedef struct {
    PID_Instance current_PID;  // 电流环PID
    PID_Instance speed_PID;    // 速度环PID  
    PID_Instance angle_PID;    // 位置环PID
    float pid_ref;             // 目标值
} Motor_Controller_s;
```

### 5. 视觉效果系统 (`Module/ws2812/`)

- 💡 **RGB灯带控制**: WS2812智能LED驱动
- ✨ **动态效果**: 流水灯、呼吸灯等多种视觉效果
- ⚡ **DMA传输**: 高效的PWM+DMA控制方案
- 🔆 **亮度调节**: 可调节的全局亮度控制

```c
typedef struct
{
    TIM_HandleTypeDef *htim;
    uint32_t tim_channel;
    DMA_HandleTypeDef *hdma_tim_up;
    uint16_t num_leds;
    uint8_t brightness;

    uint8_t *led_data_buffer;
    uint16_t *pwm_data_buffer;
    uint32_t pwm_buffer_size;

    volatile uint8_t transfer_complete;
} WS2812_Instance;
```

## ⭐技术亮点 

### 🚀 **精密的状态机设计**

采用分层状态机架构，将复杂的麻将游戏流程分解为清晰的状态转换：

- 🎮 **全局状态**: 控制整体游戏阶段
- ⚙️ **子状态**: 精确控制每个阶段的具体动作
- 💎 **原子操作**: 确保每个机械动作的完整性

### 🎯 **高精度电机控制**

- 🔄 **三环PID控制**: 电流环+速度环+位置环的串级控制
- 🤝 **多轴同步**: 实现推牌机构、升降平台、转盘的精确协调
- 📍 **反馈校正**: 基于编码器的实时位置反馈

### 🧠 **智能AI集成**

- 🔌 **模块化AI**: 支持外部AI算法模块的热插拔
- ⚡ **实时通信**: 低延迟的串口通信协议
- 🚀 **决策反馈**: AI决策结果的实时处理和执行

### 📡 **双通道RFID识别**

- 🔀 **并行处理**: 双路RFID同时工作，提高识别效率
- 🔄 **队列管理**: 环形队列确保数据不丢失
- ✅ **容错设计**: 多重校验机制保证识别准确性

### ⚡ **FreeRTOS实时系统**

- 🏃‍♂️ **任务调度**: 多任务并行处理，响应迅速
- 🔒 **资源管理**: 信号量、互斥锁等同步机制
- ⚡ **中断处理**: 高效的中断响应和处理

### 🎨 **模块化软件架构**

- 🏗️ **分层设计**: Application -> Module -> BSP -> HAL的清晰分层
- 📐 **接口统一**: 标准化的模块接口设计
- 🔧 **易于扩展**: 支持新功能模块的快速集成

### 🔧 **工程化开发**

- 🏭 **Makefile构建**: 支持命令行编译和持续集成
- 📝 **代码规范**: 统一的命名规范和代码风格
- 📚 **文档完善**: 详细的接口文档和使用说明

## ✨ 系统特色

1. 🔗 **高度集成**: 集成了机械控制、视觉识别、人工智能等多项技术
2. ⚡ **实时响应**: 基于FreeRTOS的实时系统，保证系统响应速度
3. 🎯 **精确控制**: 毫米级的机械精度控制
4. 🧠 **智能决策**: AI算法驱动的游戏策略
5. 👤 **用户友好**: 直观的LED指示和状态反馈
6. 🔧 **可扩展性**: 模块化设计支持功能扩展

---

*Super Mahjong - 传统文化与现代科技的完美结合*
