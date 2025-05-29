#include "mahjong.h"
#include "driver.h"
#include "string.h"
#include "bsp_gpio.h"
#include "ws2812.h"
#include "tim.h"

static Driver_Instance *driver;                                                        // 驱动板实例
static Motor_Instance *motor_push_1, *motor_push_2, *motor_elevator, *motor_turntable; // 电机实例
static Motor_Instance *motor_conveyor_1, *motor_conveyor_2;                            // 电机实例
static GPIOInstance *gpio_key1, *gpio_key2, *gpio_red_1, *gpio_red_2;                  // GPIO实例
static WS2812_Instance *ws2812;                                                        // WS2812实例

static int16_t key1_count, key2_count, ir_left_count, last_ir_left_count, ir_right_count, last_ir_right_count = 0; // 按键次数
static uint8_t phase = 1;                                                                                          // 轮数
static uint8_t dealStep, jumpStep, refillStep = 0;                                                                 // 发牌和跳步

static GlobalPhase global_phase = PHASE_IDLE;
static DealingSubState dealing_sub_state = DEAL_INIT;
static JumpingSubState jumping_sub_state = JUMP_INIT;
static SingleRefillSubState single_refill_sub_state = REFILL_INIT;
static OverSubState over_sub_state = OVER_INIT;

static void Key1Callback(GPIOInstance *gpio);
static void Key2Callback(GPIOInstance *gpio);
static void Red1Callback(GPIOInstance *gpio);
static void Red2Callback(GPIOInstance *gpio);

static void phase_idle_task();
static void phase_dealing_task();
static void phase_jumping_task();
static void phase_single_refill_task();
static void phase_over_task();
static void phase_error_task();

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
    motor_turntable = MotorRegister(&motor_config);

    Driver_Init_Config_s init_config = {
        .motor = {
            motor_push_1,
            motor_push_2,
            motor_elevator,
            motor_turntable,
        },
    };
    driver = DriverInit(&init_config);

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

    // WS2812初始化
    // ws2812 = WS2812_Init(&htim3, TIM_CHANNEL_1, &hdma_tim3_ch1_trig, WS2812_LED_NUM);
}

void mahjong_task()
{
    // // 确定所属阶段
    // switch (global_phase)
    // {
    // case PHASE_IDLE:
    //     phase_idle_task();
    //     break;

    // case PHASE_DEALING:
    //     phase_dealing_task();
    //     break;

    // case PHASE_JUMPING:
    //     phase_jumping_task();
    //     break;

    // case PHASE_SINGLE_REFILL:
    //     phase_single_refill_task();
    //     break;

    // case PHASE_OVER:
    //     phase_over_task();

    // case PHASE_ERROR:
    //     phase_error_task();
    //     break;

    // default:
    //     break;
    // }

    // if (phase)
    // {
    //     driver->mahjong_phase = 1;
    //     phase = 0;
    // }

    // if (key2_count % 2 == 1 || driver->callback_flag == MOTOR_CALLBACK_NONE) // 回调异常断电
    if (driver->callback_flag == MOTOR_CALLBACK_NONE) // 回调异常断电
        driver->stop_flag = MOTOR_STOP;
    else
        driver->stop_flag = MOTOR_ENABLED;

    // MotorControl(driver);

    char *pwm_cmd = "$pwm:0,0,1000,500#";
    USARTSend(driver->usart, (uint8_t *)pwm_cmd, strlen(pwm_cmd), USART_TRANSFER_BLOCKING);
}

static void Key1Callback(GPIOInstance *gpio)
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

static void Key2Callback(GPIOInstance *gpio)
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

static void Red1Callback(GPIOInstance *gpio)
{
    ir_left_count++;
}

static void Red2Callback(GPIOInstance *gpio)
{
    ir_right_count++;
}

/**
 * @brief 空闲状态任务
 */
static void phase_idle_task()
{
    // 1. 检测到开始按键按下

    // 2. 进入发牌阶段
    global_phase = PHASE_DEALING;
}

/**
 * @brief 发牌阶段任务
 */
