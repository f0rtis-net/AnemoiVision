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
    bus_operations_t *ops;
} bus_t;

typedef struct {
    bus_id bus_id;
    bus_t *bus;
} bus_entry_t;

extern bus_entry_t bus_table[MAX_BUSES_NUM];
extern bus_count_ty bus_count;

#define REGISTER_BUS(id, bus_struct) \
    do { \
        if ((bus_struct) == NULL || (bus_struct)->bus == NULL) { \
            ESP_LOGE(SYSTEM_NAME, "Null bus entry for bus [%d].", (int)(id)); \
            break; \
        } \
        if ((bus_struct)->bus->ops == NULL || (bus_struct)->bus->ops->init == NULL) { \
            ESP_LOGE(SYSTEM_NAME, "Invalid ops or missing init() for bus [%d].", (int)(id)); \
            break; \
        } \
        if (bus_count < MAX_BUSES_NUM) { \
            bus_table[bus_count].bus_id = (id); \
            bus_table[bus_count].bus = (bus_struct)->bus; \
            bus_count++; \
            ESP_LOGI(SYSTEM_NAME, "Bus [%d] registered.", (int)(id)); \
        } else { \
            ESP_LOGE(SYSTEM_NAME, "Bus table full, cannot register [%d].", (int)(id)); \
        } \
    } while (0)

#define DEFINE_BUS_REGISTER(ID, TAG_NAME, INIT_FN, DESTRUCT_FN, READ_FN, WRITE_FN)   \
    static esp_err_t INIT_FN(void);                                                  \
    static esp_err_t DESTRUCT_FN(void);                                              \
    static esp_err_t READ_FN(uint8_t addr, uint8_t *data, size_t len);               \
    static esp_err_t WRITE_FN(uint8_t addr, const uint8_t *data, size_t len);        \
                                                                                     \
    static bus_operations_t TAG_NAME##_ops = {                                       \
        .init = INIT_FN,                                                             \
        .destruct = DESTRUCT_FN,                                                     \
        .read = READ_FN,                                                             \
        .write = WRITE_FN                                                            \
    };                                                                               \
                                                                                     \
    static bus_t TAG_NAME##_bus = {                                                  \
        .id = (ID),                                                                  \
        .ops = &TAG_NAME##_ops                                                       \
    };                                                                               \
                                                                                     \
    static bus_entry_t TAG_NAME##_entry = {                                          \
        .bus_id = (ID),                                                              \
        .bus = &TAG_NAME##_bus                                                       \
    };                                                                               \
                                                                                     \
    __attribute__((constructor)) static void TAG_NAME##_auto_register(void) {        \
        REGISTER_BUS(ID, &TAG_NAME##_entry);                                         \
    }


void init_buses(void);

esp_err_t get_bus_by_id(bus_id id, bus_t **bus);

#endif
