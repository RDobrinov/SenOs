/**
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ds18x20.h"
#include "senos_sensor_base.h"
#include "senos_sensor_private.h"
#include "senos_bus_drv.h"

#define DS18X20_MAX_DECIMALS MAGNITUDE_MAX_DECIMALS  /*!< Maximum driver decimals */
#define DS18X20_DEFAULT_DECIMALS DS18X20_MAX_DECIMALS - 2  /*!< Default driver decimals */
#define DS18X20_DEFAULT_RESOLUTION DS18X20_RESOLUTION_12 /*!< Default sensor resolution */
#define DS18X20_MAX_CONVERT_TIME    750 /*!< Maximum convert time ( 12-bit resolution) */

#define DS18X20_FUNC_CONVERT    0x44
#define DS18X20_FUNC_WRITE      0x4E
#define DS18X20_FUNC_READ       0xBE

#define DS18X20_IIR_ALPHA   0x04
#define DS18X20_IIR_BETA    0x0C

typedef struct {
    union {
        struct {
            uint8_t lsb;
            uint8_t msb;
        };
        int16_t adc_readout;
    };
    uint8_t th_reg;
    uint8_t tl_reg;
    union {
        struct { 
            uint8_t bit0_4:5;
            uint8_t res:2;
            uint8_t bit7:1;
        };
        uint8_t cfg_reg;
    };
    uint8_t res5;
    uint8_t res6;
    uint8_t res7;
    uint8_t crc;
} ds18x20_pad_t;

typedef struct {
    senos_dev_handle_t handle;
    senos_sensor_handle_t base;
    //senos_sensor_api base;
    struct {
        /** Th register 0x02 */
        uint8_t th;
        /** Tl register 0x03 */
        uint8_t tl;
        /** Config register 0x04 */
        union {
            struct { 
                uint8_t bit0_4:5;
                uint8_t res:2;
                uint8_t bit7:1;
            };
            uint8_t config;
        };
    } reg;
    senos_sensor_magnitude_t temperature;
} ds18x20_sensor_t;

static const uint8_t ds18x20_adc_mask[] = {0xF8, 0xFC, 0xFE, 0xFF};

static esp_err_t ds18x20_add(senos_sensor_hw_conf_t *config, senos_sensor_handle_t **handle);
static esp_err_t ds18x20_remove(void *handle);

static esp_err_t ds18x20_init(void *handle);
static esp_err_t ds18x20_prepare(void *handle);
static esp_err_t ds18x20_measure(void *handle);
static esp_err_t ds18x20_read(void *handle);
static esp_err_t ds18x20_get(void *handle, senos_sensor_mag_caps_t *magnitudes);
static esp_err_t ds18x20_getcaps(void *handle, senos_sensor_caps_t *caps);
static esp_err_t ds18x20_config(void *handle, senos_sensor_mag_caps_t *magnitudes);

static esp_err_t ds18x20_apply_config(ds18x20_sensor_t *sensor);

static const senos_sensor_interface ds18x20_interface = { ._add = &ds18x20_add, ._remove = &ds18x20_remove};

static senos_sensor_api ds18x20_api = {
    ._init = &ds18x20_init,
    ._prepare = &ds18x20_prepare,
    ._measure = &ds18x20_measure,
    ._read = &ds18x20_read,
    ._get = &ds18x20_get,
    ._getcaps = &ds18x20_getcaps,
    ._config = &ds18x20_config
};

static esp_err_t ds18x20_add(senos_sensor_hw_conf_t *config, senos_sensor_handle_t **handle) {
    if(config->type != SENSOR_DS18x20) return ESP_ERR_NOT_SUPPORTED;
    *handle = NULL;
    esp_err_t err;
    senos_dev_cfg_t dev_cfg = {
        .bus_type = SENOS_BUS_1WIRE,
        .dev_1wire = {
            .rom_code = config->ds18x20.rom_code,
            .data_gpio = config->ds18x20.gpio,
            .crc_check = true,
            .addr_bytes = 0,
            .cmd_bytes = 1
        }
    };
    ds18x20_sensor_t *new_ds = (ds18x20_sensor_t *)calloc(1,sizeof(ds18x20_sensor_t));
    if(!new_ds) return ESP_ERR_NO_MEM;
    printf("new_ds %p\n", new_ds);
    err = senos_add_device(&dev_cfg, &new_ds->handle);
    if(ESP_OK != err) {
        free(new_ds);
        return err;
    }
    new_ds->reg.res = DS18X20_DEFAULT_RESOLUTION;
    new_ds->temperature = (senos_sensor_magnitude_t) {
        .decimals = DS18X20_DEFAULT_DECIMALS,
        .type = MAGNITUDE_TEMPERATURE,
        .metric = METRIC_DEGREES,
        .iir_filter = false,
        .resolution = DS18X20_DEFAULT_RESOLUTION,
        .oversampling = true
    };
    new_ds->base = &ds18x20_api;
    (*handle) = &new_ds->base;
    return ESP_OK;
}

static esp_err_t ds18x20_remove(void *handle) {
    ds18x20_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , ds18x20_sensor_t, base);
    if(ESP_OK != senos_remove_device(sensor->handle)) return ESP_FAIL;  /* Device deattach failed by some reason */
    free(sensor);
    return ESP_OK;
}

static esp_err_t ds18x20_init(void *handle) {
    ds18x20_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , ds18x20_sensor_t, base);
    printf("_init *bmx %p\n", sensor);
    return ds18x20_apply_config(sensor);
}

