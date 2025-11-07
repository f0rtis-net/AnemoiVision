#ifndef _DRIVER_H
#define _DRIVER_H

#include <stdio.h>
#include <esp_err.h>
#include <esp_log.h>
#include "../../bus/include/bus.h"

#define DRIVER_SYSTEM_NAME "Driver system"
#define MAX_DRIVERS_NUM 16

typedef uint8_t driver_id;
typedef uint8_t driver_count_ty;

typedef struct driver_t driver_t;

typedef struct {
    esp_err_t (*init)(driver_t *driver);
    esp_err_t (*destruct)(driver_t *driver);
    esp_err_t (*read)(driver_t *driver, uint8_t *data, size_t len);
    esp_err_t (*write)(driver_t *driver, const uint8_t *data, size_t len);
} driver_operations_t;

struct driver_t {
    driver_id id;
    bus_t *bus;                  
    driver_operations_t *ops;    
    void *ctx;                   
};

typedef struct {
    driver_id id;
    driver_t *driver;
} driver_entry_t;

extern driver_entry_t driver_table[MAX_DRIVERS_NUM];
extern driver_count_ty driver_count;

#define REGISTER_DRIVER(ID, DRIVER_ENTRY_PTR)                                          \
    do {                                                                               \
        if ((DRIVER_ENTRY_PTR) == NULL || (DRIVER_ENTRY_PTR)->driver == NULL) {        \
            ESP_LOGE(DRIVER_SYSTEM_NAME, "Null driver entry for driver [%d].", (int)(ID)); \
            break;                                                                     \
        }                                                                              \
        driver_t *_drv = (DRIVER_ENTRY_PTR)->driver;                                   \
        if (_drv->ops == NULL || _drv->ops->init == NULL) {                            \
            ESP_LOGE(DRIVER_SYSTEM_NAME, "Invalid ops or missing init() for driver [%d].", (int)(ID)); \
            break;                                                                     \
        }                                                                              \
        if (driver_count < MAX_DRIVERS_NUM) {                                          \
            driver_table[driver_count].id = (ID);                                      \
            driver_table[driver_count].driver = _drv;                                  \
            driver_count++;                                                            \
            ESP_LOGI(DRIVER_SYSTEM_NAME, "Driver [%d] registered.", (int)(ID));        \
        } else {                                                                       \
            ESP_LOGE(DRIVER_SYSTEM_NAME, "Driver table full, cannot register [%d].", (int)(ID)); \
        }                                                                              \
    } while (0)

#define DEFINE_DRIVER_REGISTER(ID, TAG_NAME, BUS_PTR, INIT_FN, DESTRUCT_FN, READ_FN, WRITE_FN) \
    static esp_err_t INIT_FN(driver_t *driver);                                                \
    static esp_err_t DESTRUCT_FN(driver_t *driver);                                            \
    static esp_err_t READ_FN(driver_t *driver, uint8_t *data, size_t len);                     \
    static esp_err_t WRITE_FN(driver_t *driver, const uint8_t *data, size_t len);              \
                                                                                               \
    static driver_operations_t TAG_NAME##_ops = {                                              \
        .init = INIT_FN,                                                                       \
        .destruct = DESTRUCT_FN,                                                               \
        .read = READ_FN,                                                                       \
        .write = WRITE_FN                                                                      \
    };                                                                                         \
                                                                                               \
    static driver_t TAG_NAME##_driver = {                                                      \
        .id = (ID),                                                                            \
        .bus = (BUS_PTR),                                                                      \
        .ops = &TAG_NAME##_ops,                                                                \
        .ctx = NULL                                                                            \
    };                                                                                         \
                                                                                               \
    static driver_entry_t TAG_NAME##_entry = {                                                 \
        .id = (ID),                                                                            \
        .driver = &TAG_NAME##_driver                                                           \
    };                                                                                         \
                                                                                               \
    __attribute__((constructor)) static void TAG_NAME##_auto_register(void) {                  \
        REGISTER_DRIVER(ID, &TAG_NAME##_entry);                                                \
    }


void init_drivers(void);

esp_err_t get_driver_by_id(driver_id id, driver_t **driver);

#endif
