#include "mahjong.h"
#include "driver.h"
#include "string.h"
#include "tim.h"
#include "rfid.h"
#include "ai.h"
// #include "l298n.h"
#include "crc.h"
#include "at8236.h"

static Driver_Instance *driver;                                                        // 驱动板实例
static Motor_Instance *motor_push_1, *motor_push_2, *motor_elevator, *motor_turntable; // 电机实例
static GPIO_Instance *gpio_key1, *gpio_key2, *gpio_red_1, *gpio_red_2;                 // GPIO实例
static RFID_Instance *rfid;                                                            // RFID实例
static AI_Instance *ai;                                                                // AI实例
static AT8236_Instance *at8236;                                                        // AT8236电机驱动板实例

static int16_t key1_count, last_key1_count, key2_count, last_key2_count = 0;               // 按键次数
static int16_t ir_left_count, last_ir_left_count, ir_right_count, last_ir_right_count = 0; // 按键次数
static uint8_t conv1_completed = 0, conv2_completed = 0;                                   // 传送带完成标志
static uint8_t phase = 1;                                                                  // 轮数
static uint8_t dealStep, jumpStep, refillStep = 0;                                         // 发牌和跳步

static GlobalGameState global_game;

static void Key1Callback(GPIO_Instance *gpio);
static void Key2Callback(GPIO_Instance *gpio);
static void Red1Callback(GPIO_Instance *gpio);
static void Red2Callback(GPIO_Instance *gpio);

static void phase_idle_task();
static void phase_dealing_task();
static void phase_jumping_task();
static void phase_single_refill_task();
static void phase_over_task();
static void phase_error_task();

static uint8_t set_send_ai_draw_data(RFIDQueue *queue);
static uint8_t set_send_ai_discard_data(RFIDQueue *queue);
static void switch_to_next_player();

// 电机控制函数声明
static void motor_conveyor_1_start();
static void motor_conveyor_1_stop();
static void motor_conveyor_2_start();
static void motor_conveyor_2_stop();
static void motor_turntable_start();
static void motor_turntable_stop();
static void motor_push_1_out();
static void motor_push_1_back();
static void motor_push_2_out();
static void motor_push_2_back();
static void motor_elevator_bg();
static void motor_elevator_b1();
static void motor_elevator_b2();
static void motor_lid_open();
static void motor_lid_close();

/***
 * @brief initialize mahjong_task
 */
