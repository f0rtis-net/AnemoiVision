#include "bus.h"

bus_entry_t bus_table[MAX_BUSES_NUM];
bus_count_ty bus_count = 0;

void init_buses(void) {
    ESP_LOGI(SYSTEM_NAME, "Start initializing buses...");

    for (bus_count_ty i = 0; i < MAX_BUSES_NUM; ++i) {
        bus_entry_t *entry = &bus_table[i];

        if (entry->bus == NULL) {
            ESP_LOGI(SYSTEM_NAME, "Skipped free slot %d", i);
            continue;
        }

        bus_t *bus = entry->bus;
        esp_err_t err = bus->ops->init();

        if (err != ESP_OK) {
            ESP_LOGE(SYSTEM_NAME, "Error init bus: [%d]. Code: %d", (int)entry->bus_id, err);
            continue;
        }

        ESP_LOGI(SYSTEM_NAME, "Bus [%d] initialized successfully!", (int)entry->bus_id);
    }
}

esp_err_t get_bus_by_id(bus_id id, bus_t **bus) {
    if (id >= MAX_BUSES_NUM) {
        return ESP_ERR_INVALID_ARG;
    }

    if (bus_table[id].bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    *bus = bus_table[id].bus;
    return ESP_OK;
}