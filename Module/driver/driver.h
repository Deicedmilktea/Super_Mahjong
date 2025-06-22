#ifndef DRIVER_H
#define DRIVER_H

#include "bsp_usart.h"
#include "controller.h"
#include "daemon.h"

#define MOTOR_CNT 4            // 电机数量,目前只支持4个电机
#define DRIVER_MAX_NUM 1       // 驱动板数量,目前只支持1个驱动板
#define DRIVER_RXBUFF_LIMIT 32 // 串口接收缓冲区限制

/**
 * @brief 闭环类型,如果需要多个闭环,则使用或运算
 *        例如需要速度环和电流环: CURRENT_LOOP|SPEED_LOOP
 */
typedef enum
{
    OPEN_LOOP = 0b0000,
    CURRENT_LOOP = 0b0001,
    SPEED_LOOP = 0b0010,
    ANGLE_LOOP = 0b0100,
    TORQUE_LOOP = 0b1000,
    // only for checking
    SPEED_AND_CURRENT_LOOP = 0b0011,
    ANGLE_AND_SPEED_LOOP = 0b0110,
    ALL_THREE_LOOP = 0b0111,
} Closeloop_Type_e;

typedef enum
{
    FEEDFORWARD_NONE = 0b00,
    CURRENT_FEEDFORWARD = 0b01,
    SPEED_FEEDFORWARD = 0b10,
    CURRENT_AND_SPEED_FEEDFORWARD = CURRENT_FEEDFORWARD | SPEED_FEEDFORWARD,
} Feedfoward_Type_e;

/* 反馈来源设定,若设为OTHER_FEED则需要指定数据来源指针,详见Motor_Controller_s*/
typedef enum
{
    MOTOR_FEED = 0,
    OTHER_FEED,
} Feedback_Source_e;

/* 电机正反转标志 */
typedef enum
{
    MOTOR_DIRECTION_NORMAL = 0,
    MOTOR_DIRECTION_REVERSE = 1
} Motor_Reverse_Flag_e;

/* 反馈量正反标志 */
typedef enum
{
    FEEDBACK_DIRECTION_NORMAL = 0,
    FEEDBACK_DIRECTION_REVERSE = 1
} Feedback_Reverse_Flag_e;

/* 电机启停标志 */
typedef enum
{
    MOTOR_FLAG_STOP = 0,
    MOTOR_FLAG_ENABLED = 1,
} Motor_Working_Type_e;

/* 接收电机返回信号标志 */
typedef enum
{
    MOTOR_CALLBACK_NONE,   // 无信号接收
    MOTOR_CALLBACK_NORMAL, // 正常接收信号
} Motor_Callback_Flag_e;

/**
 * @brief 电机控制器初始化结构体,包括三环PID的配置以及两个反馈数据来源指针
 *        如果不需要某个控制环,可以不设置对应的pid config
 *        需要其他数据来源进行反馈闭环,不仅要设置这里的指针还需要在Motor_Control_Setting_s启用其他数据来源标志
 */
typedef struct
{
    float *other_angle_feedback_ptr; // 角度反馈数据指针,注意电机使用total_angle
    float *other_speed_feedback_ptr; // 速度反馈数据指针,单位为angle per sec

    float *speed_feedforward_ptr;   // 速度前馈数据指针
    float *current_feedforward_ptr; // 电流前馈数据指针

    float pid_speed_out;
    float pid_angle_out;
    float pid_out;

    PID_Init_Config_s torque_PID;
    PID_Init_Config_s current_PID;
    PID_Init_Config_s speed_PID;
    PID_Init_Config_s angle_PID;
} Motor_Controller_Init_s;

/* 电机控制设置,包括闭环类型,反转标志和反馈来源 */
typedef struct
{
    Closeloop_Type_e outer_loop_type;              // 最外层的闭环,未设置时默认为最高级的闭环
    Closeloop_Type_e close_loop_type;              // 使用几个闭环(串级)
    Motor_Reverse_Flag_e motor_reverse_flag;       // 是否反转
    Feedback_Reverse_Flag_e feedback_reverse_flag; // 反馈是否反向
    Feedback_Source_e angle_feedback_source;       // 角度反馈类型
    Feedback_Source_e speed_feedback_source;       // 速度反馈类型
    Feedfoward_Type_e feedforward_flag;            // 前馈标志
} Motor_Control_Setting_s;

