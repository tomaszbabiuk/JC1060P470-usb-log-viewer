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

// Change these values to match your needs
#define MESSAGE_QUEUE_SIZE          (100)
#define MAX_MESSAGE_LEN             (128)


lv_obj_t * label = NULL;

/**
 * @brief Message structure for the queue
 */
typedef struct {
    uint8_t data[MAX_MESSAGE_LEN];
    size_t len;
} message_t;

static QueueHandle_t message_queue = NULL;

void lv_hello_world(void)
{
    /* Create a screen object */
    lv_obj_t * screen = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Create a label */
    label = lv_label_create(screen);
    lv_label_set_text(label, "Hello, World!");

    /* Center the label on the screen */
    lv_obj_center(label);

    /* Load the screen */
    lv_scr_load(screen);
}

/**
 * @brief UI task
 *
 * Initializes the display and creates the LVGL UI
 *
 * @param pvParameters Unused
 */
static void ui_task(void *pvParameters)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    bsp_display_lock(0);

    lv_hello_world();

    bsp_display_unlock();

    // UI task can optionally delete itself or stay running
    vTaskDelete(NULL);
}

namespace {
static const char *TAG = "UI";
} // namespace

/**
 * @brief Display update task
 *
 * Reads messages from the queue and updates the label
 *
 * @param pvParameters Unused
 */
static void display_update_task(void *pvParameters)
{
    message_t msg;

    while (1) {
        // Wait for a message from the queue
        if (xQueueReceive(message_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (label != NULL) {
                // Convert binary data to printable string
                char buf[MAX_MESSAGE_LEN * 2 + 1];
                size_t buf_idx = 0;
                
                for (size_t i = 0; i < msg.len && buf_idx < sizeof(buf) - 1; i++) {
                    if (msg.data[i] >= 32 && msg.data[i] < 127) {
                        // Printable ASCII character
                        buf[buf_idx++] = msg.data[i];
                    } else if (msg.data[i] == '\n' || msg.data[i] == '\r') {
                        // Preserve newlines
                        buf[buf_idx++] = msg.data[i];
                    } else {
                        // Non-printable character, skip or represent as .
                        buf[buf_idx++] = '.';
                    }
                }
                buf[buf_idx] = '\0';
                
                bsp_display_lock(0);
                lv_label_set_text(label, buf);
                bsp_display_unlock();
            }
        }
    }
}

/**
 * @brief Main application
 *
 * Initializes the UI and USB task
 */
extern "C" void app_main(void)
{
    // Create the message queue to hold the last 100 messages
    message_queue = xQueueCreate(MESSAGE_QUEUE_SIZE, sizeof(message_t));
    assert(message_queue != NULL);

    // Create the UI task pinned to core 0 (main core)
    BaseType_t ui_task_created = xTaskCreatePinnedToCore(ui_task, "ui_task", 4096, NULL, tskIDLE_PRIORITY + 2, NULL, 0);
    assert(ui_task_created == pdTRUE);

    // Create the display update task
    BaseType_t display_task_created = xTaskCreate(display_update_task, "display_task", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    assert(display_task_created == pdTRUE);

    // Create the USB task
    usb_task_start(message_queue);
}
