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

/** Common bus command structure */

typedef struct sensos_drv_api {
    esp_err_t (*_read)(senos_dev_transaction_t *transaction, void *handle);
    esp_err_t (*_write)(senos_dev_transaction_t *transaction, void *handle);
    esp_err_t (*_wr)(senos_dev_transaction_t *transaction, void *handle);
    esp_err_t (*_reset)(void *handle);
    esp_err_t (*_scan)(senos_dev_transaction_t *transaction, void *handle);
} sensos_drv_api_t;

typedef struct {
    uint32_t device_id;
    sensos_drv_api_t *base;
} senos_dev_handle;

typedef senos_dev_handle *senos_dev_handle_t;

typedef struct {
    esp_err_t (*_attach)(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle);
    esp_err_t (*_deattach)(senos_dev_handle_t *handle);
} senos_bus_drv;

typedef senos_bus_drv *senos_bus_drv_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_BUS_DRV_PRIVATE_DEFS_H_ */