static void phase_dealing_task()
{
    switch (dealing_sub_state)
    {
    case DEAL_INIT: // 初始化发牌阶段
        dealing_sub_state = DEAL_TRAY_DOWN_B1_START;
        break;

    case DEAL_TRAY_DOWN_B1_START: // 托盘下降启动
        MOTOR_ELEVATOR_B1;
        dealing_sub_state = DEAL_TRAY_DOWN_B1_WAIT_COMPLETE;
        break;

    case DEAL_TRAY_DOWN_B1_WAIT_COMPLETE: // 等待托盘下降完成
        if (abs(driver->motor[1]->motor_controller.pid_ref - driver->motor[1]->measure.total_ecd) < 50)
            dealing_sub_state = DEAL_LAYER1_CONV_START;
        break;

    case DEAL_LAYER1_CONV_START: // 启动传送带上第一层麻将
        MOTOR_CONVEYOR_1_START;
        MOTOR_CONVEYOR_2_START;
        MOTOR_TURNTABLE_START;
        dealing_sub_state = DEAL_LAYER1_CONV_WAIT_TILE;
        break;

    case DEAL_LAYER1_CONV_WAIT_TILE: // 等待传送带的牌到达
        if (ir_left_count > last_ir_left_count)
            MOTOR_CONVEYOR_1_STOP;
        if (ir_right_count > last_ir_right_count)
            MOTOR_CONVEYOR_2_STOP;
        if (ir_left_count > last_ir_left_count && ir_right_count > last_ir_right_count)
        {
            // MOTOR_TURNTABLE_STOP;
            last_ir_left_count = ir_left_count;   // 更新红外计数
            last_ir_right_count = ir_right_count; // 更新红外计数
            dealing_sub_state = DEAL_TRAY_DOWN_B2_START;
        }
        break;

    case DEAL_TRAY_DOWN_B2_START: // 托盘下降启动
        MOTOR_ELEVATOR_B2;
        dealing_sub_state = DEAL_TRAY_DOWN_B2_WAIT_COMPLETE;
        break;

    case DEAL_TRAY_DOWN_B2_WAIT_COMPLETE: // 等待托盘下降完成
        if (abs(driver->motor[2]->motor_controller.pid_ref - driver->motor[2]->measure.total_ecd) < 50)
            dealing_sub_state = DEAL_LAYER2_CONV_START;
        break;

    case DEAL_LAYER2_CONV_START: // 启动传送带上第二层麻将
        MOTOR_CONVEYOR_1_START;
        MOTOR_CONVEYOR_2_START;
        MOTOR_TURNTABLE_START;
        dealing_sub_state = DEAL_LAYER2_CONV_WAIT_TILE;
        break;

    case DEAL_LAYER2_CONV_WAIT_TILE: // 等待传送带的牌到达
        if (ir_left_count > last_ir_left_count)
            MOTOR_CONVEYOR_1_STOP;
        if (ir_right_count > last_ir_right_count)
            MOTOR_CONVEYOR_2_STOP;
        if (ir_left_count > last_ir_left_count && ir_right_count > last_ir_right_count)
        {
            // MOTOR_TURNTABLE_STOP;
            last_ir_left_count = ir_left_count;   // 更新红外计数
            last_ir_right_count = ir_right_count; // 更新红外计数
            dealing_sub_state = DEAL_TRAY_UP_START;
        }
        break;

    case DEAL_TRAY_UP_START: // 托盘上升启动
        MOTOR_ELEVATOR_BG;
        dealing_sub_state = DEAL_TRAY_UP_WAIT_COMPLETE;
        break;

    case DEAL_TRAY_UP_WAIT_COMPLETE: // 等待托盘上升完成
        if (abs(driver->motor[2]->motor_controller.pid_ref - driver->motor[2]->measure.total_ecd) < 50)
            dealing_sub_state = DEAL_WAIT_TILE_CAUGHT;
        break;

    case DEAL_WAIT_TILE_CAUGHT: // 等待牌被接走
        if (ir_left_count > last_ir_left_count && ir_right_count > last_ir_right_count)
        {
            dealStep++;
            if (dealStep >= 12)
            {
                global_phase = PHASE_JUMPING;
                dealStep = 0; // 重置发牌轮数
            }

            dealing_sub_state = DEAL_INIT; // 重置发牌状态机
            ir_left_count = 0;             // 重置红外计数
            ir_right_count = 0;            // 重置红外计数
            last_ir_left_count = 0;        // 重置红外计数
            last_ir_right_count = 0;       // 重置红外计数
        }
        break;

    default:
        break;
    }

    driver->stop_flag = MOTOR_ENABLED;
}