/**
 * @brief 电机初始化配置结构体
 */
typedef struct
{
    Motor_Controller_Init_s controller_param_init_config;
    Motor_Control_Setting_s controller_setting_init_config;
} Motor_Init_Config_s;

/* 电机控制器,包括其他来源的反馈数据指针,3环控制器和电机的参考输入*/
// 后续增加前馈数据指针
typedef struct
{
    float *other_angle_feedback_ptr; // 其他反馈来源的反馈数据指针
    float *other_speed_feedback_ptr;
    float *speed_feedforward_ptr;
    float *current_feedforward_ptr;

    PID_Instance current_PID;
    PID_Instance speed_PID;
    PID_Instance angle_PID;

    float pid_ref; // 将会作为每个环的输入和输出顺次通过串级闭环
    float pid_speed_out;
    float pid_angle_out;
    float pid_out;
} Motor_Controller_s;

/* 电机反馈信息*/
typedef struct
{
    int16_t init_ecd;         // 编码器初始值
    int16_t last_ecd;         // 上一次读取的编码器值
    int16_t ecd;              // 0-8191,刻度总共有8192格 (modified to support negative values)
    float angle_single_round; // 单圈角度
    int32_t total_ecd;        // 总编码器值,注意方向 (modified to support negative values)
    float total_angle;        // 总角度,注意方向
    int32_t total_round;      // 总圈数,注意方向
    float speed_aps;          // 角速度,单位为:度/秒
    int16_t real_current;     // 实际电流
} Motor_Measure_s;

/**
 * @brief 电机实例
 *
 */
typedef struct
{
    Motor_Measure_s measure;                // 电机测量数据
    Motor_Control_Setting_s motor_settings; // 电机设置
    Motor_Controller_s motor_controller;    // 电机控制器
    Motor_Working_Type_e stop_flag;         // 启停标志
} Motor_Instance;

/**
 * @brief 驱动板实例
 *
 */
typedef struct
{
    uint8_t mahjong_phase;
    Motor_Instance *motor[MOTOR_CNT];    // 电机实例数组,最多4个电机
    USART_Instance *usart;               // 电机实例对应的串口实例
    Motor_Working_Type_e stop_flag;      // 启停标志
    Motor_Callback_Flag_e callback_flag; // 接收信号标志
} Driver_Instance;

/***
 * @brief 驱动板初始化配置结构体
 */
typedef struct
{
    Motor_Instance *motor[MOTOR_CNT];
    USART_Init_Config_s usart_config;   // 驱动板实例对应的串口实例
    Daemon_Init_Config_s daemon_config; // 驱动板实例对应的守护进程实例
} Driver_Init_Config_s;

Driver_Instance *DriverInit(Driver_Init_Config_s *init_config);
void DriverCallback(USART_Instance *usart_instance);
Motor_Instance *MotorRegister(Motor_Init_Config_s *init_config);
void MotorSetRef(Motor_Instance *motor_instance, float ref);
void MotorControl(Driver_Instance *driver);

/**
 * @brief 判断电机是否到达指定位置
 * @param motor 电机实例
 * @param tolerance 允许的误差范围
 */
uint8_t MotorIsAtPosition(Motor_Instance *motor, int16_t tolerance);

/**
 * @brief 在接收缓冲区中查找并处理完整的数据帧
 * @param driver_instance 驱动实例
 * @param buffer 接收缓冲区
 * @param length 缓冲区长度
 * @return 1表示找到并处理了完整帧，0表示没有找到完整帧
 */
uint8_t FindAndProcessFrame(Driver_Instance *driver_instance, uint8_t *buffer, uint16_t length);

/**
 * @brief 处理一个完整的数据帧
 * @param driver_instance 驱动实例
 * @param frame_buffer 帧数据缓冲区
 * @param frame_length 帧长度
 * @return 1表示处理成功，0表示处理失败
 */
uint8_t ProcessCompleteFrame(Driver_Instance *driver_instance, uint8_t *frame_buffer, uint16_t frame_length);

#endif // !DRIVER_H
