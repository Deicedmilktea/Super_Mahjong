#include "at8236.h"
#include "tim.h"

/**
 * @brief AT8236电机驱动板实例化函数
 *
 * @param init_config 初始化配置
 * @return AT8236_Instance* 返回AT8236实例指针
 */
AT8236_Instance *AT8236Init(AT8236_Init_Config_s *init_config)
{
    AT8236_Instance *at8236 = (AT8236_Instance *)malloc(sizeof(AT8236_Instance));
    if (at8236 == NULL)
    {
        return NULL; // 内存分配失败
    }
    memset(at8236, 0, sizeof(AT8236_Instance));

    Motor_Init_Config_s motor_config = {
        .controller_param_init_config = {},
        .controller_setting_init_config = {},
    };

    at8236->motor_a = MotorRegister(&motor_config);
    if (at8236->motor_a == NULL)
    {
        free(at8236);
        return NULL; // 电机A实例化失败
    }
    at8236->pwm_ena = init_config->pwm_ena;
    at8236->gpio_a1 = GPIORegister(&init_config->gpio_a1_config);
    at8236->gpio_a2 = GPIORegister(&init_config->gpio_a2_config);
    at8236->mode_a = init_config->mode_a;
    HAL_TIM_PWM_Start((TIM_HandleTypeDef *)at8236->gpio_a1->timer_handle, at8236->gpio_a1->timer_channel);
    HAL_TIM_PWM_Start((TIM_HandleTypeDef *)at8236->gpio_a2->timer_handle, at8236->gpio_a2->timer_channel);

    at8236->motor_b = MotorRegister(&motor_config);
    if (at8236->motor_b == NULL)
    {
        free(at8236);
        return NULL; // 电机B实例化失败
    }
    at8236->pwm_enb = init_config->pwm_enb;
    at8236->gpio_b1 = GPIORegister(&init_config->gpio_b1_config);
    at8236->gpio_b2 = GPIORegister(&init_config->gpio_b2_config);
    at8236->mode_b = init_config->mode_b;
    HAL_TIM_PWM_Start((TIM_HandleTypeDef *)at8236->gpio_b1->timer_handle, at8236->gpio_b1->timer_channel);
    HAL_TIM_PWM_Start((TIM_HandleTypeDef *)at8236->gpio_b2->timer_handle, at8236->gpio_b2->timer_channel);

    return at8236;
}

/**
 * @brief 设置AT8236电机驱动板的工作模式
 *
 * @param at8236 AT8236实例指针
 * @param mode_a 电机A工作模式
 * @param mode_b 电机B工作模式
 */
void AT8236Control(AT8236_Instance *at8236, AT8236_Motor_Mode_e mode_a, AT8236_Motor_Mode_e mode_b)
{
    if (at8236 == NULL)
    {
        return; // 检查AT8236实例是否为NULL
    }

    // 更新电机A和B的工作模式
    at8236->mode_a = mode_a;
    at8236->mode_b = mode_b;

    // 控制电机A
    switch (mode_a)
    {
    case AT8236_STOP:
        // 电机A停止：两个PWM通道都设为0
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_a1->timer_handle,
                              at8236->gpio_a1->timer_channel, 0);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_a2->timer_handle,
                              at8236->gpio_a2->timer_channel, 0);
        break;

    case AT8236_FORWARD:
        // 电机A正转：A1输出PWM，A2输出0
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_a1->timer_handle,
                              at8236->gpio_a1->timer_channel, at8236->pwm_ena);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_a2->timer_handle,
                              at8236->gpio_a2->timer_channel, 0);
        break;

    case AT8236_BACKWARD:
        // 电机A反转：A1输出0，A2输出PWM
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_a1->timer_handle,
                              at8236->gpio_a1->timer_channel, 0);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_a2->timer_handle,
                              at8236->gpio_a2->timer_channel, at8236->pwm_ena);
        break;

    default:
        // 默认停止：两个PWM通道都设为0
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_a1->timer_handle,
                              at8236->gpio_a1->timer_channel, 0);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_a2->timer_handle,
                              at8236->gpio_a2->timer_channel, 0);
        break;
    }

    // 控制电机B
    switch (mode_b)
    {
    case AT8236_STOP:
        // 电机B停止：两个PWM通道都设为0
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_b1->timer_handle,
                              at8236->gpio_b1->timer_channel, 0);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_b2->timer_handle,
                              at8236->gpio_b2->timer_channel, 0);
        break;

    case AT8236_FORWARD:
        // 电机B正转：B1输出PWM，B2输出0
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_b1->timer_handle,
                              at8236->gpio_b1->timer_channel, at8236->pwm_enb);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_b2->timer_handle,
                              at8236->gpio_b2->timer_channel, 0);
        break;

    case AT8236_BACKWARD:
        // 电机B反转：B1输出0，B2输出PWM
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_b1->timer_handle,
                              at8236->gpio_b1->timer_channel, 0);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_b2->timer_handle,
                              at8236->gpio_b2->timer_channel, at8236->pwm_enb);
        break;

    default:
        // 默认停止：两个PWM通道都设为0
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_b1->timer_handle,
                              at8236->gpio_b1->timer_channel, 0);
        __HAL_TIM_SET_COMPARE((TIM_HandleTypeDef *)at8236->gpio_b2->timer_handle,
                              at8236->gpio_b2->timer_channel, 0);
        break;
    }
}