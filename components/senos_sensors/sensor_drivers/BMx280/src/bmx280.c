/**
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "bmx280.h"
#include "senos_sensor_base.h"
#include "senos_sensor_private.h"
#include "senos_bus_drv.h"

#define BMX280_CALIB00_REG 0x88
#define BMX280_CHIPID_REG 0xD0
#define BMX280_RESET_REG 0xE0
#define BMX280_CALIB26_REG 0xE1
#define BMX280_CTRLH_REG 0xF2
#define BMX280_STATUS_REG 0xF3
#define BMX280_CTRLMEAS_REG 0xF4
#define BMX280_CONFIG_REG 0xF5
#define BMX280_DATA_REG 0xF7

#define BMX280_ID_BME280 0x60
#define BMX280_ID_BMP280 0x58

#define BMX280_DEFAULT_OVERSAMPLING_TEMP 0x01
#define BMX280_DEFAULT_OVERSAMPLING_PRES 0x01
#define BMX280_DEFAULT_OVERSAMPLING_HUMI 0x01
#define BMX280_DEFAULT_IIR_FILTER 0x00
#define BMX280_MAGNITUDE_DISABLED 0x00

#define BMX280_C00DATA_LENGTH 26
#define BMX280_C26DATA_LENGTH 7

typedef struct {
    senos_dev_handle_t handle;
    senos_sensor_handle_t base;
    struct {
        /** ctrl_hum 0xD0 */
        union {
            struct {
                uint8_t chip_id:7;
                uint8_t chip_id_bit7:1;
            };
            uint8_t reg_chipid;
        };
        /** ctrl_hum 0xF2 */
        union {
            struct {
                uint8_t osrs_h:3;
                uint8_t ctrl_hum_bits:5;
            };
            uint8_t reg_ctrl_humi;
        };
        /** Ctrl_meas register 0xF4 */
        union {
            struct { 
                uint8_t mode:2;
                uint8_t osrs_p:3;
                uint8_t osrs_t:3;
            };
            uint8_t reg_ctrl_meas;
        };
        /** Config register 0xF5 */
        union {
            struct {
                uint8_t spi3w_en:1;
                uint8_t config_bit1:1;
                uint8_t filter:3;
                uint8_t t_sb:3;
            };
            uint8_t reg_config;
        };
    };    
    struct {
        uint16_t T1;
        int16_t T2;
        int16_t T3;
        uint16_t P1;
        int16_t P2;
        int16_t P3;
        int16_t P4;
        int16_t P5;
        int16_t P6;
        int16_t P7;
        int16_t P8;
        int16_t P9;
        /* */
        uint8_t REG_A0;
        uint8_t H1;
        /* callibration 26 */
        int16_t H2;
        uint8_t H3;
        /* Bosch mesh */
        int16_t H4;
        int16_t H5;
        int8_t H6;
    } cal;
    uint32_t temperature;
    uint32_t pressure;
    uint32_t humidity;
} bmx280_sensor_t;

static esp_err_t bmx280_add(senos_sensor_hw_conf_t *config, senos_sensor_handle_t *handle);
static esp_err_t bmx280_remove(void *handle);

static senos_sensor_interface bme280_interface = { ._add = &bmx280_add, ._remove = &bmx280_remove};

static esp_err_t bmx280_init(void *handle);
static esp_err_t bmx280_prepare(void *handle);
static esp_err_t bmx280_measure(void *handle);
static esp_err_t bmx280_read(void *handle);
static esp_err_t bmx280_get(void *handle, senos_sensor_magnitude_t *magnitude, float *value);
static esp_err_t bmx280_getcaps(void *handle, senos_sensor_magnitude_t *magnitudes, size_t *len);
static esp_err_t bmx280_config(void *handle, senos_sensor_magnitude_t *magnitudes, size_t *len);

static senos_sensor_api bmx280_api = {
    ._init = &bmx280_init,
    ._prepare = &bmx280_prepare,
    ._measure = &bmx280_prepare,
    ._read = &bmx280_read,
    ._get = &bmx280_get,
    ._getcaps = &bmx280_getcaps,
    ._config = &bmx280_config
};

