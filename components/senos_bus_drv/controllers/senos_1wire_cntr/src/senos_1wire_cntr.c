/*
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senos_bus_drv_private_defs.h"
#include "onewire/onewire_bus.h"
#include "onewire/onewire_device.h"
#include "onewire/onewire_cmd.h"
#include "onewire/onewire_crc.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "../src/rmt_private.h"

#include "esp_err.h"

static esp_err_t senos_1wire_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle);
static esp_err_t senos_1wire_deattach(senos_dev_handle_t *handle);

static senos_bus_drv bus_control = {._attach = &senos_1wire_attach, ._deattach = &senos_1wire_deattach};

static esp_err_t senos_1wire_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle) {
    return ESP_OK;
}

static esp_err_t senos_1wire_deattach(senos_dev_handle_t *handle) {
    return ESP_OK;
}

void *senos_1wire_get_ctrl_handle(void) {
    printf("bus_control.attach: %p\n", bus_control._attach);
    printf("bus_control.deattach: %p\n", bus_control._deattach);
    return &bus_control;
}