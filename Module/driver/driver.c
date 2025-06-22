#include "driver.h"
#include <string.h> // Added for string functions
#include <stdio.h>  // Added for sscanf

static uint8_t idx = 0;
static Driver_Instance *driver_instances[DRIVER_MAX_NUM] = {NULL}; // 驱动板实例数组,最多1个驱动板

/**
 * @brief register motor instance
 */
Motor_Instance *MotorRegister(Motor_Init_Config_s *init_config)
{
    Motor_Instance *motor = (Motor_Instance *)malloc(sizeof(Motor_Instance));
    memset(motor, 0, sizeof(Motor_Instance));

    motor->measure.ecd = 0;
    motor->measure.last_ecd = 0;
    motor->measure.total_ecd = 0;

    motor->motor_settings = init_config->controller_setting_init_config;
    PIDInit(&motor->motor_controller.angle_PID, &init_config->controller_param_init_config.angle_PID);
    PIDInit(&motor->motor_controller.speed_PID, &init_config->controller_param_init_config.speed_PID);
    PIDInit(&motor->motor_controller.current_PID, &init_config->controller_param_init_config.current_PID);

    return motor;
}

/***
 * @brief initialize driver instance
 */
Driver_Instance *DriverInit(Driver_Init_Config_s *init_config)
{
    Driver_Instance *driver = (Driver_Instance *)malloc(sizeof(Driver_Instance));
    memset(driver, 0, sizeof(Driver_Instance));

    // 注册usart实例
    init_config->usart_config.id = driver; // 设置id为驱动板实例,用于回调函数中识别
    driver->usart = USARTRegister(&init_config->usart_config);

    for (int i = 0; i < MOTOR_CNT; ++i)
    {
        driver->motor[i] = init_config->motor[i];
    }

    driver->stop_flag = MOTOR_FLAG_STOP;         // 默认状态为停止
    driver->callback_flag = MOTOR_CALLBACK_NONE; // 默认没有回调标志

    driver_instances[idx++] = driver; // 将驱动板实例添加到数组中

    return driver;
}

// 改进的驱动回调函数，增加帧同步功能
void DriverCallback(USART_Instance *_usart_instance)
{
    Driver_Instance *driver_instance = (Driver_Instance *)_usart_instance->id;
    uint8_t *rx_buf = driver_instance->usart->recv_buff;
    uint16_t rx_len = driver_instance->usart->data_len;

    // // 基本长度检查
    // if (rx_len < 16 || rx_len > 32)
    // {
    //     driver_instance->callback_flag = MOTOR_CALLBACK_NONE;
    //     return;
    // }

    // 尝试在接收缓冲区中查找完整的帧
    if (FindAndProcessFrame(driver_instance, rx_buf, rx_len))
    {
        // 成功处理了一个完整帧
        driver_instance->callback_flag = MOTOR_CALLBACK_NORMAL;
        return;
    }

    // // 如果没有找到完整帧，设置正常回调标志
    // driver_instance->callback_flag = MOTOR_CALLBACK_NONE;
}

/**
 * @brief 在接收缓冲区中查找并处理完整的数据帧
 * @param driver_instance 驱动实例
 * @param buffer 接收缓冲区
 * @param length 缓冲区长度
 * @return 1表示找到并处理了完整帧，0表示没有找到完整帧
 */
uint8_t FindAndProcessFrame(Driver_Instance *driver_instance, uint8_t *buffer, uint16_t length)
{
    // 查找起始标志 '$'
    for (uint16_t start_pos = 0; start_pos < length; start_pos++)
    {
        if (buffer[start_pos] == '$')
        {
            // 找到起始标志，现在查找结束标志 '#'
            for (uint16_t end_pos = start_pos + 1; end_pos < length; end_pos++)
            {
                if (buffer[end_pos] == '#')
                {
                    // 找到完整帧，计算帧长度
                    uint16_t frame_length = end_pos - start_pos + 1;

                    // 验证帧长度合理性
                    if (frame_length >= 14 && frame_length <= 64)
                    {
                        // 处理这个完整帧
                        if (ProcessCompleteFrame(driver_instance, &buffer[start_pos], frame_length))
                        {
                            return 1; // 成功处理
                        }
                    }

                    // 如果当前帧处理失败，继续查找下一个可能的帧
                    start_pos = end_pos; // 从当前结束位置继续搜索
                    break;
                }
            }
        }
    }

    return 0; // 没有找到完整帧
}

