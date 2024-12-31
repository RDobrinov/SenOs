/*
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senos_bus_drv_private_defs.h"
#include "driver/i2c_master.h"
#include "../i2c_private.h"

#include "esp_err.h"

static esp_err_t senos_i2c_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle);
static esp_err_t senos_i2c_deattach(senos_dev_handle_t *handle);

static senos_bus_drv bus_control = {._attach = &senos_i2c_attach, ._deattach = &senos_i2c_deattach};

static esp_err_t senos_i2c_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle) {
    return ESP_OK;
}

static esp_err_t senos_i2c_deattach(senos_dev_handle_t *handle) {
    return ESP_OK;
}

void *senos_i2c_get_ctrl_handle(void) {
    printf("bus_control.attach: %p\n", bus_control._attach);
    printf("bus_control.deattach: %p\n", bus_control._deattach);
    return &bus_control;
}