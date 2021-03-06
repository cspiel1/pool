/**
 * @file wifi.c
 *
 * Copyright (C) 2021 Christian Spielberger
 */
#include <stdio.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_sntp.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi.h"
#include "config.h"
#include "log.h"

#define GPIO_LED            22

#define CONFIG_ESP_MAXIMUM_RETRY 9
#define NTP_SERVER "de.pool.ntp.org"
/* Western European Time */
#define TZ_INFO "WEST-1DWEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi";

static int s_retry_delay = 0;  /* in seconds */
static bool led = true;
static bool flash = true;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "Wifi event_id %d", event_id);
    logw("Wifi event_id %d", event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();

        if (!s_retry_delay)
            s_retry_delay = 1;
        else if (s_retry_delay < 256)
            s_retry_delay *= 2;

        flash = true;
        ESP_LOGI(TAG, "retry to connect to the AP");
    }
    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_delay = 0;
        flash = false;
        setenv("TZ", TZ_INFO, 1);
        tzset();
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, NTP_SERVER);
        sntp_init();
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        gpio_set_level(GPIO_LED, true);
        ESP_LOGI(TAG, "Wifi ok, LED on");
        logw("Wifi ok, LED on");
    }
}


static void vendor_ie_cb(void *ctx, wifi_vendor_ie_type_t type,
        const uint8_t sa[6], const vendor_ie_data_t *vnd_ie, int rssi)
{
    logw("%s:%d type=%u rssi=%d", __FUNCTION__, __LINE__, type, rssi);
}


int wifi_init_sta(void)
{
    int err = ESP_OK;

    s_wifi_event_group = xEventGroupCreate();

    esp_wifi_set_vendor_ie_cb(vendor_ie_cb, NULL);
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
/*            .threshold.authmode = WIFI_AUTH_WPA2_PSK,*/

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", CONFIG_ESP_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", CONFIG_ESP_WIFI_SSID);
        err = ESP_ERR_WIFI_BASE + 1;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        err = ESP_ERR_WIFI_BASE + 2;
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);

    return err;
}


void wifi_check(void)
{
    if (flash) {
        led = !led;
        gpio_set_level(GPIO_LED, led);
    }

    if (s_retry_delay > 0) {

        s_retry_delay--;
        if (!s_retry_delay) {
            logw("Wifi reconnect");
            esp_wifi_connect();
        }
    }
}


void wifi_scan(void)
{
    wifi_scan_config_t scan_config = { 0 };
    uint16_t g_scan_ap_num;
    wifi_ap_record_t *g_ap_list_buffer;

    logw("%s", __FUNCTION__);
/*    scan_config.ssid      = CONFIG_ESP_WIFI_SSID;*/
    scan_config.scan_type = WIFI_SCAN_TYPE_PASSIVE;

    logw("Starting wifi scan");
    esp_wifi_scan_start(&scan_config, 0);
    logw("wifi scan started");
    ESP_LOGI(TAG, "wifi scan started");
    return;

    esp_wifi_scan_get_ap_num(&g_scan_ap_num);
    if (g_scan_ap_num) {
        g_ap_list_buffer = malloc(g_scan_ap_num * sizeof(wifi_ap_record_t));
        if (g_ap_list_buffer) {
            if (esp_wifi_scan_get_ap_records(&g_scan_ap_num,
                            (wifi_ap_record_t *) g_ap_list_buffer) == ESP_OK) {
                for (int i = 0; i < g_scan_ap_num; i++) {
                    ESP_LOGI(TAG, "[%s][rssi=%d]",
                             g_ap_list_buffer[i].ssid,
                             g_ap_list_buffer[i].rssi);
                    logw("[%s][rssi=%d]",
                             g_ap_list_buffer[i].ssid,
                             g_ap_list_buffer[i].rssi);
                }
            }

            free(g_ap_list_buffer);
        }
    }

}
