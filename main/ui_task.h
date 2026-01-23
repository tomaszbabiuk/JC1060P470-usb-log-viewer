/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef UI_TASK_H
#define UI_TASK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_task_start(void *message_queue);

#ifdef __cplusplus
}
#endif

#endif // USB_TASK_H
