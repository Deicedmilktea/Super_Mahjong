#include "driver.h"
#include <string.h> // Added for string functions
#include <stdio.h>  // Added for sscanf

/***
 * @brief initialize driver instance
 */
Driver_Instance *DriverInit(Driver_Init_Config_s *init_config)
{
    Driver_Instance *driver = (Driver_Instance *)malloc(sizeof(Driver_Instance));
    memset(driver, 0, sizeof(Driver_Instance));

    init_config->usart_config = (USART_Init_Config_s){
        .recv_buff_size = USART_RXBUFF_LIMIT,
        .usart_handle = &huart2,
        .id = driver,
        .usart_module_callback = DriverCallback,
    };

    // 注册usart实例
    driver->usart = USARTRegister(&init_config->usart_config);

    for (int i = 0; i < MOTOR_CNT; ++i)
    {
        driver->motor[i] = init_config->motor[i];
    }

    return driver;
}

void DriverCallback(USART_Instance *_usart_instance)
{
    Driver_Instance *driver_instance = (Driver_Instance *)_usart_instance->id;
    uint8_t *usart_rx_buf = driver_instance->usart->recv_buff;
    uint16_t rx_len = driver_instance->usart->data_len; // Actual length of received data from ISR

    // Basic validation:
    // 1. Data must exist (rx_len > 0).
    // 2. Actual received length (rx_len) must not exceed the capacity of the DMA buffer (USART_RXBUFF_LIMIT).
    //    If rx_len > USART_RXBUFF_LIMIT, it indicates a critical error (e.g., DMA misconfiguration or overflow).
    // 3. Message must end with '#' character.
    if (rx_len < 17 || rx_len > USART_RXBUFF_LIMIT || usart_rx_buf[0] != '$' || usart_rx_buf[rx_len - 3] != '#')
    {
        // Optionally, log an error or handle specific cases like rx_len > USART_RXBUFF_LIMIT.
        // Clearing driver_instance->usart->data_len might be done by the caller or USART service.
        return;
    }

    // Use a Variable Length Array (VLA) for the temporary buffer.
    // Sized by actual received length (rx_len) + 1 for the null terminator.
    // This requires C99 or a compiler extension (e.g., GCC).
    // USART_RXBUFF_LIMIT still acts as an upper bound for rx_len due to the check above.
    char temp_buf[rx_len + 1];

    // Copy the received data into the temporary buffer.
    memcpy(temp_buf, usart_rx_buf, rx_len);
    // Null-terminate the string in the temporary buffer for safe string operations.
    temp_buf[rx_len] = '\0';

    int m1, m2, m3, m4;

    // Process $MAll: type messages
    // Example: $MAll:0,0,1,0#
    if (strncmp(temp_buf, "$MAll:", 6) == 0)
    {
        int parsed_count = sscanf(temp_buf, "$MAll:%d,%d,%d,%d#", &m1, &m2, &m3, &m4);
        if (parsed_count == 4)
        {
            // MOTOR_CNT should be defined (e.g., in driver.h or a configuration file)
            // Ensure we don't write out of bounds for the motor array.
            if (MOTOR_CNT >= 4)
            {
                driver_instance->motor[0]->measure.total_ecd = m1;
                driver_instance->motor[1]->measure.total_ecd = m2;
                driver_instance->motor[2]->measure.total_ecd = m3;
                driver_instance->motor[3]->measure.total_ecd = m4;
            }
            // Message processed. The main USART buffer (usart_rx_buf) is typically cleared
            // by the USART service (e.g., in HAL_UARTEx_RxEventCallback after this callback returns).
            return; // Successfully processed $MAll:
        }
    }
    // Process $MTEP: type messages (retained from previous logic)
    // Example: $MTEP:123,456,789,101#
    else if (strncmp(temp_buf, "$MTEP:", 6) == 0)
    {
        int parsed_count = sscanf(temp_buf, "$MTEP:%d,%d,%d,%d#", &m1, &m2, &m3, &m4);
        if (parsed_count == 4)
        {
            if (MOTOR_CNT >= 4)
            {
                driver_instance->motor[0]->measure.last_ecd = driver_instance->motor[0]->measure.ecd;
                driver_instance->motor[1]->measure.last_ecd = driver_instance->motor[1]->measure.ecd;
                driver_instance->motor[2]->measure.last_ecd = driver_instance->motor[2]->measure.ecd;
                driver_instance->motor[3]->measure.last_ecd = driver_instance->motor[3]->measure.ecd;

                driver_instance->motor[0]->measure.ecd = m1;
                driver_instance->motor[1]->measure.ecd = m2;
                driver_instance->motor[2]->measure.ecd = m3;
                driver_instance->motor[3]->measure.ecd = m4;
            }
            return; // Successfully processed $MTEP:
        }
    }

    // If the message was not recognized or parsing failed for a recognized type,
    // it will fall through. The USART buffer will be cleared by the calling service.
    // No explicit action needed here unless specific error handling for malformed known types is required.
}
