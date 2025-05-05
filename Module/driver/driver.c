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

    driver->mtype = init_config->mtype;
    driver->deadzone = init_config->deadzone;
    driver->mline = init_config->mline;
    driver->mphase = init_config->mphase;

    driver->usart = USARTRegister(&init_config->usart_config);

    for (int i = 0; i < MOTOR_CNT; ++i)
    {
        driver->motor[i] = init_config->motor[i];
    }

    return driver;
}

/**
 * @brief Callback function to process received motor data.
 *        Parses strings like "$MAll:M1,M2,M3,M4#" or "$MTEP:M1,M2,M3,M4#".
 * @note This function currently only stores the M1 value into the provided
 *       motor_instance. You need to adapt this based on how you manage
 *       data for the four motors.
 * @param Driver_instance The driver instance associated with this callback.
 */
void DriverCallback(Driver_Instance *driver_instance)
{
    if (!driver_instance || !driver_instance->usart)
    {
        return; // Safety check
    }

    uint8_t *rx_buf = driver_instance->usart->recv_buff;
    uint8_t rx_len = driver_instance->usart->recv_buff_size;

    // Ensure buffer is null-terminated for string functions
    // Make sure USART_RXBUFF_LIMIT is large enough to accommodate the null terminator
    if (rx_len < USART_RXBUFF_LIMIT)
    {
        rx_buf[rx_len] = '\0';
    }
    else
    {
        rx_buf[USART_RXBUFF_LIMIT - 1] = '\0'; // Null-terminate at the end if full
    }

    int m1, m2, m3, m4;

    // Check for Total Encoder Data: "$MAll:M1,M2,M3,M4#"
    if (strncmp((char *)rx_buf, "$MAll:", 6) == 0)
    {
        int parsed_count = sscanf((char *)rx_buf, "$MAll:%d,%d,%d,%d#", &m1, &m2, &m3, &m4);
        if (parsed_count == 4)
        {
            // Successfully parsed 4 values
            driver_instance->motor[0]->measure.total_ecd = (uint32_t)m1;
            driver_instance->motor[1]->measure.total_ecd = (uint32_t)m2;
            driver_instance->motor[2]->measure.total_ecd = (uint32_t)m3;
            driver_instance->motor[3]->measure.total_ecd = (uint32_t)m4;
        }
    }
    // Check for Real-time Encoder Data: "$MTEP:M1,M2,M3,M4#"
    else if (strncmp((char *)rx_buf, "$MTEP:", 6) == 0)
    {
        int parsed_count = sscanf((char *)rx_buf, "$MTEP:%d,%d,%d,%d#", &m1, &m2, &m3, &m4);
        if (parsed_count == 4)
        {
            // Successfully parsed 4 values
            driver_instance->motor[0]->measure.last_ecd = driver_instance->motor[0]->measure.ecd;
            driver_instance->motor[1]->measure.last_ecd = driver_instance->motor[1]->measure.ecd;
            driver_instance->motor[2]->measure.last_ecd = driver_instance->motor[2]->measure.ecd;
            driver_instance->motor[3]->measure.last_ecd = driver_instance->motor[3]->measure.ecd;

            driver_instance->motor[0]->measure.ecd = (uint16_t)m1;
            driver_instance->motor[1]->measure.ecd = (uint16_t)m2;
            driver_instance->motor[2]->measure.ecd = (uint16_t)m3;
            driver_instance->motor[3]->measure.ecd = (uint16_t)m4;
        }
    }

    // Optional: Clear buffer or reset size if needed after processing
    memset(driver_instance->usart->recv_buff, 0, driver_instance->usart->recv_buff_size);
    driver_instance->usart->recv_buff_size = 0;
}
