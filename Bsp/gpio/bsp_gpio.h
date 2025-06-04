#include "gpio.h"
#include "stdint.h"

#define GPIO_MX_DEVICE_NUM 20

/**
 * @brief 用于判断中断来源,注意和CUBEMX中配置一致
 *
 */
typedef enum
{
    GPIO_EXTI_MODE_RISING,
    GPIO_EXTI_MODE_FALLING,
    GPIO_EXTI_MODE_RISING_FALLING,
    GPIO_EXTI_MODE_NONE,
} GPIO_EXTI_MODE_e;

/**
 * @brief GPIO实例结构体定义
 *
 */
typedef struct tmpgpio
{
    GPIO_TypeDef *GPIOx;             // GPIOA,GPIOB,GPIOC...
    GPIO_PinState pin_state;         // 引脚状态,Set,Reset;not frequently used
    GPIO_EXTI_MODE_e exti_mode;      // 外部中断模式 not frequently used
    uint16_t GPIO_Pin;               // 引脚号,
    TIM_HandleTypeDef *timer_handle; // 计时器句柄,用于定时器触发的GPIO
    uint16_t timer_channel;          // 计时器通道,用于定时器触发的GPIO
    // 这些引脚是stm32f4xx_hal_gpio.h中定义的宏!!! 一定要注意
    // 随便取个名字当临时声明
    void (*gpio_model_callback)(struct tmpgpio *); // exti中断回调函数
    void *id;                                      // 区分不同的GPIO实例

} GPIO_Instance;

/**
 * @brief GPIO初始化配置结构体定义
 *
 */
typedef struct
{
    GPIO_TypeDef *GPIOx;             // GPIOA,GPIOB,GPIOC...
    GPIO_PinState pin_state;         // 引脚状态,Set,Reset not frequently used
    GPIO_EXTI_MODE_e exti_mode;      // 外部中断模式 not frequently used
    uint16_t GPIO_Pin;               // 引脚号,@note 这里的引脚号是GPIO_PIN_0,GPIO_PIN_1...
    TIM_HandleTypeDef *timer_handle; // 计时器句柄,用于定时器触发的GPIO
    uint16_t timer_channel;          // 计时器通道,用于定时器触发的GPIO
    // 这些引脚是stm32f4xx_hal_gpio.h中定义的宏!!! 一定要注意

    void (*gpio_model_callback)(GPIO_Instance *); // exti中断回调函数
    void *id;                                     // 区分不同的GPIO实例

} GPIO_Init_Config_s;

/**
 * @brief 注册GPIO实例
 *
 * @param GPIO_config
 * @return GPIO_Instance*
 */
GPIO_Instance *GPIORegister(GPIO_Init_Config_s *GPIO_config);

/**
 * @brief GPIO API,切换GPIO电平
 *
 * @param _instance
 */
void GPIOToggle(GPIO_Instance *_instance);

/**
 * @brief 设置GPIO电平
 *
 * @param _instance
 */
void GPIOSet(GPIO_Instance *_instance);

/**
 * @brief 复位GPIO电平
 *
 * @param _instance
 */
void GPIOReset(GPIO_Instance *_instance);

/**
 * @brief 读取GPIO电平
 *
 * @param _instance
 * @return GPIO_PinState
 */
GPIO_PinState GPIORead(GPIO_Instance *_instance);