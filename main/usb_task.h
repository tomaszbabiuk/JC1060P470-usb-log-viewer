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
