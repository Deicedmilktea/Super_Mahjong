#ifndef DRIVER_H
#define DRIVER_H

#include "bsp_usart.h"

#define MOTOR_CNT 4      // 电机数量,目前只支持4个电机
#define DRIVER_MAX_NUM 1 // 驱动板数量,目前只支持1个驱动板

/* 电机反馈信息*/
typedef struct
{
    uint16_t last_ecd;        // 上一次读取的编码器值
    int16_t ecd;              // 0-8191,刻度总共有8192格 (modified to support negative values)
    float angle_single_round; // 单圈角度
    int32_t total_ecd;        // 总编码器值,注意方向 (modified to support negative values)
    float total_angle;        // 总角度,注意方向
    int32_t total_round;      // 总圈数,注意方向
} Motor_Measure_s;

/**
 * @brief 电机实例
 *
 */
typedef struct
{
    Motor_Measure_s measure; // 电机测量数据
} Motor_Instance;

/**
 * @brief 电机初始化配置结构体
 */
typedef struct
{

} Motor_Init_Config_s;

/**
 * @brief 驱动板实例
 *
 */
typedef struct
{
    Motor_Instance *motor[MOTOR_CNT]; // 电机实例数组,最多4个电机
    USART_Instance *usart;            // 电机实例对应的串口实例
} Driver_Instance;

/***
 * @brief 驱动板初始化配置结构体
 */
typedef struct
{
    Motor_Instance *motor[MOTOR_CNT];
    USART_Init_Config_s usart_config; // 驱动板实例对应的串口实例
} Driver_Init_Config_s;

Driver_Instance *DriverInit(Driver_Init_Config_s *init_config);
void DriverCallback(USART_Instance *usart_instance);

#endif // !DRIVER_H