/**
 * @brief 处理一个完整的数据帧
 * @param driver_instance 驱动实例
 * @param frame_buffer 帧数据缓冲区
 * @param frame_length 帧长度
 * @return 1表示处理成功，0表示处理失败
 */
uint8_t ProcessCompleteFrame(Driver_Instance *driver_instance, uint8_t *frame_buffer, uint16_t frame_length)
{
    char temp_buf[frame_length + 1];
    int m1, m2, m3, m4;

    // 复制并添加字符串结束符
    memcpy(temp_buf, frame_buffer, frame_length);
    temp_buf[frame_length] = '\0';

    // 验证帧格式（起始符和结束符）
    if (temp_buf[0] != '$' || temp_buf[frame_length - 1] != '#')
    {
        return 0;
    }

    // 处理 $MAll: 类型消息
    if (strncmp(temp_buf, "$MAll:", 6) == 0)
    {
        int parsed_count = sscanf(temp_buf, "$MAll:%d,%d,%d,%d#", &m1, &m2, &m3, &m4);
        if (parsed_count == 4)
        {
            driver_instance->motor[0]->measure.total_ecd = m1;
            driver_instance->motor[1]->measure.total_ecd = m2;
            driver_instance->motor[2]->measure.total_ecd = m3;
            driver_instance->motor[3]->measure.total_ecd = m4;

            // 复位逻辑
            if (driver_instance->mahjong_phase)
            {
                driver_instance->motor[0]->measure.init_ecd = m1;
                driver_instance->motor[1]->measure.init_ecd = m2;
                driver_instance->motor[2]->measure.init_ecd = m3;
                driver_instance->motor[3]->measure.init_ecd = m4;
                driver_instance->mahjong_phase = 0;
            }

            return 1; // 成功处理
        }
    }
    // 处理 $MTEP: 类型消息
    else if (strncmp(temp_buf, "$MTEP:", 6) == 0)
    {
        int parsed_count = sscanf(temp_buf, "$MTEP:%d,%d,%d,%d#", &m1, &m2, &m3, &m4);
        if (parsed_count == 4)
        {
            driver_instance->motor[0]->measure.last_ecd = driver_instance->motor[0]->measure.ecd;
            driver_instance->motor[1]->measure.last_ecd = driver_instance->motor[1]->measure.ecd;
            driver_instance->motor[2]->measure.last_ecd = driver_instance->motor[2]->measure.ecd;
            driver_instance->motor[3]->measure.last_ecd = driver_instance->motor[3]->measure.ecd;

            driver_instance->motor[0]->measure.ecd = m1;
            driver_instance->motor[1]->measure.ecd = m2;
            driver_instance->motor[2]->measure.ecd = m3;
            driver_instance->motor[3]->measure.ecd = m4;

            return 1; // 成功处理
        }
    }

    return 0; // 未知帧类型或解析失败
}

/**
 * @brief set motor ref
 */
void MotorSetRef(Motor_Instance *motor, float ref)
{
    motor->motor_controller.pid_ref = ref;
}

/**
 * @brief motor control
 */
