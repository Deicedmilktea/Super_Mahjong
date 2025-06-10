#include "ai.h"
#include "crc.h"

#define MAX_AI_INSTANCES 3 // Maximum number of AI modules
static AI_Instance *ai_instances[MAX_AI_INSTANCES] = {NULL};
static uint8_t ai_instance_count = 0;

/**
 * @brief Initializes an AI_Instance.
 * @param init_config Pointer to the AI_Init_Config_s containing initialization parameters.
 * @return Pointer to the initialized AI_Instance, or NULL on failure.
 */
AI_Instance *AIInit(AI_Init_Config_s *init_config)
{
    if (ai_instance_count >= MAX_AI_INSTANCES || !init_config)
    {
        return NULL; // Max instances reached or invalid config
    }

    AI_Instance *ai_instance = (AI_Instance *)malloc(sizeof(AI_Instance));
    memset(ai_instance, 0, sizeof(AI_Instance));

    ai_instance->usart = USARTRegister(&init_config->usart_config);
    if (!ai_instance->usart)
    {
        free(ai_instance);
        return NULL; // USART registration failed
    }

    ai_instances[ai_instance_count++] = ai_instance;
    return ai_instance;
}

/**
 * @brief USART callback function for AI data.
 *        This function is called when data is received on the AI's USART.
 * @param _usart_instance Pointer to the USART_Instance that triggered the callback.
 */
void AICallback(USART_Instance *_usart_instance)
{
    if (!_usart_instance || !_usart_instance->id)
    {
        return; // Basic validation
    }

    AI_Instance *ai_instance = (AI_Instance *)_usart_instance->id;
    uint8_t *rx_buf = ai_instance->usart->recv_buff;
    uint16_t rx_len = ai_instance->usart->data_len;

    if (rx_len < sizeof(AI_Receive_s) || rx_buf[0] != AI_RECV_HEADER || rx_buf[rx_len - 1] != CRC16_CCITT(rx_buf, rx_len - 2))
    {
        return; // Invalid data length or header
    }

    ai_instance->ai_recv = *(AI_Receive_s *)rx_buf;
    if (ai_instance->ai_recv.index != ai_instance->last_index)
    {
        ai_instance->is_recv = 1;                              // Set received flag
        ai_instance->last_index = ai_instance->ai_recv.index; // Update last index
    }
}

/**
 * @brief Sends data to the AI module.
 * @param ai_instance Pointer to the AI_Instance.
 * @param data_to_send Pointer to the AI_Send_s structure to send.
 */
void AISendData(AI_Instance *ai_instance, AI_Send_s *data_to_send)
{
    if (!ai_instance || !ai_instance->usart || !data_to_send)
    {
        return; // Invalid parameters
    }

    USARTSend(ai_instance->usart, (uint8_t *)data_to_send, sizeof(AI_Send_s), USART_TRANSFER_BLOCKING);
}