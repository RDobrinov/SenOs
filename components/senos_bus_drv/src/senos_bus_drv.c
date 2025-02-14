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

static void *(*pvSenosBusCtrlHandle[3])(void) = {&fnSenosOneWireCtrlGetHandle, &fnSenosI2CCtrlGetHandle, &fnSenosSPICtrlGetHandle};

esp_err_t fnSenosBusAddDevice(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle) {
    senos_bus_drv_t bus = (senos_bus_drv_t)(*pvSenosBusCtrlHandle[dev_cfg->bus_type])();
    if(!bus) return ESP_ERR_NOT_SUPPORTED; 
    return bus->_attach(dev_cfg, handle);
}

esp_err_t fnSenosBusRemoveDevice(senos_dev_handle_t handle) {
    senos_bus_drv_t bus = (senos_bus_drv_t)(*pvSenosBusCtrlHandle[handle->bus_type])();
    return bus->_deattach(handle);
}

esp_err_t fnSenosBusProbeDevice(senos_dev_cfg_t *dev_cfg) {
    senos_bus_drv_t bus = (senos_bus_drv_t)(*pvSenosBusCtrlHandle[dev_cfg->bus_type])();
    if(!bus) return ESP_FAIL; 
    return bus->_probe(dev_cfg);
}

esp_err_t fnSenosBusScanBus(senos_dev_cfg_t *dev_cfg, uint8_t *list, size_t *num_of_devices) {
    senos_bus_drv_t bus = (senos_bus_drv_t)(*pvSenosBusCtrlHandle[dev_cfg->bus_type])();
    if(!bus) return ESP_FAIL; 
    return bus->_scanbus(dev_cfg, list, num_of_devices);
}