void mahjong_init()
{
    Motor_Init_Config_s motor_config = {
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 5,    // 0.2 0.3 1
                .Ki = 0.05, // 0
                .Kd = 0.15, // 0.015
                .Improve = PID_Integral_Limit | PID_Derivative_On_Measurement,
                .MaxOut = 2000,
                .DeadBand = 10,
                .IntegralLimit = 500,
            },
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED,
            .outer_loop_type = ANGLE_LOOP,
            .close_loop_type = ANGLE_LOOP,
        },
    };

    motor_push_1 = MotorRegister(&motor_config);
    motor_push_2 = MotorRegister(&motor_config);
    motor_elevator = MotorRegister(&motor_config);
    motor_config.controller_setting_init_config.close_loop_type = SPEED_LOOP;
    motor_config.controller_setting_init_config.outer_loop_type = SPEED_LOOP;
    motor_turntable = MotorRegister(&motor_config); // 转盘电机直接采用pwm控制

    Driver_Init_Config_s init_config = {
        .motor = {
            motor_push_1,
            motor_push_2,
            motor_elevator,
            motor_turntable,
        },
        .usart_config = {
            .recv_buff_size = USART_RXBUFF_LIMIT,
            .usart_handle = &huart3,                 // 使用USART2
            .id = driver,                            // 传入驱动板实例
            .usart_module_callback = DriverCallback, // 串口回调函数
        },
    };
    driver = DriverInit(&init_config);

    // RFID_Init_Config_s rfid_init_config = {
    //     .usart_config = {
    //         .recv_buff_size = USART_RXBUFF_LIMIT,
    //         .usart_handle = &huart3,               // 使用USART3
    //         .id = rfid,                            // 传入RFID实例
    //         .usart_module_callback = RFIDCallback, // 串口回调函数
    //     },
    // };
    // rfid = RFIDInit(&rfid_init_config);

    AI_Init_Config_s ai_init_config = {
        .usart_config = {
            .recv_buff_size = AI_RECV_SIZE,
            .usart_handle = &huart1,             // 使用USART1
            .id = ai,                            // 传入AI实例
            .usart_module_callback = AICallback, // 串口回调函数
        },
    };
    ai = AIInit(&ai_init_config);

    AT8236_Init_Config_s at8236_init_config = {
        .gpio_a1_config = {
            .GPIOx = GPIOD,
            .GPIO_Pin = GPIO_PIN_12,
            .timer_handle = &htim4,
            .timer_channel = TIM_CHANNEL_1,
        },
        .gpio_a2_config = {
            .GPIOx = GPIOD,
            .GPIO_Pin = GPIO_PIN_13,
            .timer_handle = &htim4,
            .timer_channel = TIM_CHANNEL_2,
        },
        .pwm_ena = AT8236_MOTOR_PWM,
        .mode_a = AT8236_STOP,

        .gpio_b1_config = {
            .GPIOx = GPIOC,
            .GPIO_Pin = GPIO_PIN_6,
            .timer_handle = &htim8,
            .timer_channel = TIM_CHANNEL_1,
        },
        .gpio_b2_config = {
            .GPIOx = GPIOC,
            .GPIO_Pin = GPIO_PIN_8,
            .timer_handle = &htim8,
            .timer_channel = TIM_CHANNEL_3,
        },
        .pwm_enb = AT8236_MOTOR_PWM,
        .mode_b = AT8236_STOP,
    };
    at8236 = AT8236Init(&at8236_init_config);

    // 按键初始化
    GPIO_Init_Config_s gpio_init = {
        .exti_mode = GPIO_EXTI_MODE_FALLING, // 注意和CUBEMX的配置一致
        .GPIO_Pin = GPIO_PIN_2,              // GPIO引脚
        .GPIOx = GPIOE,                      // GPIO外设
        .gpio_model_callback = Key1Callback, // EXTI回调函数
    };
    gpio_key1 = GPIORegister(&gpio_init); // 注册GPIO实例

    gpio_init.GPIO_Pin = GPIO_PIN_3;              // GPIO引脚
    gpio_init.gpio_model_callback = Key2Callback; // EXTI回调函数
    gpio_key2 = GPIORegister(&gpio_init);         // 注册GPIO实例

    // 红外1 gpio初始化
    gpio_init.GPIO_Pin = GPIO_PIN_11;             // GPIO引脚
    gpio_init.GPIOx = GPIOF;                      // GPIO外设
    gpio_init.gpio_model_callback = Red1Callback; // EXTI回调函数
    gpio_red_1 = GPIORegister(&gpio_init);        // 注册红外1_GPIO实例

    // 红外2 gpio初始化
    gpio_init.GPIO_Pin = GPIO_PIN_12;             // GPIO引脚
    gpio_init.GPIOx = GPIOF;                      // GPIO外设
    gpio_init.gpio_model_callback = Red2Callback; // EXTI回调函数
    gpio_red_2 = GPIORegister(&gpio_init);        // 注册红外2_GPIO实例

    char *commands[] = {
        "$mtype:2#",
        "$mline:13#",
        "$mphase:34.014#",
        "$deadzone:1600#",
        "$MPID:0.8,0.06,0.5#",
        "$upload:1,0,0#"};

    for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        USARTSend(driver->usart, (uint8_t *)commands[i], strlen(commands[i]), USART_TRANSFER_BLOCKING);
    }

    // 牌局阶段init
    global_game.current_dealer = PLAYER_ID_EAST;             // 初始庄家为东
    global_game.ai = PLAYER_ID_NORTH;                        // AI玩家为南
    global_game.current_player = global_game.current_dealer; // 初始行动玩家为庄家
    global_game.index = 0;                                   // 初始操作数量索引为0
    global_game.global_phase = PHASE_IDLE;
    global_game.dealing_sub_state = DEAL_INIT;
    global_game.jumping_sub_state = JUMP_INIT;
    global_game.single_refill_sub_state = REFILL_INIT;
    global_game.over_sub_state = OVER_INIT;

    // AI发送数据初始化
    ai->ai_send.head = 0xAA;
    ai->ai_send.index = 0;                  // 操作数量索引
    ai->ai_send.current_phase = PHASE_IDLE; // 发牌阶段
    ai->ai_send.player = PLAYER_ID_EAST;    // 玩家编号 (东)
    ai->ai_send.action = ACTION_DRAW;       // 玩家操作类型 (杠)
    ai->ai_send.tile_type = TILE_TYPE_WAN;  // 牌类型 (万/条/筒/字)
    ai->ai_send.tile_value = 0;             // 牌面值
    ai->ai_send.check_sum = 0;              // 校验和
    ai->ai_send.tail = 0x55;                // 数据尾标识符
}

