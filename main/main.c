/**
 * @file main.c
 *
 * Copyright (C) 2021 Christian Spielberger
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_chip_info.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "string.h"
#include <sys/socket.h>
#include <errno.h>

#include "wifi.h"
#include "ota.h"
#include "pool.h"
#include "webui.h"

static const char *TAG = "main";

void app_main(void)
{
    static httpd_handle_t server = NULL;

    // Set timezone to China Standard Time
    setenv("TZ", ":Europe/Vienna", 1);
    tzset();

    ESP_LOGI(TAG, "Starting Pool main");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is %s chip with %d CPU core(s), WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    ESP_LOGI(TAG, "silicon revision %d, ", chip_info.revision);

    ESP_LOGI(TAG, "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    ESP_LOGI(TAG, "Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
            ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    xTaskCreate(&pool_loop, "pool_loop", 8192, NULL, 5, NULL);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                IP_EVENT_STA_GOT_IP,
                &webui_connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                WIFI_EVENT_STA_DISCONNECTED,
                &webui_disconnect_handler, &server));

    server = start_webserver();

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (webui_upgrade())
            xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);

        wifi_check();
    }
}
