#include "mahjong.h"
#include "driver.h"
#include "string.h"
#include "bsp_gpio.h"

static Driver_Instance *driver;                                                  // 驱动板实例
static Motor_Instance *motor_push_1, *motor_push_2, *motor_elevator, *motor_lid; // 电机实例
static GPIOInstance *gpio_key;
static int16_t key_num = 0; // 按键次数

static void KeyCallback(GPIOInstance *gpio);

/***
 * @brief initialize mahjong_task
 */
void mahjong_init()
{
    Motor_Init_Config_s motor_config = {
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 0.3, // 0.2
                .Ki = 0, // 0
                .Kd = 0.02,   // 0.015
                .Improve = PID_Integral_Limit | PID_Derivative_On_Measurement,
                .MaxOut = 1000,
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

    GPIO_Init_Config_s gpio_init = {
        .exti_mode = GPIO_EXTI_MODE_FALLING, // 注意和CUBEMX的配置一致
        .GPIO_Pin = GPIO_PIN_2,              // GPIO引脚
        .GPIOx = GPIOE,                      // GPIO外设
        .gpio_model_callback = KeyCallback,  // EXTI回调函数
    };
    gpio_key = GPIORegister(&gpio_init); // 注册GPIO实例

    const char *commands[] = {"$mtype:2#", "$mline:13#", "$mphase:34.014#", "$deadzone:1600#", "$MPID:0.8,0.06,0.5#", "$upload:1,0,0#"};

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

    MotorControl(driver);
    HAL_Delay(50); // 10ms
}

static void KeyCallback(GPIOInstance *gpio)
{
    key_num++;

    if (key_num % 2 == 1)
    {
        MotorSetRef(motor_push_1, motor_push_1->measure.total_ecd + 1000);
        MotorSetRef(motor_push_2, motor_push_2->measure.total_ecd + 1000);
        MotorSetRef(motor_elevator, motor_elevator->measure.total_ecd + 1000);
        MotorSetRef(motor_lid, motor_lid->measure.total_ecd + 1000);
    }

    else
    {
        MotorSetRef(motor_push_1, motor_push_1->measure.total_ecd - 1000);
        MotorSetRef(motor_push_2, motor_push_2->measure.total_ecd - 1000);
        MotorSetRef(motor_elevator, motor_elevator->measure.total_ecd - 1000);
        MotorSetRef(motor_lid, motor_lid->measure.total_ecd - 1000);
    }
}