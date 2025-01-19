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
    esp_err_t (*_probe)(senos_dev_cfg_t *dev_cfg);
    esp_err_t (*_scanbus)(senos_dev_cfg_t *dev_cfg, uint8_t *list, size_t *num_of_devices);
} senos_bus_drv;

typedef senos_bus_drv *senos_bus_drv_t;

typedef struct {
    uint32_t snd;
    uint32_t rcv;
    uint64_t timeouts:16;
    uint64_t crc_error:16;
    uint64_t other:16;
    uint64_t notused:12;
    uint64_t spi_reserved:4;
} senos_device_stats_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_BUS_DRV_PRIVATE_DEFS_H_ */