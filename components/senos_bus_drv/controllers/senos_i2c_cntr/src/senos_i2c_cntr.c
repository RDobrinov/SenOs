/*
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senos_bus_drv_private_defs.h"
#include "driver/i2c_master.h"
#include "../i2c_private.h"

#include "senos_i2c_private.h"
#include "idf_gpio_driver.h"

#include "esp_err.h"

/** Type of device list element for attached i2c devices */
typedef struct senos_i2c_device {
    uint32_t device_id;                 /*!< Device ID */
    struct {
        uint32_t cmd_bytes:2;       /*!< Device command bytes 0-2 */
        uint32_t addr_bytes:4;      /*!< Device command bytes 0-8 */
        uint32_t xfer_timeout_ms:4; /*!< Device timeout */
        uint32_t not_used:22;       /*!< Not used */
    };
    senos_device_stats_t stats;     /*!< Device stats counters */
    i2c_master_dev_handle_t handle; /*!< I2C master device handle */
    senos_drv_api_t base;           /*!< Device API handle */
    struct senos_i2c_device *next;/*!< Pointer to next elements */
} senos_i2c_device_t;


static esp_err_t senos_i2c_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle);
static esp_err_t senos_i2c_deattach( senos_dev_handle_t handle);
static esp_err_t senos_i2c_bus_scan(senos_dev_cfg_t *dev_cfg, uint8_t *list, size_t *num_of_devices);

