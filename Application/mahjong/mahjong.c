#include "mahjong.h"
#include "driver.h"

static Motor_Instance motor_0, motor_1, motor_2, motor_3; // 电机实例

/***
 * @brief initialize mahjong_task
 */
void mahjong_init()
{
    Driver_Init_Config_s init_config = {
        .motor = {&motor_0, &motor_1, &motor_2, &motor_3},
        .usart_config.recv_buff_size = 50,
        .usart_config.usart_handle = &huart2,
        .usart_config.module_callback = NULL, // TODO: add callback function
        .mtype = 2,                           // TODO: add motor type
        .deadzone = 1600,                     // TODO: add deadzone value
        .mline = 13,                          // TODO: add motor line
        .mphase = 34.014,                     // TODO: add motor phase
    };
    Driver_Instance *driver_instance = DriverInit(&init_config);

    USARTSend(driver_instance->usart, (uint8_t *)"$mtype:2#", 10, USART_TRANSFER_BLOCKING);
    USARTSend(driver_instance->usart, (uint8_t *)"$mline:13#", 11, USART_TRANSFER_BLOCKING);
    USARTSend(driver_instance->usart, (uint8_t *)"$mphase:34.014#", 16, USART_TRANSFER_BLOCKING);
    USARTSend(driver_instance->usart, (uint8_t *)"$deadzone:1600#", 16, USART_TRANSFER_BLOCKING);
    USARTSend(driver_instance->usart, (uint8_t *)"$MPID:0.8,0.06,0.5#", 20, USART_TRANSFER_BLOCKING);
    USARTSend(driver_instance->usart, (uint8_t *)"$upload:1,0,0#", 15, USART_TRANSFER_BLOCKING);
}