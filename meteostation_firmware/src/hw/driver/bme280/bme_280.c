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
#define BME280_TEMP_MSB  0xF7

#define BME280_DRIVER_ID 0x00
#define I2C_BUS_ID       0x00

typedef struct {
    uint16_t t1;
    int16_t t2, t3;

    uint16_t p1;
    int16_t p2, p3, p4, p5, p6, p7, p8, p9;

    uint8_t h1;
    int16_t h2;
    uint8_t h3;
    int16_t h4;
    int16_t h5;
    int8_t  h6;
} bme_calibration_data_t;

typedef struct {
    uint8_t address;
    bme_calibration_data_t calibration_data;
} bme280_ctx_t;

static esp_err_t bme280_init(driver_t *driver);
static esp_err_t bme280_destruct(driver_t *driver);
static esp_err_t bme280_read(driver_t *driver, void* data, size_t len);
static esp_err_t bme280_write(driver_t *driver, const void* data, size_t len);


DEFINE_DRIVER_REGISTER(
    BME280_DRIVER_ID,
    bme280,
    NULL,
    bme280_init,
    bme280_destruct,
    bme280_read,
    bme280_write
);

esp_err_t calibrate(driver_t* driver)
{
    if (!driver || !driver->ctx)
        return ESP_ERR_INVALID_ARG;

    bme280_ctx_t* ctx = (bme280_ctx_t*)driver->ctx;
    bme_calibration_data_t* cd = &ctx->calibration_data;

    uint8_t calib1[26]; 
    uint8_t calib2[7];   

    bus_t* bus = driver->bus;

    uint8_t reg = 0x88;
    esp_err_t err = bus->ops->write(ctx->address, &reg, 1);
    if (err != ESP_OK) return err;

    err = bus->ops->read(ctx->address, calib1, 26);
    if (err != ESP_OK) return err;

    reg = 0xE1;
    err = bus->ops->write(ctx->address, &reg, 1);
    if (err != ESP_OK) return err;

    err = bus->ops->read(ctx->address, calib2, 7);
    if (err != ESP_OK) return err;

    cd->t1 = (uint16_t)(calib1[1] << 8 | calib1[0]);
    cd->t2 = (int16_t)(calib1[3] << 8 | calib1[2]);
    cd->t3 = (int16_t)(calib1[5] << 8 | calib1[4]);

    cd->p1 = (uint16_t)(calib1[7] << 8 | calib1[6]);
    cd->p2 = (int16_t)(calib1[9] << 8 | calib1[8]);
    cd->p3 = (int16_t)(calib1[11] << 8 | calib1[10]);
    cd->p4 = (int16_t)(calib1[13] << 8 | calib1[12]);
    cd->p5 = (int16_t)(calib1[15] << 8 | calib1[14]);
    cd->p6 = (int16_t)(calib1[17] << 8 | calib1[16]);
    cd->p7 = (int16_t)(calib1[19] << 8 | calib1[18]);
    cd->p8 = (int16_t)(calib1[21] << 8 | calib1[20]);
    cd->p9 = (int16_t)(calib1[23] << 8 | calib1[22]);

    cd->h1 = calib1[25];

    cd->h2 = (int16_t)(calib2[1] << 8 | calib2[0]);
    cd->h3 = calib2[2];

    cd->h4 = (int16_t)((calib2[3] << 4) | (calib2[4] & 0x0F));
    cd->h5 = (int16_t)((calib2[5] << 4) | (calib2[4] >> 4));

    cd->h6 = (int8_t)calib2[6];

    ESP_LOGI(TAG, "Calibration data loaded.");

    return ESP_OK;
}


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
        BME280_CTRL_HUM,  0x05,
        BME280_CTRL_MEAS, 0x57,
        BME280_CONFIG,    0xB0
    };

    for (int i = 0; i < sizeof(setup); i += 2)
        bus->ops->write(ctx->address, &setup[i], 2);

    ESP_LOGI(TAG, "BME280 initialized successfully.");

    ESP_LOGI(TAG, "Start reading calibration data...");

    err = calibrate(driver);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Calibration is not successful!. Abort");
        return ESP_ERR_NOT_FINISHED;
    }

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

static esp_err_t bme280_read(driver_t *driver, void* data, size_t len)
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

