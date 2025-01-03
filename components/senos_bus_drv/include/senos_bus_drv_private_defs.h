/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENOS_BUS_DRV_PRIVATE_DEFS_H_
#define _SENOS_BUS_DRV_PRIVATE_DEFS_H_

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

typedef struct {
    esp_err_t (*_attach)(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle);
    esp_err_t (*_deattach)(senos_dev_handle_t handle);
} senos_bus_drv;

typedef senos_bus_drv *senos_bus_drv_t;

typedef struct {
    uint64_t timeouts:16;
    uint64_t crc_error:16;
    uint64_t other:16;
    uint64_t spi_corr:4;
    uint64_t notused:12;
    uint32_t snd;
    uint32_t rcv;
} senos_device_stats_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_BUS_DRV_PRIVATE_DEFS_H_ */
