#ifndef _DRIVER_H
#define _DERIVER_H

#include <stdio.h>
#include <esp_err.h>
#include <esp_log.h>
#include "../../bus/include/bus.h"

#define SYSTEM_NAME "driver_system"

#define MAX_DRIVERS_NUM 16
typedef uint8_t drivers_count_ty;

typedef uint8_t driver_id;

typedef struct {
    esp_err_t (*init)(driver_t* driver);
    esp_err_t (*destruct)(driver_t* driver);

    esp_err_t (*read)(driver_t* driver, uint8_t* data, size_t len);
    esp_err_t (*write)(driver_t* driver, const uint8_t* data, size_t len);

} driver_operations_t;

typedef struct {
    driver_id id;
    bus_t bus;
    driver_operations_t* ops;
    
    void* ctx;
} driver_t;

typedef struct {
    driver_id id;
    driver_t* driver;
} driver_entry_t;

static driver_entry_t drivers_table[MAX_DRIVERS_NUM];
static drivers_count_ty drivers_count = 0;

#define REGISTER_DRIVER(id, driver_struct) \
    do { \
        if (driver_struct->driver == NULL) { \
            ESP_LOGE(SYSTEM_NAME, "Found null driver_struct for driver: [%d]. Skip driver registration...", id); \
            break; \
        } \
        if (driver_struct->ops == NULL || (driver_struct->ops != NULL && driver_struct->ops->init == NULL)) { \
            ESP_LOGE(SYSTEM_NAME, "Found invalid ops structure, or no init function for driver: [%d]. Skip driver registration...", id); \
            break; \
        } \
        if (drivers_count < MAX_DRIVERS) {            \
            drivers_table[drivers_count].id = id;     \
            drivers_table[drivers_count].driver = driver_struct;     \
            drivers_count++;                           \
        }       \
    } while(0);

static void init_drivers(void) {
    ESP_LOGI(SYSTEM_NAME, "Start initializing drivers...");

    for(drivers_count_ty i = 0; i < MAX_DRIVERS_NUM; ++i) {
        driver_entry_t* entry = &drivers_table[i];

        if (entry == NULL) {
            ESP_LOGD(SYSTEM_NAME, "Skipped free slot %d", i);
            continue;
        }

        driver_t* driver = entry->driver;

        esp_err_t err = driver->ops->init(driver);

        if (err != ESP_OK) {
            ESP_LOGE(SYSTEM_NAME, "Handled error when init driver: [%d]. Error code: %d", driver->id, err);
            continue;
        }

        ESP_LOGI(SYSTEM_NAME, "Driver with id %d was inited successfuly!", driver->id);
    }
}

#endif