/**
 * @brief 跳牌阶段任务
 */
static void phase_jumping_task()
{
    switch (jumping_sub_state)
    {
    case JUMP_INIT: // 初始化跳牌阶段
        jumping_sub_state = JUMP_TRAY_DOWN_B1_START;
        break;

    case JUMP_TRAY_DOWN_B1_START: // 托盘下降启动
        MOTOR_ELEVATOR_B1;
        jumping_sub_state = JUMP_TRAY_DOWN_B1_WAIT_COMPLETE;
        break;

    case JUMP_TRAY_DOWN_B1_WAIT_COMPLETE: // 等待托盘下降完成
        if (abs(driver->motor[2]->motor_controller.pid_ref - driver->motor[2]->measure.total_ecd) < 50)
            jumping_sub_state = JUMP_CONV_START;
        break;

    case JUMP_CONV_START: // 启动传送带上第一层麻将
        jumpStep++;
        if (jumpStep == 1)
        {

            MOTOR_CONVEYOR_1_START;
            MOTOR_CONVEYOR_2_START;
        }
        else
        {
            MOTOR_CONVEYOR_1_START;
            MOTOR_CONVEYOR_2_STOP;
        }
        MOTOR_TURNTABLE_START;
        jumping_sub_state = JUMP_CONV_WAIT_TILE;
        break;

    case JUMP_CONV_WAIT_TILE: // 等待传送带的牌到达
        if (jumpStep == 1)
        {
            if (ir_left_count > last_ir_left_count)
                MOTOR_CONVEYOR_1_STOP;
            if (ir_right_count > last_ir_right_count)
                MOTOR_CONVEYOR_2_STOP;

            if (ir_left_count > last_ir_left_count && ir_right_count > last_ir_right_count)
            {
                MOTOR_TURNTABLE_STOP;
                last_ir_left_count = ir_left_count;   // 更新红外计数
                last_ir_right_count = ir_right_count; // 更新红外计数
                jumping_sub_state = JUMP_TRAY_UP_START;
            }
        }
        else
        {
            if (ir_left_count > last_ir_left_count)
            {
                MOTOR_CONVEYOR_1_STOP;
                last_ir_left_count = ir_left_count; // 更新红外计数
                jumping_sub_state = JUMP_TRAY_UP_START;
            }
        }

    case JUMP_TRAY_UP_START: // 托盘上升启动
        MOTOR_ELEVATOR_BG;
        jumping_sub_state = JUMP_TRAY_UP_WAIT_COMPLETE;
        break;

    case JUMP_TRAY_UP_WAIT_COMPLETE: // 等待托盘上升完成
        if (abs(driver->motor[2]->motor_controller.pid_ref - driver->motor[2]->measure.total_ecd) < 50)
            jumping_sub_state = JUMP_WAIT_TILE_CAUGHT;

    case JUMP_WAIT_TILE_CAUGHT: // 等待牌被接走
        if (ir_left_count > last_ir_left_count && ir_right_count > last_ir_right_count)
        {
            jumpStep++;
            if (jumpStep >= 4)
            {
                global_phase = PHASE_SINGLE_REFILL;
                jumpStep = 0; // 重置跳牌步数
            }

            jumping_sub_state = JUMP_INIT; // 重置跳牌状态机
            ir_left_count = 0;             // 重置红外计数
            ir_right_count = 0;            // 重置红外计数
            last_ir_left_count = 0;        // 重置红外计数
            last_ir_right_count = 0;       // 重置红外计数
        }

    default:
        break;
    }

    driver->stop_flag = MOTOR_ENABLED;
}

