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

lv_obj_t * log_container = NULL;
static QueueHandle_t message_queue = NULL;

static void lv_hello_world(void)
{
    /* Create a screen object */
    lv_obj_t * screen = lv_obj_create(NULL);
    
    log_container = lv_obj_create(screen);
    lv_obj_set_size(log_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(log_container, LV_FLEX_FLOW_COLUMN);


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

    vTaskDelete(NULL);
}

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
            uint32_t logs = lv_obj_get_child_count(log_container);

            bsp_display_lock(0);
            lv_obj_t *log_lbl = lv_label_create(log_container);
            lv_obj_set_width(log_lbl, LV_PCT(100));
            lv_obj_set_height(log_lbl, LV_SIZE_CONTENT);
            lv_label_set_text(log_lbl, msg.data);
            if (logs > 50) {
                lv_obj_t * first_child = lv_obj_get_child(log_container, 0);
                lv_obj_del(first_child);
            }
            lv_obj_scroll_to_view(log_lbl, LV_ANIM_OFF);
            bsp_display_unlock();
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
    BaseType_t display_task_created = xTaskCreate(display_update_task, "display_task", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    assert(display_task_created == pdTRUE);

    // Create the USB task
    usb_task_start(message_queue);
}
