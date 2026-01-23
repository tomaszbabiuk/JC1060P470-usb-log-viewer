#include <stdio.h>
#include <string.h>
#include <memory>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "usb/cdc_acm_host.h"
#include "usb/vcp_ftdi.hpp"
#include "usb/vcp_ch34x.hpp"
#include "usb/vcp_cp210x.hpp"
#include "usb/vcp.hpp"
#include "usb/usb_host.h"

#include "usb_task.h"
#include "messaging.h"

using namespace esp_usb;

#define EXAMPLE_BAUDRATE            (115200)
#define EXAMPLE_STOP_BITS           (0)      // 0: 1 stopbit, 1: 1.5 stopbits, 2: 2 stopbits
#define EXAMPLE_PARITY              (0)      // 0: None, 1: Odd, 2: Even, 3: Mark, 4: Space
#define EXAMPLE_DATA_BITS           (8)
#define UART_INPUT_BUFFER_SIZE      (256)

char line_buffer[UART_INPUT_BUFFER_SIZE];

namespace {
static const char *TAG = "VCP example";
static SemaphoreHandle_t device_disconnected_sem;

bool exclude_ftdi_ansisgr_newlines(uint8_t in) {
  static bool ftdi_frame_started = false;
  static bool ansi_sgr_started = false;
  static bool new_line_started = false;
  // ftdi
  if (in == 0x01) {
    ftdi_frame_started = true;
    return true;
  }

  if (ftdi_frame_started && (in == 0x60 || in == 0x62)) {
    ftdi_frame_started = false;
    return true;
  }

  // ansi-sgr
  if (in == 0x1b) {
    ansi_sgr_started = true;
    return true;
  }

  if (ansi_sgr_started && in == 0x6d) {
    ansi_sgr_started = false;
    return true;
  }

  if (ansi_sgr_started) {
    // any byte inside the sgr
    return true;
  }

  // new line
  if (in == 0x0d) {
    new_line_started = true;
    return true;
  }

  if (new_line_started && in == 0x0a) {
    new_line_started = false;
    return true;
  }

  return false;
}

static bool handle_rx(const uint8_t *data, size_t data_len, void *arg)
{
	static message_t out_message;
  	QueueHandle_t message_queue = (QueueHandle_t)arg;
  	if (message_queue != NULL) {
        for (int i = 0; i < data_len; i++) {
            uint8_t byte = data[i];
            bool excluded = exclude_ftdi_ansisgr_newlines(byte);
            bool flush = false;
            if (excluded) {
            if (byte == 0x0a) {
                flush = true;
            }
            } else {
            out_message.data[out_message.len] = byte;
            out_message.len++;
            }

            if (out_message.len == MAX_MESSAGE_LEN -1) {
                flush = true;
            }

            if (flush) {
                printf("%s\n", (char *)out_message.data);
				xQueueSendToBack(message_queue, &out_message, 0);
                memset(out_message.data, 0, MAX_MESSAGE_LEN);
                out_message.len = 0;
            }
        }
    }
    return true;
}


static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %d", event->data.error);
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "Device suddenly disconnected");
        xSemaphoreGive(device_disconnected_sem);
        break;
    case CDC_ACM_HOST_SERIAL_STATE:
        ESP_LOGI(TAG, "Serial state notif 0x%04X", event->data.serial_state.val);
        break;
    case CDC_ACM_HOST_NETWORK_CONNECTION:
    default: break;
    }
}

static void usb_lib_task(void *arg)
{
    while (1) {
        // Start handling system events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "USB: All devices freed");
            // Continue handling USB events to allow device reconnection
        }
    }
}

static void usb_task_internal(void *arg)
{
    QueueHandle_t message_queue = (QueueHandle_t)arg;

    // Create semaphore for device disconnection
    device_disconnected_sem = xSemaphoreCreateBinary();
    assert(device_disconnected_sem);

    // Install USB Host driver. Should only be called once in entire application
    ESP_LOGI(TAG, "Installing USB Host");
    usb_host_config_t host_config = {};
    host_config.skip_phy_setup = false;
    host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Create a task that will handle USB library events
    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 10, NULL);
    assert(task_created == pdTRUE);

    ESP_LOGI(TAG, "Installing CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    // Register VCP drivers to VCP service
    VCP::register_driver<FT23x>();
    VCP::register_driver<CP210x>();
    VCP::register_driver<CH34x>();

    // Do everything else in a loop, so we can demonstrate USB device reconnections
    while (true) {
        const cdc_acm_host_device_config_t dev_config = {
            .connection_timeout_ms = 5000, // 5 seconds, enough time to plug the device in or experiment with timeout
            .out_buffer_size = UART_INPUT_BUFFER_SIZE,
            .in_buffer_size = UART_INPUT_BUFFER_SIZE,
            .event_cb = handle_event,
            .data_cb = handle_rx,
            .user_arg = message_queue,
        };

        // You don't need to know the device's VID and PID. Just plug in any device and the VCP service will load correct (already registered) driver for the device
        ESP_LOGI(TAG, "Opening any VCP device...");
        auto vcp = std::unique_ptr<CdcAcmDevice>(VCP::open(&dev_config));

        if (vcp == nullptr) {
            ESP_LOGI(TAG, "Failed to open VCP device");
            continue;
        }
        vTaskDelay(10);

        ESP_LOGI(TAG, "Setting up line coding");
        cdc_acm_line_coding_t line_coding = {
            .dwDTERate = EXAMPLE_BAUDRATE,
            .bCharFormat = EXAMPLE_STOP_BITS,
            .bParityType = EXAMPLE_PARITY,
            .bDataBits = EXAMPLE_DATA_BITS,
        };
        ESP_ERROR_CHECK(vcp->line_coding_set(&line_coding));

        // Send some dummy data
        ESP_LOGI(TAG, "Sending data through CdcAcmDevice");
        uint8_t data[] = "test_string";
        ESP_ERROR_CHECK(vcp->tx_blocking(data, sizeof(data)));
        ESP_ERROR_CHECK(vcp->set_control_line_state(true, true));

        // We are done. Wait for device disconnection and start over
        ESP_LOGI(TAG, "Done. You can reconnect the VCP device to run again.");
        xSemaphoreTake(device_disconnected_sem, portMAX_DELAY);
    }
}
} // namespace

void usb_task_start(void *message_queue)
{
    // Create the USB task
    BaseType_t app_task_created = xTaskCreate(usb_task_internal, "usb_task", 4096, message_queue, tskIDLE_PRIORITY, NULL);
    assert(app_task_created == pdTRUE);
}
