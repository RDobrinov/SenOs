/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENOS_SENSORS_H_
#define _SENOS_SENSORS_H_

#include <inttypes.h>
#include "esp_event.h"
#include "esp_err.h"

#include "senos_sensor_base.h"
//#include "senos_sensor_private.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SENSOR_EVENT_SENSOR_ADDED,
    SENSOR_EVENT_SENSOR_REMOVED,
    SENSOR_EVENT_DATA
} senos_sensor_event_t;

ESP_EVENT_DECLARE_BASE(SENSOR_EVENT);

typedef struct {
    senos_sensor_hw_conf_t hw_cfg;
    senos_sensor_meas_timing_t timing;
} senos_sensor_config_t;

esp_err_t fnSenosSensorInit(void);
esp_err_t fnSenosSensorGetEvtLoop(esp_event_loop_handle_t *evt_loop);
esp_err_t fnSenosSensorScan(senos_sensor_hw_conf_t *dv, uint8_t *list, size_t *len);
esp_err_t fnSenosSensorAdd(senos_sensor_config_t *dv);

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSORS_H_ */