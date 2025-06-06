#include "ws2812.h"

#define WS2812_MAX_INSTANCES 1
static WS2812_Instance *ws2812_instances[WS2812_MAX_INSTANCES] = {NULL};
static uint8_t ws2812_instance_idx = 0;

WS2812_Instance *WS2812_Init(TIM_HandleTypeDef *htim, uint32_t channel, DMA_HandleTypeDef *hdma_tim_up, uint16_t num_leds)
{
    if (ws2812_instance_idx >= WS2812_MAX_INSTANCES)
    {
        // Max instances reached
        return NULL;
    }

    WS2812_Instance *ws = (WS2812_Instance *)malloc(sizeof(WS2812_Instance));
    if (!ws)
    {
        return NULL; // Malloc failed
    }
    memset(ws, 0, sizeof(WS2812_Instance));

    ws->htim = htim;
    ws->tim_channel = channel;
    ws->hdma_tim_up = hdma_tim_up; // Assuming this DMA handle is for the TIM Update event or the specific CC channel
    ws->num_leds = num_leds;
    ws->brightness = 255;      // Default to full brightness
    ws->transfer_complete = 1; // Initially, no transfer is in progress

    // Each LED requires 24 bits (8 bits for G, 8 for R, 8 for B).
    // Each bit is one PWM pulse.
    ws->pwm_buffer_size = (num_leds * 24) + WS2812_RESET_PULSES;
    ws->pwm_data_buffer = (uint16_t *)malloc(ws->pwm_buffer_size * sizeof(uint16_t));
    if (!ws->pwm_data_buffer)
    {
        free(ws);
        return NULL; // Malloc failed
    }
    memset(ws->pwm_data_buffer, 0, ws->pwm_buffer_size * sizeof(uint16_t));

    // LED data buffer stores GRB values for each LED
    ws->led_data_buffer = (uint8_t *)malloc(num_leds * 3 * sizeof(uint8_t));
    if (!ws->led_data_buffer)
    {
        free(ws->pwm_data_buffer);
        free(ws);
        return NULL; // Malloc failed
    }
    memset(ws->led_data_buffer, 0, num_leds * 3 * sizeof(uint8_t)); // Clear all LEDs initially

    ws2812_instances[ws2812_instance_idx++] = ws;

    return ws;
}

/**
 * @brief Set the color of a single pixel in the WS2812 LED strip.
 * @param ws Pointer to the WS2812 instance.
 * @param pixel_n The pixel index to set (0-based).
 * @param r Red color value (0-255).
 * @param g Green color value (0-255).
 * @param b Blue color value (0-255).
 */
void WS2812_SetPixelColor(WS2812_Instance *ws, uint16_t pixel_n, uint8_t r, uint8_t g, uint8_t b)
{
    if (!ws || pixel_n >= ws->num_leds)
    {
        return;
    }
    // WS2812 typically expects GRB order
    ws->led_data_buffer[pixel_n * 3 + 0] = g;
    ws->led_data_buffer[pixel_n * 3 + 1] = r;
    ws->led_data_buffer[pixel_n * 3 + 2] = b;
}

/**
 * @brief Set the color of all pixels in the WS2812 LED strip.
 * @param ws Pointer to the WS2812 instance.
 * @param r Red color value (0-255).
 * @param g Green color value (0-255).
 * @param b Blue color value (0-255).
 */
void WS2812_SetAllPixelsColor(WS2812_Instance *ws, uint8_t r, uint8_t g, uint8_t b)
{
    if (!ws)
    {
        return;
    }
    for (uint16_t i = 0; i < ws->num_leds; ++i)
    {
        WS2812_SetPixelColor(ws, i, r, g, b);
    }
}

/**
 * @brief Clear all pixels in the WS2812 LED strip (set to black).
 * @param ws Pointer to the WS2812 instance.
 * This function sets all pixels to black (0, 0, 0).
 * It is equivalent to turning off all LEDs.
 */
void WS2812_Clear(WS2812_Instance *ws)
{
    if (!ws)
    {
        return;
    }
    WS2812_SetAllPixelsColor(ws, 0, 0, 0);
}

/**
 * @brief Set the brightness of the WS2812 LED strip.
 * @param ws Pointer to the WS2812 instance.
 * @param brightness Brightness value (0-255).
 * 0 is off, 255 is full brightness.
 */
