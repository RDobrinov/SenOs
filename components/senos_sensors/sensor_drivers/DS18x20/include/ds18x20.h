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

#define DS18X20_RESOLUTION_9   0
#define DS18X20_RESOLUTION_10  1
#define DS18X20_RESOLUTION_11  2
#define DS18X20_RESOLUTION_12  3

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Init DS18X20 sensor driver
 *
 * @return Handle to driver API
 */
void *ds18x20_get_interface(void);

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_DS18X20_DRIVER_H_ */