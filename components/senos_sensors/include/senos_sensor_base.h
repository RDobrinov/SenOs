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
    uint32_t magnitude_index:4; /*!< Magnitude index */
    uint32_t magnitude:6;       /*!< Magnitude type */
    uint32_t decimals:4;        /*!< Magnitide decimals */
    uint32_t resolution:4;      /*!< Sensor resolution 1 low to 15 high. 0 means Auto/Default */
    uint32_t iir_filter:1;      /*!< IIR Filter active */
    uint32_t oversampling:3;    /*!< Oversampling 0=No oversampling/Magnitude disabled */
    uint32_t reserved:10;
} senos_sensor_magnitude_t;

typedef struct {
    senos_known_sensors_t type;
    union {
        struct {
            uint64_t rom_code;
            gpio_num_t gpio;
        } ds18x20;
        struct {
            uint32_t scl:8;
            uint32_t sda:8;
            uint32_t spi3w:1;
            uint32_t reserved:15;
        } bmx280;
        struct {
            uint32_t mosi:8;
            uint32_t miso:8;
            uint32_t sclk:8;
            uint32_t cs:8;
        } max31865;
    };
} senos_sensor_hw_conf_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSOR_BASE_H_ */