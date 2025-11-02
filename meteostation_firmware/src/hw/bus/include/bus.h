#ifndef _BUS_H
#define _BUS_H

#include <stdlib.h>
#include <esp_err.h>
#include <esp_log.h>

#define SYSTEM_NAME "Bus system"
#define MAX_BUSES_NUM 8

typedef uint8_t bus_count_ty;

typedef uint64_t bus_id;

typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*destruct)(void);
    esp_err_t (*read)(uint8_t addr, uint8_t *data, size_t len);
    esp_err_t (*write)(uint8_t addr, const uint8_t *data, size_t len);
} bus_operations_t;

typedef struct {
    bus_id id;
    bus_operations_t* ops;
} bus_t;

typedef struct {
    bus_id *id;
    bus_t *bus;
} bus_entry_t;

static bus_entry_t bus_table[MAX_BUSES_NUM];
static bus_count_ty bus_count = 0;

#define REGISTER_BUS(id, bus_struct)                       \
    do {                                           \
        if (bus_struct->bus == NULL) { \
            ESP_LOGE(SYSTEM_NAME, "Found null bus_struct for bus: [%d]. Skip bus registration...", id); \
            break; \
        } \
        if (bus_struct->ops == NULL || (bus_struct->ops != NULL && bus_struct->ops->init == NULL)) { \
            ESP_LOGE(SYSTEM_NAME, "Found invalid ops structure, or no init function for bus: [%d]. Skip bus registration...", id); \
            break; \
        } \
        if (bus_count < MAX_BUSES) {              \
            bus_table[bus_count].id = id; \
            bus_table[bus_count].bus = bus_struct;       \
            bus_count++;                           \
        }                                          \
    } while(0)


static void init_buses(void) {
    ESP_LOGI(SYSTEM_NAME, "Start initializing buses...");

    for(bus_count_ty i = 0; i < MAX_BUSES_NUM; ++i) {
        bus_entry_t* entry = &bus_table[i];

        if (entry == NULL) {
            ESP_LOGD(SYSTEM_NAME, "Skipped free slot %d", i);
            continue;
        }

        bus_t* bus = entry->bus;

        esp_err_t err = bus->ops->init();

        if (err != ESP_OK) {
            ESP_LOGE(SYSTEM_NAME, "Handled error when init bus: [%d]. Error code: %d", entry->id, err);
            continue;
        }

        ESP_LOGI(SYSTEM_NAME, "Bus with id %d was inited successfuly!", entry->id);
    }
}


#endif