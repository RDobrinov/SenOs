/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENOS_BUS_DRV_H_
#define _SENOS_BUS_DRV_H_

#include <inttypes.h>
#include "esp_err.h"

#include "senos_bus_devices.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Brief
 *
 * @return
 *      - return
 */
esp_err_t senos_add_device(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle);
esp_err_t senos_remove_device(senos_dev_handle_t handle);
esp_err_t senos_scan_bus(senos_dev_cfg_t *dev_cfg, uint8_t *list, size_t *num_of_devices);

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_BUS_DRV_H_ */
