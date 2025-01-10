/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENOS_SPI_CNTR_H_
#define _SENOS_SPI_CNTR_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get handle to spi bus control
 *
 * @return
 *      - Handle to SPI bus
 */
void *senos_spi_get_ctrl_handle(void);

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SPI_CNTR_H_ */