void mahjong_task()
{
    // 确定所属阶段
    switch (global_game.global_phase)
    {
    case PHASE_IDLE:
        phase_idle_task();
        break;

    case PHASE_DEALING:
        phase_dealing_task();
        break;

    case PHASE_JUMPING:
        phase_jumping_task();
        break;

    case PHASE_SINGLE_REFILL:
        phase_single_refill_task();
        break;

    case PHASE_OVER:
        phase_over_task();
        break;

    case PHASE_ERROR:
        phase_error_task();
        break;

    default:
        break;
    }

    if (phase)
    {
        driver->mahjong_phase = 1;
        phase = 0;
    }

    // if (key2_count % 2 == 1 || driver->callback_flag == MOTOR_CALLBACK_NONE) // 回调异常断电
    if (driver->callback_flag == MOTOR_CALLBACK_NONE) // 回调异常断电
        driver->stop_flag = MOTOR_FLAG_STOP;
    else
        driver->stop_flag = MOTOR_FLAG_ENABLED;

    // MotorControl(driver);
    // AT8236Control(at8236); // 控制AT8236电机驱动板

    char *pwm_cmd = "$pwm:-2000,0,0,0#";
    USARTSend(driver->usart, (uint8_t *)pwm_cmd, strlen(pwm_cmd), USART_TRANSFER_BLOCKING);
    // HAL_GPIO_WritePin(GPIOG, GPIO_PIN_2, GPIO_PIN_SET);
    // HAL_GPIO_WritePin(GPIOG, GPIO_PIN_4, GPIO_PIN_RESET);
    // phase = HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_2);
    // phase1 = HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_4);
    // L298NControl(l298n, MOTOR_FORWARD, MOTOR_FORWARD); // 启动L298N电机驱动板A通道
    // HAL_UART_Transmit(&huart1, (uint8_t *)"hello\r\n", 7, 100); // 测试串口通信

    // ai->ai_send.current_phase = PHASE_SINGLE_REFILL;                                      // 发牌阶段
    // ai->ai_send.player = PLAYER_ID_SOUTH;                                                 // 玩家编号 (东)
    // ai->ai_send.action = ACTION_DISCARD;                                                  // 玩家操作类型 (杠)
    // ai->ai_send.tile_type = TILE_TYPE_TIAO;                                               // 牌类型 (万/条/筒/字)
    // ai->ai_send.tile_value = 3;                                                           // 牌面值
    // ai->ai_send.check_sum = CRC16_CCITT((uint8_t *)&ai->ai_send, sizeof(AI_Send_s) - 3); // 计算校验和
    // AISendData(ai, &ai->ai_send);                                                         // 发送AI数据

    // AT8236Control(at8236, AT8236_FORWARD, AT8236_FORWARD); // 启动AT8236电机驱动板A通道
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET);

    // __HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_1, 6000);
    // __HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_2, 0);
}

/**
 * @brief 空闲状态任务
 */
static void phase_idle_task()
{
    // 1. 检测到开始按键按下

    // 2. 确定庄家

    // 3. 进入发牌阶段
    global_game.global_phase = PHASE_DEALING;
}

/**
 * @brief 发牌阶段任务
 */
