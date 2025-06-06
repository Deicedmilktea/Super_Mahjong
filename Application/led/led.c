#include "led.h"
#include "ws2812.h"
#include "tim.h"
#include "bsp_dwt.h"
#include "mahjong.h"

static WS2812_Instance *ws2812;          // WS2812实例
static GlobalGameState *global_game_led; // 全局游戏状态

static uint32_t last_time = 0;

void LEDInit(void)
{
    // WS2812初始化
    ws2812 = WS2812_Init(&htim3, TIM_CHANNEL_1, &hdma_tim3_ch1_trig, WS2812_LED_NUM);
}

void LEDTask(void)
{
    global_game_led = get_global_game_state(); // 获取全局游戏状态

    if (global_game_led == NULL)
        return; // 如果全局游戏状态为空，直接返回

    if (global_game_led->global_phase == PHASE_ERROR)
    {
        // 错误状态 - 所有LED红色常亮
        WS2812_SetAllPixelsColor(ws2812, 255, 0, 0); // 红色
        WS2812_Show(ws2812);                         // 显示更新
    }

    else if (global_game_led->global_phase == PHASE_IDLE || global_game_led->global_phase == PHASE_OVER)
    {
        // 流水灯特效 - 通过亮度渐变实现
        if (HAL_GetTick() - last_time >= 50) // 流水灯每50ms更新一次
        {
            // 使用紫红色作为主色，通过亮度渐变实现流水灯效果
            WS2812_WaterFlow(ws2812, 255, 0, 255, 6, WS2812_LED_NUM - 1, 128, 0);
            last_time = HAL_GetTick();
        }
    }

    else
    {
        // 呼吸灯特效 - 通过亮度渐变实现
        if (HAL_GetTick() - last_time >= 10) // 呼吸灯每10ms更新一次
        {
            // 使用紫红色作为主色，通过亮度渐变实现呼吸灯效果
            WS2812_Breathing(ws2812, 255, 0, 255,
                             global_game_led->current_player * WS2812_LED_EACH_PLAYER,
                             global_game_led->current_player * WS2812_LED_EACH_PLAYER + WS2812_LED_EACH_PLAYER - 1,
                             1, 128, 0);
            last_time = HAL_GetTick();
        }
    }
}
