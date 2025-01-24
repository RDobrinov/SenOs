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