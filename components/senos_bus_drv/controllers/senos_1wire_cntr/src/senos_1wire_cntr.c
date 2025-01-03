/*
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "onewire/onewire_bus.h"
#include "onewire/onewire_device.h"
#include "onewire/onewire_cmd.h"
#include "onewire/onewire_crc.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "../src/rmt_private.h"

#include "senos_bus_drv_private_defs.h"
#include "senos_1wire_private.h"

#include "idf_gpio_driver.h"

#include "esp_err.h"

typedef struct senos_1wire_device {
    struct {
        uint32_t cmd_bytes:2;       /*!< Device command bytes 0-2 */
        uint32_t addr_bytes:4;      /*!< Device command bytes 0-8 */
        uint32_t crc_check:1;       /*!< Check 1-Wire crc */
        uint32_t not_used:25;       /*!< Not used */
    };
    uint32_t device_id;             /*!< 1-Wire device id */ ///???????
    uint64_t address;               /*!< 1-Wire ROM address */
    senos_device_stats_t stats;     /*!< Device stats counters */
    onewire_bus_handle_t handle;    /*!< 1-Wire bus handle */
    senos_drv_api_t base;           /*!< Device API handle */
    struct senos_1wire_device *next;/*!< Pointer to next device */
} senos_1wire_device_t;

static esp_err_t senos_1wire_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle);
static esp_err_t senos_1wire_deattach( senos_dev_handle_t handle);

