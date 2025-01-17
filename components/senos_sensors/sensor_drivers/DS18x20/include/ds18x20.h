/**
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _SENOS_DS18X20_DRIVER_H_
#define _SENOS_DS18X20_DRIVER_H_

#include <inttypes.h>
#include "senos_bus_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Init DS18X20 sensor driver
 *
 * @return Handle to driver API
 */
void *ds18x20_get_driver(void);

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_DS18X20_DRIVER_H_ */