/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENOS_BUS_DRV_H_
#define _SENOS_BUS_DRV_H_

#include <inttypes.h>
#include "esp_err.h"
#include "driver/i2c_master.h"  //Are you sure???

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
esp_err_t fnSenosBusAddDevice(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle);
esp_err_t fnSenosBusRemoveDevice(senos_dev_handle_t handle);
esp_err_t fnSenosBusProbeDevice(senos_dev_cfg_t *dev_cfg);
esp_err_t fnSenosBusScanBus(senos_dev_cfg_t *dev_cfg, uint8_t *list, size_t *num_of_devices);

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_BUS_DRV_H_ */
