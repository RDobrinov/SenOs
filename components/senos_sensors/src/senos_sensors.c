/*
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senos_sensor_private.h"
#include "bmx280.h"
#include "senos_sensors.h"

static void *(*senos_get_bus[4])(void) = {NULL, &bmx280_get_interface, NULL, NULL};

esp_err_t senos_sensor_add(senos_sensor_hw_conf_t *dv, senos_sensor_handle_t *handle) {
    senos_sensor_interface_t interface = (senos_sensor_interface_t)(*senos_get_bus[dv->type])();
}
