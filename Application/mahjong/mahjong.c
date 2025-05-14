#include "mahjong.h"
#include "driver.h"
#include "string.h"
#include "bsp_gpio.h"

static Driver_Instance *driver;                                                  // 驱动板实例
static Motor_Instance *motor_push_1, *motor_push_2, *motor_elevator, *motor_lid; // 电机实例
static GPIOInstance *gpio_key1, *gpio_key2, *gpio_red_1, *gpio_red_2;            // GPIO实例
static int16_t key1_num, key2_num, red1_num, red2_num = 0;                       // 按键次数
static uint8_t phase = 1;                                                        // 轮数

static void Key1Callback(GPIOInstance *gpio);
static void Key2Callback(GPIOInstance *gpio);
static void Red1Callback(GPIOInstance *gpio);
static void Red2Callback(GPIOInstance *gpio);

/***
 * @brief initialize mahjong_task
 */
void mahjong_init()
{
    Motor_Init_Config_s motor_config = {
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 1,     // 0.2 0.3
                .Ki = 0,     // 0
                .Kd = 0.015, // 0.015
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
    motor_lid = MotorRegister(&motor_config);

    Driver_Init_Config_s init_config = {
        .motor = {
            motor_push_1,
            motor_push_2,
            motor_elevator,
            motor_lid,
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
}

void mahjong_task()
{
    //"$pwm:0,0,0,0#" 控制电机转动,速度的范围为(-3600~3600)
    // const char *pwm_cmd = "$pwm:100,0,0,0#";
    // USARTSend(driver->usart, (uint8_t *)pwm_cmd, strlen(pwm_cmd), USART_TRANSFER_BLOCKING);

    if (red1_num < 24 && red2_num < 24)
    {
    }
    else if (red1_num == 24 && red2_num == 24)
    {
    }
    else
    {
    }

    if (phase)
    {
        driver->mahjong_phase = 1;
        phase = 0;
    }

    if (key2_num % 2 == 1)
        driver->stop_flag = MOTOR_STOP;
    else
        driver->stop_flag = MOTOR_ENABLED;

    MotorControl(driver);
}

static void Key1Callback(GPIOInstance *gpio)
{
    key1_num++;

    if (key1_num % 2 == 1)
    {
        MotorSetRef(motor_push_1, motor_push_1->measure.init_ecd + 1000); // 推牌参数1000
        MotorSetRef(motor_push_2, motor_push_2->measure.init_ecd + 1000);
        MotorSetRef(motor_elevator, motor_elevator->measure.init_ecd + 1000);
        MotorSetRef(motor_lid, motor_lid->measure.init_ecd + 1000);
    }

    else
    {
        MotorSetRef(motor_push_1, motor_push_1->measure.init_ecd);
        MotorSetRef(motor_push_2, motor_push_2->measure.init_ecd);
        MotorSetRef(motor_elevator, motor_elevator->measure.init_ecd);
        MotorSetRef(motor_lid, motor_lid->measure.init_ecd);
    }
}

static void Key2Callback(GPIOInstance *gpio)
{
    key2_num++;
}

static void Red1Callback(GPIOInstance *gpio)
{
    red1_num++;
}

static void Red2Callback(GPIOInstance *gpio)
{
    red2_num++;
}