/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef MESSAGING_H
#define MESSAGING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MESSAGE_QUEUE_SIZE (50)
#define MAX_MESSAGE_LEN (256)

/**
 * @brief Message structure for the queue
 */
typedef struct {
  char data[MAX_MESSAGE_LEN];
  size_t len;
} message_t;

#ifdef __cplusplus
}
#endif

#endif // USB_TASK_H