static esp_err_t bme280_write(driver_t *driver, const void *data, size_t len)
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

typedef struct {
    float temp_degree;
    int32_t fine_tune_temp;
} temp_data_t;

temp_data_t compensate_temp(bme_calibration_data_t* calib, float raw_temp) {
    double var1, var2;

    var1 = (((double)raw_temp)/16384.0 - ((double)calib->t1)/1024.0) * ((double)calib->t2);

    var2 = ((((double)raw_temp)/131072.0 - ((double)calib->t1)/8192.0) *
         (((double)raw_temp)/131072.0 - ((double)calib->t1)/8192.0)) * ((double)calib->t3);

    temp_data_t temp_data = {
        .fine_tune_temp = (var1 + var2),
        .temp_degree = (var1 + var2) / 5120.0
    };

    return temp_data;
}

float compensate_humidity(bme_calibration_data_t* calib, int32_t fine_temp, float raw_humidity) {
    double var_H;

    var_H = (((double)fine_temp) - 76800.0);

    var_H = (raw_humidity - (((double)calib->h4) * 64.0 + ((double)calib->h5) / 16384.0 * var_H)) *
        (((double)calib->h2) / 65536.0 * (1.0 + ((double)calib->h6) / 67108864.0 * var_H *
        (1.0 + ((double)calib->h3) / 67108864.0 * var_H)));
    var_H = var_H * (1.0 - ((double)calib->h1) * var_H / 524288.0);

    if(var_H > 100.0)
        var_H = 100.0;
    else if(var_H < 0.0)
        var_H = 0.0;

    return var_H;
}

float compensate_pressure(bme_calibration_data_t* calib, int32_t fine_temp, float raw_humidity) {
    double var1, var2, p;
    double pressure_min = 30000.0;
	double pressure_max = 110000.0;

    var1 = ((double)fine_temp/2.0) - 64000.0;
    var2 = var1 * var1 * ((double)calib->p6) / 32768.0;
    var2 = var2 + var1 * ((double)calib->p5) * 2.0;
    var2 = (var2 / 4.0) + (((double)calib->p4) * 65536.0);
    var1 = (((double)calib->p3) * var1 * var1 / 524288.0 + ((double)calib->p2) * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * ((double)calib->p1);

    if(var1 == 0.0){
        return 0;
    }

    if(var1){
        p = 1048576.0 - (double)raw_humidity;
        p = (p - (var2 / 4096.0)) * 6250.0 / var1;
        var1 = ((double)calib->p9) * p * p / 2147483648.0;
        var2 = p * ((double)calib->p8) / 32768.0;
        p = p + (var1 + var2 + ((double)calib->p7)) / 16.0;
        
        if (p < pressure_min)
            p = pressure_min;
        else if (p > pressure_max)
            p = pressure_max;

    } else {
        p = pressure_min;
    }

    return p/100.0; 
}

esp_err_t bme280_read_data(bme280_data_t *data)
{
    driver_t *drv = NULL;
    esp_err_t err = get_driver_by_id(BME280_DRIVER_ID, &drv);
    if (err != ESP_OK)
        return err;

    if (!drv || !drv->ctx)
        return ESP_ERR_INVALID_STATE;

    uint8_t raw_data[8];
    
    err = bme280_read(drv, raw_data, 8);

    if (err != ESP_OK)
        return err;

    uint32_t raw_pres = (raw_data[0] << 12) | (raw_data[1] << 4) | (raw_data[2] >> 4); 
    uint32_t raw_temp = (raw_data[3] << 12) | (raw_data[4] << 4) | (raw_data[5] >> 4); 
    uint32_t raw_hum = (raw_data[6] << 8) | raw_data[7];  

    bme280_ctx_t* ctx = (bme280_ctx_t*)drv->ctx;

    temp_data_t temp_data = compensate_temp(&ctx->calibration_data, raw_temp);
    float compensated_hum = compensate_humidity(&ctx->calibration_data, (int32_t)temp_data.fine_tune_temp, raw_hum);
    float compensated_press = compensate_pressure(&ctx->calibration_data, (int32_t)temp_data.fine_tune_temp, raw_pres);

    data->temp = temp_data.temp_degree;
    data->humidity = compensated_hum;
    data->pressure = compensated_press;

    return ESP_OK;
}
