/*
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/spi_master.h"
#include "senos_bus_drv_private_defs.h"
#include "senos_spi_private.h"
#include "idf_gpio_driver.h"

#include "esp_err.h"

#define SPIBUS_HOST_MAX (SPI_HOST_MAX - 1)

/**
 * @brief Type of driver available spi host
*/
typedef struct {
    union {
        struct {
            uint32_t mosi_gpio:7;   /*!< MOSI/SDO IO Pin when host in use */
            uint32_t miso_gpio:7;   /*!< MISO/SDI IO Pin when host in use */
            uint32_t sclk_gpio:7;   /*!< SCLK IO Pin when host in use */
            uint32_t host_id:4;     /*!< IDF host ID (SPI2_HOST..etc)*/
            uint32_t free:1;        /*!< It's host free for usage flag */
            uint32_t reserved:6;    /*!< Not used */
        };
        uint32_t val;
    };
} senos_spibus_host_t;

/** Type of device list element for attached i2c devices */
typedef struct senos_spi_device {
    uint32_t device_id;             /*!< Device ID */
    senos_device_stats_t stats;     /*!< Device stats counters */
    spi_device_handle_t handle;     /*!< SPI Device handle */
    senos_drv_api_t base;           /*!< Device API handle */
    struct senos_spi_device *next;  /*!< Pointer to next elements */
} senos_spi_device_t;

static esp_err_t senos_spi_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle);
static esp_err_t senos_spi_deattach( senos_dev_handle_t handle);
static esp_err_t senos_spi_bus_scan(senos_dev_cfg_t *dev_cfg, uint8_t *list, size_t *num_of_devices);

