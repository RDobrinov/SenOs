/**
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _SENOS_MAX31865_DRIVER_H_
#define _SENOS_MAX31865_DRIVER_H_

#include <inttypes.h>
#include "senos_bus_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Init MAX31865 sensor driver
 *
 * @return Handle to driver API
 */
void *max31865_get_driver(void);

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_MAX31865_DRIVER_H_ */