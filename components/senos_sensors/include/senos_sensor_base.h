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

/** Type of known sensors */
typedef enum {
    SENSOR_DS18X20,
    SENSOR_BMX280,
    SENSOR_MAX31865,
    SENSOR_LAST
} senos_known_sensors_t;

/** Sensor hardware descriptor
 *  Define connections and other hardware characteristics. Cannot be changed after attach
 */
typedef struct {
    senos_known_sensors_t type;
    union {
        // DS Series Sensors
        struct {
            uint64_t rom_code;  /*!< Dallas 1-Wire ROM code */
            gpio_num_t gpio;    /*!< 1-Wire IO Pin */
        } ds18x20;
        // Bosch BMP/BME Sensors
        struct {
            uint32_t scl:8;         /*!< SCL IO Pin */
            uint32_t sda:8;         /*!< SDA IO Pin */
            uint32_t spi3w:1;       /*!< SPI mode in usage (Not implemented) */
            uint32_t reserved:15;   /*!< Not used */
        } bmx280;
        // MAX31865 Pt100 module
        struct {
            uint32_t mosi:8;            /*!< MOSI/SDO IO Pin */
            uint32_t miso:8;            /*!< MISO/SDI IO Pin */
            uint32_t sclk:8;            /*!< SCLK/SCL IO Pin */
            uint32_t cs:8;              /*!< CS IO Pin */
            uint32_t filter_select:1;   /*!< Notch filter select */
            uint32_t wires_select:1;    /*!< Wires mode select */
            uint32_t r_ref:16;          /*!< Rref resistance value */
            uint32_t reserved:14;       /*!< Not Used */
        } max31865;
    };
} senos_sensor_hw_conf_t;

typedef struct {
    uint32_t measure_interval:16;
    uint32_t report_every:8;
    uint32_t notused:8;
} senos_sensor_meas_timing_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSOR_BASE_H_ */