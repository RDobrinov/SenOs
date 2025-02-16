/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * MultiSense OS magnitudes
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

/** Magnitude decimal divider values */
extern const uint16_t senos_sensor_magnitude_divider[];

/** Magnitudes */
typedef enum {
    MAGNITUDE_NONE,         /*!< No magnitude */
    MAGNITUDE_TEMPERATURE,  /*!< Temperature */
    MAGNITUDE_HUMIDITY,     /*!< Relative humidity */
    MAGNITUDE_PRESSURE,     /*!< Pressure */
    MAGNITUDE_LAST          /*!< Failsafe magnitude */
} senos_magnitudes_t;

/** Magnitude metrics */
typedef enum {
    METRIC_NONE = 0,    /*!< No metric */
    METRIC_DEGREES,     /*!< Degrees */
    METRIC_PRECENTAGE,  /*!< Precentage */
    METRIC_PASCAL,      /*!< Pascal */
    METRIC_HECTOPASCAL  /*!< Hectopascal */
} senos_metrics_t;

/** Type of sensor magnitude */
typedef struct {
    /** REMARK FOR DEVELOPER
     * Never insert new variable in first 17 bits, use reserved only
     * This will broke _get private function implementation
     */
    struct {
        uint32_t type:6;            /*!< Magnitude type */
        uint32_t metric:6;          /*!< Magnitude metric */
        uint32_t decimals:3;        /*!< Magnitide decimals provided by sensor */
        uint32_t valid:1;           /*!< Data is valid */
        uint32_t report_valid:1;    /*!< Report value loaded */
        uint32_t resolution:4;      /*!< Sensor resolution 1 low to 15 high. 0 means Auto/Default */
        uint32_t iir_filter:3;      /*!< IIR Filter order. Custom implementation for different sensors */
        uint32_t oversampling:3;    /*!< Oversampling 0=No oversampling/Magnitude disabled */
        uint32_t iir_init:1;        /*!< IIR Filter init value loaded */
        uint32_t reserved:4;
    };
    uint32_t value;         /*!< Magnitude value */
    uint32_t report_value;  /*!< Report value. DO NOT CHANGE INSIDE _get */
} senos_sensor_magnitude_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSOR_MAGNITUDES_H_ */