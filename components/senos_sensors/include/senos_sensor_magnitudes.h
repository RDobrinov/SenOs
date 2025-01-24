/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * MultiSense OS base sensor definitions
 */

#ifndef _SENOS_SENSOR_MAGNITUDES_H_
#define _SENOS_SENSOR_MAGNITUDES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

const uint16_t senos_sensor_magnitude_devider[] = {1, 10, 100, 1000, 10000};

typedef enum {
    MAGNITUDE_NONE,
    MAGNITUDE_TEMPERATURE,
    MAGNITUDE_HUMIDITY,
    MAGNITUDE_PRESSURE,
    MAGNITUDE_LAST
} senos_magnitudes_t;

typedef struct {
    uint32_t type:6;       /*!< Magnitude type */
    uint32_t decimals:3;        /*!< Magnitide decimals provided by sensor */
    uint32_t sel_decimals:3;    /*!< Magnitide decimals selected by user */
    uint32_t resolution:4;      /*!< Sensor resolution 1 low to 15 high. 0 means Auto/Default */
    uint32_t iir_filter:3;      /*!< IIR Filter order. Custom implementation for diffrent sensors */
    uint32_t oversampling:3;    /*!< Oversampling 0=No oversampling/Magnitude disabled */
    uint32_t valid:1;           /*!< Data is valid */
    uint32_t reserved:9;
    uint32_t value;                /*!< Magnitude value */
} senos_sensor_magnitude_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSOR_MAGNITUDES_H_ */