static void phase_dealing_task()
{
    switch (global_game.dealing_sub_state)
    {
    // 初始化发牌阶段
    case DEAL_INIT:
        dealStep++; // 发牌轮数增加
        global_game.dealing_sub_state = DEAL_TRAY_DOWN_B1_START;
        break;

    // 托盘下降启动
    case DEAL_TRAY_DOWN_B1_START:
        motor_elevator_b1();
        global_game.dealing_sub_state = DEAL_TRAY_DOWN_B1_WAIT_COMPLETE;
        break;

    // 等待托盘下降完成
    case DEAL_TRAY_DOWN_B1_WAIT_COMPLETE:
        if (MotorIsAtPosition(driver->motor[MOTOR_ELEVATOR], 50))
            global_game.dealing_sub_state = DEAL_LAYER1_CONV_START;
        break;

    // 启动传送带上第一层麻将
    case DEAL_LAYER1_CONV_START:
        motor_conveyor_1_start();
        motor_conveyor_2_start();
        motor_turntable_start();
        conv1_completed = 0; // 重置传送带1完成标志
        conv2_completed = 0; // 重置传送带2完成标志
        global_game.dealing_sub_state = DEAL_LAYER1_CONV_WAIT_TILE;
        break;

    // 等待传送带的牌到达
    case DEAL_LAYER1_CONV_WAIT_TILE:
        if ((ir_left_count > last_ir_left_count && !conv1_completed) || (last_key1_count != key1_count && !conv1_completed)) // 注入手动挡基因
        {
            if (set_send_ai_draw_data(&rfid->draw_tile_1))
            {
                motor_conveyor_1_stop();
                last_ir_left_count = ir_left_count; // 更新红外计数
                last_key1_count = key1_count;       // 更新按键计数
                conv1_completed = 1;                // 标记传送带1完成
            }
        }
        if ((ir_right_count > last_ir_right_count && !conv2_completed) || (last_key1_count != key1_count && !conv2_completed)) // 注入手动挡基因
        {
            if (set_send_ai_draw_data(&rfid->draw_tile_2))
            {
                motor_conveyor_2_stop();
                last_ir_right_count = ir_right_count; // 更新红外计数
                last_key2_count = key2_count;         // 更新按键计数
                conv2_completed = 1;                  // 标记传送带2完成
            }
        }

        // 只有当两个传送带都完成后才能进入下一个状态
        if (conv1_completed && conv2_completed)
        {
            if (global_game.current_player == global_game.ai) // 如果当前玩家是AI
            {
                // 如果当前玩家是AI，启动推牌电机
                motor_push_1_out();
                motor_push_2_out();
                global_game.dealing_sub_state = DEAL_PUSH_OUT_1_WAIT_COMPLETE;
            }
            else
            {
                // motor_turntable_stop();
                global_game.dealing_sub_state = DEAL_TRAY_DOWN_B2_START;
            }
        }
        break;

    // 推牌电机等待完成
    case DEAL_PUSH_OUT_1_WAIT_COMPLETE:
        // 等待推牌电机向外推完成
        if (MotorIsAtPosition(driver->motor[MOTOR_PUSH_1], 50) && MotorIsAtPosition(driver->motor[MOTOR_PUSH_2], 50))
        {
            // 推牌完成后，向后退
            motor_push_1_back();
            motor_push_2_back();
            global_game.dealing_sub_state = DEAL_LAYER2_CONV_START; // 进入下一个状态
        }
        break;

    // 托盘下降启动
    case DEAL_TRAY_DOWN_B2_START:
        motor_elevator_b2();
        global_game.dealing_sub_state = DEAL_TRAY_DOWN_B2_WAIT_COMPLETE;
        break;

    // 等待托盘下降完成
    case DEAL_TRAY_DOWN_B2_WAIT_COMPLETE:
        if (MotorIsAtPosition(driver->motor[MOTOR_ELEVATOR], 50))
            global_game.dealing_sub_state = DEAL_LAYER2_CONV_START;
        break;

    // 启动传送带上第二层麻将
    case DEAL_LAYER2_CONV_START:
        motor_conveyor_1_start();
        motor_conveyor_2_start();
        motor_turntable_start();
        conv1_completed = 0; // 重置传送带1完成标志
        conv2_completed = 0; // 重置传送带2完成标志
        global_game.dealing_sub_state = DEAL_LAYER2_CONV_WAIT_TILE;
        break;

    // 等待传送带的牌到达
    case DEAL_LAYER2_CONV_WAIT_TILE:
        if ((ir_left_count > last_ir_left_count && !conv1_completed) || (last_key1_count != key1_count && !conv1_completed)) // 注入手动挡基因
        {
            if (set_send_ai_draw_data(&rfid->draw_tile_1))
            {
                motor_conveyor_1_stop();
                last_ir_left_count = ir_left_count; // 更新红外计数
                last_key1_count = key1_count;       // 更新按键计数
                conv1_completed = 1;                // 标记传送带1完成
            }
        }
        if ((ir_right_count > last_ir_right_count && !conv2_completed) || (last_key1_count != key1_count && !conv2_completed)) // 注入手动挡基因
        {
            if (set_send_ai_draw_data(&rfid->draw_tile_2))
            {
                motor_conveyor_2_stop();
                last_ir_right_count = ir_right_count; // 更新红外计数
                last_key1_count = key1_count;         // 更新按键计数
                conv2_completed = 1;                  // 标记传送带2完成
            }
        }

        // 只有当两个传送带都完成后才能进入下一个状态
        if (conv1_completed && conv2_completed)
        {
            // motor_turntable_stop();
            global_game.dealing_sub_state = DEAL_TRAY_UP_START;
            if (global_game.current_player == global_game.ai) // 如果当前玩家是AI
            {
                // 如果当前玩家是AI，启动推牌电机
                motor_push_1_out();
                motor_push_2_out();
                global_game.dealing_sub_state = DEAL_PUSH_OUT_2_WAIT_COMPLETE;
            }
            else
            {
                // turntable_stop();
                global_game.dealing_sub_state = DEAL_TRAY_UP_START; // 进入托盘上升状态
            }
        }
        break;

    // 推牌电机等待完成
    case DEAL_PUSH_OUT_2_WAIT_COMPLETE:
        // 等待推牌电机向外推完成
        if (MotorIsAtPosition(driver->motor[MOTOR_PUSH_1], 50) && MotorIsAtPosition(driver->motor[MOTOR_PUSH_2], 50))
        {
            // 推牌完成后，向后退
            motor_push_1_back();
            motor_push_2_back();

            switch_to_next_player(); // 切换到下一个玩家
            if (dealStep >= 12)
            {
                global_game.global_phase = PHASE_JUMPING;
                dealStep = 0; // 重置发牌轮数
            }

            global_game.dealing_sub_state = DEAL_INIT; // 重置发牌状态机
            ir_left_count = 0;                         // 重置红外计数
            ir_right_count = 0;                        // 重置红外计数
            last_ir_left_count = 0;                    // 重置红外计数
            last_ir_right_count = 0;                   // 重置红外计数
        }
        break;

    // 托盘上升启动
    case DEAL_TRAY_UP_START:
        motor_elevator_bg();
        global_game.dealing_sub_state = DEAL_TRAY_UP_WAIT_COMPLETE;
        break;

    // 等待托盘上升完成
    case DEAL_TRAY_UP_WAIT_COMPLETE:
        if (MotorIsAtPosition(driver->motor[MOTOR_ELEVATOR], 50))
            global_game.dealing_sub_state = DEAL_WAIT_TILE_CAUGHT;
        break;

    // 等待牌被接走
    case DEAL_WAIT_TILE_CAUGHT:
        if ((ir_left_count > last_ir_left_count && ir_right_count > last_ir_right_count) || (last_key1_count != key1_count))
        {
            switch_to_next_player(); // 切换到下一个玩家
            if (dealStep >= 12)
            {
                global_game.global_phase = PHASE_JUMPING;
                dealStep = 0; // 重置发牌轮数
            }

            global_game.dealing_sub_state = DEAL_INIT; // 重置发牌状态机
            ir_left_count = 0;                         // 重置红外计数
            ir_right_count = 0;                        // 重置红外计数
            last_ir_left_count = 0;                    // 重置红外计数
            last_ir_right_count = 0;                   // 重置红外计数
            last_key1_count = key1_count;              // 重置按键计数
        }
        break;

    default:
        break;
    }

    driver->stop_flag = MOTOR_FLAG_ENABLED;
}

/**
 * @brief 跳牌阶段任务
 */
