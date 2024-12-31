/*
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senos_bus_drv.h"
#include "senos_bus_drv_private_defs.h"
#include "senos_i2c_cntr.h"

void *(*senos_get_bus[3])(void) = {NULL, &senos_i2c_get_ctrl_handle, NULL};

esp_err_t senos_add_device(senos_dev_cfg_t *dev_cfg) {
    senos_bus_drv_t bus = (senos_bus_drv_t)(*senos_get_bus[1])();
    //senos_bus_drv_t bus = (senos_bus_drv_t)senos_i2c_get_ctrl_handle();
    if(!bus) return ESP_FAIL; 
    printf("Bus Pointer: %p\n", bus);
    printf("bus->attach: %p\n", bus->_attach);
    printf("bus->attach: %p\n", bus->_deattach);
    senos_dev_handle_t *i2c_device = (senos_dev_handle_t *)calloc(1, sizeof(senos_dev_handle_t));
    bus->_attach(dev_cfg, i2c_device);
    return ESP_OK;
}