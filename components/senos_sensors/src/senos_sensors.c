/*
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senos_bus_drv.h"
#include "senos_sensors.h"
#include "senos_sensor_private.h"
#include "ds18x20.h"
#include "bmx280.h"
#include "max31865.h"

const uint16_t senos_sensor_magnitude_divider[] = {1, 10, 100, 1000, 10000};

static void *(*senos_get_bus[4])(void) = {&ds18x20_get_interface, &bmx280_get_interface, &max31865_get_interface, NULL};
static senos_drv_bus_t _sensor2bus[3] = {SENOS_BUS_1WIRE, SENOS_BUS_I2C, SENOS_BUS_SPI};

esp_err_t senos_sensor_scan(senos_sensor_hw_conf_t *dv, uint8_t *list, size_t *len) {
    if(dv->type != SENSOR_DS18X20) return ESP_ERR_NOT_SUPPORTED;
    senos_dev_cfg_t dc = {
        .bus_type = _sensor2bus[dv->type],
        .dev_1wire = {
            .data_gpio = dv->ds18x20.gpio
        }
    };
    return senos_scan_bus(&dc, list, len);
}

esp_err_t senos_sensor_add(senos_sensor_hw_conf_t *dv, senos_sensor_handle_t **handle) {
    *handle = NULL;
    if(!(*senos_get_bus[dv->type])) return ESP_ERR_NOT_SUPPORTED;
    senos_sensor_interface_t interface = (senos_sensor_interface_t)(*senos_get_bus[dv->type])();
    if(!interface) return ESP_ERR_NOT_SUPPORTED;
    return interface->_add(dv,handle);
}
