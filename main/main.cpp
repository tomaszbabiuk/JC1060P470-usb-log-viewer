/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"

#include "usb_task.h"
#include "ui_task.h"
#include "messaging.h"


static QueueHandle_t message_queue = NULL;

extern "C" void app_main(void)
{
    // Create the message queue to hold the last 100 messages
    message_queue = xQueueCreate(MESSAGE_QUEUE_SIZE, sizeof(message_t));
    assert(message_queue != NULL);

    ui_task_start(message_queue);
    usb_task_start(message_queue);
}
