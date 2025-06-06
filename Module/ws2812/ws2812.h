#ifndef WS2812_H
#define WS2812_H

#include "stm32f4xx_hal.h" // Assuming STM32F4 series, adjust if different
#include <stdint.h>
#include <stdlib.h> // For malloc/free
#include <string.h> // For memset

// Define these values based on your specific timer clock and desired WS2812 timings
// Assuming an 800kHz data rate for WS2812.
// If APB1 Timer Clock is, for example, 84MHz, then Timer Period (ARR) could be 104 (84MHz / 800kHz = 105, so ARR=104 for 0-104).
// T0H: 0.4us  (approx. 33 PWM counts if ARR is 104)
// T0L: 0.85us (approx. 71 PWM counts)
// T1H: 0.8us  (approx. 67 PWM counts if ARR is 104)
// T1L: 0.45us (approx. 38 PWM counts)
// These are just examples and MUST be tuned.
#define WS2812_PWM_HIGH_BIT (67) // Example value for '1' bit (duty cycle for T1H)
#define WS2812_PWM_LOW_BIT (33)  // Example value for '0' bit (duty cycle for T0H)
#define WS2812_RESET_PULSES (50) // Number of zero-duty pulses for reset (min 50us)

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} WS2812_RGB_Color;

typedef struct
{
    TIM_HandleTypeDef *htim;        // Timer handle for PWM generation
    uint32_t tim_channel;           // Timer channel (e.g., TIM_CHANNEL_1)
    DMA_HandleTypeDef *hdma_tim_up; // DMA handle for Timer Update event (if used for PWM data transfer)
                                    // Or specific DMA channel for TIM_CCx if preferred.
                                    // For simplicity, using TIM_UP often works well.

    uint16_t num_leds;  // Number of LEDs in the strip
    uint8_t brightness; // Global brightness (0-255)

    uint8_t *led_data_buffer;  // Buffer for GRB color data (num_leds * 3 bytes)
    uint16_t *pwm_data_buffer; // Buffer for DMA transfer (num_leds * 24 bits + reset pulses)
    uint32_t pwm_buffer_size;  // Size of pwm_data_buffer in elements

    volatile uint8_t transfer_complete; // Flag to indicate DMA transfer completion
} WS2812_Instance;

/**
 * @brief Initializes a WS2812 LED strip instance.
 * @param htim Pointer to the TIM_HandleTypeDef for PWM generation.
 * @param channel The TIM channel to use (e.g., TIM_CHANNEL_1).
 * @param hdma Pointer to the DMA_HandleTypeDef for the timer's update or CC event.
 * @param num_leds Number of LEDs in the strip.
 * @return Pointer to the initialized WS2812_Instance, or NULL on failure.
 */
WS2812_Instance *WS2812_Init(TIM_HandleTypeDef *htim, uint32_t channel, DMA_HandleTypeDef *hdma_tim_up, uint16_t num_leds);

/**
 * @brief Sets the color of a single pixel.
 * @param ws Pointer to the WS2812_Instance.
 * @param pixel_n Index of the pixel to set.
 * @param r Red component (0-255).
 * @param g Green component (0-255).
 * @param b Blue component (0-255).
 */
void WS2812_SetPixelColor(WS2812_Instance *ws, uint16_t pixel_n, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Sets the color of all pixels to the same color.
 * @param ws Pointer to the WS2812_Instance.
 * @param r Red component (0-255).
 * @param g Green component (0-255).
 * @param b Blue component (0-255).
 */
void WS2812_SetAllPixelsColor(WS2812_Instance *ws, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Clears all LEDs (sets them to black/off).
 * @param ws Pointer to the WS2812_Instance.
 */
void WS2812_Clear(WS2812_Instance *ws);

/**
 * @brief Sends the current color data to the LED strip.
 * @param ws Pointer to the WS2812_Instance.
 */
void WS2812_Show(WS2812_Instance *ws);

/**
 * @brief Sets the global brightness for the LED strip.
 * @param ws Pointer to the WS2812_Instance.
 * @param brightness Brightness value (0-255).
 */
void WS2812_SetBrightness(WS2812_Instance *ws, uint8_t brightness);

/**
 * @brief DMA transfer complete callback (to be called from HAL_TIM_PWM_PulseFinishedCallback).
 * @param htim Pointer to the TIM_HandleTypeDef.
 */
void WS2812_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim);

/**
 * @brief Implements a flowing water light effect on the WS2812 strip.
 * @param ws Pointer to the WS2812_Instance.
 * @param r Red component (0-255) for the flowing light.
 * @param g Green component (0-255) for the flowing light.
 * @param b Blue component (0-255) for the flowing light.
 * @param r Red component (0-255) for the base color.
 * @param g Green component (0-255) for the base color.
 * @param b Blue component (0-255) for the base color.
 * @param flow_length Number of LEDs to be part of the 'lit' section of the flow.
 * @param max_brightness Maximum brightness for the lit LEDs (0-255).
 * @param min_brightness Minimum brightness for the dim LEDs (0-255).
 */
void WS2812_WaterFlow(WS2812_Instance *ws, uint8_t r, uint8_t g, uint8_t b, uint8_t flow_length, uint8_t max_brightness, uint8_t min_brightness);

#endif // !WS2812_H
