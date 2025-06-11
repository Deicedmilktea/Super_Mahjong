#ifndef AT8236_H
#define AT8236_H

#include "driver.h"
#include "bsp_gpio.h"

typedef enum
{
    AT8236_STOP,     // 电机停止
    AT8236_FORWARD,  // 电机正转
    AT8236_BACKWARD, // 电机反转
} AT8236_Motor_Mode_e;

typedef struct
{
    Motor_Instance *motor_a;    // 电机A实例
    GPIO_Instance *gpio_a1;     // 电机A控制引脚1
    GPIO_Instance *gpio_a2;     // 电机A控制引脚2
    uint16_t pwm_ena;           // 电机A使能PWM值
    AT8236_Motor_Mode_e mode_a; // 电机A工作模式

    Motor_Instance *motor_b;    // 电机B实例
    GPIO_Instance *gpio_b1;     // 电机B控制引脚1
    GPIO_Instance *gpio_b2;     // 电机B控制引脚2
    uint16_t pwm_enb;           // 电机B使能PWM值
    AT8236_Motor_Mode_e mode_b; // 电机B工作模式
} AT8236_Instance;

/**
 * @brief AT8236电机驱动板初始化配置结构体
 */
typedef struct
{
    GPIO_Init_Config_s gpio_a1_config; // 电机A控制引脚1配置
    GPIO_Init_Config_s gpio_a2_config; // 电机A控制引脚2配置
    uint16_t pwm_ena;                  // 电机A使能PWM值
    AT8236_Motor_Mode_e mode_a;        // 电机A工作模式

    GPIO_Init_Config_s gpio_b1_config; // 电机B控制引脚1配置
    GPIO_Init_Config_s gpio_b2_config; // 电机B控制引脚2配置
    uint16_t pwm_enb;                  // 电机B使能PWM值
    AT8236_Motor_Mode_e mode_b;        // 电机B工作模式
} AT8236_Init_Config_s;

/**
 * @brief AT8236电机驱动板实例化函数
 *
 * @param init_config 初始化配置
 * @return AT8236_Instance* 返回AT8236实例指针
 */
AT8236_Instance *AT8236Init(AT8236_Init_Config_s *init_config);

/**
 * @brief 设置AT8236电机驱动板的工作模式
 */
void AT8236Control(AT8236_Instance *at8236, AT8236_Motor_Mode_e mode_a, AT8236_Motor_Mode_e mode_b);

#endif // !AT8236_H