static esp_err_t ds18x20_prepare(void *handle) {
    return ESP_OK;
}

static esp_err_t ds18x20_measure(void *handle) {
    ds18x20_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , ds18x20_sensor_t, base);
    senos_dev_transaction_t tr = {
        .data = (uint8_t *)&sensor->reg,
        .dev_reg = DS18X20_FUNC_CONVERT
    };
    return (*(sensor->handle->api))->_write(&tr, sensor->handle);
}

static esp_err_t ds18x20_read(void *handle) {
    esp_err_t err;
    ds18x20_pad_t pad;
    ds18x20_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , ds18x20_sensor_t, base);
    senos_dev_transaction_t tr = {
        .data = (uint8_t *)&pad,
        .dev_reg = DS18X20_FUNC_READ,
        .rdBytes = 9
    };
    err = (*(sensor->handle->api))->_read(&tr, sensor->handle);
    if(ESP_OK != err) return err;
    pad.lsb = ds18x20_adc_mask[pad.res] & pad.lsb;
    if(sensor->temperature.iir_filter) {
        /* Result in Q20.12 */
        sensor->temperature.value = (uint32_t)(((int32_t)sensor->temperature.value >> 4) * DS18X20_IIR_ALPHA + ((int16_t)pad.adc_readout << 4 ) * DS18X20_IIR_BETA);
    } else sensor->temperature.value = (uint32_t)((int16_t)pad.adc_readout << 8 );
    return ESP_OK;
}

static esp_err_t ds18x20_get(void *handle, senos_sensor_mag_caps_t *magnitudes) {
    ds18x20_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , ds18x20_sensor_t, base);
    for(senos_sensor_mag_caps_t *mag = magnitudes; mag != NULL; mag = mag->next) {
        mag->magnitude = (senos_sensor_magnitude_t){.type = mag->magnitude.type};
        switch (mag->magnitude.type) {
            case MAGNITUDE_TEMPERATURE:
                /** Transfer two LSB ow magnitude bmx structure to two LSB in result linked magnitudes list 
                 * This will transfer type, metric, decimals and valid flag to result structure
                */
                *((uint32_t *)&mag->magnitude) = *((uint32_t *)&sensor->temperature) & 0x0000FFFFLU;
                mag->magnitude.value = (uint32_t)(((int32_t)((float)((int32_t)sensor->temperature.value / 4096.0) * 100)) / 
                    senos_sensor_magnitude_devider[(MAGNITUDE_MAX_DECIMALS - sensor->temperature.decimals)]);
                break;
            default:
                mag->magnitude.valid = false;
                break;
        }
    }
    return ESP_OK;
}

static esp_err_t ds18x20_getcaps(void *handle, senos_sensor_caps_t *caps) {
    //ds18x20_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , ds18x20_sensor_t, base);
    caps->mag_caps = NULL;
    caps->warmup_time = 0;
    caps->prepare_time = 0;
    caps->cooldown_time = 0;
    caps->measure_time = DS18X20_MAX_CONVERT_TIME;
    senos_sensor_mag_caps_t *new_magnitude = (senos_sensor_mag_caps_t *)calloc(1, sizeof(senos_sensor_mag_caps_t));
    if(!new_magnitude) return ESP_ERR_NO_MEM;
    new_magnitude->magnitude = (senos_sensor_magnitude_t) {
        .decimals = DS18X20_MAX_DECIMALS,
        .type = MAGNITUDE_TEMPERATURE,
        .metric = METRIC_DEGREES,
        .iir_filter = true,
        .resolution = DS18X20_RESOLUTION_12,
        .oversampling = true
    };
    new_magnitude->next = NULL;
    caps->mag_caps = new_magnitude;
    return ESP_OK;
}

static esp_err_t ds18x20_config(void *handle, senos_sensor_mag_caps_t *magnitudes) {
    if(!magnitudes) return ESP_ERR_INVALID_ARG;
    ds18x20_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , ds18x20_sensor_t, base);
    for(senos_sensor_mag_caps_t *mag = magnitudes; mag != NULL; mag = mag->next) {
        if(mag->magnitude.decimals > DS18X20_MAX_DECIMALS) mag->magnitude.decimals = DS18X20_MAX_DECIMALS;
        if(mag->magnitude.resolution > DS18X20_RESOLUTION_12) mag->magnitude.decimals = DS18X20_RESOLUTION_12;
        switch (mag->magnitude.type) {
            case MAGNITUDE_TEMPERATURE:
                sensor->temperature.decimals = mag->magnitude.decimals;
                sensor->temperature.iir_filter = mag->magnitude.iir_filter;
                sensor->temperature.resolution = mag->magnitude.resolution;
            default:
                break;
        }
    }
    return ds18x20_apply_config(sensor);
}

static esp_err_t ds18x20_apply_config(ds18x20_sensor_t *sensor) {
    sensor->reg.res = sensor->temperature.resolution;
    senos_dev_transaction_t tr = {
        .data = (uint8_t *)&sensor->reg,
        .dev_reg = DS18X20_FUNC_WRITE,
        .rdBytes = 0,
        .wrBytes = 3
    };
    return (*(sensor->handle->api))->_write(&tr, sensor->handle);
}

void *ds18x20_get_interface(void) {
    return (senos_sensor_interface *)&ds18x20_interface;
}