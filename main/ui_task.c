#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "lvgl.h"
#include "messaging.h"

static lv_obj_t *log_container = NULL;

static void lv_hello_world(void) {
  /* Create a screen object */
  lv_obj_t *screen = lv_obj_create(NULL);

  log_container = lv_obj_create(screen);
  lv_obj_set_size(log_container, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(log_container, LV_FLEX_FLOW_COLUMN);

  /* Load the screen */
  lv_scr_load(screen);
}

static void ui_task(void *arg) {
  QueueHandle_t message_queue = (QueueHandle_t)arg;

  bsp_display_cfg_t cfg = {.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
                           .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
                           .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
                           .flags = {
                               .buff_dma = true,
                               .buff_spiram = false,
                               .sw_rotate = false,
                           }};
  bsp_display_start_with_config(&cfg);
  bsp_display_backlight_on();

  bsp_display_lock(0);

  lv_hello_world();

  bsp_display_unlock();

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
        lv_obj_t *first_child = lv_obj_get_child(log_container, 0);
        lv_obj_del(first_child);
      }
      lv_obj_scroll_to_view(log_lbl, LV_ANIM_OFF);
      bsp_display_unlock();
    }
  }

  vTaskDelete(NULL);
}

void ui_task_start(void *message_queue) {
  BaseType_t ui_task_created = xTaskCreatePinnedToCore(
      ui_task, "ui_task", 4096, message_queue, tskIDLE_PRIORITY + 2, NULL, 0);
  assert(ui_task_created == pdTRUE);
}