void WS2812_SetBrightness(WS2812_Instance *ws, uint8_t brightness)
{
    if (!ws)
    {
        return;
    }
    ws->brightness = brightness;
}

/**
 * @brief Show the current LED data on the WS2812 strip.
 * This function prepares the PWM data for the WS2812 LEDs
 * and starts the DMA transfer to send the data to the LEDs.
 */
void WS2812_Show(WS2812_Instance *ws)
{
    if (!ws || !ws->transfer_complete)
    {
        return; // Previous transfer not finished or instance invalid
    }
    ws->transfer_complete = 0; // Mark transfer as started

    uint32_t pwm_idx = 0;
    uint8_t r, g, b;

    for (uint16_t i = 0; i < ws->num_leds; ++i)
    {
        // Apply brightness: color_component * brightness / 255
        // WS2812 expects GRB order
        g = (ws->led_data_buffer[i * 3 + 0] * ws->brightness) >> 8; // Fast division by 256
        r = (ws->led_data_buffer[i * 3 + 1] * ws->brightness) >> 8;
        b = (ws->led_data_buffer[i * 3 + 2] * ws->brightness) >> 8;

        uint32_t color_grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;

        // Convert 24-bit color to 24 PWM duty cycles
        for (int8_t j = 23; j >= 0; --j) // MSB first
        {
            if ((color_grb >> j) & 0x01) // If bit is 1
            {
                ws->pwm_data_buffer[pwm_idx++] = WS2812_PWM_HIGH_BIT; // High pulse for bit 1
            }
            else // If bit is 0
            {
                ws->pwm_data_buffer[pwm_idx++] = WS2812_PWM_LOW_BIT; // Low pulse for bit 0
            }
        }
    }

    // Add reset pulses (zeros)
    for (uint16_t i = 0; i < WS2812_RESET_PULSES; ++i)
    {
        ws->pwm_data_buffer[pwm_idx++] = 0;
    }

    // Assuming TIM_CCx DMA is configured for the channel:
    if (HAL_TIM_PWM_Start_DMA(ws->htim, ws->tim_channel, (uint32_t *)ws->pwm_data_buffer, ws->pwm_buffer_size) != HAL_OK)
    {
        ws->transfer_complete = 1; // Error starting DMA, allow next transfer
        // Handle error, e.g., log or assert
    }
}

// This function should be called from the global HAL_TIM_PWM_PulseFinishedCallback
// when the DMA transfer for the WS2812 timer channel is complete.
void WS2812_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    for (uint8_t i = 0; i < ws2812_instance_idx; ++i)
    {
        if (ws2812_instances[i] && ws2812_instances[i]->htim->Instance == htim->Instance)
        {
            // Check if the callback is for the correct channel if the timer has multiple PWM channels in use.
            // This check might be more complex if the DMA is on TIM_UP and not TIM_CCx.
            // For TIM_CCx DMA, htim->ChannelActive will indicate the channel.
            // However, PulseFinishedCallback is often for the whole period (all data sent).

            HAL_TIM_PWM_Stop_DMA(ws2812_instances[i]->htim, ws2812_instances[i]->tim_channel);
            ws2812_instances[i]->transfer_complete = 1;
            break;
        }
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    // Example: If TIM3 is used for WS2812 on Channel 1
    if (htim->Instance == TIM3) // Replace TIM3 with your actual timer
    {
        WS2812_TIM_PWM_PulseFinishedCallback(htim);
    }
    // Add other timer DMA completion handlers if necessary
}

/*
Note on HAL_TIM_PWM_PulseFinishedCallback:
You need to ensure this function is called correctly.
In your stm32f4xx_it.c (or wherever HAL callbacks are defined), you'll have:

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
  // Example: If TIM3 is used for WS2812 on Channel 1
  // if (htim->Instance == TIM3) // Replace TIM3 with your actual timer
  // {
  //    WS2812_TIM_PWM_PulseFinishedCallback(htim);
  // }
  // Add other timer DMA completion handlers if necessary
}

Make sure the DMA for the timer (e.g., TIMx_CHy or TIMx_UP) is configured
and its interrupt is enabled in the NVIC. The DMA should be in Normal mode, not Circular.
The timer's PWM period (ARR) and the WS2812_PWM_HIGH_BIT/LOW_BIT values must be
carefully calculated based on your system clock and the WS2812 datasheet timings.
*/