static void phase_jumping_task()
{
    switch (global_game.jumping_sub_state)
    {
    // 初始化跳牌阶段
    case JUMP_INIT:
        jumpStep++;
        global_game.jumping_sub_state = JUMP_TRAY_DOWN_B1_START;
        break;

    // 托盘下降启动
    case JUMP_TRAY_DOWN_B1_START:
        motor_elevator_b1();
        global_game.jumping_sub_state = JUMP_TRAY_DOWN_B1_WAIT_COMPLETE;
        break;

    // 等待托盘下降完成
    case JUMP_TRAY_DOWN_B1_WAIT_COMPLETE:
        if (MotorIsAtPosition(driver->motor[MOTOR_ELEVATOR], 50))
            global_game.jumping_sub_state = JUMP_CONV_START;
        break;

    // 启动传送带上第一层麻将
    case JUMP_CONV_START:
        if (jumpStep == 1) // 庄家跳牌
        {
            motor_conveyor_1_start();
            motor_conveyor_2_start();
            conv1_completed = 0; // 重置传送带1完成标志
            conv2_completed = 0; // 重置传送带2完成标志
        }
        else
        {
            motor_conveyor_1_start();
            motor_conveyor_2_stop();
            conv1_completed = 0; // 重置传送带1完成标志
            conv2_completed = 1; // 传送带2不工作，直接标记为完成
        }
        motor_turntable_start();
        global_game.jumping_sub_state = JUMP_CONV_WAIT_TILE;
        break;

    // 等待传送带的牌到达
    case JUMP_CONV_WAIT_TILE:
        if (jumpStep == 1) // 庄家跳牌
        {
            if ((ir_left_count > last_ir_left_count && !conv1_completed) || (last_key1_count != key1_count && !conv1_completed)) // 注入手动挡基因
            {
                if (set_send_ai_draw_data(&rfid->draw_tile_1))
                {
                    motor_conveyor_1_stop();
                    last_ir_left_count = ir_left_count; // 更新红外计数
                    last_key1_count = key1_count;       // 更新按键计数
                    conv1_completed = 1;                // 标记传送带1完成
                }
            }
            if ((ir_right_count > last_ir_right_count && !conv2_completed) || (last_key1_count != key1_count && !conv2_completed))
            {
                if (set_send_ai_draw_data(&rfid->draw_tile_2))
                {
                    motor_conveyor_2_stop();
                    last_ir_right_count = ir_right_count; // 更新红外计数
                    last_key1_count = key1_count;         // 更新按键计数
                    conv2_completed = 1;                  // 标记传送带2完成
                }
            }

            // 只有当两个传送带都完成后才能进入下一个状态
            if (conv1_completed && conv2_completed)
            {
                if (global_game.current_player == global_game.ai) // 如果当前玩家是AI
                {
                    // 如果当前玩家是AI，启动推牌电机
                    motor_push_1_out();
                    motor_push_2_out();
                    global_game.jumping_sub_state = JUMP_PUSH_OUT_WAIT_COMPLETE;
                }
                else
                {
                    // motor_turntable_stop();
                    global_game.jumping_sub_state = JUMP_TRAY_UP_START; // 进入托盘上升状态
                }
            }
        }
        else // 非庄家跳牌
        {
            if ((ir_left_count > last_ir_left_count && !conv1_completed) || (last_key1_count != key1_count && !conv1_completed))
            {
                if (set_send_ai_draw_data(&rfid->draw_tile_1))
                {
                    motor_conveyor_1_stop();
                    last_ir_left_count = ir_left_count; // 更新红外计数
                    last_key1_count = key1_count;       // 更新按键计数
                    conv1_completed = 1;                // 标记传送带1完成
                }
            }

            // 只有当传送带1完成后才能进入下一个状态（因为传送带2已在启动时标记为完成）
            if (conv1_completed && conv2_completed)
            {
                if (global_game.current_player == global_game.ai) // 如果当前玩家是AI
                {
                    // 如果当前玩家是AI，启动推牌电机
                    motor_push_1_out();
                    global_game.jumping_sub_state = JUMP_PUSH_OUT_WAIT_COMPLETE;
                }
                else
                {
                    // motor_turntable_stop();
                    global_game.jumping_sub_state = JUMP_TRAY_UP_START; // 进入托盘上升状态
                }
            }
        }
        break;

    // 推牌电机等待完成
    case JUMP_PUSH_OUT_WAIT_COMPLETE:
        // 等待推牌电机向外推完成
        if (MotorIsAtPosition(driver->motor[MOTOR_PUSH_1], 50) && MotorIsAtPosition(driver->motor[MOTOR_PUSH_2], 50))
        {
            // 推牌完成后，向后退
            motor_push_1_back();
            motor_push_2_back();

            switch_to_next_player(); // 切换到下一个玩家
            if (jumpStep >= 4)
            {
                global_game.global_phase = PHASE_SINGLE_REFILL;
                jumpStep = 0; // 重置跳牌步数
            }

            global_game.jumping_sub_state = JUMP_INIT; // 重置跳牌状态机
            ir_left_count = 0;                         // 重置红外计数
            ir_right_count = 0;                        // 重置红外计数
            last_ir_left_count = 0;                    // 重置红外计数
            last_ir_right_count = 0;                   // 重置红外计数
        }
        break;

    // 托盘上升启动
    case JUMP_TRAY_UP_START:
        motor_elevator_bg();
        global_game.jumping_sub_state = JUMP_TRAY_UP_WAIT_COMPLETE;
        break;

    // 等待托盘上升完成
    case JUMP_TRAY_UP_WAIT_COMPLETE:
        if (MotorIsAtPosition(driver->motor[MOTOR_ELEVATOR], 50))
            global_game.jumping_sub_state = JUMP_WAIT_TILE_CAUGHT;
        break;

    // 等待牌被接走
    case JUMP_WAIT_TILE_CAUGHT:
        if ((ir_left_count > last_ir_left_count && ir_right_count > last_ir_right_count) || (last_key1_count != key1_count))
        {
            switch_to_next_player(); // 切换到下一个玩家
            if (jumpStep >= 4)
            {
                global_game.global_phase = PHASE_SINGLE_REFILL;
                jumpStep = 0; // 重置跳牌步数
            }

            global_game.jumping_sub_state = JUMP_INIT; // 重置跳牌状态机
            ir_left_count = 0;                         // 重置红外计数
            ir_right_count = 0;                        // 重置红外计数
            last_ir_left_count = 0;                    // 重置红外计数
            last_ir_right_count = 0;                   // 重置红外计数
            last_key1_count = key1_count;              // 重置按键计数
        }
        break;

    default:
        break;
    }

    driver->stop_flag = MOTOR_FLAG_ENABLED;
}

