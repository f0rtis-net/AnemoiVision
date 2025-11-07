#include <esp_log.h>
#include "../include/driver.h"
#include "../../bus/include/bus.h"
#include "bme_280.h"

static const char *TAG = "BME280_DRIVER";

#define BME280_I2C_ADDR  0x76
#define BME280_ID_REG    0xD0
#define BME280_RESET_REG 0xE0
#define BME280_CTRL_HUM  0xF2
#define BME280_CTRL_MEAS 0xF4
#define BME280_CONFIG    0xF5
#define BME280_TEMP_MSB  0xFA

#define BME280_DRIVER_ID 0x01
#define I2C_BUS_ID       0x00

typedef struct {
    uint8_t address;
} bme280_ctx_t;

static esp_err_t bme280_init(driver_t *driver);
static esp_err_t bme280_destruct(driver_t *driver);
static esp_err_t bme280_read(driver_t *driver, uint8_t *data, size_t len);
static esp_err_t bme280_write(driver_t *driver, const uint8_t *data, size_t len);


DEFINE_DRIVER_REGISTER(
    BME280_DRIVER_ID,
    bme280,
    NULL,
    bme280_init,
    bme280_destruct,
    bme280_read,
    bme280_write
);

static esp_err_t bme280_init(driver_t *driver)
{
    ESP_LOGI(TAG, "Initializing BME280 driver...");

    bme280_ctx_t *ctx = calloc(1, sizeof(bme280_ctx_t));
    if (!ctx)
        return ESP_ERR_NO_MEM;

    ctx->address = BME280_I2C_ADDR;

    bus_t *bus = NULL;
    esp_err_t err = get_bus_by_id(I2C_BUS_ID, &bus);
    if (err != ESP_OK || bus == NULL) {
        ESP_LOGE(TAG, "Failed to get I2C bus: %s", esp_err_to_name(err));
        free(ctx);
        return err;
    }

    driver->ctx = ctx;
    driver->bus = bus;

    uint8_t reg = BME280_ID_REG;
    uint8_t id_val = 0;

    err = bus->ops->write(ctx->address, &reg, 1);
    if (err == ESP_OK)
        err = bus->ops->read(ctx->address, &id_val, 1);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BME280 not responding at 0x%02X", ctx->address);
        free(ctx);
        return err;
    }

    ESP_LOGI(TAG, "BME280 ID = 0x%02X", id_val);

    uint8_t setup[] = {
        BME280_CTRL_HUM,  0x01,
        BME280_CTRL_MEAS, 0x27,
        BME280_CONFIG,    0xA0
    };

    for (int i = 0; i < sizeof(setup); i += 2)
        bus->ops->write(ctx->address, &setup[i], 2);

    ESP_LOGI(TAG, "BME280 initialized successfully.");
    return ESP_OK;
}

static esp_err_t bme280_destruct(driver_t *driver)
{
    if (!driver || !driver->ctx)
        return ESP_ERR_INVALID_ARG;

    free(driver->ctx);
    driver->ctx = NULL;
    ESP_LOGI(TAG, "BME280 driver destroyed.");
    return ESP_OK;
}

static esp_err_t bme280_read(driver_t *driver, uint8_t *data, size_t len)
{
    if (!driver || !driver->ctx)
        return ESP_ERR_INVALID_ARG;

    bme280_ctx_t *ctx = (bme280_ctx_t *)driver->ctx;

    uint8_t reg = BME280_TEMP_MSB;
    esp_err_t err = driver->bus->ops->write(ctx->address, &reg, 1);
    if (err != ESP_OK)
        return err;

    return driver->bus->ops->read(ctx->address, data, len);
}

static esp_err_t bme280_write(driver_t *driver, const uint8_t *data, size_t len)
{
    if (!driver || !driver->ctx)
        return ESP_ERR_INVALID_ARG;

    bme280_ctx_t *ctx = (bme280_ctx_t *)driver->ctx;
    return driver->bus->ops->write(ctx->address, data, len);
}

esp_err_t bme280_read_id(uint8_t *id)
{
    driver_t *drv = NULL;
    esp_err_t err = get_driver_by_id(BME280_DRIVER_ID, &drv);
    if (err != ESP_OK)
        return err;

    if (!drv || !drv->ctx)
        return ESP_ERR_INVALID_STATE;

    uint8_t reg = BME280_ID_REG;
    err = drv->ops->write(drv, &reg, 1);   
    if (err != ESP_OK)
        return err;

    return drv->ops->read(drv, id, 1);     
}

esp_err_t bme280_read_data(bme280_data_t *data)
{
    driver_t *drv = NULL;
    esp_err_t err = get_driver_by_id(BME280_DRIVER_ID, &drv);
    if (err != ESP_OK)
        return err;

    if (!drv || !drv->ctx)
        return ESP_ERR_INVALID_STATE;

    uint8_t reg = BME280_TEMP_MSB;
    uint8_t raw[3];

    err = drv->ops->write(drv, &reg, 1);
    if (err != ESP_OK)
        return err;

    err = drv->ops->read(drv, raw, 3);
    if (err != ESP_OK)
        return err;

    int32_t raw_temp = ((int32_t)raw[0] << 12) |
                       ((int32_t)raw[1] << 4) |
                       (raw[2] >> 4);

    data->temp = raw_temp / 16384.0f;
    data->humidity = 0.0f;
    data->pressure = 0.0f;

    return ESP_OK;
}