/**
 * @brief Implements a brightness-based flowing water light effect on the WS2812 strip.
 * @param ws Pointer to the WS2812_Instance.
 * @param r Red component (0-255).
 * @param g Green component (0-255).
 * @param b Blue component (0-255).
 * @param start_pos Starting position of the effect (inclusive).
 * @param end_pos Ending position of the effect (inclusive).
 * @param max_brightness Maximum brightness for the lit LEDs (0-255).
 * @param min_brightness Minimum brightness for the dim LEDs (0-255).
 */
void WS2812_WaterFlow(WS2812_Instance *ws, uint8_t r, uint8_t g, uint8_t b,
                      uint16_t start_pos, uint16_t end_pos, uint8_t max_brightness, uint8_t min_brightness)
{
    if (!ws || ws->num_leds == 0)
    {
        return;
    }

    // 验证起始和结束位置的有效性
    if (start_pos >= ws->num_leds || end_pos >= ws->num_leds || start_pos > end_pos)
    {
        return;
    }

    // 计算有效LED段的长度
    uint16_t segment_length = end_pos - start_pos + 1;

    // 存储当前动画位置的静态变量
    static uint16_t current_pos = 0;

    // 清除所有LED
    WS2812_Clear(ws);

    // 为指定范围内的LED设置颜色和亮度
    for (uint16_t i = start_pos; i <= end_pos; ++i)
    {
        // 计算当前LED与动画位置的相对位置
        int16_t normalized_pos = (current_pos - start_pos + segment_length) % segment_length;
        int16_t relative_pos = (i - start_pos + segment_length) % segment_length;
        int16_t distance = (relative_pos - normalized_pos + segment_length) % segment_length;

        // 计算每个LED的亮度，随距离递减
        uint8_t brightness;
        if (distance <= segment_length / 2) // 前半部分
        {
            brightness = max_brightness - ((max_brightness - min_brightness) * distance) / (segment_length / 2);
        }
        else // 后半部分，保持最小亮度
        {
            brightness = min_brightness;
        }

        // 设置LED颜色，应用计算出的亮度
        WS2812_SetPixelColor(ws, i,
                             (r * brightness) / 255,
                             (g * brightness) / 255,
                             (b * brightness) / 255);
    }

    // 更新动画位置
    current_pos = (current_pos + 1) % segment_length;

    // 显示更新后的LED状态
    WS2812_Show(ws);

    // 等待DMA传输完成
    while (!ws->transfer_complete)
    {
        // 可以在这里添加超时处理或在RTOS任务中使用yield
    }
}

/**
 * @brief 在指定范围内实现呼吸灯效果
 */
void WS2812_Breathing(WS2812_Instance *ws, uint8_t r, uint8_t g, uint8_t b,
                      uint16_t start_pos, uint16_t end_pos,
                      uint8_t speed, uint8_t max_brightness, uint8_t min_brightness)
{
    if (!ws || ws->num_leds == 0)
    {
        return;
    }

    // 验证起始和结束位置的有效性
    if (start_pos >= ws->num_leds || end_pos >= ws->num_leds || start_pos > end_pos)
    {
        return;
    }

    // 确保亮度范围有效
    if (max_brightness < min_brightness)
    {
        uint8_t temp = max_brightness;
        max_brightness = min_brightness;
        min_brightness = temp;
    }

    // 确保速度不为0
    if (speed == 0)
    {
        speed = 1;
    }

    // 使用静态变量记录呼吸过程
    static uint8_t breath_value = 0;
    static int8_t breath_direction = 1; // 1表示变亮，-1表示变暗

    // 清除所有LED
    WS2812_Clear(ws);

    // 计算当前呼吸值
    breath_value += breath_direction * speed;

    // 检查是否需要改变方向
    if (breath_value >= max_brightness)
    {
        breath_value = max_brightness;
        breath_direction = -1;
    }
    else if (breath_value <= min_brightness)
    {
        breath_value = min_brightness;
        breath_direction = 1;
    }

    // 设置指定范围内LED的颜色和亮度
    for (uint16_t i = start_pos; i <= end_pos; i++)
    {
        WS2812_SetPixelColor(ws, i,
                             (r * breath_value) / 255,
                             (g * breath_value) / 255,
                             (b * breath_value) / 255);
    }

    // 显示更新后的LED状态
    WS2812_Show(ws);

    // 等待DMA传输完成
    while (!ws->transfer_complete)
    {
        // 可以在这里添加超时处理或在RTOS任务中使用yield
    }
}
