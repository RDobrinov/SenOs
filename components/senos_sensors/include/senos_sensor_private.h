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
    esp_err_t (*_init)(void *handle);
    esp_err_t (*_prepare)(void *handle);
    esp_err_t (*_measure)(void *handle);
    esp_err_t (*_read)(void *handle);
    esp_err_t (*_get)(void *handle, senos_sensor_magnitude_t *magnitude, float *value);
    esp_err_t (*_getcaps)(void *handle, senos_sensor_magnitude_t *magnitudes, size_t *len);
    esp_err_t (*_config)(void *handle, senos_sensor_magnitude_t *magnitudes, size_t *len);
    bool (*_ready)(void *handle);
} senos_sensor_api;

typedef senos_sensor_api *senos_sensor_handle_t;

typedef struct {
    esp_err_t (*_add)(senos_sensor_hw_conf_t *config, senos_sensor_handle_t *handle);
    esp_err_t (*_remove)(void *handle);
} senos_sensor_interface;

typedef senos_sensor_interface *senos_sensor_interface_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSOR_PRIVATE_H_ */