static esp_err_t senos_spi_read(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_spi_write(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_spi_wr(senos_dev_transaction_t *transaction, void *handle);
static esp_err_t senos_spi_desc(void *handle, char *stats, size_t max_chars, bool type);
static esp_err_t senos_spi_stats(void *handle, char *stats, size_t max_chars);
static esp_err_t senos_spi_reset(void *handle);

/** Internal helper functions */
static senos_spi_device_t *senos_spi_find_device(uint32_t id);

static senos_spibus_host_t *spi_hosts;    /*!< Available SPI Hosts status */
static senos_spi_device_t *device_list = NULL; /*!< Attached to bus devices */
static senos_bus_drv bus_control = {._attach = &senos_spi_attach, ._deattach = &senos_spi_deattach, ._scanbus = &senos_spi_bus_scan}; /*!< Bus control pointers */
static senos_drv_api senos_spi_api = {
    ._read = &senos_spi_read,
    ._write = &senos_spi_write,
    ._wr = &senos_spi_wr,
    ._desc = &senos_spi_desc,
    ._stats = &senos_spi_stats,
    ._reset = &senos_spi_reset
};  /*!< Sensor API */

void spi_hex(const uint8_t *buf, size_t len) {
    if( !len ) return;
    //ESP_LOGI("hexdump", "%p", buf);
    printf("spi_hex: [%p] ", buf);
    for(int i=0; i<len; i++) printf("%02X ", buf[i]);
    printf("\n");
    return;
}

void *senos_spi_get_ctrl_handle(void) {
    if(!spi_hosts) {
        spi_hosts = (senos_spibus_host_t *)calloc(SPIBUS_HOST_MAX, sizeof(senos_spibus_host_t));
        if(!spi_hosts) return NULL;

        for(spi_host_device_t spi = SPI2_HOST; spi < SPI_HOST_MAX; spi++) spi_hosts[spi-1] = (senos_spibus_host_t){ .host_id = spi, .free = 1};
        gpio_drv_init();
    }
    return &bus_control;
}

static esp_err_t senos_spi_attach(senos_dev_cfg_t *dev_cfg, senos_dev_handle_t *handle) {
    if(dev_cfg->bus_type != SENOS_BUS_SPI) return ESP_ERR_INVALID_ARG;
    senos_spi_device_id_t new_device_id = {
        .mosi_gpio = dev_cfg->dev_spi.mosi_gpio,
        .miso_gpio = dev_cfg->dev_spi.miso_gpio,
        .sclk_gpio = dev_cfg->dev_spi.sclk_gpio,
        .cs_gpio = dev_cfg->dev_spi.cs_gpio,
        .host_id = SPI_HOST_MAX
    };

    for(uint8_t i=0; i< SPIBUS_HOST_MAX; i++) {
        if(!spi_hosts[i].free) {
            if( !((new_device_id.id ^ spi_hosts[i].val) & 0x1FFFFFUL)) {
                new_device_id.host_id = spi_hosts[i].host_id;
                break;
            }
        }
    }
    if(senos_spi_find_device(new_device_id.id)) return ESP_ERR_NOT_SUPPORTED;

    uint64_t bus_pinmask = (
            BIT64(dev_cfg->dev_spi.mosi_gpio) | BIT64(dev_cfg->dev_spi.miso_gpio) | 
            BIT64(dev_cfg->dev_spi.sclk_gpio) | BIT64(dev_cfg->dev_spi.cs_gpio));
    esp_err_t ret = ESP_OK;
    uint8_t bus_index = 0;
    bool new_spi_bus = false;
    if(new_device_id.host_id == SPI_HOST_MAX) {
        while (bus_index < SPIBUS_HOST_MAX && !spi_hosts[bus_index].free) bus_index++;
        if(bus_index == SPIBUS_HOST_MAX) return ESP_ERR_NOT_FOUND;
        if(!gpio_drv_reserve_pins(bus_pinmask)) return ESP_ERR_INVALID_ARG;
        new_device_id.host_id = spi_hosts[bus_index].host_id;
        /* Probing new host */
        spi_bus_config_t bus_config = {
            .mosi_io_num = new_device_id.mosi_gpio,
            .miso_io_num = new_device_id.miso_gpio,
            .sclk_io_num = new_device_id.sclk_gpio,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 128
        };
        ret = spi_bus_initialize(new_device_id.host_id, &bus_config, SPI_DMA_CH_AUTO);
        if(ESP_OK != ret) {
            gpio_drv_free_pins(bus_pinmask);
            return ESP_ERR_INVALID_STATE == ret ? ESP_ERR_NOT_FOUND : ret;
        }
        spi_hosts[bus_index].free = false;
        new_spi_bus = true;
    }

    if(!new_spi_bus && !gpio_drv_reserve(dev_cfg->dev_spi.cs_gpio)) {
        return ESP_ERR_INVALID_ARG;
    }

    senos_spi_device_t *new_device = (senos_spi_device_t *)calloc(1, sizeof(senos_spi_device_t));
    *handle = (senos_dev_handle_t)calloc(1, sizeof(senos_dev_handle));
    if(!new_device || !(*handle)) {
        if(new_spi_bus) {
            spi_bus_free(new_device_id.host_id);
            spi_hosts[bus_index].free = true;
            gpio_drv_free_pins(bus_pinmask);
        } else gpio_drv_free(dev_cfg->dev_spi.cs_gpio);
        free(*handle);
        free(new_device);
        return ESP_ERR_NO_MEM;
    }

    spi_device_interface_config_t dev_config = {
        .spics_io_num = new_device_id.cs_gpio,
        .mode = dev_cfg->dev_spi.mode,
        .command_bits = dev_cfg->dev_spi.cmd_bits,
        .address_bits = dev_cfg->dev_spi.addr_bits,
        .dummy_bits = dev_cfg->dev_spi.dummy_bits,
        .clock_speed_hz = dev_cfg->dev_spi.clock_speed,
        .cs_ena_pretrans = dev_cfg->dev_spi.pretrans,
        .cs_ena_posttrans = dev_cfg->dev_spi.postrans,
        .input_delay_ns = dev_cfg->dev_spi.input_delay,
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX | dev_cfg->dev_spi.flags
    };
    ret = spi_bus_add_device(new_device_id.host_id, &dev_config, &(new_device->handle));
    if(ESP_OK != ret) {
        if(new_spi_bus) {
            spi_bus_free(new_device_id.host_id);
            gpio_drv_free_pins(bus_pinmask);
            spi_hosts[bus_index].free = true;
        } else gpio_drv_free(dev_cfg->dev_spi.cs_gpio);
        free(new_device);
        free(*handle);
        return ret;
    }
    new_device->device_id = new_device_id.id;
    new_device->stats = (senos_device_stats_t){};
    new_device->stats.spi_reserved = ((dev_cfg->dev_spi.addr_bits + dev_cfg->dev_spi.cmd_bits) >> 3);
    new_device->next = device_list;
    new_device->base = &senos_spi_api;

    (*handle)->api = &new_device->base;
    (*handle)->bus_type = SENOS_BUS_SPI;
    (*handle)->device_id = new_device->device_id;
    /** */
    return ESP_OK;
}

static esp_err_t senos_spi_deattach(senos_dev_handle_t handle) {
    if(!handle) return ESP_ERR_INVALID_ARG;
    if(handle->bus_type != SENOS_BUS_SPI) return ESP_ERR_INVALID_ARG;
    senos_spi_device_t *device = device_list, *target = NULL;
    senos_spi_device_t *req_device = __containerof(handle->api, senos_spi_device_t, base);
    while(device && (device->device_id != req_device->device_id)) {
        target = device;
        device = device->next;
    }
    if(device != req_device) return ESP_ERR_INVALID_ARG;
    
    esp_err_t ret = spi_bus_remove_device(device->handle);
    if(ESP_OK != ret) return ret;
    if(target) target->next = device->next;
    else device_list = device->next;
    ret = spi_bus_free(((senos_spi_device_id_t)device->device_id).host_id);
    if(ret == ESP_OK) {
        gpio_drv_free_pins(BIT64(((senos_spi_device_id_t)device->device_id).miso_gpio) | BIT64(((senos_spi_device_id_t)device->device_id).mosi_gpio) |
            BIT64(((senos_spi_device_id_t)device->device_id).sclk_gpio) | BIT64(((senos_spi_device_id_t)device->device_id).cs_gpio));
        for(int i = 0; i < SPIBUS_HOST_MAX; i++) {
            if(spi_hosts[i].host_id == ((senos_spi_device_id_t)device->device_id).host_id) {
                spi_hosts[i].free = true;
                spi_hosts[i].val = ( spi_hosts[i].val & 0xFFE00000UL );
                break;
            }
        }
    } else {
        gpio_drv_free(((senos_spi_device_id_t)device->device_id).cs_gpio);
    }
    free(device);
    return ESP_OK;
}
static esp_err_t senos_spi_bus_scan(senos_dev_cfg_t *dev_cfg, uint8_t *list, size_t *num_of_devices) {
    return ESP_FAIL;
}

static esp_err_t senos_spi_read(senos_dev_transaction_t *transaction, void *handle) {
    esp_err_t err ;
    if(transaction->rdBytes == 0) return ESP_ERR_INVALID_SIZE;
    senos_spi_device_t *device = __containerof(((senos_dev_handle_t)handle)->api , senos_spi_device_t, base);
    spi_transaction_t spibus_execute = {
        .addr = transaction->dev_reg,
        .cmd = transaction->dev_cmd,
        .rx_buffer = transaction->data,
        .tx_buffer = NULL,
    };
    spibus_execute.rxlength = (transaction->rdBytes) << 3;
    spibus_execute.length = spibus_execute.rxlength;
    err = spi_device_polling_transmit(device->handle, &spibus_execute);
    if(ESP_OK == err) {
        device->stats.snd += device->stats.spi_reserved;
        device->stats.rcv += transaction->rdBytes;
    } else {
        if(ESP_ERR_TIMEOUT == err) device->stats.timeouts++;
        else device->stats.other++;
    }
    return err;
}

static esp_err_t senos_spi_write(senos_dev_transaction_t *transaction, void *handle) {
    esp_err_t err;
    if(transaction->wrBytes == 0) return ESP_ERR_INVALID_SIZE;
    uint8_t *dma_buf = NULL;
    senos_spi_device_t *device = __containerof(((senos_dev_handle_t)handle)->api , senos_spi_device_t, base);
    spi_transaction_t spibus_execute = {
        .addr = transaction->dev_reg,
        .cmd = transaction->dev_cmd,
        .rxlength = 0,
        .length = (transaction->rdBytes) << 3,
        .rx_buffer = NULL,
        .tx_buffer = NULL,
    };
    if(transaction->wrBytes <= 4) {
        spibus_execute.flags |= SPI_TRANS_USE_TXDATA;
        *((uint32_t *)spibus_execute.tx_data) = *((uint32_t *)transaction->data);
    } else {
        dma_buf = heap_caps_aligned_calloc(4, 1, transaction->wrBytes, MALLOC_CAP_DMA);
        if(!dma_buf) return ESP_ERR_NO_MEM;
        spibus_execute.tx_buffer = dma_buf;
        memcpy(dma_buf, transaction->data, transaction->wrBytes);
    }
    err = spi_device_polling_transmit(device->handle, &spibus_execute);
    if(ESP_OK == err) {
        device->stats.snd += (device->stats.spi_reserved + transaction->wrBytes);
    } else {
        if(ESP_ERR_TIMEOUT == err) device->stats.timeouts++;
        else device->stats.other++;
    }
    if(dma_buf) heap_caps_free(dma_buf);
    return err;
}

static esp_err_t senos_spi_wr(senos_dev_transaction_t *transaction, void *handle) {
    return ESP_FAIL;
    /*esp_err_t err;
    senos_spi_device_t *device = __containerof(((senos_dev_handle_t)handle)->api , senos_spi_device_t, base);
    if(ESP_OK != _prepare_transaction(transaction, device)) return ESP_ERR_INVALID_SIZE;
    return err;*/
}

static esp_err_t senos_spi_desc(void *handle, char *stats, size_t max_chars, bool type) {
    if(max_chars < 34 || !stats) return ESP_ERR_INVALID_SIZE;
    senos_spi_device_t *device = __containerof(((senos_dev_handle_t)handle)->api , senos_spi_device_t, base);
    if(type) { //((senos_spi_device_id_t)device->device_id)
        snprintf(stats, max_chars, "ID %08lX on SPI%02d with CS IO%02u, MOSI IO%02u, MISO IO%02u, SCLK IO%02u", device->device_id,
        ((senos_spi_device_id_t)device->device_id).host_id, ((senos_spi_device_id_t)device->device_id).cs_gpio, 
        ((senos_spi_device_id_t)device->device_id).mosi_gpio, ((senos_spi_device_id_t)device->device_id).miso_gpio, 
        ((senos_spi_device_id_t)device->device_id).sclk_gpio);
    } else {
        snprintf(stats, max_chars, "CS%02u @ spi/p%02ucl%02udo%02udi%02u", ((senos_spi_device_id_t)device->device_id).cs_gpio, 
            ((senos_spi_device_id_t)device->device_id).host_id, ((senos_spi_device_id_t)device->device_id).sclk_gpio, 
            ((senos_spi_device_id_t)device->device_id).mosi_gpio, ((senos_spi_device_id_t)device->device_id).miso_gpio);
    }
    return ESP_OK;
}

static esp_err_t senos_spi_stats(void *handle, char *stats, size_t max_chars) {
    if(max_chars == 0 || !stats) return ESP_ERR_INVALID_SIZE;
    senos_spi_device_t *device = __containerof(((senos_dev_handle_t)handle)->api , senos_spi_device_t, base);
    float rmkb = (device->stats.rcv > 1000000) ? (float)device->stats.rcv/1000000.0 : (float)device->stats.rcv/1000.0;
    float smkb = (device->stats.snd > 1000000) ? (float)device->stats.snd/1000000.0 : (float)device->stats.snd/1000.0;
    snprintf(stats, max_chars, "RX Bytes %lu (%4.2f %s), TX Bytes %lu (%4.2f %s) Bus Errors CRC %u, Timeouts %u, Other %u", 
            device->stats.rcv, rmkb, (device->stats.rcv > 1000000) ? "MB" : "KB", 
            device->stats.snd, smkb, (device->stats.snd > 1000000) ? "MB" : "KB",
            device->stats.crc_error, device->stats.timeouts, device->stats.other);
    return ESP_OK;
}

static esp_err_t senos_spi_reset(void *handle)
{
    return ESP_OK;
}

/** Internal helper functions */
static senos_spi_device_t *senos_spi_find_device(uint32_t id){
    for(senos_spi_device_t *device = device_list; device != NULL; device = device->next)
        if(device->device_id == id) return device;
    return NULL;
}

