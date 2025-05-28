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

    init_config->usart_config = (USART_Init_Config_s){
        .recv_buff_size = USART_RXBUFF_LIMIT,
        .usart_handle = &huart2,
        .id = driver,
        .usart_module_callback = DriverCallback,
    };

    // 注册usart实例
    driver->usart = USARTRegister(&init_config->usart_config);

    for (int i = 0; i < MOTOR_CNT; ++i)
    {
        driver->motor[i] = init_config->motor[i];
    }

    driver->stop_flag = MOTOR_STOP;              // 默认状态为停止
    driver->callback_flag = MOTOR_CALLBACK_NONE; // 默认没有回调标志

    driver_instances[idx++] = driver; // 将驱动板实例添加到数组中

    return driver;
}

void DriverCallback(USART_Instance *_usart_instance)
{
    Driver_Instance *driver_instance = (Driver_Instance *)_usart_instance->id;
    uint8_t *rx_buf = driver_instance->usart->recv_buff;
    uint16_t rx_len = driver_instance->usart->data_len; // Actual length of received data from ISR

    if (rx_len < 16 || rx_len > USART_RXBUFF_LIMIT || rx_buf[0] != '$' || rx_buf[rx_len - 3] != '#')
    {
        return;
    }

    char temp_buf[rx_len + 1];

    // Copy the received data into the temporary buffer.
    memcpy(temp_buf, rx_buf, rx_len);
    // Null-terminate the string in the temporary buffer for safe string operations.
    temp_buf[rx_len] = '\0';

    int m1, m2, m3, m4;

    // Process $MAll: type messages
    // Example: $MAll:0,0,1,0#
    if (strncmp(temp_buf, "$MAll:", 6) == 0)
    {
        int parsed_count = sscanf(temp_buf, "$MAll:%d,%d,%d,%d#", &m1, &m2, &m3, &m4);
        if (parsed_count == 4)
        {
            driver_instance->motor[0]->measure.total_ecd = m1;
            driver_instance->motor[1]->measure.total_ecd = m2;
            driver_instance->motor[2]->measure.total_ecd = m3;
            driver_instance->motor[3]->measure.total_ecd = m4;

            // use for reset
            if (driver_instance->mahjong_phase)
            {
                driver_instance->motor[0]->measure.init_ecd = m1;
                driver_instance->motor[1]->measure.init_ecd = m2;
                driver_instance->motor[2]->measure.init_ecd = m3;
                driver_instance->motor[3]->measure.init_ecd = m4;
                driver_instance->mahjong_phase = 0;
            }

            return;
        }
    }

    // Process $MTEP: type messages (retained from previous logic)
    // Example: $MTEP:123,456,789,101#
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
            return;
        }
    }
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
    for (size_t i = 0; i < 1; ++i)
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
        set[i] = (int16_t)pid_ref;
    }

    // 发送电机控制数据
    // 发送数据格式: $pwm:0,0,0,0#
    if (driver->stop_flag == MOTOR_STOP)
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