/**
 * @brief 单张摸牌并补牌阶段任务
 */
static void phase_single_refill_task()
{
    switch (global_game.single_refill_sub_state)
    {
    // 初始化单张摸牌并补牌阶段
    case REFILL_INIT:
        refillStep++; // 单张摸牌步数增加
        global_game.single_refill_sub_state = REFILL_TRAY_DOWN_B1_START;
        break;

    // 托盘下降启动
    case REFILL_TRAY_DOWN_B1_START:
        motor_elevator_b1();
        global_game.single_refill_sub_state = REFILL_TRAY_DOWN_B1_WAIT_COMPLETE;
        break;

    // 等待托盘下降完成
    case REFILL_TRAY_DOWN_B1_WAIT_COMPLETE:
        if (MotorIsAtPosition(driver->motor[MOTOR_ELEVATOR], 50))
            global_game.single_refill_sub_state = REFILL_CONV_START;
        break;

    // 启动传送带上第一层麻将
    case REFILL_CONV_START:
        motor_conveyor_1_start();
        motor_conveyor_2_stop();
        motor_turntable_start();
        global_game.single_refill_sub_state = REFILL_CONV_WAIT_TILE;
        break;

    // 等待传送带的牌到达
    case REFILL_CONV_WAIT_TILE:
        if ((ir_left_count > last_ir_left_count) || (last_key1_count != key1_count)) // 注入手动挡基因
        {
            if (set_send_ai_draw_data(&rfid->draw_tile_1))
            {
                motor_conveyor_1_stop();
                last_ir_left_count = ir_left_count; // 更新红外计数
                last_key1_count = key1_count;       // 更新按键计数

                if (global_game.current_player == global_game.ai) // 如果当前玩家是AI
                {
                    // 如果当前玩家是AI，启动推牌电机
                    motor_push_1_out();
                    global_game.single_refill_sub_state = REFILL_PUSH_OUT_WAIT_COMPLETE;
                }
                else
                {
                    // motor_turntable_stop();
                    global_game.single_refill_sub_state = REFILL_TRAY_UP_START; // 进入托盘上升状态
                }
            }
        }
        break;

    // 推牌电机等待完成
    case REFILL_PUSH_OUT_WAIT_COMPLETE:
        // 等待推牌电机向外推完成
        if (MotorIsAtPosition(driver->motor[MOTOR_PUSH_1], 50) && MotorIsAtPosition(driver->motor[MOTOR_PUSH_2], 50))
        {
            // 推牌完成后，向后退
            motor_push_1_back();
            motor_push_2_back();

            if (ai->is_recv) // 如果机器人打出牌
            {
                ai->is_recv = 0;         // 重置接收标志
                switch_to_next_player(); // 切换到下一个玩家
                if (refillStep >= 55)    // 如果单张摸牌步数超过55，则进入结束阶段
                {
                    global_game.global_phase = PHASE_OVER;
                    refillStep = 0; // 重置补牌步数
                }

                global_game.single_refill_sub_state = REFILL_INIT; // 重置跳牌状态机
                ir_left_count = 0;                                 // 重置红外计数
                ir_right_count = 0;                                // 重置红外计数
                last_ir_left_count = 0;                            // 重置红外计数
                last_ir_right_count = 0;                           // 重置红外计数
            }
        }
        break;

    // 托盘上升启动
    case REFILL_TRAY_UP_START:
        motor_elevator_bg();
        global_game.single_refill_sub_state = REFILL_TRAY_UP_WAIT_COMPLETE;
        break;

    // 等待托盘上升完成
    case REFILL_TRAY_UP_WAIT_COMPLETE:
        if (MotorIsAtPosition(driver->motor[MOTOR_ELEVATOR], 50))
            global_game.single_refill_sub_state = REFILL_WAIT_TILE_CAUGHT;
        break;

    // 等待牌被接走
    case REFILL_WAIT_TILE_CAUGHT:
        if ((ir_left_count > last_ir_left_count && ir_right_count > last_ir_right_count) || (last_key1_count != key1_count)) // 注入手动挡基因
        {
            if (set_send_ai_discard_data(&rfid->discard_tile)) // 如果有人打出牌
            {
                switch_to_next_player(); // 切换到下一个玩家
                if (refillStep >= 55)    // 所有牌都被接完引起的牌局自然结束，以108张牌局为例
                {
                    global_game.global_phase = PHASE_OVER;
                    refillStep = 0; // 重置补牌步数
                }

                global_game.single_refill_sub_state = REFILL_INIT; // 重置跳牌状态机
                ir_left_count = 0;                                 // 重置红外计数
                ir_right_count = 0;                                // 重置红外计数
                last_ir_left_count = 0;                            // 重置红外计数
                last_ir_right_count = 0;                           // 重置红外计数
                last_key1_count = key1_count;                      // 重置按键计数
            }
        }
        break;

    default:
        break;
    }

    driver->stop_flag = MOTOR_FLAG_ENABLED;
}

