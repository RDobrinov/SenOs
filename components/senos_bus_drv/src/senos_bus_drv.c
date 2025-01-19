/*
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senos_bus_drv.h"
#include "senos_bus_drv_private_defs.h"
#include "senos_i2c_cntr.h"
#include "senos_1wire_cntr.h"
#include "senos_spi_cntr.h"

static void *(*senos_get_bus[3])(void) = {&senos_1wire_get_ctrl_handle, &senos_i2c_get_ctrl_handle, &senos_spi_get_ctrl_handle};

esp_err_t senos_add_device(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle) {
    //handle = NULL;
    senos_bus_drv_t bus = (senos_bus_drv_t)(*senos_get_bus[dev_cfg->bus_type])();
    if(!bus) return ESP_FAIL; 
    //senos_dev_handle_t new_device;
    printf("\n *** handle %p *** \n\n", handle);
    esp_err_t ret = bus->_attach(dev_cfg, handle);
    //if( ESP_OK == ret) *handle = new_device;
    return ret;
    /*
    //bus->_attach(dev_cfg, &(new_device->);
    //(*(new_device->base))->
    //printf("senos_add_device [%08lX] new_device:%p, api:%p\n", new_device->device_id, new_device, new_device->api);
    senos_drv_api_t driver = *(new_device->api);
    senos_dev_transaction_t transaction = {
        .dev_cmd = 0x44,
        .rdBytes = 9,
        .wrBytes = 0
    };
    driver->_read(&transaction, new_device);
    bus->_deattach(new_device);
    return ESP_OK;
    */
}

esp_err_t senos_remove_device(senos_dev_handle_t handle) {
    senos_bus_drv_t bus = (senos_bus_drv_t)(*senos_get_bus[handle->bus_type])();
    return bus->_deattach(handle);
}

esp_err_t senos_probe_device(senos_dev_cfg_t *dev_cfg) {
    senos_bus_drv_t bus = (senos_bus_drv_t)(*senos_get_bus[dev_cfg->bus_type])();
    if(!bus) return ESP_FAIL; 
    return bus->_probe(dev_cfg);
}

esp_err_t senos_scan_bus(senos_dev_cfg_t *dev_cfg, uint8_t *list, size_t *num_of_devices) {
    senos_bus_drv_t bus = (senos_bus_drv_t)(*senos_get_bus[dev_cfg->bus_type])();
    if(!bus) return ESP_FAIL; 
    return bus->_scanbus(dev_cfg, list, num_of_devices);
}