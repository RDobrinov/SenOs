/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENOS_I2C_CNTR_H_
#define _SENOS_I2C_CNTR_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get handle to i2c bus control
 *
 * @return
 *      - Handle to I2C bus
 */
void *senos_i2c_get_ctrl_handle(void);

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_I2C_CNTR_H_ */
