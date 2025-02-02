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
#include "senos_sensor_magnitudes.h"

typedef struct senos_sensor_mag_caps {
    senos_sensor_magnitude_t magnitude;
    struct senos_sensor_mag_caps *next;
} senos_sensor_mag_caps_t;

typedef struct {
    senos_sensor_mag_caps_t *mag_caps;
    uint32_t warmup_time;
    uint32_t prepare_time;
    uint32_t measure_time;
    uint32_t cooldown_time;
} senos_sensor_caps_t;

typedef struct {
    esp_err_t (*_init)(void *handle);
    esp_err_t (*_prepare)(void *handle);
    esp_err_t (*_measure)(void *handle);
    esp_err_t (*_read)(void *handle);
    esp_err_t (*_getvalue)(void *handle, senos_sensor_mag_caps_t *magnitudes);
    esp_err_t (*_getcaps)(void *handle, senos_sensor_caps_t *caps);
    esp_err_t (*_config)(void *handle, senos_sensor_mag_caps_t *magnitudes);
    uint32_t (*_getid)(void *handle);
    bool (*_ready)(void *handle);
} senos_sensor_api;

typedef senos_sensor_api *senos_sensor_handle_t;

typedef struct {
    esp_err_t (*_add)(senos_sensor_hw_conf_t *config, senos_sensor_handle_t **handle);
    esp_err_t (*_remove)(void *handle);
} senos_sensor_interface;

typedef senos_sensor_interface *senos_sensor_interface_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSOR_PRIVATE_H_ */