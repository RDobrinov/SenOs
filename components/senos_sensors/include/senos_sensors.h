/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENOS_SENSORS_H_
#define _SENOS_SENSORS_H_

#include <inttypes.h>
#include "esp_err.h"

#include "senos_sensor_base.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t senos_sensor_add(senos_sensor_hw_conf_t *dv);

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SENSORS_H_ */