static esp_err_t bmx280_add(senos_sensor_hw_conf_t *config, senos_sensor_handle_t *handle) {
    if(config->type != SENSOR_BMx280) return ESP_ERR_NOT_SUPPORTED;
    bmx280_sensor_t *new_bmx = (bmx280_sensor_t *)calloc(1,sizeof(bmx280_sensor_t));
    if(!new_bmx) return ESP_ERR_NO_MEM;
    //new_bmx->base = &bmx280_api;
    esp_err_t err;
    senos_dev_cfg_t dev_cfg = {
        .bus_type = SENOS_BUS_I2C,
        .dev_i2c = {
            .scl_gpio = config->bmx280.scl,
            .sda_gpio = config->bmx280.sda,
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,  //i2c_master.h е дефиницията. не е ммного добра идея да е точно така
            .scl_speed_hz = 400000U,
            .xfer_timeout_ms = 10,
            .device_address = 0x76,
            .disable_ack_check = false,
            .addr_bytes = 1,
            .cmd_bytes = 0
        }
    };
    if(ESP_OK != senos_probe_device(&dev_cfg)) {
        dev_cfg.dev_i2c.device_address = 0x77;
        if(ESP_OK != senos_probe_device(&dev_cfg)) return ESP_ERR_NOT_FOUND;
    }
    err = senos_add_device(&dev_cfg, &new_bmx->handle);
    if(ESP_OK != err) {
        free(new_bmx);
        return err;
    }
    new_bmx->chip_id = 0;
    new_bmx->spi3w_en = config->bmx280.spi3w;   //Направо да се фиксира на 0 щото няма SPI
    new_bmx->mode = 0;
    new_bmx->t_sb = 0;
    new_bmx->filter = BMX280_DEFAULT_IIR_FILTER;
    new_bmx->osrs_t = BMX280_DEFAULT_OVERSAMPLING_TEMP;
    new_bmx->osrs_p = BMX280_DEFAULT_OVERSAMPLING_PRES;
    new_bmx->osrs_h = BMX280_MAGNITUDE_DISABLED;
    //new_bmx->osrs_h = (new_bmx->chip_id == BMX280_ID_BME280) ? BMX280_DEFAULT_OVERSAMPLING_HUMI : BMX280_MAGNITUDE_DISABLED;
    new_bmx->base = &bmx280_api;
    //(*(senos_sensor_handle_t *)handle) = &new_bmx->base;
    *handle = &new_bmx->base;
    return ESP_OK;
}

static esp_err_t bmx280_remove(void *handle) {
    return ESP_OK;
}

/** Communication API */
static esp_err_t bmx280_init(void *handle) {
    bmx280_sensor_t *bmx = __containerof((senos_sensor_handle_t)handle , bmx280_sensor_t, base);
    if(bmx->chip_id != 0) return ESP_OK;
    //senos_sensor_api *api = *((senos_sensor_handle_t *)handle);
    uint8_t chip_id;
    esp_err_t err;
    //senos_drv_api_t driver = *(new_bmx->handle->api);
    senos_dev_transaction_t rtrans = {
        .data = &chip_id,
        .dev_reg = BMX280_CHIPID_REG,
        .rdBytes = 1
    };
    err = (*(bmx->handle->api))->_read(&rtrans, bmx->handle);
    if(ESP_OK != err) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    bmx->chip_id = chip_id & 0x7f;
    if(bmx->chip_id != BMX280_ID_BMP280 || bmx->chip_id != BMX280_ID_BME280) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    rtrans.dev_reg = BMX280_CALIB00_REG;
    rtrans.rdBytes = BMX280_C00DATA_LENGTH;
    rtrans.data = &bmx->cal;
    err = (*(bmx->handle->api))->_read(&rtrans, bmx->handle);
    if(ESP_OK != err) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    if(bmx->chip_id == BMX280_ID_BME280) {
        bmx->osrs_h = BMX280_DEFAULT_OVERSAMPLING_HUMI;
        rtrans.dev_reg = BMX280_CALIB26_REG;
        rtrans.rdBytes = BMX280_C26DATA_LENGTH;
        rtrans.data = &bmx->cal.H2;
        err = (*(bmx->handle->api))->_read(&rtrans, bmx->handle);
        if(ESP_OK != err) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        /*uint8_t E5 = (new_bmx->cal.H4 & 0xFF00) >> 8;
        new_bmx->cal.H6 = (new_bmx->cal.H5 & 0xFF00) >> 8;
        //new_bmx->cal.H4 = ((new_bmx->cal.H4 & 0x00FF) << 8 | ((E5 & 0x0F) << 4)) >> 4;
        new_bmx->cal.H4 = (new_bmx->cal.H4 & 0x00FF) << 4 | (E5 & 0x0F);
        new_bmx->cal.H5 = (new_bmx->cal.H5 & 0x00FF) << 4 | ((E5 & 0xF0) >> 4); */
        uint8_t E5 = bmx->cal.H4 >> 8;
        bmx->cal.H6 = bmx->cal.H5 >> 8;
        bmx->cal.H4 = (bmx->cal.H4 & 0xFF) << 4 | (E5 & 0x0F);
        bmx->cal.H5 = (bmx->cal.H5 & 0xFF) << 4 | (E5 >> 4);
    }
    return ESP_OK;
}

static esp_err_t bmx280_prepare(void *handle) {
    return ESP_OK;
}

static esp_err_t bmx280_measure(void *handle) {
    return ESP_OK;
}

static esp_err_t bmx280_read(void *handle) {
    return ESP_OK;
}

static esp_err_t bmx280_get(void *handle, senos_sensor_magnitude_t *magnitude, float *value) {
    return ESP_OK;
}

static esp_err_t bmx280_getcaps(void *handle, senos_sensor_magnitude_t *magnitudes, size_t *len) {
    return ESP_OK;
}

static esp_err_t bmx280_config(void *handle, senos_sensor_magnitude_t *magnitudes, size_t *len) {
    return ESP_OK;
}

void *bmx280_get_interface(void) {
    return NULL;
}