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

#define BMX280_REG_CALIB00 0x88
#define BMX280_REG_CHIPID 0xD0
#define BMX280_REG_RESET 0xE0
#define BMX280_REG_CALIB26 0xE1
#define BMX280_REG_CTRLH 0xF2
#define BMX280_REG_STATUS 0xF3
#define BMX280_REG_CTRLMEAS 0xF4
#define BMX280_REG_CONFIG 0xF5
#define BMX280_REG_DATA 0xF7

#define BMX280_ID_BME280 0x60
#define BMX280_ID_BMP280 0x58

#define BMX280_DEFAULT_OVERSAMPLING_TEMP 0x01
#define BMX280_DEFAULT_OVERSAMPLING_PRES 0x01
#define BMX280_DEFAULT_OVERSAMPLING_HUMI 0x01
#define BMX280_DEFAULT_IIR_FILTER 0x00
#define BMX280_MAGNITUDE_DISABLED 0x00

#define BMX280_C00DATA_LENGTH 26
#define BMX280_C26DATA_LENGTH 7

#define BMX280_MODE_SLEEP 0x00
#define BMX280_MODE_FORCED 0x02

/**
 * Values in Q24.8 format.
 */
#define BMX280_PRESSURE_MIN 7680000
#define BMX280_PRESSURE_MAX 32000000

