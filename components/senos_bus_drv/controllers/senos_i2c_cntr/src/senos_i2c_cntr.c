/*
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senos_bus_drv_private_defs.h"
#include "driver/i2c_master.h"
#include "../i2c_private.h"

#include "senos_i2c_private.h"
#include "idf_gpio_driver.h"

#include "esp_err.h"

/** Type of device list element for attached i2c devices */
typedef struct senos_i2c_device {
    uint32_t device_id;                 /*!< Device ID */
    struct {
        uint32_t cmd_bytes:2;       /*!< Device command bytes 0-2 */
        uint32_t addr_bytes:4;      /*!< Device command bytes 0-8 */
        uint32_t xfer_timeout_ms:4; /*!< Device timeout */
        uint32_t not_used:22;       /*!< Not used */
    };
    senos_device_stats_t stats;     /*!< Device stats counters */
    i2c_master_dev_handle_t handle; /*!< I2C master device handle */
    struct senos_i2c_device *next;/*!< Pointer to next elements */
} senos_i2c_device_t;


static esp_err_t senos_i2c_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle);
static esp_err_t senos_i2c_deattach( senos_dev_handle_t handle);
static esp_err_t senos_i2c_bus_scan(senos_dev_cfg_t *dev_cfg, uint8_t *list, size_t *num_of_devices);

static esp_err_t senos_i2c_read(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_i2c_write(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_i2c_wr(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_i2c_reset(void *handle);

static senos_i2c_device_t *device_list = NULL;
static senos_bus_drv bus_control = {._attach = &senos_i2c_attach, ._deattach = &senos_i2c_deattach, ._scanbus = &senos_i2c_bus_scan};
static senos_drv_api senos_1wire_api = {
    ._read = &senos_i2c_read,
    ._write = &senos_i2c_write,
    ._wr = &senos_i2c_read,
    ._reset = &senos_i2c_reset
};

void *senos_i2c_get_ctrl_handle(void) {
    return &bus_control;
}

static esp_err_t senos_i2c_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle) {
    if(dev_cfg->bus_type != SENOS_BUS_I2C) return ESP_ERR_INVALID_ARG;
    uint64_t pinmask = (BIT64(dev_cfg->dev_i2c.scl_gpio) | BIT64(dev_cfg->dev_i2c.sda_gpio));
    i2c_master_bus_handle_t bus_handle = NULL;
    bool new_bus_handle = false;
    *handle = NULL;
    for(senos_i2c_device_t *device = device_list; device != NULL && !bus_handle; device = device->next) {
        if(device->handle->master_bus->base->scl_num == dev_cfg->dev_i2c.scl_gpio && device->handle->master_bus->base->sda_num == dev_cfg->dev_i2c.sda_gpio) 
            bus_handle = device->handle->master_bus;
    }
    return ESP_OK;
}
static esp_err_t senos_i2c_deattach( senos_dev_handle_t handle) {
    return ESP_OK;
}
static esp_err_t senos_i2c_bus_scan(senos_dev_cfg_t *dev_cfg, uint8_t *list, size_t *num_of_devices) {
    return ESP_OK;
}

static esp_err_t senos_i2c_read(senos_dev_transaction_t *transaction, void *handle) {
    return ESP_OK;
}

static esp_err_t senos_i2c_write(senos_dev_transaction_t *transaction, void *handle) {
    return ESP_OK;
}

static esp_err_t senos_i2c_wr(senos_dev_transaction_t *transaction, void *handle) {
    return ESP_OK;
}

static esp_err_t senos_i2c_reset(void *handle)
{
    return ESP_OK;
}