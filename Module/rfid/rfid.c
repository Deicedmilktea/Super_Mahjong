#include "rfid.h"
#include "usart.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_RFID_INSTANCES 1 // Maximum number of RFID modules
static RFID_Instance *rfid_instances[MAX_RFID_INSTANCES] = {NULL};
static uint8_t idx = 0;

// Forward declaration of the callback
static void RFIDCallback(USART_Instance *_usart_instance);

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
RFID_Instance *RFIDInit(UART_HandleTypeDef *usart_handle)
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

    RFIDQueueInit(&rfid_instance->draw_tile);
    RFIDQueueInit(&rfid_instance->discard_tile);

    USART_Init_Config_s usart_config = {
        .recv_buff_size = USART_RXBUFF_LIMIT, // Assuming USART_RXBUFF_LIMIT is defined elsewhere
        .usart_handle = usart_handle,
        .id = rfid_instance, // Pass the rfid_instance as ID
        .usart_module_callback = RFIDCallback,
    };

    rfid_instance->usart = USARTRegister(&usart_config);
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
static void RFIDCallback(USART_Instance *_usart_instance)
{
    if (!_usart_instance || !_usart_instance->id)
    {
        return;
    }

    RFID_Instance *rfid_instance = (RFID_Instance *)_usart_instance->id;
    uint8_t *rx_buf = rfid_instance->usart->recv_buff;
    uint16_t rx_len = rfid_instance->usart->data_len;

    // Basic validation: must start with '$' and end with '#' (before CR/LF)
    // Example: $DRAW:12# or $DISC:34#
    if (rx_len < 7 || rx_buf[0] != '$' || rx_buf[rx_len - 1] != '#') // Adjusted for typical CR/LF, check actual termination
    {
        // If data ends with CR/LF, adjust index: e.g., rx_buf[rx_len - 3] == '#' for \r\n
        // For simplicity, assuming '#' is the very last char or just before it.
        // This part might need adjustment based on actual data stream termination.
        if (rx_len >= 3 && rx_buf[0] == '$' && rx_buf[rx_len - 3] == '#') // Check for $...#\r\n
        {
            // Valid frame ending with #\r\n
        }
        else if (rx_len >= 2 && rx_buf[0] == '$' && rx_buf[rx_len - 2] == '#') // Check for $...#\n
        {
            // Valid frame ending with #\n
        }
        else
        {
            return; // Invalid frame
        }
    }

    char temp_buf[rx_len + 1];
    memcpy(temp_buf, rx_buf, rx_len);
    temp_buf[rx_len] = '\0'; // Null-terminate for string functions

    int tile_value;
    Tile new_tile; // Assuming Tile can be assigned an int or has a field for it.
                   // This might need adjustment based on Tile definition.

    // // Parse $DRAW:tile_id#
    // if (strncmp(temp_buf, "$DRAW:", 6) == 0)
    // {
    //     if (sscanf(temp_buf, "$DRAW:%d#", &tile_value) == 1)
    //     {
    //         // Assuming Tile is a simple type or has a field like 'id'
    //         // If Tile is a struct, e.g., typedef struct { int id; Suit suit; } Tile;
    //         // then new_tile.id = tile_value; and potentially set other fields.
    //         // For now, direct assignment or casting if Tile is an int alias.
    //         // This is a placeholder. Actual Tile assignment depends on its definition.
    //         if (sizeof(Tile) == sizeof(int))
    //         { // Basic check
    //             memcpy(&new_tile, &tile_value, sizeof(Tile));
    //         }
    //         else
    //         {
    //             // Handle complex Tile structure assignment here
    //             // e.g. new_tile.id = tile_value;
    //             // For now, let's assume Tile is just an int for simplicity
    //             if (sizeof(int) <= sizeof(Tile)) // Check if tile_value can fit
    //                 new_tile = (Tile)tile_value; // This is a simplification
    //             else
    //             {
    //                 // Error or more complex mapping
    //                 return;
    //             }
    //         }
    //         RFIDEnqueue(&rfid_instance->draw_tile, new_tile);
    //     }
    // }
    // // Parse $DISC:tile_id#
    // else if (strncmp(temp_buf, "$DISC:", 6) == 0)
    // {
    //     if (sscanf(temp_buf, "$DISC:%d#", &tile_value) == 1)
    //     {
    //         // Similar to $DRAW, assuming Tile can be represented by tile_value
    //         if (sizeof(Tile) == sizeof(int))
    //         {
    //             memcpy(&new_tile, &tile_value, sizeof(Tile));
    //         }
    //         else
    //         {
    //             if (sizeof(int) <= sizeof(Tile))
    //                 new_tile = (Tile)tile_value; // Simplification
    //             else
    //             {
    //                 return;
    //             }
    //         }
    //         RFIDEnqueue(&rfid_data->discard_tile, new_tile);
    //     }
    // }
    // // Add more parsers for other RFID commands if needed
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
