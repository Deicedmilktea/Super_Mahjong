#ifndef LED_H
#define LED_H

#define WS2812_LED_NUM 32
#define WS2812_LED_EACH_PLAYER 8

void LEDInit(void);
void LEDTask(void);

#endif // !LED_H