typedef struct {
    senos_dev_handle_t handle;
    senos_sensor_handle_t base;
    struct {
        /** ctrl_hum 0xD0 */
        union {
            struct {
                uint8_t chip_id;
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
    senos_sensor_magnitude_t temperature;
    senos_sensor_magnitude_t pressure;
    senos_sensor_magnitude_t humidity;
} bmx280_sensor_t;

static esp_err_t bmx280_add(senos_sensor_hw_conf_t *config, senos_sensor_handle_t *handle);
static esp_err_t bmx280_remove(void *handle);

static const senos_sensor_interface bme280_interface = { ._add = &bmx280_add, ._remove = &bmx280_remove};

static esp_err_t bmx280_init(void *handle);
static esp_err_t bmx280_prepare(void *handle);
static esp_err_t bmx280_measure(void *handle);
static esp_err_t bmx280_read(void *handle);
static esp_err_t bmx280_get(void *handle, senos_sensor_magnitude_t *magnitude, float *value);
static esp_err_t bmx280_getcaps(void *handle, senos_sensor_caps_t *caps);
static esp_err_t bmx280_config(void *handle, senos_sensor_mag_caps_t *magnitudes);

/**
 * esp_err_t (*_getcaps)(void *handle, senos_sensor_caps_t *caps);
    esp_err_t (*_config)(void *handle, senos_sensor_mag_caps_t *magnitudes);
 */

static senos_sensor_api bmx280_api = {
    ._init = &bmx280_init,
    ._prepare = &bmx280_prepare,
    ._measure = &bmx280_prepare,
    ._read = &bmx280_read,
    ._get = &bmx280_get,
    ._getcaps = &bmx280_getcaps,
    ._config = &bmx280_config
};

static esp_err_t bmx_apply_config(bmx280_sensor_t *bmx);

static esp_err_t bmx280_add(senos_sensor_hw_conf_t *config, senos_sensor_handle_t *handle) {
    if(config->type != SENSOR_BMx280) return ESP_ERR_NOT_SUPPORTED;
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
    bmx280_sensor_t *new_bmx = (bmx280_sensor_t *)calloc(1,sizeof(bmx280_sensor_t));
    if(!new_bmx) return ESP_ERR_NO_MEM;
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
    new_bmx->temperature = (senos_sensor_magnitude_t) {
        .decimals = 2,
        .sel_decimals = 2,
        .type = MAGNITUDE_TEMPERATURE,
        .iir_filter = false,
        .oversampling = BMX280_DEFAULT_OVERSAMPLING_TEMP,
    };
    new_bmx->pressure = (senos_sensor_magnitude_t) {
        .decimals = 1,  //Точност под съмнение. !Направо нула! Компенсацията е в паскали ама едва ли някой го вълнува десети от хектопаскалите
        .sel_decimals = 1,
        .type = MAGNITUDE_PRESSURE,
        .iir_filter = false,
        .oversampling = BMX280_DEFAULT_OVERSAMPLING_PRES,
    };
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
    //uint8_t reg_value;
    esp_err_t err;
    //senos_drv_api_t driver = *(new_bmx->handle->api);
    senos_dev_transaction_t init_tr = {
        .data = &bmx->reg_chipid,
        .dev_reg = BMX280_REG_CHIPID,
        .rdBytes = 1
    };
    err = (*(bmx->handle->api))->_read(&init_tr, bmx->handle);
    if(ESP_OK != err) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    //bmx->chip_id = bmx->reg_chipid & 0x7f;
    if(bmx->chip_id != BMX280_ID_BMP280 || bmx->chip_id != BMX280_ID_BME280) {
        bmx->chip_id = 0;
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    init_tr.dev_reg = BMX280_REG_CALIB00;
    init_tr.rdBytes = BMX280_C00DATA_LENGTH;
    init_tr.data = &bmx->cal;
    err = (*(bmx->handle->api))->_read(&init_tr, bmx->handle); 
    if(ESP_OK != err) goto config_error;

    if(bmx->chip_id == BMX280_ID_BME280) {
        bmx->osrs_h = BMX280_DEFAULT_OVERSAMPLING_HUMI;
        bmx->humidity = (senos_sensor_magnitude_t) {
            .decimals = 1, //Това също.
            .sel_decimals = 1,
            .type = MAGNITUDE_HUMIDITY,
            .iir_filter = false,
            .oversampling = BMX280_DEFAULT_OVERSAMPLING_PRES,
        };
        init_tr.dev_reg = BMX280_REG_CALIB26;
        init_tr.rdBytes = BMX280_C26DATA_LENGTH;
        init_tr.data = &bmx->cal.H2;
        err = (*(bmx->handle->api))->_read(&init_tr, bmx->handle);
        if(ESP_OK != err) goto config_error;
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
    if(bmx_apply_config(bmx)) goto config_error;
    return ESP_OK;

config_error:
    bmx->chip_id = 0;
    return ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t bmx280_prepare(void *handle) {
    return ESP_OK;
}

static esp_err_t bmx280_measure(void *handle) {
    esp_err_t err;
    bmx280_sensor_t *bmx = __containerof((senos_sensor_handle_t)handle , bmx280_sensor_t, base);
    bmx->mode = BMX280_MODE_FORCED;
    senos_dev_transaction_t transaction = {
        .data = &bmx->reg_ctrl_meas,
        .dev_reg = BMX280_REG_CTRLMEAS,
        .wrBytes = 1
    };
    err = (*(bmx->handle->api))->_write(&transaction, bmx->handle);
    bmx->mode = BMX280_MODE_SLEEP;
    return err;
}

static esp_err_t bmx280_read(void *handle) {
    esp_err_t err;
    uint8_t data[8];
    uint32_t adc_value;
    int32_t fine_t, c1, c2, c3, c4, c5;
    bmx280_sensor_t *bmx = __containerof((senos_sensor_handle_t)handle , bmx280_sensor_t, base);
    bmx->temperature.valid = false;
    bmx->pressure.valid = false;
    bmx->humidity.valid = false;
    senos_dev_transaction_t transaction = {
        .data = data,
        .dev_reg = BMX280_REG_DATA,
        .rdBytes = 8
    };
    if(bmx->chip_id != BMX280_ID_BME280) transaction.rdBytes = 6;
    err = (*(bmx->handle->api))->_read(&transaction, bmx->handle);
    if(ESP_OK != err) return err;
    /** Temperature in Degrees C
     * 2233 represents 22.33 DegC
    */
    if(bmx->osrs_t) {
        adc_value = ((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | ((uint32_t)data[5] >> 4);
        c1 = (int32_t)( (adc_value >> 3) - ((int32_t)bmx->cal.T1 << 2));
        c1 = (c1 * (int32_t)bmx->cal.T2) >> 11;
        c2 = (int32_t)(adc_value >> 4) - (int32_t)bmx->cal.T1;
        c2 = (((c2 * c2) >> 12) * (int32_t)bmx->cal.T3) >> 14;
        fine_t = c1 + c2;
        bmx->temperature.value = ((fine_t * 5 + 128) >> 8) / 100;   /* Това определя двата разряда и на практика изрязва цифри след десетичната точка */
        bmx->temperature.valid = true;
        /** Realtive Humidity in % Q22.1 format 
         * 57000 represents 57000/1024 55.66 %RH
        */
        if(bmx->osrs_h) {
            adc_value = ((uint32_t)data[6] << 8) | (uint32_t)data[7];
            c1 = fine_t - 76800L;
            c2 = (int32_t)(adc_value << 14);
            c3 = (int32_t)((int32_t)bmx->cal.H4 << 20);
            c4 = c1 * (int32_t)bmx->cal.H5;
            c5 = (16384L + c2 - c3 - c4) >> 15;
            c2 = (c1 * (int32_t)bmx->cal.H6) >> 10;
            c3 = (c1 * (int32_t)bmx->cal.H3) >> 11;
            c4 = (((32768L + c2) * c2) >> 10) + 2097152L;
            c2 = (8192L + (c4 * (int32_t)bmx->cal.H2)) >> 14;
            c3 = c5 * c2;
            c4 = ((c3 >> 15) * (c3 >> 15)) >> 7;
            c5 = c3 - ((c4 * (int32_t)bmx->cal.H1) >> 4);
            bmx->humidity.value = ((uint32_t)(c5 < 0 ? 0 : (c5 > 419430400 ? 419430400 : c5)) >> 12) / 1024.0;
            bmx->humidity.valid = true;
        }
        /** Pressure in Pa Q24.8 format 
         * 26086400 represents 26086400/256 101900Pa 1019hPa
        */
        if(bmx->osrs_p) {
            int64_t v1, v2, v3;
            adc_value = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | ((uint32_t)data[2] >> 4);
            v1 = (int64_t)fine_t - 128000LL;
            v2 = v1 * v1 * (int64_t)bmx->cal.P6;
            v2 += ((v1 * (int64_t)bmx->cal.P5) << 17);
            v2 += ((int64_t)bmx->cal.P4 << 35);
            v1 = ((v1 * v1 * (int64_t)bmx->cal.P3) >> 8) + ((v1 * ((int64_t)bmx->cal.P2) << 12));
            v1 = (140737488355328LL + v1) * ((int64_t)bmx->cal.P1) >> 33;
            if(v1) {
                v3 = 1048576 - adc_value;
                v3 = (3125 * ((v3 << 31) - v2)) / v1;
                v1 = ((int64_t)bmx->cal.P9 * (v3 >> 8) * (v3 >> 8)) >> 25;
                v2 = ((int64_t)bmx->cal.P8 * v3) >> 19;
                bmx->pressure.value = ((uint32_t)(((v1 + v2 + v3) >> 8) + ((int64_t)bmx->cal.P7 << 4)) / 256.0);
                bmx->pressure.valid = true;
            } else {
                bmx->pressure.value = 30000.0;
            }
        }
    }
    return ESP_OK;
}

static esp_err_t bmx280_get(void *handle, senos_sensor_magnitude_t *magnitude, float *value) {
    return ESP_OK;
}

static esp_err_t bmx280_getcaps(void *handle, senos_sensor_caps_t *caps) {
    esp_err_t err;
    bmx280_sensor_t *bmx = __containerof((senos_sensor_handle_t)handle , bmx280_sensor_t, base);
    caps->mag_caps = NULL;
    caps->warmup_time = 0;
    caps->prepare_time = 0;
    caps->cooldown_time = 0;
    caps->measure_time = 2 + (bmx->osrs_t << 1) + (bmx->osrs_p << 1) + (bmx->osrs_h << 1);
    if(caps->measure_time < 10) caps->measure_time = 10;
    if(!bmx->osrs_t) return ESP_ERR_NOT_SUPPORTED;
    senos_sensor_mag_caps_t *new_magnitude = (senos_sensor_caps_t *)calloc(1, sizeof(senos_sensor_caps_t));
    if(!new_magnitude) goto nomem_error;
    new_magnitude->magnitude = bmx->temperature;
    new_magnitude->next = caps->mag_caps;
    caps->mag_caps = new_magnitude;
    new_magnitude = (senos_sensor_caps_t *)calloc(1, sizeof(senos_sensor_caps_t));
    if(!new_magnitude) goto nomem_error;
    new_magnitude->magnitude = bmx->pressure;
    new_magnitude->next = caps->mag_caps;
    caps->mag_caps = new_magnitude;
    new_magnitude = (senos_sensor_caps_t *)calloc(1, sizeof(senos_sensor_caps_t));
    if(!new_magnitude) goto nomem_error;
    new_magnitude->magnitude = bmx->humidity;
    new_magnitude->next = caps->mag_caps;
    caps->mag_caps = new_magnitude;
    return ESP_OK;
nomem_error:
    while(caps->mag_caps) {
        senos_sensor_mag_caps_t *next = caps->mag_caps;
        free(caps->mag_caps);
        caps->mag_caps = next;
    }
}
static esp_err_t bmx280_config(void *handle, senos_sensor_mag_caps_t *magnitudes) {
    if(!magnitudes) return ESP_ERR_INVALID_ARG;
    bmx280_sensor_t *bmx = __containerof((senos_sensor_handle_t)handle , bmx280_sensor_t, base);
    for(senos_sensor_mag_caps_t *mag = magnitudes; mag != NULL; mag = mag->next) {
        switch (mag->magnitude.type)
        {
            case MAGNITUDE_TEMPERATURE:
                bmx->temperature.sel_decimals = mag->magnitude.sel_decimals;
                bmx->filter = mag->magnitude.iir_filter;
                bmx->osrs_t = mag->magnitude.oversampling;
                break;
            
            case MAGNITUDE_PRESSURE:
                bmx->pressure.sel_decimals = mag->magnitude.sel_decimals;
                bmx->osrs_p = mag->magnitude.oversampling;
                break;

            case MAGNITUDE_HUMIDITY:
                if(bmx->chip_id == BMX280_ID_BME280) {
                    bmx->humidity.sel_decimals = mag->magnitude.sel_decimals;
                    bmx->osrs_h = mag->magnitude.oversampling;
                }
                break;

            default:
                break;
        }
    }
    return bmx_apply_config(bmx);
}

static esp_err_t bmx_apply_config(bmx280_sensor_t *bmx) {
    esp_err_t err;
    senos_dev_transaction_t reg_tr = {
        .dev_reg = BMX280_REG_CTRLH,
        .data = &bmx->reg_ctrl_humi,
        //init_tr.rdBytes = 0;
        .wrBytes = 1
    };
    err = (*(bmx->handle->api))->_write(&reg_tr, bmx->handle);
    if(ESP_OK != err) return err;

    reg_tr.dev_reg = BMX280_REG_CONFIG;
    reg_tr.data = &bmx->reg_config;
    err = (*(bmx->handle->api))->_write(&reg_tr, bmx->handle);
    if(ESP_OK != err) return err;

    reg_tr.dev_reg = BMX280_REG_CTRLMEAS;
    reg_tr.data = &bmx->reg_ctrl_meas;
    err = (*(bmx->handle->api))->_write(&reg_tr, bmx->handle);
    return err;
}

void *bmx280_get_interface(void) {
    return &bme280_interface;
}