static esp_err_t senos_1wire_read(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_1wire_write(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_1wire_wr(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_1wire_reset(void *handle);
static esp_err_t senos_1wire_scan(senos_dev_transaction_t *transaction, void *handle);



static senos_1wire_device_t *senos_1wire_find_device(uint32_t id);

/**
 * @brief Swap 1-Wire ROM Code
 *
 * @param[in] val ROM Code
 * @return
 *      - Swapped ROM Code
 */
static uint64_t _swap_romcode(uint64_t val);

/**
 * @brief Generate internal 16bit ID
 *
 * @param[in] data Pointer to 1-Wire device ROM Code
 * @return
 *      - 16bit internal ID
 */
static uint16_t _genid(uint8_t *data);

static senos_1wire_device_t *device_list = NULL;
static senos_bus_drv bus_control = {._attach = &senos_1wire_attach, ._deattach = &senos_1wire_deattach};
static senos_drv_api senos_1wire_api = {
    ._read = &senos_1wire_read,
    ._write = &senos_1wire_write,
    ._wr = &senos_1wire_wr,
    ._reset = &senos_1wire_reset,
    ._scan = &senos_1wire_scan
};

void *senos_1wire_get_ctrl_handle(void) {
    //printf("1wire bus_control.attach: %p\n", bus_control._attach);
    //printf("1wire bus_control.deattach: %p\n", bus_control._deattach);
    return &bus_control;
}

static esp_err_t senos_1wire_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle) {
    if(dev_cfg->bus_type != SENOS_BUS_1WIRE) return ESP_ERR_INVALID_ARG;
    onewire_bus_rmt_obj_t *bus_rmt;
    onewire_bus_handle_t bus_handle = NULL;
    bool new_handle =false;
    for(senos_1wire_device_t *device = device_list; device != NULL; device = device->next) {
        bus_rmt = __containerof(device->handle, onewire_bus_rmt_obj_t, base);
        if(bus_rmt->tx_channel->gpio_num == dev_cfg->dev_1wire.data_gpio) { 
            bus_handle = device->handle;
            break;
        }
    };
    if(!bus_handle) {
        /* -> RMT channel acquisition & create OneWire bus */
        /* *> Keep in mind *<
         * max_rx_bytes is allocate memory for rx channel. Same time this value blocks creating new rmt channel due lack of memory
         * For example max_rx_bytes = 32 allow only one 1-wire bus to be created.
         * 8 bytes per channel do not overlap channels. (ESP32 has 64 blocks, S3 48 blocks)
        */
        if(!gpio_drv_reserve(dev_cfg->dev_1wire.data_gpio)) return ESP_ERR_NOT_ALLOWED;
        onewire_bus_config_t bus_config = { .bus_gpio_num = dev_cfg->dev_1wire.data_gpio};
        onewire_bus_rmt_config_t rmt_config = {.max_rx_bytes = 16};
        if(ESP_OK != onewire_new_bus_rmt(&bus_config, &rmt_config, &bus_handle)) {
            gpio_drv_free(dev_cfg->dev_1wire.data_gpio);
            return ESP_ERR_NO_MEM;
        }
        new_handle =  true;
    }
    bus_rmt = __containerof(bus_handle, onewire_bus_rmt_obj_t, base);
    senos_1wire_device_id_t device_id = {
        .gpio = dev_cfg->dev_1wire.data_gpio,
        .tx_channel = bus_rmt->tx_channel->channel_id,
        .rx_channel = bus_rmt->rx_channel->channel_id,
        .reserved = 0x00,
        .intr_id = _genid((uint8_t *)(&(dev_cfg->dev_1wire.rom_code))),
    };
    if(senos_1wire_find_device(device_id.id)) return ESP_ERR_NOT_SUPPORTED;
    senos_1wire_device_t *new_device = (senos_1wire_device_t *)calloc(1, sizeof(senos_1wire_device_t));
    if(!new_device) {
        if(new_handle) onewire_bus_del(bus_handle);
        gpio_drv_free(dev_cfg->dev_1wire.data_gpio);
        return ESP_ERR_NO_MEM;
    }
    *handle = (senos_dev_handle_t)calloc(1, sizeof(senos_dev_handle));
    *new_device = (senos_1wire_device_t) {
        .stats = (senos_device_stats_t){},
        .device_id = device_id.id,
        .address = _swap_romcode(dev_cfg->dev_1wire.rom_code),
        .cmd_bytes = dev_cfg->dev_1wire.cmd_bytes,
        .addr_bytes = dev_cfg->dev_1wire.addr_bytes,
        .crc_check = dev_cfg->dev_1wire.crc_check,
        .handle = bus_handle,
        .base = &senos_1wire_api,
        .next = device_list
    };
    device_list = new_device;
    (*handle)->api = &new_device->base;
    (*handle)->device_id = new_device->device_id;
    printf("senos_1wire_attach new_device:%p, rmt_bus:%p, base_api:%p\n", new_device, new_device->handle, new_device->base);
    return ESP_OK;
}

static esp_err_t senos_1wire_deattach(senos_dev_handle_t handle) {
    senos_1wire_device_t *device = device_list, *target = NULL;
    senos_1wire_device_t *req_device = __containerof(handle->api, senos_1wire_device_t, base);
    senos_1wire_device_id_t req_device_id = {.id = req_device->device_id};

    while(device && (device->device_id != req_device->device_id)) {
        target = device;
        device = device->next;
    }
    printf("senos_1wire_deattach [%08lX] found:%p, requested:%p, targeted:%p\n", req_device_id.id, device, req_device, target);
    if(!device) { 
        printf(" [%08lX] Not found\n", req_device->device_id);
        return ESP_ERR_NOT_FOUND;
    }
    if(device != req_device) { 
        printf(" [%08lX] Invalid handle %p <> %p\n",req_device->device_id, device, req_device);
        return ESP_ERR_INVALID_ARG;
    }
    if(target) target->next = device->next;
    else device_list = device->next;
    target = NULL;
    for(device = device_list; device && !target; device = device->next) {
        if(device->handle == req_device->handle) {
            target = device;
            printf("senos_1wire_deattach: 1Wire Bus %p is still active\n", device->handle);
        }
    }
    if(!target) {
        printf("senos_1wire_deattach: Delete %p 1Wire Bus\n", req_device->handle);
        onewire_bus_del(req_device->handle);
        gpio_drv_free(req_device_id.gpio);
    }
    free(req_device);
    return ESP_OK;
}


static esp_err_t senos_1wire_read(senos_dev_transaction_t *transaction, void *handle) {
    //printf("senos_1wire_read rd: %u, wr:%u, cmd:%04X, reg:%016llX\n", transaction->rdBytes, transaction->wrBytes, transaction->dev_cmd, transaction->dev_reg);
    return ESP_OK;
}

static esp_err_t senos_1wire_write(senos_dev_transaction_t *transaction, void *handle) {
    return ESP_OK;
}

static esp_err_t senos_1wire_wr(senos_dev_transaction_t *transaction, void *handle) {
    return ESP_OK;
}

static esp_err_t senos_1wire_reset(void *handle) {
    return ESP_OK;
}

static esp_err_t senos_1wire_scan(senos_dev_transaction_t *transaction, void *handle) {
    return ESP_OK;
}

/* Internal functions */

static senos_1wire_device_t *senos_1wire_find_device(uint32_t id) {
    for(senos_1wire_device_t *device = device_list; device != NULL; device = device->next) { 
        if(device->device_id == id) return device;
    }
    return NULL;
}

static uint64_t _swap_romcode(uint64_t val)
{
    val = ((val << 8) & 0xFF00FF00FF00FF00ULL ) | ((val >> 8) & 0x00FF00FF00FF00FFULL );
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL ) | ((val >> 16) & 0x0000FFFF0000FFFFULL );
    return (val << 32) | (val >> 32);
}

static uint16_t _genid(uint8_t *data) //crc16_mcrf4xx
{
    uint8_t len = 8;
    uint8_t t;
    uint8_t L;
    uint64_t mac = 0;
    esp_efuse_mac_get_default((uint8_t *)(&mac));
    uint16_t id = *(((int16_t *)(&mac))+2);
    if (data) {
        while (len--) {
            id ^= *data++;
            L = id ^ (id << 4);
            t = (L << 3) | (L >> 5);
            L ^= (t & 0x07);
            t = (t & 0xF8) ^ (((t << 1) | (t >> 7)) & 0x0F) ^ (uint8_t)(id >> 8);
            id = (L << 8) | t;
        }
    }
    return id;
}