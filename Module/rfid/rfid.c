#include "rfid.h"
#include "usart.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_RFID_INSTANCES 1 // Maximum number of RFID modules
static RFID_Instance *rfid_instances[MAX_RFID_INSTANCES] = {NULL};
static uint8_t idx = 0;
static uint16_t last_draw_index = 0;    // 上次摸牌的索引
static uint16_t last_discard_index = 0; // 上次弃牌的索引

/**
 * @brief Initializes an RFID queue.
 * @param queue Pointer to the RFIDQueue to initialize.
 */
static void RFIDQueueInit(RFIDQueue *queue)
{
    if (!queue)
        return;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    memset(queue->tiles, 0, sizeof(queue->tiles));
}

/**
 * @brief Checks if the RFID queue is empty.
 * @param queue Pointer to the RFIDQueue.
 * @return 1 if empty, 0 otherwise.
 */
uint8_t RFIDQueueIsEmpty(RFIDQueue *queue)
{
    if (!queue)
        return 1; // Or handle error appropriately
    return queue->count == 0;
}

/**
 * @brief Checks if the RFID queue is full.
 * @param queue Pointer to the RFIDQueue.
 * @return 1 if full, 0 otherwise.
 */
uint8_t RFIDQueueIsFull(RFIDQueue *queue)
{
    if (!queue)
        return 0; // Or handle error appropriately
    return queue->count == RFID_BUFFER_SIZE;
}

/**
 * @brief Enqueues a tile into the RFID queue.
 * @param queue Pointer to the RFIDQueue.
 * @param tile The tile to enqueue.
 * @return 1 on success, 0 on failure (queue full or invalid).
 */
uint8_t RFIDEnqueue(RFIDQueue *queue, Tile tile)
{
    if (!queue || RFIDQueueIsFull(queue))
    {
        return 0; // Queue is full or invalid
    }
    queue->tiles[queue->tail] = tile;
    queue->tail = (queue->tail + 1) % RFID_BUFFER_SIZE;
    queue->count++;
    return 1;
}

/**
 * @brief Dequeues a tile from the RFID queue.
 * @param queue Pointer to the RFIDQueue.
 * @param tile Pointer to store the dequeued tile.
 * @return 1 on success, 0 on failure (queue empty or invalid).
 */
uint8_t RFIDDequeue(RFIDQueue *queue, Tile *tile)
{
    if (!queue || RFIDQueueIsEmpty(queue) || !tile)
    {
        return 0; // Queue is empty or invalid parameters
    }
    *tile = queue->tiles[queue->head];
    queue->head = (queue->head + 1) % RFID_BUFFER_SIZE;
    queue->count--;
    return 1;
}

/**
 * @brief Initializes an RFID_Instance instance.
 * @param usart_handle Pointer to the USART_HandleTypeDef for this RFID module.
 * @return Pointer to the initialized RFID_Instance instance, or NULL on failure.
 */
RFID_Instance *RFIDInit(RFID_Init_Config_s *init_config)
{
    if (idx >= MAX_RFID_INSTANCES)
    {
        return NULL; // Max instances reached
    }

    RFID_Instance *rfid_instance = (RFID_Instance *)malloc(sizeof(RFID_Instance));
    if (!rfid_instance)
    {
        return NULL; // Malloc failed
    }
    memset(rfid_instance, 0, sizeof(RFID_Instance));

    RFIDQueueInit(&rfid_instance->draw_tile_1);
    RFIDQueueInit(&rfid_instance->draw_tile_2);
    RFIDQueueInit(&rfid_instance->discard_tile);

    rfid_instance->usart = USARTRegister(&init_config->usart_config);
    if (!rfid_instance->usart)
    {
        free(rfid_instance);
        return NULL; // USART registration failed
    }

    rfid_instances[idx++] = rfid_instance;
    return rfid_instance;
}

/**
 * @brief USART callback function for RFID data.
 * @param _usart_instance Pointer to the USART_Instance that triggered the callback.
 */
void RFIDCallback(USART_Instance *_usart_instance)
{
    if (!_usart_instance || !_usart_instance->id)
    {
        return; // Basic validation: ensure usart_instance and its ID are valid
    }

    RFID_Instance *rfid_instance = (RFID_Instance *)_usart_instance->id;
    uint8_t *rx_buf = rfid_instance->usart->recv_buff;
    uint16_t rx_len = rfid_instance->usart->data_len;

    // Check if received data length is sufficient for RFID_Receive_Data_s
    if (rx_len != sizeof(RFID_Receive_Data_s) || rx_buf[0] != RFID_DATA_HEAD)
    {
        return;
    }

    // Copy the received buffer into a local variable to ensure alignment and avoid modifying the buffer directly
    RFID_Receive_Data_s received_data;
    memcpy(&received_data, rx_buf, sizeof(RFID_Receive_Data_s));

    // Process the received data based on the action
    switch (received_data.action)
    {
    case ACTION_DRAW:
        if (received_data.index > last_draw_index)
        {
            Tile tile;
            tile.type = received_data.type;
            tile.value = received_data.value;
            if (received_data.channel == 1)
            {
                RFIDEnqueue(&rfid_instance->draw_tile_1, tile);
            }
            else if (received_data.channel == 2)
            {
                RFIDEnqueue(&rfid_instance->draw_tile_2, tile);
            }
            else
            {
                // Invalid channel, handle error if necessary
                return;
            }
            last_draw_index = received_data.index;
        }
        break;
    case ACTION_DISCARD:
        if (received_data.index > last_discard_index)
        {
            Tile tile;
            tile.type = received_data.type;
            tile.value = received_data.value;
            RFIDEnqueue(&rfid_instance->discard_tile, tile);
            last_discard_index = received_data.index;
        }
        break;
    default:
        break;
    }
}

/**
 * @brief Gets the count of currently registered RFID instances.
 * @return Number of RFID instances.
 */
uint8_t GetRFIDInstanceCount()
{
    return idx;
}

/**
 * @brief Gets a pointer to a registered RFID instance by index.
 * @param index The index of the RFID instance.
 * @return Pointer to RFID_Instance instance or NULL if index is out of bounds.
 */
RFID_Instance *GetRFIDInstance(uint8_t index)
{
    if (index < idx)
    {
        return rfid_instances[index];
    }
    return NULL;
}
