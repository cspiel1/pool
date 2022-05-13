/**
 * @file pool.c
 *
 * Copyright (C) 2021 Christian Spielberger
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "webui.h"
#include "pool.h"

static const char *TAG = "pool";

#define GPIO_WAT_MINUS       4
#define GPIO_WAT_PLUS       18
#define GPIO_CL_MINUS       19
#define GPIO_CL_PLUS         5
#define GPIO_POWER          23
#define GPIO_BEEP           22
#define GPIO_FAN            21

#define GPIO_LOW_FLOW       15

#define GPIO_OUTPUT_PIN_SEL  (\
        (1ULL<<GPIO_WAT_MINUS) | \
        (1ULL<<GPIO_WAT_PLUS)  | \
        (1ULL<<GPIO_CL_MINUS)  | \
        (1ULL<<GPIO_CL_PLUS)   | \
        (1ULL<<GPIO_POWER)     | \
        (1ULL<<GPIO_BEEP)      | \
        (1ULL<<GPIO_FAN)         \
        )

#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_LOW_FLOW))

#define ESP_INTR_FLAG_DEFAULT 0
#define DEFAULT_VREF    1100
#define NO_OF_SAMPLES   64

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

static void IRAM_ATTR low_flow(void* arg)
{
    bool *changed = (bool *) arg;

    *changed = true;
}


static void switch_on_off(bool on, int lev)
{
    gpio_set_level(GPIO_POWER, on);
    gpio_set_level(GPIO_FAN, on);

    if (on) {
        ESP_LOGI(TAG, "Switch on ...");
        gpio_set_level(GPIO_WAT_MINUS, lev);
        gpio_set_level(GPIO_WAT_PLUS, !lev);
        gpio_set_level(GPIO_CL_MINUS, lev);
        gpio_set_level(GPIO_CL_PLUS, !lev);
    } else {
        ESP_LOGW(TAG, "Switch off ...");
        gpio_set_level(GPIO_WAT_MINUS, 0);
        gpio_set_level(GPIO_WAT_PLUS, 0);
        gpio_set_level(GPIO_CL_MINUS, 0);
        gpio_set_level(GPIO_CL_PLUS, 0);
    }
}


static void handle_flow_change(int lev)
{
    int on = !gpio_get_level(GPIO_LOW_FLOW);
    if (on)
        ESP_LOGI(TAG, "Flow Ok");
    else
        ESP_LOGW(TAG, "Low flow detected");

    switch_on_off(on && webui_check_time(), lev);
}


void pool_loop(void *pvParameter)
{
    bool changed = false;
    bool run = false;
    esp_adc_cal_characteristics_t *adc_chars;
    const adc_channel_t channel = ADC_CHANNEL_6; /* GPIO34 */
    const adc_bits_width_t width = ADC_WIDTH_BIT_12;
    const adc_atten_t atten = ADC_ATTEN_DB_0;
    const adc_unit_t unit = ADC_UNIT_1;
    (void) pvParameter;
    ESP_LOGI(TAG, "Init GPIO");

    /* configure outputs */
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    /* configure inputs with interrupt for rising edge*/
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    /* install gpio isr service */
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_LOW_FLOW, low_flow, &changed);

    /* configure ADC */
    adc1_config_width(width);
    adc1_config_channel_atten(channel, atten);
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width,
            DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);


    /* main loop */
    ESP_LOGI(TAG, "Starting pool main loop ...");

    int cnt = 0;

    int lev = !gpio_get_level(GPIO_LOW_FLOW);
    gpio_set_level(GPIO_POWER, false);
    gpio_set_level(GPIO_FAN, false);
    if (lev) {
        ESP_LOGI(TAG, "Flow Ok on startup");
    } else
        ESP_LOGW(TAG, "Low flow detected at startup");

    uint32_t adc = 0;

    /* currently only 3 hours */
    while (true) {

        /* flip voltage from +/- every 20 minutes */
        const int d = 20*60;
        vTaskDelay(100 / portTICK_PERIOD_MS);

        cnt++;

        if (!webui_check_time()) {
            if (run) {
                switch_on_off(false, 0);
                run = false;
            }

            continue;
        }

        if (!run) {
            run = true;
            changed = true;
        }

        if (changed) {
            changed = false;
            handle_flow_change(lev);
        }
        else if (cnt % (10 * d) == 0 && !gpio_get_level(GPIO_LOW_FLOW)) {
            uint32_t voltage;
            lev = !lev;
            ESP_LOGI(TAG, "switch to %d\n", lev);
            gpio_set_level(GPIO_WAT_MINUS, lev);
            gpio_set_level(GPIO_WAT_PLUS, !lev);
            gpio_set_level(GPIO_CL_MINUS, lev);
            gpio_set_level(GPIO_CL_PLUS, !lev);

            adc = adc1_get_raw((adc1_channel_t) channel);
            voltage = esp_adc_cal_raw_to_voltage(adc, adc_chars);
            ESP_LOGI(TAG, "Raw: %d\tVoltage: %dmV\n", adc, voltage);
        }

    }
}