/**
 * @brief 单张摸牌并补牌阶段任务
 */
static void phase_single_refill_task()
{
    switch (single_refill_sub_state)
    {
    case REFILL_INIT: // 初始化单张摸牌并补牌阶段
        single_refill_sub_state = REFILL_TRAY_DOWN_B1_START;
        break;

    case REFILL_TRAY_DOWN_B1_START: // 托盘下降启动
        MOTOR_ELEVATOR_B1;
        single_refill_sub_state = REFILL_TRAY_DOWN_B1_WAIT_COMPLETE;
        break;

    case REFILL_TRAY_DOWN_B1_WAIT_COMPLETE: // 等待托盘下降完成
        if (abs(driver->motor[2]->motor_controller.pid_ref - driver->motor[2]->measure.total_ecd) < 50)
            single_refill_sub_state = REFILL_CONV_START;
        break;

    case REFILL_CONV_START: // 启动传送带上第一层麻将
        MOTOR_CONVEYOR_1_START;
        MOTOR_CONVEYOR_2_STOP;
        MOTOR_TURNTABLE_START;
        single_refill_sub_state = REFILL_CONV_WAIT_TILE;
        break;

    case REFILL_CONV_WAIT_TILE: // 等待传送带的牌到达
        if (ir_left_count > last_ir_left_count)
        {
            MOTOR_CONVEYOR_1_STOP;
            last_ir_left_count = ir_left_count; // 更新红外计数
            single_refill_sub_state = REFILL_TRAY_UP_START;
        }

    case REFILL_TRAY_UP_START: // 托盘上升启动
        MOTOR_ELEVATOR_BG;
        single_refill_sub_state = REFILL_TRAY_UP_WAIT_COMPLETE;
        break;

    case REFILL_TRAY_UP_WAIT_COMPLETE: // 等待托盘上升完成
        if (abs(driver->motor[2]->motor_controller.pid_ref - driver->motor[2]->measure.total_ecd) < 50)
            single_refill_sub_state = REFILL_WAIT_TILE_CAUGHT;
        break;

    case REFILL_WAIT_TILE_CAUGHT: // 等待牌被接走
        if (ir_left_count > last_ir_left_count && ir_right_count > last_ir_right_count)
        {
            refillStep++;
            if (refillStep >= 91) // 所有牌都被接完引起的牌局自然结束
            {
                global_phase = PHASE_OVER;
                refillStep = 0; // 重置补牌步数
            }

            single_refill_sub_state = REFILL_INIT; // 重置跳牌状态机
            ir_left_count = 0;                     // 重置红外计数
            ir_right_count = 0;                    // 重置红外计数
            last_ir_left_count = 0;                // 重置红外计数
            last_ir_right_count = 0;               // 重置红外计数
        }

    default:
        break;
    }

    driver->stop_flag = MOTOR_ENABLED;
}

/**
 * @brief 阶段结束任务
 */
static void phase_over_task()
{
    switch (over_sub_state)
    {
    case OVER_INIT: // 初始化阶段结束
                    // if 检测到按键按下
        over_sub_state = OVER_LID_OPEN_START;
        break;

    case OVER_LID_OPEN_START:
        MOTOR_LID_OPEN;
        over_sub_state = OVER_LID_OPEN_WAIT_COMPLETE;
        break;

    case OVER_LID_OPEN_WAIT_COMPLETE:
        if (abs(driver->motor[3]->motor_controller.pid_ref - driver->motor[3]->measure.total_ecd) < 50)
            over_sub_state = OVER_LID_CLOSE_START;
        break;

    case OVER_LID_CLOSE_START:
        MOTOR_LID_CLOSE;
        over_sub_state = OVER_LID_CLOSE_WAIT_COMPLETE;
        break;

    case OVER_LID_CLOSE_WAIT_COMPLETE:
        if (abs(driver->motor[3]->motor_controller.pid_ref - driver->motor[3]->measure.total_ecd) < 50)
        {
            over_sub_state = OVER_INIT;
            global_phase = PHASE_IDLE;
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
    driver->stop_flag = MOTOR_STOP;
}