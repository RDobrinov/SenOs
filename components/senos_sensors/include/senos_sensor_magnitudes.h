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

#define MAGNITUDE_MAX_DECIMALS 4    /*!< Maximum decimals in value returned by sensor drivers */
#define MAGNITUDE_MAX_DEVIDER_COUNT MAGNITUDE_MAX_DECIMALS + 1 /*!< Decimal deviders count */

extern const uint16_t senos_sensor_magnitude_divider[];

typedef enum {
    MAGNITUDE_NONE,
    MAGNITUDE_TEMPERATURE,
    MAGNITUDE_HUMIDITY,
    MAGNITUDE_PRESSURE,
    MAGNITUDE_LAST
} senos_magnitudes_t;

typedef enum {
    METRIC_NONE,
    METRIC_DEGREES,
    METRIC_PRECENTAGE,
    METRIC_HECTOPASCSAL
} senos_metrics_t;

typedef struct {
    /** REMARK FOR DEVELOPER
     * Never insert new variable in first 16 bits, use reserved only
     * This will broke _get private function implementation
     */
    struct {
        uint32_t type:6;            /*!< Magnitude type */
        uint32_t metric:6;          /*!< Magnitude metric */
        uint32_t decimals:3;        /*!< Magnitide decimals provided by sensor */
        uint32_t valid:1;           /*!< Data is valid */
        uint32_t resolution:4;      /*!< Sensor resolution 1 low to 15 high. 0 means Auto/Default */
        uint32_t iir_filter:3;      /*!< IIR Filter order. Custom implementation for diffrent sensors */
        uint32_t oversampling:3;    /*!< Oversampling 0=No oversampling/Magnitude disabled */
        uint32_t reserved:6;
    };
    uint32_t value;             /*!< Magnitude value */
} senos_sensor_magnitude_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSOR_MAGNITUDES_H_ */