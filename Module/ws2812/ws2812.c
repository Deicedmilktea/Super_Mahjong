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

    // It's crucial that the HAL_TIM_PWM_PulseFinishedCallback is correctly routed.
    // This often involves defining HAL_TIM_PWM_PulseFinishedCallback in your stm32f4xx_it.c or similar
    // and then calling this module's WS2812_TIM_PWM_PulseFinishedCallback if the htim matches.
    // For example, in your HAL_TIM_PWM_PulseFinishedCallback:
    // if (htim->Instance == TIMx) { // TIMx being the timer used for WS2812
    //     WS2812_TIM_PWM_PulseFinishedCallback(htim);
    // }

    return ws;
}

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

void WS2812_Clear(WS2812_Instance *ws)
{
    if (!ws)
    {
        return;
    }
    WS2812_SetAllPixelsColor(ws, 0, 0, 0);
}

void WS2812_SetBrightness(WS2812_Instance *ws, uint8_t brightness)
{
    if (!ws)
    {
        return;
    }
    ws->brightness = brightness;
}

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
                ws->pwm_data_buffer[pwm_idx++] = WS2812_PWM_HIGH_BIT;
            }
            else // If bit is 0
            {
                ws->pwm_data_buffer[pwm_idx++] = WS2812_PWM_LOW_BIT;
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
