#ifndef _BME_280_h
#define _BME_280_h

#include <esp_err.h>

typedef struct {
    float temp;
    float pressure;
    float humidity;
} bme280_data_t;

esp_err_t bme280_read_id(uint8_t *id);
esp_err_t bme280_read_data(bme280_data_t *data);

#endif