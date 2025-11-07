#include "driver.h"

driver_entry_t driver_table[MAX_DRIVERS_NUM];
driver_count_ty driver_count = 0;

void init_drivers(void)
{
    ESP_LOGI(DRIVER_SYSTEM_NAME, "Start initializing drivers...");

    for (driver_count_ty i = 0; i < driver_count; ++i) {
        driver_entry_t *entry = &driver_table[i];
        if (entry == NULL || entry->driver == NULL) {
            ESP_LOGW(DRIVER_SYSTEM_NAME, "Skipped null slot %d", i);
            continue;
        }

        driver_t *drv = entry->driver;
        if (drv->ops == NULL || drv->ops->init == NULL) {
            ESP_LOGE(DRIVER_SYSTEM_NAME, "Driver [%d] missing init()", drv->id);
            continue;
        }

        esp_err_t err = drv->ops->init(drv);
        if (err != ESP_OK) {
            ESP_LOGE(DRIVER_SYSTEM_NAME, "Error initializing driver [%d]: %s", drv->id, esp_err_to_name(err));
            continue;
        }

        ESP_LOGI(DRIVER_SYSTEM_NAME, "Driver [%d] initialized successfully!", drv->id);
    }
}

esp_err_t get_driver_by_id(driver_id id, driver_t **driver) {
    if (id >= MAX_DRIVERS_NUM) {
        return ESP_ERR_INVALID_ARG;
    }

    if (driver_table[id].driver == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    *driver = driver_table[id].driver;
    return ESP_OK;
}