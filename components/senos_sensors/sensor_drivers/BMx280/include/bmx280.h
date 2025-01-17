/**
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _SENOS_BMX280_DRIVER_H_
#define _SENOS_BMX280_DRIVER_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Init BMx260 sensor driver
 *
 * @return Handle to driver API
 */
void *bmx280_get_driver(void);

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_BMX280_DRIVER_H_ */