static esp_err_t senos_i2c_read(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_i2c_write(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_i2c_wr(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_i2c_desc(void *handle, char *stats, size_t max_chars, bool type);
static esp_err_t senos_i2c_stats(void *handle, char *stats, size_t max_chars);
static esp_err_t senos_i2c_reset(void *handle);

/** Internal helper functions */
static senos_i2c_device_t *senos_i2c_find_device(uint32_t id);
static void senos_i2c_release_master(i2c_master_bus_handle_t handle);
static esp_err_t _prepare_transaction(senos_dev_transaction_t *transaction, senos_i2c_device_t *handle);

static senos_i2c_device_t *device_list = NULL;
static senos_bus_drv bus_control = {._attach = &senos_i2c_attach, ._deattach = &senos_i2c_deattach, ._scanbus = &senos_i2c_bus_scan};
static senos_drv_api senos_i2c_api = {
    ._read = &senos_i2c_read,
    ._write = &senos_i2c_write,
    ._wr = &senos_i2c_wr,
    ._desc = &senos_i2c_desc,
    ._stats = &senos_i2c_stats,
    ._reset = &senos_i2c_reset
};

static uint8_t transaction_buffer[32];

void i2c_hex(const uint8_t *buf, size_t len) {
    if( !len ) return;
    //ESP_LOGI("hexdump", "%p", buf);
    printf("i2c_hex: [%p] ", buf);
    for(int i=0; i<len; i++) printf("%02X ", buf[i]);
    printf("\n");
    return;
}

void *senos_i2c_get_ctrl_handle(void) {
    return &bus_control;
}

static esp_err_t senos_i2c_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle) {
    if(dev_cfg->bus_type != SENOS_BUS_I2C) return ESP_ERR_INVALID_ARG;
    uint64_t pinmask = (BIT64(dev_cfg->dev_i2c.scl_gpio) | BIT64(dev_cfg->dev_i2c.sda_gpio));
    i2c_master_bus_handle_t bus_handle = NULL;
    *handle = NULL;
    for(senos_i2c_device_t *device = device_list; device != NULL && !bus_handle; device = device->next) {
        if(device->handle->master_bus->base->scl_num == dev_cfg->dev_i2c.scl_gpio && device->handle->master_bus->base->sda_num == dev_cfg->dev_i2c.sda_gpio) 
            bus_handle = device->handle->master_bus;
    }
    if(!bus_handle) {
        if(!gpio_drv_reserve_pins(pinmask)) return ESP_ERR_INVALID_ARG;
        i2c_master_bus_config_t i2c_bus_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = -1,
            .scl_io_num = dev_cfg->dev_i2c.scl_gpio,
            .sda_io_num = dev_cfg->dev_i2c.sda_gpio, 
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true
        };
        esp_err_t err = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
        if(ESP_OK != err) {
            //free(port_handle);
            gpio_drv_free_pins(pinmask);
            return err;
        }
    } /* <- I2C Controller acquisition */
    senos_i2c_device_id_t new_id = {
        .gpioscl = dev_cfg->dev_i2c.scl_gpio,
        .gpiosda = dev_cfg->dev_i2c.sda_gpio,
        .i2caddr = dev_cfg->dev_i2c.device_address,
        .i2cbus = bus_handle->base->port_num
    };
    if(senos_i2c_find_device(new_id.id)) return ESP_ERR_NOT_SUPPORTED;

    i2c_master_dev_handle_t *new_device_handle = (i2c_master_dev_handle_t *)calloc(1, sizeof(i2c_master_dev_handle_t));
    i2c_device_config_t device_conf = (i2c_device_config_t) { 
        .dev_addr_length = dev_cfg->dev_i2c.dev_addr_length, 
        .device_address = dev_cfg->dev_i2c.device_address,
        //.scl_speed_hz = 10000 * payload->i2c_device.scl_speed_hz,
        .scl_speed_hz = dev_cfg->dev_i2c.scl_speed_hz,
        .flags.disable_ack_check = dev_cfg->dev_i2c.disable_ack_check
    };
    esp_err_t err = i2c_master_bus_add_device(bus_handle, &device_conf, new_device_handle);
    if(ESP_OK != err) {
        free(new_device_handle);
        senos_i2c_release_master(bus_handle);
        return err;
    }

    senos_i2c_device_t *new_device = (senos_i2c_device_t *)calloc(1, sizeof(senos_i2c_device_t));
    if(!new_device) {
        i2c_master_bus_rm_device(*new_device_handle);
        free(new_device_handle);
        senos_i2c_release_master(bus_handle);
        return ESP_ERR_NO_MEM;
    }
    *handle = (senos_dev_handle_t)calloc(1, sizeof(senos_dev_handle));
    *new_device = (senos_i2c_device_t) {
        .device_id = new_id.id,
        .cmd_bytes = (dev_cfg->dev_i2c.cmd_bytes > 2) ? 2 : dev_cfg->dev_i2c.cmd_bytes,
        .addr_bytes = (dev_cfg->dev_i2c.addr_bytes > 8) ? 8 : dev_cfg->dev_i2c.addr_bytes,
        .xfer_timeout_ms = dev_cfg->dev_i2c.xfer_timeout_ms,
        .stats = (senos_device_stats_t){},
        .handle = *new_device_handle,
        .base = &senos_i2c_api,
        .next = device_list
    };
    device_list = new_device; /* <- END Attach device */

    (*handle)->api = &new_device->base;
    (*handle)->bus_type = SENOS_BUS_I2C;
    (*handle)->device_id = new_device->device_id;
    printf("device:%p, handle:%p->%p\n", new_device, *handle, (*handle)->api);
    return ESP_OK;
}

static esp_err_t senos_i2c_deattach( senos_dev_handle_t handle) {
    if(!handle) return ESP_ERR_INVALID_ARG;
    if(handle->bus_type != SENOS_BUS_I2C) return ESP_ERR_INVALID_ARG;
    senos_i2c_device_t *device = device_list, *target = NULL;
    senos_i2c_device_t *req_device = __containerof(handle->api, senos_i2c_device_t, base);
    while(device && (device->device_id != req_device->device_id)) {
        target = device;
        device = device->next;
    }
    if(device != req_device) return ESP_ERR_INVALID_ARG;
    i2c_master_bus_handle_t master_handle = device->handle->master_bus;
    if( ESP_OK == i2c_master_bus_rm_device(device->handle) ) {
        if(target) target->next = device->next;
        else device_list = device->next;
        free(req_device);
        free(handle);
    } else return ESP_FAIL;
    senos_i2c_release_master(master_handle);
    return ESP_OK;
}
static esp_err_t senos_i2c_bus_scan(senos_dev_cfg_t *dev_cfg, uint8_t *list, size_t *num_of_devices) {
    return ESP_OK;
}

static esp_err_t senos_i2c_read(senos_dev_transaction_t *transaction, void *handle) {
    esp_err_t err;
    senos_i2c_device_t *device = __containerof(((senos_dev_handle_t)handle)->api , senos_i2c_device_t, base);
    if(transaction->rdBytes == 0) return ESP_ERR_INVALID_SIZE;
    if(ESP_OK != _prepare_transaction(transaction, device)) return ESP_ERR_INVALID_SIZE;
    size_t command_len = device->cmd_bytes + device->addr_bytes;
    //printf("senos_i2c_read command_len:%d, transaction->rdBytes:%d\n", command_len, transaction->rdBytes);
    if(command_len > 0) {
        err = i2c_master_transmit_receive(device->handle, transaction_buffer, command_len, transaction->data, transaction->rdBytes, device->xfer_timeout_ms);
        if(ESP_OK == err) device->stats.snd += command_len;
    } else err = i2c_master_receive(device->handle, transaction->data, transaction->rdBytes, device->xfer_timeout_ms);
    if(ESP_OK != err) {
        if(ESP_ERR_TIMEOUT == err) device->stats.timeouts++;
        else device->stats.other++;
    } else device->stats.rcv += transaction->rdBytes;
    return err;
}

static esp_err_t senos_i2c_write(senos_dev_transaction_t *transaction, void *handle) {
    esp_err_t err;
    senos_i2c_device_t *device = __containerof(((senos_dev_handle_t)handle)->api , senos_i2c_device_t, base);
    if(ESP_OK != _prepare_transaction(transaction, device)) return ESP_ERR_INVALID_SIZE;
    size_t total_bytes_write = device->cmd_bytes + device->addr_bytes + transaction->wrBytes;
    //printf("senos_i2c_write total_bytes_write:%d\n", total_bytes_write);
    if(total_bytes_write == 0) return ESP_ERR_INVALID_SIZE;
    err = i2c_master_transmit(device->handle, transaction_buffer, total_bytes_write, device->xfer_timeout_ms);
    if(ESP_OK != err) {
        if(ESP_ERR_TIMEOUT == err) device->stats.timeouts++;
        else device->stats.other++;
    } else device->stats.snd += total_bytes_write;
    return err;
}

static esp_err_t senos_i2c_wr(senos_dev_transaction_t *transaction, void *handle) {
    esp_err_t err;
    senos_i2c_device_t *device = __containerof(((senos_dev_handle_t)handle)->api , senos_i2c_device_t, base);
    if(ESP_OK != _prepare_transaction(transaction, device)) return ESP_ERR_INVALID_SIZE;
    size_t total_bytes_write = device->cmd_bytes + device->addr_bytes + transaction->wrBytes;
    //printf("senos_i2c_wr total_bytes_write:%d, transaction->rdBytes:%d\n", total_bytes_write, transaction->rdBytes);
    if(total_bytes_write == 0) return ESP_ERR_INVALID_SIZE;
    err = i2c_master_transmit_receive(device->handle, transaction_buffer, total_bytes_write, transaction->data, transaction->rdBytes, device->xfer_timeout_ms);
    if(ESP_OK != err) {
        if(ESP_ERR_TIMEOUT == err) device->stats.timeouts++;
        else device->stats.other++;
    } else  {
        device->stats.snd += total_bytes_write;
        device->stats.rcv += transaction->rdBytes;
    }
    return err;
}

static esp_err_t senos_i2c_desc(void *handle, char *stats, size_t max_chars, bool type) {
    if(max_chars < 34 || !stats) return ESP_ERR_INVALID_SIZE;
    senos_i2c_device_t *device = __containerof(((senos_dev_handle_t)handle)->api , senos_i2c_device_t, base);
    if(type) {
        snprintf(stats, max_chars, "ID %08lX Address 0x%03X @ %03ukHz on I2C%2d with SCL IO%02u, SDA IO%02u", device->device_id, 
                device->handle->device_address, (unsigned int)device->handle->scl_speed_hz/1000, device->handle->master_bus->base->port_num,
                device->handle->master_bus->base->scl_num, device->handle->master_bus->base->sda_num);
    } else {
        snprintf(stats, max_chars, "0x%02X @ i2c/p%02ucl%02uda%02ucs%03u", device->handle->device_address, device->handle->master_bus->base->port_num,
            device->handle->master_bus->base->scl_num, device->handle->master_bus->base->sda_num, (unsigned int)device->handle->scl_speed_hz/1000);
    }
    return ESP_OK;
}

static esp_err_t senos_i2c_stats(void *handle, char *stats, size_t max_chars) {
    if(max_chars == 0 || !stats) return ESP_ERR_INVALID_SIZE;
    senos_i2c_device_t *device = __containerof(((senos_dev_handle_t)handle)->api , senos_i2c_device_t, base);
    float rmkb = (device->stats.rcv > 1000000) ? (float)device->stats.rcv/1000000.0 : (float)device->stats.rcv/1000.0;
    float smkb = (device->stats.snd > 1000000) ? (float)device->stats.snd/1000000.0 : (float)device->stats.snd/1000.0;
    snprintf(stats, max_chars, "RX Bytes %lu (%4.2f %s), TX Bytes %lu (%4.2f %s) Bus Errors CRC %u, Timeouts %u, Other %u", 
            device->stats.rcv, rmkb, (device->stats.rcv > 1000000) ? "MB" : "KB", 
            device->stats.snd, smkb, (device->stats.snd > 1000000) ? "MB" : "KB",
            device->stats.crc_error, device->stats.timeouts, device->stats.other);
    return ESP_OK;
}

static esp_err_t senos_i2c_reset(void *handle)
{
    return ESP_OK;
}

/** Internal helper functions */
static senos_i2c_device_t *senos_i2c_find_device(uint32_t id){
    for(senos_i2c_device_t *device = device_list; device != NULL; device = device->next)
        if(device->device_id == id) return device;
    return NULL;
}

static void senos_i2c_release_master(i2c_master_bus_handle_t handle) {
    if(handle->device_list.slh_first) return;
    if(ESP_OK == i2c_del_master_bus(handle)){
        gpio_drv_free_pins( BIT64(handle->base->scl_num) | BIT64(handle->base->sda_num));
    } else {
        /* Error state - I2C Controller remains locked / unusable */
    }
    return;
}

static esp_err_t _prepare_transaction(senos_dev_transaction_t *transaction, senos_i2c_device_t *handle) {
    if((handle->cmd_bytes + handle->addr_bytes + transaction->wrBytes) > 32) return ESP_ERR_INVALID_SIZE;
    //transaction_buffer[0] = ONEWIRE_CMD_MATCH_ROM;
    //*(uint64_t *)(&transaction_buffer[1]) = handle->address;
    if(handle->cmd_bytes > 0) {
        if(handle->cmd_bytes ==2) *((uint16_t *)&transaction_buffer[0]) = (uint16_t)( 0xFFFF & transaction->dev_cmd);
        else transaction_buffer[0] = (uint8_t)( 0xFF & transaction->dev_cmd);
    }
    if(handle->addr_bytes > 0) {
        memcpy(&transaction_buffer[handle->cmd_bytes], &(transaction->dev_reg), handle->addr_bytes);
    }
    if(transaction->wrBytes > 0) {
        memcpy(&transaction_buffer[handle->cmd_bytes + handle->addr_bytes], transaction->data, transaction->wrBytes);
    }
    i2c_hex(transaction_buffer, 32);
    return ESP_OK;
}