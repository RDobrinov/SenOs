/**
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _SENOS_MAX31865_DRIVER_H_
#define _SENOS_MAX31865_DRIVER_H_

#include <inttypes.h>

#define MAX31865_FILTER_60HZ 0
#define MAX31865_FILTER_50HZ 1

#define MAX31865_2WIRE_SENSOR 0
#define MAX31865_3WIRE_SENSOR 1
#define MAX31865_4WIRE_SENSOR 0

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Init MAX31865 sensor driver
 *
 * @return Handle to driver API
 */
void *max31865_get_interface(void);

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_MAX31865_DRIVER_H_ */