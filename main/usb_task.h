/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef USB_TASK_H
#define USB_TASK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MESSAGE_QUEUE_SIZE          (50)
#define MAX_MESSAGE_LEN             (256)

/**
 * @brief Message structure for the queue
 */
typedef struct {
    char data[MAX_MESSAGE_LEN] = { '\0' };
    size_t len;
} message_t;

/**
 * @brief Initialize and start the USB task
 *
 * This function creates and starts the USB handling task. It should be called
 * from app_main after initializing the message queue.
 *
 * @param message_queue Handle to the message queue for receiving USB data
 */
void usb_task_start(void *message_queue);

#ifdef __cplusplus
}
#endif

#endif // USB_TASK_H
