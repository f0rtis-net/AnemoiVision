#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "hw/bus/include/bus.h"
#include "hw/driver/include/driver.h"
#include "hw/driver/bme280/bme_280.h"
static const char *TAG = "example";

void app_main(void)
{
    init_buses();
    init_drivers();

    while (1) {
        bme280_data_t data;

        esp_err_t err = bme280_read_data(&data);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "error: %s", esp_err_to_name(err));
            break;
        }

        ESP_LOGI(TAG, "temp: %f", data.temp);
        ESP_LOGI(TAG, "hum: %f", data.humidity);
        ESP_LOGI(TAG, "pressure: %f", data.pressure);

        vTaskDelay(pdTICKS_TO_MS(100));
    }
}