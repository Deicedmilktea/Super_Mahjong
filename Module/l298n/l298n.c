#include "l298n.h"
#include "tim.h"

/**
 * @brief L298N电机驱动板实例化函数
 *
 * @param init_config 初始化配置
 * @return L298N_Instance* 返回L298N实例指针
 */
L298N_Instance *L298NInit(L298N_Init_Config_s *init_config)
{
    L298N_Instance *l298n = (L298N_Instance *)malloc(sizeof(L298N_Instance));
    if (l298n == NULL)
    {
        return NULL; // 内存分配失败
    }
    memset(l298n, 0, sizeof(L298N_Instance));

    Motor_Init_Config_s motor_config = {
        .controller_param_init_config = {},
        .controller_setting_init_config = {},
    };

    l298n->motor_a = MotorRegister(&motor_config);
    if (l298n->motor_a == NULL)
    {
        free(l298n);
        return NULL; // 电机A实例化失败
    }
    l298n->gpio_ena = GPIORegister(&init_config->gpio_ena_config);
    l298n->gpio_ena->timer_handle = init_config->gpio_ena_config.timer_handle;
    l298n->gpio_ena->timer_channel = init_config->gpio_ena_config.timer_channel;
    l298n->pwm_ena = init_config->pwm_ena;
    l298n->gpio_a1 = GPIORegister(&init_config->gpio_a1_config);
    l298n->gpio_a2 = GPIORegister(&init_config->gpio_a2_config);
    l298n->mode_a = init_config->mode_a;
    HAL_TIM_PWM_Start((TIM_HandleTypeDef *)l298n->gpio_ena->timer_handle, l298n->gpio_ena->timer_channel);

    l298n->motor_b = MotorRegister(&motor_config);
    if (l298n->motor_b == NULL)
    {
        free(l298n);
        return NULL; // 电机B实例化失败
    }
    l298n->gpio_enb = GPIORegister(&init_config->gpio_enb_config);
    l298n->gpio_enb->timer_handle = init_config->gpio_enb_config.timer_handle;
    l298n->gpio_enb->timer_channel = init_config->gpio_enb_config.timer_channel;
    l298n->pwm_enb = init_config->pwm_enb;
    l298n->gpio_b1 = GPIORegister(&init_config->gpio_b1_config);
    l298n->gpio_b2 = GPIORegister(&init_config->gpio_b2_config);
    l298n->mode_b = init_config->mode_b;
    HAL_TIM_PWM_Start((TIM_HandleTypeDef *)l298n->gpio_enb->timer_handle, l298n->gpio_enb->timer_channel);

    return l298n;
}

/**
 * @brief 设置L298N电机驱动板的工作模式
 *
 * @param l298n L298N实例指针
 * @param mode_a 电机A工作模式
 * @param mode_b 电机B工作模式
 */
void L298NControl(L298N_Instance *l298n, L298N_Motor_Mode_e mode_a, L298N_Motor_Mode_e mode_b)
{
    if (l298n == NULL)
    {
        return; // 检查L298N实例是否为NULL
    }

    l298n->mode_a = mode_a;
    l298n->mode_b = mode_b;

    // 设置电机A的工作模式
    switch (mode_a)
    {
    case L298N_STOP:
        GPIOReset(l298n->gpio_a1);
        GPIOReset(l298n->gpio_a2);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)l298n->gpio_ena->timer_handle, l298n->gpio_ena->timer_channel, 0);
        break;
    case L298N_FORWARD:
        GPIOSet(l298n->gpio_a1);
        GPIOReset(l298n->gpio_a2);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)l298n->gpio_ena->timer_handle, l298n->gpio_ena->timer_channel, l298n->pwm_ena);
        break;
    case L298N_BACKWARD:
        GPIOReset(l298n->gpio_a1);
        GPIOSet(l298n->gpio_a2);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)l298n->gpio_ena->timer_handle, l298n->gpio_ena->timer_channel, l298n->pwm_ena);
        break;
    }

    // 设置电机B的工作模式
    switch (mode_b)
    {
    case L298N_STOP:
        GPIOReset(l298n->gpio_b1);
        GPIOReset(l298n->gpio_b2);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)l298n->gpio_enb->timer_handle, l298n->gpio_enb->timer_channel, 0);
        break;
    case L298N_FORWARD:
        GPIOSet(l298n->gpio_b1);
        GPIOReset(l298n->gpio_b2);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)l298n->gpio_enb->timer_handle, l298n->gpio_enb->timer_channel, l298n->pwm_enb);
        break;
    case L298N_BACKWARD:
        GPIOReset(l298n->gpio_b1);
        GPIOSet(l298n->gpio_b2);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)l298n->gpio_enb->timer_handle, l298n->gpio_enb->timer_channel, l298n->pwm_enb);
        break;
    }
}