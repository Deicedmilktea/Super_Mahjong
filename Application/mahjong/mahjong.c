#include "mahjong.h"
#include "driver.h"

static Driver_Instance *driver;                                                    // 驱动板实例
static Motor_Instance motor_turntable, motor_belt_1, motor_belt_2, motor_elevator; // 电机实例

/***
 * @brief initialize mahjong_task
 */
void mahjong_init()
{
    Driver_Init_Config_s init_config = {
        .motor = {&motor_turntable, &motor_belt_1, &motor_belt_2, &motor_elevator},
    };

    driver = DriverInit(&init_config);

    const char *commands[] = {
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
    const char *pwm_cmd = "$pwm:0,0,0,0#";
    USARTSend(driver->usart, (uint8_t *)pwm_cmd, strlen(pwm_cmd), USART_TRANSFER_BLOCKING);
}
