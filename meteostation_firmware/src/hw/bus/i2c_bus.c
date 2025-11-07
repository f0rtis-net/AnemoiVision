#include "include/bus.h"
#include <esp_log.h>
#include <driver/i2c_master.h>

#define I2C_BUS_ID        0x0
#define I2C_MASTER_NUM    I2C_NUM_0
#define I2C_MASTER_SDA_IO GPIO_NUM_4
#define I2C_MASTER_SCL_IO GPIO_NUM_3

static const char *TAG = "I2C_BUS";

static i2c_master_bus_handle_t bus_handle = NULL;

typedef struct {
    uint8_t addr;
    i2c_master_dev_handle_t dev;
} i2c_device_entry_t;

static i2c_device_entry_t devices[8];
static size_t device_count = 0;


DEFINE_BUS_REGISTER(
    I2C_BUS_ID,
    i2c,
    init_i2c_bus,
    destroy_i2c_bus,
    read_i2c_bus,
    write_i2c_bus
)

esp_err_t init_i2c_bus(void)
{
    ESP_LOGI(TAG, "Initializing I2C bus...");

    if (bus_handle) {
        ESP_LOGW(TAG, "Bus already initialized");
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2C bus: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C bus ready.");
    return ESP_OK;
}

esp_err_t destroy_i2c_bus(void)
{
    if (!bus_handle) return ESP_ERR_INVALID_STATE;

    for (size_t i = 0; i < device_count; i++) {
        if (devices[i].dev) {
            i2c_master_bus_rm_device(devices[i].dev);
            devices[i].dev = NULL;
        }
    }
    device_count = 0;

    esp_err_t err = i2c_del_master_bus(bus_handle);
    bus_handle = NULL;

    ESP_LOGI(TAG, "I2C bus destroyed.");
    return err;
}

static i2c_master_dev_handle_t get_device_handle(uint8_t addr)
{
    for (size_t i = 0; i < device_count; i++) {
        if (devices[i].addr == addr)
            return devices[i].dev;
    }

    if (device_count >= sizeof(devices)/sizeof(devices[0])) {
        ESP_LOGE(TAG, "I2C device table full!");
        return NULL;
    }

    i2c_device_config_t dev_cfg = {
        .device_address = addr,
        .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t dev;
    esp_err_t err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device 0x%02X: %s", addr, esp_err_to_name(err));
        return NULL;
    }

    devices[device_count++] = (i2c_device_entry_t){ .addr = addr, .dev = dev };
    ESP_LOGI(TAG, "Device 0x%02X registered on bus", addr);
    return dev;
}

esp_err_t read_i2c_bus(uint8_t addr, uint8_t *data, size_t len)
{
    if (!bus_handle) return ESP_ERR_INVALID_STATE;
    i2c_master_dev_handle_t dev = get_device_handle(addr);
    if (!dev) return ESP_FAIL;

    esp_err_t err = i2c_master_receive(dev, data, len, -1);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "I2C read error from 0x%02X: %s", addr, esp_err_to_name(err));

    return err;
}

esp_err_t write_i2c_bus(uint8_t addr, const uint8_t *data, size_t len)
{
    if (!bus_handle) return ESP_ERR_INVALID_STATE;
    i2c_master_dev_handle_t dev = get_device_handle(addr);
    if (!dev) return ESP_FAIL;

    esp_err_t err = i2c_master_transmit(dev, data, len, -1);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "I2C write error to 0x%02X: %s", addr, esp_err_to_name(err));

    return err;
}