/**
 * @brief 阶段结束任务
 */
static void phase_over_task()
{
    switch (global_game.over_sub_state)
    {
    // 初始化阶段
    case OVER_INIT:
        // if 检测到按键按下
        global_game.over_sub_state = OVER_LID_OPEN_START;
        break;

    // 打开盖板开始
    case OVER_LID_OPEN_START:
        motor_lid_open();
        global_game.over_sub_state = OVER_LID_OPEN_WAIT_COMPLETE;
        break;

    // 等待盖板打开完成
    case OVER_LID_OPEN_WAIT_COMPLETE:
        // if ()到达位置
        global_game.over_sub_state = OVER_LID_CLOSE_START;
        break;

    // 关闭盖板开始
    case OVER_LID_CLOSE_START:
        motor_lid_close();
        global_game.over_sub_state = OVER_LID_CLOSE_WAIT_COMPLETE;
        break;

    // 等待盖板关闭完成
    case OVER_LID_CLOSE_WAIT_COMPLETE:
        // if到达位置
        if (abs(driver->motor[3]->motor_controller.pid_ref - driver->motor[3]->measure.total_ecd) < 50)
        {
            global_game.over_sub_state = OVER_INIT;
            global_game.global_phase = PHASE_IDLE;
        }
        break;

    default:
        break;
    }
}

/**
 * @brief 错误状态任务
 */
static void phase_error_task()
{
    driver->stop_flag = MOTOR_FLAG_STOP;
}
/**
 * @brief 获取全局游戏状态指针
 */
GlobalGameState *get_global_game_state()
{
    return &global_game;
}

/**
 * @brief 切换到下一个玩家
 * @note 从庄家依次往后，但是到最后一位下一位就是第一位
 *       玩家顺序：东(0) -> 南(1) -> 西(2) -> 北(3) -> 东(0) ...
 */
static void switch_to_next_player(void)
{
    // 将当前玩家ID递增，如果超过最大值则回到第一个玩家
    global_game.current_player = (global_game.current_player + 1) % NUM_PLAYERS;
}

/**
 * @brief 设置并发送给AI的摸牌数据
 */
static uint8_t set_send_ai_draw_data(RFIDQueue *queue)
{
    Tile tile;
    if (RFIDDequeue(queue, &tile)) // 从RFID队列中取出牌
    {
        ai->ai_send.current_phase = global_game.global_phase;                                // 设置AI发送的发牌阶段
        ai->ai_send.action = ACTION_DRAW;                                                    // 设置AI发送的操作类型为摸牌
        ai->ai_send.player = global_game.current_player;                                     // 设置AI发送的玩家编号
        ai->ai_send.index = global_game.index;                                               // 设置AI发送的操作数量索引
        ai->ai_send.tile_type = tile.type;                                                   // 设置AI发送的牌类型
        ai->ai_send.tile_value = tile.value;                                                 // 设置AI发送的牌面值
        ai->ai_send.check_sum = CRC16_CCITT((uint8_t *)&ai->ai_send, sizeof(AI_Send_s) - 3); // 计算校验和
        for (uint8_t i = 0; i < 10; i++)
        {
            AISendData(ai, &ai->ai_send); // 发送AI数据
        }
        global_game.index++; // 增加操作数量索引
        return 1;            // 成功发送数据
    }
    else
        return 0;
}

/**
 * @brief 设置并发送给AI的打出牌数据
 */
static uint8_t set_send_ai_discard_data(RFIDQueue *queue)
{
    Tile tile;
    if (RFIDDequeue(queue, &tile)) // 从RFID队列中取出牌
    {
        ai->ai_send.current_phase = global_game.global_phase;                                // 设置AI发送的发牌阶段
        ai->ai_send.action = ACTION_DISCARD;                                                 // 设置AI发送的操作类型为打出牌
        ai->ai_send.player = global_game.current_player;                                     // 设置AI发送的玩家编号
        ai->ai_send.index = global_game.index;                                               // 设置AI发送的操作数量索引
        ai->ai_send.tile_type = tile.type;                                                   // 设置AI发送的牌类型
        ai->ai_send.tile_value = tile.value;                                                 // 设置AI发送的牌面值
        ai->ai_send.check_sum = CRC16_CCITT((uint8_t *)&ai->ai_send, sizeof(AI_Send_s) - 3); // 计算校验和
        for (uint8_t i = 0; i < 10; i++)
        {
            AISendData(ai, &ai->ai_send); // 发送AI数据
        }
        global_game.index++; // 增加操作数量索引
        return 1;            // 成功发送数据
    }
    else
        return 0;
}

