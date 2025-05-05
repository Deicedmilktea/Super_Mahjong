#ifndef DRIVER_H
#define DRIVER_H

#include "bsp_usart.h"

#define MOTOR_CNT 4 // 电机数量,目前只支持4个电机

/* 电机反馈信息*/
typedef struct
{
    uint16_t last_ecd;        // 上一次读取的编码器值
    uint16_t ecd;             // 0-8191,刻度总共有8192格
    float angle_single_round; // 单圈角度
    uint32_t total_ecd;       // 总编码器值,注意方向
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
    uint8_t mtype;                    // 电机类型 310电机
    uint16_t deadzone;                // 死区值
    uint8_t mline;                    // 编码器相位线
    float mphase;                     // 减速比
} Driver_Instance;

/***
 * @brief 驱动板初始化配置结构体
 */
typedef struct
{
    Motor_Instance *motor[MOTOR_CNT];
    USART_Init_Config_s usart_config; // 电机实例对应的串口实例
    uint8_t mtype;                    // 电机类型 310电机
    uint16_t deadzone;                // 死区值
    uint8_t mline;                    // 编码器相位线
    float mphase;                     // 减速比
} Driver_Init_Config_s;

Driver_Instance *DriverInit(Driver_Init_Config_s *init_config);

#endif // !DRIVER_H