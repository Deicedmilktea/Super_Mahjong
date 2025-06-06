#include "led.h"
#include "ws2812.h"
#include "tim.h"
#include "bsp_dwt.h"
#include "mahjong.h"

static WS2812_Instance *ws2812;          // WS2812实例
static GlobalGameState *global_game_led; // 全局游戏状态

void LEDInit(void)
{
    // WS2812初始化
    ws2812 = WS2812_Init(&htim3, TIM_CHANNEL_1, &hdma_tim3_ch1_trig, WS2812_LED_NUM);
}

void LEDTask(void)
{
    global_game_led = get_global_game_state(); // 获取全局游戏状态

    // 流水灯特效 - 通过亮度渐变实现
    static uint32_t last_update = 0;
    if (HAL_GetTick() - last_update >= 10) // 每10ms更新一次
    {
        // 使用紫红色作为主色，通过亮度渐变实现流水灯效果
        WS2812_WaterFlow(ws2812, 255, 0, 255, WS2812_LED_NUM / 2, 128, 0);
        last_update = HAL_GetTick();
    }
}
