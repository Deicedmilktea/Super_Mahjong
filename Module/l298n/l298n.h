#ifndef L298N_H
#define L298N_H

#include "driver.h"
#include "bsp_gpio.h"

typedef enum
{
    L298N_STOP,     // 电机停止
    L298N_FORWARD,  // 电机正转
    L298N_BACKWARD, // 电机反转
} L298N_Motor_Mode_e;

typedef struct
{
    Motor_Instance *motor_a;   // 电机A实例
    GPIO_Instance *gpio_ena;   // 电机A使能引脚
    uint16_t pwm_ena;          // 电机A使能PWM值
    GPIO_Instance *gpio_a1;    // 电机A控制引脚1
    GPIO_Instance *gpio_a2;    // 电机A控制引脚2
    L298N_Motor_Mode_e mode_a; // 电机A工作模式

    Motor_Instance *motor_b;   // 电机B实例
    GPIO_Instance *gpio_enb;   // 电机B使能引脚
    uint16_t pwm_enb;          // 电机B使能PWM值
    GPIO_Instance *gpio_b1;    // 电机B控制引脚1
    GPIO_Instance *gpio_b2;    // 电机B控制引脚2
    L298N_Motor_Mode_e mode_b; // 电机B工作模式
} L298N_Instance;

/**
 * @brief L298N电机驱动板初始化配置结构体
 */
typedef struct
{
    GPIO_Init_Config_s gpio_ena_config; // 电机A使能引脚配置
    GPIO_Init_Config_s gpio_a1_config;  // 电机A控制引脚1配置
    GPIO_Init_Config_s gpio_a2_config;  // 电机A控制引脚2配置
    uint16_t pwm_ena;                   // 电机A使能PWM值
    L298N_Motor_Mode_e mode_a;          // 电机A工作模式

    GPIO_Init_Config_s gpio_enb_config; // 电机B使能引脚配置
    GPIO_Init_Config_s gpio_b1_config;  // 电机B控制引脚1配置
    GPIO_Init_Config_s gpio_b2_config;  // 电机B控制引脚2配置
    uint16_t pwm_enb;                   // 电机B使能PWM值
    L298N_Motor_Mode_e mode_b;          // 电机B工作模式
} L298N_Init_Config_s;

/**
 * @brief L298N电机驱动板实例化函数
 *
 * @param init_config 初始化配置
 * @return L298N_Instance* 返回L298N实例指针
 */
L298N_Instance *L298NInit(L298N_Init_Config_s *init_config);

/**
 * @brief 设置L298N电机驱动板的工作模式
 */
void L298NControl(L298N_Instance *l298n, L298N_Motor_Mode_e mode_a, L298N_Motor_Mode_e mode_b);

#endif // !L298N_H