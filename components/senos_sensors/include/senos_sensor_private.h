/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * MultiSense OS sensor task private structures
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

/** Single magnitude element */
typedef struct senos_sensor_mag_caps {
    senos_sensor_magnitude_t magnitude; /*!< Magnitude descriptor */
    struct senos_sensor_mag_caps *next; /*!< Pointer to next magnitude */
} senos_sensor_mag_caps_t;

/** Sensor and magnitude capabilites */
typedef struct {
    senos_sensor_mag_caps_t *mag_caps;  /*!< First magnitude */
    uint32_t warmup_time;               /*!< Warm Up time in miliseconds */
    uint32_t prepare_time;              /*!< Prepare time in miliseconds */
    uint32_t measure_time;              /*!< Measure time in miliseconds */
    uint32_t cooldown_time;             /*!< Cooldown time in miliseconds */
} senos_sensor_caps_t;

typedef struct {
    senos_sensor_caps_t caps;
    
} senos_sensor_device_t;

typedef struct {
    esp_err_t (*_init)(void *handle);
    esp_err_t (*_prepare)(void *handle);
    esp_err_t (*_measure)(void *handle);
    esp_err_t (*_read)(void *handle);
    esp_err_t (*_getvalue)(void *handle, senos_sensor_mag_caps_t *magnitudes);
    esp_err_t (*_getcaps)(void *handle, senos_sensor_caps_t *caps);
    esp_err_t (*_config)(void *handle, senos_sensor_mag_caps_t *magnitudes);
    uint32_t (*_getid)(void *handle);
    char* (*_getname)(void *handle);
    bool (*_ready)(void *handle);
} senos_sensor_api;

typedef senos_sensor_api *senos_sensor_handle_t;

typedef struct {
    esp_err_t (*_add)(senos_sensor_hw_conf_t *config, senos_sensor_handle_t **handle);
    esp_err_t (*_remove)(void *handle);
} senos_sensor_interface;

typedef senos_sensor_interface *senos_sensor_interface_t;

typedef enum {
    SENOS_SENSOR_INIT,
    SENOS_SENSOR_WAIT,
    SENOS_SENSOR_WARM,
    SENOS_SENSOR_PREPARE,
    SENOS_SENSOR_MEASURE,
    SENOS_SENSOR_READ,
    SENOS_SENSOR_GET,
    SENOS_SENSOR_COOL
} senos_sensor_states_t;

typedef struct {
    senos_sensor_states_t state;
    uint32_t end;
} senos_sensor_timing_t;

typedef struct senos_sensor {
    struct {
        uint32_t wait_for_next;
        uint32_t report:8;
        uint32_t meas_count:8;
        uint32_t not_used:16;
    } meas_conf;
    senos_sensor_timing_t state_timing;
    senos_sensor_caps_t caps;
    senos_sensor_handle_t *handle;
    struct senos_sensor *next;
} senos_sensor_data_t;

typedef senos_sensor_data_t *senos_sensor_data_handle_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSOR_PRIVATE_H_ */