// 电机控制函数实现
/**
 * @brief 启动传送带1
 */
void motor_conveyor_1_start()
{
    // 传送带1启动逻辑，目前为空实现
    at8236->mode_a = AT8236_FORWARD;
}

/**
 * @brief 停止传送带1
 */
void motor_conveyor_1_stop()
{
    // 传送带1停止逻辑，目前为空实现
    at8236->mode_a = AT8236_STOP;
}

/**
 * @brief 启动传送带2
 */
void motor_conveyor_2_start()
{
    // 传送带2启动逻辑，目前为空实现
    at8236->mode_b = AT8236_FORWARD;
}

/**
 * @brief 停止传送带2
 */
void motor_conveyor_2_stop()
{
    // 传送带2停止逻辑，目前为空实现
    at8236->mode_b = AT8236_STOP;
}

/**
 * @brief 启动转盘电机
 */
void motor_turntable_start()
{
    MotorSetRef(driver->motor[MOTOR_TURNTABLE], TURNTABLE_START_PWM);
}

/**
 * @brief 停止转盘电机
 */
void motor_turntable_stop()
{
    MotorSetRef(driver->motor[MOTOR_TURNTABLE], TURNTABLE_STOP_PWM);
}

/**
 * @brief 推牌电机1向外推
 */
void motor_push_1_out()
{
    MotorSetRef(driver->motor[MOTOR_PUSH_1], PUSH_1_OUT_ENCODER);
}

/**
 * @brief 推牌电机1向后退
 */
void motor_push_1_back()
{
    MotorSetRef(driver->motor[MOTOR_PUSH_1], PUSH_1_BACK_ENCODER);
}

/**
 * @brief 推牌电机2向外推
 */
void motor_push_2_out()
{
    MotorSetRef(driver->motor[MOTOR_PUSH_2], PUSH_2_OUT_ENCODER);
}

/**
 * @brief 推牌电机2向后退
 */
void motor_push_2_back()
{
    MotorSetRef(driver->motor[MOTOR_PUSH_2], PUSH_2_BACK_ENCODER);
}

/**
 * @brief 升降台电机回到背景位置
 */
void motor_elevator_bg()
{
    MotorSetRef(driver->motor[MOTOR_ELEVATOR], ELEVATOR_BG_ENCODER);
}

/**
 * @brief 升降台电机到B1位置
 */
void motor_elevator_b1()
{
    MotorSetRef(driver->motor[MOTOR_ELEVATOR], ELEVATOR_B1_ENCODER);
}

/**
 * @brief 升降台电机到B2位置
 */
void motor_elevator_b2()
{
    MotorSetRef(driver->motor[MOTOR_ELEVATOR], ELEVATOR_B2_ENCODER);
}

/**
 * @brief 开启盖板
 */
void motor_lid_open()
{
    // 这里应该实现开启盖板的逻辑
    // 目前使用宏定义的值作为占位符
    // MotorSetRef(driver->motor[MOTOR_LID], MOTOR_LID_OPEN);
}

/**
 * @brief 关闭盖板
 */
void motor_lid_close()
{
    // 这里应该实现关闭盖板的逻辑
    // 目前使用宏定义的值作为占位符
    // MotorSetRef(driver->motor[MOTOR_LID], MOTOR_LID_CLOSE);
}

static void Key1Callback(GPIO_Instance *gpio)
{
    key1_count++;

    if (key1_count % 2 == 1)
    {
        MotorSetRef(motor_push_1, motor_push_1->measure.init_ecd + 1000); // 推牌参数1000 前进为正
        // MotorSetRef(motor_push_2, motor_push_2->measure.init_ecd - 1000); // 升牌参数1000 向上为负
        // MotorSetRef(motor_elevator, motor_elevator->measure.init_ecd + 1000);
        // MotorSetRef(motor_turntable, motor_turntable->measure.init_ecd + 1000);
    }

    else
    {
        MotorSetRef(motor_push_1, motor_push_1->measure.init_ecd);
        // MotorSetRef(motor_push_2, motor_push_2->measure.init_ecd);
        // MotorSetRef(motor_elevator, motor_elevator->measure.init_ecd);
        // MotorSetRef(motor_turntable, motor_turntable->measure.init_ecd);
    }
}

static void Key2Callback(GPIO_Instance *gpio)
{
    key2_count++;

    if (key2_count % 2 == 1)
    {
        MotorSetRef(motor_push_2, motor_push_2->measure.init_ecd - 1000); // 升牌参数1000 向上为负
    }

    else
    {
        MotorSetRef(motor_push_2, motor_push_2->measure.init_ecd);
    }
}

static void Red1Callback(GPIO_Instance *gpio)
{
    ir_left_count++;
}

static void Red2Callback(GPIO_Instance *gpio)
{
    ir_right_count++;
}