/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * MultiSense OS base sensor definitions
 */

#ifndef _SENOS_SENSOR_PRIVATE_H_
#define _SENOS_SENSOR_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>
#include "esp_err.h"

#include "senos_sensor_base.h"

typedef struct {
    esp_err_t (*_add)(void *data, senos_sensor_api_t **handle);
    esp_err_t (*_remove)(void *handle);
} senos_sensor_interface;

typedef senos_sensor_interface *senos_sensor_interface_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSOR_PRIVATE_H_ */