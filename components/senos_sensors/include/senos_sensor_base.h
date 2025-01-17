/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * MultiSense OS base sensor definitions
 */

#ifndef _SENOS_SENSOR_BASE_H_
#define _SENOS_SENSOR_BASE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"

/**
 * @brief Create new 1-Wire bus handle
 *
 * @return
 *      - Created handle
 */

typedef enum {
    SENSOR_DS18x20,
    SENSOR_BMx280,
    SENSOR_MAX31865,
    SENSOR_LAST
} senos_known_sensors_t;

typedef enum {
    MAGNITUDE_NONE,
    MAGNITUDE_TEMPERATURE,
    MAGNITUDE_HUMIDITY,
    MAGNITUDE_PRESSURE,
    MAGNITUDE_LAST
} senos_magnitudes_t;

typedef struct {
    uint32_t magnitude_index:4;
    uint32_t magnitude:8;
    uint32_t decimals:4;
    uint32_t reserved:16;
} senos_sensor_magnitude_t;

typedef struct {
    senos_known_sensors_t type;
    union {
        struct {
            uint64_t rom_code;
            gpio_num_t gpio;
        } ds18x20;
        struct {
            uint32_t scl:16;
            uint32_t sda:16;
        } bmx280;
        struct {
            uint32_t mosi:8;
            uint32_t miso:8;
            uint32_t sclk:8;
            uint32_t cs:8;
        } max31865;
    };
} senos_sensor_hw_conf_t;

typedef struct {
    uint32_t resolution:4;      /*!< Sensor resolution 1 low to 15 high. 0 means Auto/Default */
    uint32_t iir_filter:1;      /*!< IIR Filter active */
    uint32_t oversampling:3;    /*!< Oversampling 0=No oversampling */
    uint32_t reserved:24;       /*!< Not used */
} senos_sensor_base_conf_t;

typedef struct {
    esp_err_t (*_init)(void *handle);
    esp_err_t (*_prepare)(void *handle);
    esp_err_t (*_measure)(void *handle);
    esp_err_t (*_read)(void *handle);
    esp_err_t (*_get)(void *handle, senos_sensor_magnitude_t *magnitude, float *value);
    esp_err_t (*_magnitudes)(void *handle, senos_sensor_magnitude_t *magnitudes, size_t *len);
    esp_err_t (*_config)(void *handle, senos_sensor_base_conf_t *conf);
    bool (*_ready)(void *handle);
} senos_sensor_api;

typedef senos_sensor_api *senos_sensor_api_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSOR_BASE_H_ */