void MotorControl(Driver_Instance *driver)
{
    int16_t set[MOTOR_CNT] = {0}; // 电机控制发送设定值
    Motor_Instance *motor;
    Motor_Control_Setting_s *motor_setting; // 电机控制参数
    Motor_Controller_s *motor_controller;   // 电机控制器
    Motor_Measure_s *measure;               // 电机测量值
    float pid_measure, pid_ref;             // 电机PID测量值和设定值

    // 遍历所有电机实例,进行串级PID的计算并设置发送报文的值
    for (size_t i = 0; i < MOTOR_CNT; ++i)
    { // 减小访存开销,先保存指针引用
        motor = driver->motor[i];
        motor_setting = &motor->motor_settings;
        motor_controller = &motor->motor_controller;
        measure = &motor->measure;
        pid_ref = motor_controller->pid_ref; // 保存设定值,防止motor_controller->pid_ref在计算过程中被修改
        if (motor_setting->motor_reverse_flag == MOTOR_DIRECTION_REVERSE)
            pid_ref *= -1; // 设置反转

        // pid_ref会顺次通过被启用的闭环充当数据的载体
        // 计算位置环,只有启用位置环且外层闭环为位置时会计算速度环输出
        if ((motor_setting->close_loop_type & ANGLE_LOOP) && motor_setting->outer_loop_type == ANGLE_LOOP)
        {
            if (motor_setting->angle_feedback_source == OTHER_FEED)
                pid_measure = *motor_controller->other_angle_feedback_ptr;
            else
                pid_measure = measure->total_ecd; // MOTOR_FEED,对total angle闭环,防止在边界处出现突跃

            if (motor_setting->motor_reverse_flag == MOTOR_DIRECTION_REVERSE) // 一个角度环的神奇bug，这里不能有反转
                pid_ref *= -1;                                                // 设置反转
            // 更新pid_ref进入下一个环
            pid_ref = PIDCalculate(&motor_controller->angle_PID, pid_measure, pid_ref);
        }

        // 此处为了方便，直接用作pwm输出的
        if ((motor_setting->close_loop_type & SPEED_LOOP) && (motor_setting->outer_loop_type == SPEED_LOOP))
        {
        }

        // // 计算速度环,(外层闭环为速度或位置)且(启用速度环)时会计算速度环
        // if ((motor_setting->close_loop_type & SPEED_LOOP) && (motor_setting->outer_loop_type & (ANGLE_LOOP | SPEED_LOOP)))
        // {
        //     if (motor_setting->feedforward_flag & SPEED_FEEDFORWARD)
        //         pid_ref += *motor_controller->speed_feedforward_ptr;

        //     if (motor_setting->speed_feedback_source == OTHER_FEED)
        //         pid_measure = *motor_controller->other_speed_feedback_ptr;
        //     else // MOTOR_FEED
        //         pid_measure = measure->speed_aps;
        //     // 更新pid_ref进入下一个环
        //     pid_ref = PIDCalculate(&motor_controller->speed_PID, pid_measure, pid_ref);
        // }

        // // 计算电流环,目前只要启用了电流环就计算,不管外层闭环是什么,并且电流只有电机自身传感器的反馈
        // if (motor_setting->feedforward_flag & CURRENT_FEEDFORWARD)
        //     pid_ref += *motor_controller->current_feedforward_ptr;
        // if (motor_setting->close_loop_type & CURRENT_LOOP)
        // {
        //     pid_ref = PIDCalculate(&motor_controller->current_PID, measure->real_current, pid_ref);
        // }

        // if (motor_setting->outer_loop_type == ANGLE_LOOP)
        // {
        //     if (motor_setting->motor_reverse_flag == MOTOR_DIRECTION_REVERSE) // 一个角度环的神奇bug，这里让电流反过来
        //         pid_ref *= -1;                                                // 设置反转
        // }
        // else // 角度环不需要这个
        // {
        //     if (motor_setting->feedback_reverse_flag == FEEDBACK_DIRECTION_REVERSE)
        //         pid_ref *= -1;
        // }

        // 获取最终输出
        motor_controller->pid_out = pid_ref;
        set[i] = (int16_t)pid_ref;
    }

    // 发送电机控制数据
    // 发送数据格式: $pwm:0,0,0,0#
    if (driver->stop_flag == MOTOR_FLAG_STOP)
    {
        char *pwm_cmd = "$pwm:0,0,0,0#";
        USARTSend(driver->usart, (uint8_t *)pwm_cmd, strlen(pwm_cmd), USART_TRANSFER_BLOCKING);
    }
    else
    {
        char send_buf[64];
        snprintf(send_buf, sizeof(send_buf), "$pwm:%d,%d,%d,%d#", set[0], set[1], set[2], set[3]);
        USARTSend(driver->usart, (uint8_t *)send_buf, strlen(send_buf), USART_TRANSFER_BLOCKING);
    }
}

/**
 * @brief 判断电机是否到达指定位置
 * @param motor 电机实例
 * @param tolerance 允许的误差范围
 */
uint8_t MotorIsAtPosition(Motor_Instance *motor, int16_t tolerance)
{
    if (abs(motor->motor_controller.pid_ref - (float)motor->measure.total_ecd) < tolerance)
        return 1; // 到达指定位置
    else
        return 0; // 未到达指定位置
}