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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

#define BMX280_MAX_DECIMALS MAGNITUDE_MAX_DECIMALS  /*!< Maximum BMX sensor driver decimals */

/**
 * Values in Q24.8 format.
 */
#define BMX280_PRESSURE_MIN 7680000
#define BMX280_PRESSURE_MAX 32000000

typedef struct {
    senos_dev_handle_t handle;
    senos_sensor_handle_t base;
    //senos_sensor_api base;
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
        uint8_t REG_E4;
        /* Bosch mesh */
        int16_t H4;
        int16_t H5;
        int8_t H6;
    } cal;
    senos_sensor_magnitude_t temperature;
    senos_sensor_magnitude_t pressure;
    senos_sensor_magnitude_t humidity;
} bmx280_sensor_t;

void bmx_hd(const uint8_t *buf, size_t len) {
    if( !len ) return;
    for(int i=0; i<len; i++) printf("%02X ", buf[i]);
    printf("\n");
    return;
}

static esp_err_t bmx280_add(senos_sensor_hw_conf_t *config, senos_sensor_handle_t **handle);
static esp_err_t bmx280_remove(void *handle);

static const senos_sensor_interface bmx280_interface = { ._add = &bmx280_add, ._remove = &bmx280_remove};

static esp_err_t bmx280_init(void *handle);
static esp_err_t bmx280_prepare(void *handle);
static esp_err_t bmx280_measure(void *handle);
static esp_err_t bmx280_read(void *handle);
static esp_err_t bmx280_getvalue(void *handle, senos_sensor_mag_caps_t *magnitudes);
static esp_err_t bmx280_getcaps(void *handle, senos_sensor_caps_t *caps);
static esp_err_t bmx280_config(void *handle, senos_sensor_mag_caps_t *magnitudes);

static uint32_t bmx280_getid(void *handle);

/**
 * esp_err_t (*_getcaps)(void *handle, senos_sensor_caps_t *caps);
    esp_err_t (*_config)(void *handle, senos_sensor_mag_caps_t *magnitudes);
 */

static senos_sensor_api bmx280_api = {
    ._init = &bmx280_init,
    ._prepare = &bmx280_prepare,
    ._measure = &bmx280_measure,
    ._read = &bmx280_read,
    ._getvalue = &bmx280_getvalue,
    ._getcaps = &bmx280_getcaps,
    ._config = &bmx280_config,
    ._getid = &bmx280_getid
};

static esp_err_t bmx_apply_config(bmx280_sensor_t *bmx);

static esp_err_t bmx280_add(senos_sensor_hw_conf_t *config, senos_sensor_handle_t **handle) {
    if(config->type != SENSOR_BMX280) return ESP_ERR_NOT_SUPPORTED;
    *handle = NULL;
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
        printf("probe 0x76\n");
        dev_cfg.dev_i2c.device_address = 0x77;
        printf("probe 0x76\n");
        if(ESP_OK != senos_probe_device(&dev_cfg)) return ESP_ERR_NOT_FOUND;
    }
    printf("OK\n");
    bmx280_sensor_t *new_bmx = (bmx280_sensor_t *)calloc(1,sizeof(bmx280_sensor_t));
    if(!new_bmx) return ESP_ERR_NO_MEM;
    printf("new_bmx %p\n", new_bmx);
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
        .decimals = BMX280_MAX_DECIMALS,
        .type = MAGNITUDE_TEMPERATURE,
        .metric = METRIC_DEGREES,
        .iir_filter = false,
        .oversampling = BMX280_DEFAULT_OVERSAMPLING_TEMP,
    };
    new_bmx->pressure = (senos_sensor_magnitude_t) {
        .decimals = BMX280_MAX_DECIMALS,  //Точност под съмнение. !Направо нула! Компенсацията е в паскали ама едва ли някой го вълнува десети от хектопаскалите
        .type = MAGNITUDE_PRESSURE,
        .metric = METRIC_HECTOPASCSAL,
        .iir_filter = false,
        .oversampling = BMX280_DEFAULT_OVERSAMPLING_PRES,
    };
    //new_bmx->osrs_h = (new_bmx->chip_id == BMX280_ID_BME280) ? BMX280_DEFAULT_OVERSAMPLING_HUMI : BMX280_MAGNITUDE_DISABLED;
    new_bmx->base = &bmx280_api;
    //(*(senos_sensor_handle_t *)handle) = &new_bmx->base;
    (*handle) = &new_bmx->base;
    printf("END\n");
    return ESP_OK;
}

static esp_err_t bmx280_remove(void *handle) {
    bmx280_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , bmx280_sensor_t, base);
    if(ESP_OK != senos_remove_device(sensor->handle)) return ESP_FAIL;  /* Device deattach failed by some reason */
    free(sensor);
    return ESP_OK;
}

/** Communication API */
static esp_err_t bmx280_init(void *handle) {
    bmx280_sensor_t *bmx = __containerof((senos_sensor_handle_t *)handle , bmx280_sensor_t, base);
    printf("_init *bmx %p\n", bmx);

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
    printf("_init error: %d, %02X\n", err, bmx->reg_chipid);
    if(ESP_OK != err) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    //bmx->chip_id = bmx->reg_chipid & 0x7f;
    if(bmx->chip_id != BMX280_ID_BMP280 && bmx->chip_id != BMX280_ID_BME280) {
        bmx->chip_id = 0;
        return ESP_ERR_NOT_SUPPORTED;
    }
    printf("ID OK\n");
    vTaskDelay(2);
    init_tr.dev_reg = BMX280_REG_CALIB00;
    init_tr.rdBytes = BMX280_C00DATA_LENGTH;
    //init_tr.data = (uint8_t *)&bmx->cal;
    init_tr.data = (uint8_t *)&bmx->cal.T1;
    printf("Pointers %p, %p, %p\n", init_tr.data, &bmx->cal.T1, &bmx->cal);
    err = (*(bmx->handle->api))->_read(&init_tr, bmx->handle);
    /** Testing */
    //uint8_t *buf = (uint8_t *)&bmx->cal.T1;
    printf("%x,%x,%x,%x\n",init_tr.data[1], init_tr.data[0], init_tr.data[3], init_tr.data[4]);
    uint16_t T1 = (uint16_t)(((uint16_t)init_tr.data[1] << 8) | (uint16_t)init_tr.data[0]);
    int16_t T2 = (int16_t)(((uint16_t)init_tr.data[3] << 8) | ((uint16_t)init_tr.data[2]));
    /** */
    //bmx_hd((uint8_t *)&bmx->cal, BMX280_C00DATA_LENGTH);
    bmx_hd((uint8_t *)&bmx->cal.T1, BMX280_C00DATA_LENGTH);
    printf("[%u, %d] %u, %d\n", bmx->cal.T1, bmx->cal.T2, T1, T2);
    if(ESP_OK != err) goto config_error;

    if(bmx->chip_id == BMX280_ID_BME280) {
        bmx->osrs_h = BMX280_DEFAULT_OVERSAMPLING_HUMI;
        bmx->humidity = (senos_sensor_magnitude_t) {
            .decimals = BMX280_MAX_DECIMALS, //Това също.
            .type = MAGNITUDE_HUMIDITY,
            .metric = METRIC_PRECENTAGE,
            .iir_filter = false,
            .oversampling = BMX280_DEFAULT_OVERSAMPLING_PRES,
        };
        init_tr.dev_reg = BMX280_REG_CALIB26;
        init_tr.rdBytes = BMX280_C26DATA_LENGTH;
        init_tr.data = (uint8_t *)&bmx->cal.H2;
        err = (*(bmx->handle->api))->_read(&init_tr, bmx->handle);
        printf("H2: ");
        bmx_hd((uint8_t *)&bmx->cal.H2, BMX280_C26DATA_LENGTH);
        if(ESP_OK != err) goto config_error;
        /*uint8_t E5 = (new_bmx->cal.H4 & 0xFF00) >> 8;
        new_bmx->cal.H6 = (new_bmx->cal.H5 & 0xFF00) >> 8;
        //new_bmx->cal.H4 = ((new_bmx->cal.H4 & 0x00FF) << 8 | ((E5 & 0x0F) << 4)) >> 4;
        new_bmx->cal.H4 = (new_bmx->cal.H4 & 0x00FF) << 4 | (E5 & 0x0F);
        new_bmx->cal.H5 = (new_bmx->cal.H5 & 0x00FF) << 4 | ((E5 & 0xF0) >> 4); */
        //uint8_t E5 = bmx->cal.H4 >> 8;
        //bmx->cal.H6 = (char)(bmx->cal.H5 >> 8);
        uint8_t E5 = (uint8_t)(bmx->cal.H4 & 0xFF);
        bmx->cal.H6 = (uint8_t)(bmx->cal.H5 & 0xFF);
        printf("[%p]H2, [%p]H3, [%p] ", &bmx->cal.H2, &bmx->cal.REG_E4, &bmx->cal.H3);
        printf("[%p]H4:%04X, [%p]H5:%04X, [%p]H6:%02X\n", &bmx->cal.H4, (uint16_t)bmx->cal.H4, 
                &bmx->cal.H5, (uint16_t)bmx->cal.H5, &bmx->cal.H6, (uint8_t)bmx->cal.H6);
        printf("E5:%02X, H5:%04X", E5, (uint16_t)bmx->cal.H5);
        bmx->cal.H5 = ((bmx->cal.H4 >> 8) & 0xFF) << 4 | (E5 >> 4);
        bmx->cal.H4 = (bmx->cal.REG_E4) << 4 | (E5 & 0x0F);
        //bmx->cal.H4 = (bmx->cal.H4 & 0xFF) << 4 | (E5 & 0x0F);
        //bmx->cal.H5 = (bmx->cal.H5 & 0xFF) << 4 | (E5 >> 4);
        printf("\n%u, %d, %u, %d, %d, %d\n", bmx->cal.H1, bmx->cal.H2, bmx->cal.H3, bmx->cal.H4, bmx->cal.H5, bmx->cal.H6);
    }
    if(bmx_apply_config(bmx)) goto config_error;
    bmx_hd((uint8_t *)bmx, sizeof(bmx280_sensor_t));
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
    bmx280_sensor_t *bmx = __containerof((senos_sensor_handle_t *)handle , bmx280_sensor_t, base);
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
    int32_t fine_t, c1, c2; //, c3, c4, c5;
    bmx280_sensor_t *bmx = __containerof((senos_sensor_handle_t *)handle , bmx280_sensor_t, base);
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
    //bmx_hd(data, 8);
    if(bmx->osrs_t) {
        adc_value = ((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | ((uint32_t)data[5] >> 4);
        printf("adc_value_t %lu\n", adc_value);
        c1 = (int32_t)( (adc_value >> 3) - ((int32_t)bmx->cal.T1 << 1));
        c1 = (c1 * (int32_t)bmx->cal.T2) >> 11;
        c2 = (int32_t)(adc_value >> 4) - (int32_t)bmx->cal.T1;
        c2 = (((c2 * c2) >> 12) * (int32_t)bmx->cal.T3) >> 14;
        fine_t = c1 + c2;
        bmx->temperature.value = (uint32_t)(fine_t * 5 + 128);  //Q24.8 (by 256) but already multiple by 100 i.e. 2 decimals
        bmx->temperature.valid = true;
        /** Realtive Humidity in % Q22.10 format 
         * 57000 represents 57000/1024 55.66 %RH
        */
        if(bmx->osrs_h) {
            adc_value = ((uint32_t)data[6] << 8) | (uint32_t)data[7];
            //printf("adc_value_h %lu\n", adc_value);
            /*
            c1 = fine_t - (int32_t)76800;
            c2 = (int32_t)(adc_value << 14);
            c3 = (int32_t)((int32_t)bmx->cal.H4 << 20);
            c4 = (int32_t)bmx->cal.H5 * c1;
            c5 = ((int32_t)16384 + c2 - c3 - c4) >> 15;
            c2 = (c1 * (int32_t)bmx->cal.H6) >> 10;
            c3 = (c1 * (int32_t)bmx->cal.H3) >> 11;
            //c4 = (((32768L + c2) * c2) >> 10) + 2097152L;
            c4 = ((((int32_t)32768 + c3) * c2) >> 10) + (int32_t)2097152;
            c2 = (8192L + (c4 * (int32_t)bmx->cal.H2)) >> 14;
            c3 = c5 * c2;
            c4 = ((c3 >> 15) * (c3 >> 15)) >> 7;
            c5 = c3 - ((c4 * (int32_t)bmx->cal.H1) >> 4);
            bmx->humidity.value = (uint32_t)((c5 < 0 ? 0 : (c5 > 419430400 ? 419430400 : c5)) >> 12); //Q22.10 by 1024
            */            
            c1 = (fine_t - ((int32_t)76800));

            c1 = (((((adc_value << 14) - (((int32_t)bmx->cal.H4) << 20) -
                (((int32_t)bmx->cal.H5) * c1)) + ((int32_t)16384)) >> 15) *
                (((((((c1 * ((int32_t)bmx->cal.H6)) >> 10) *
                (((c1 * ((int32_t)bmx->cal.H3)) >> 11) + ((int32_t)32768))) >> 10) +
                ((int32_t)2097152)) * ((int32_t)bmx->cal.H2) + 8192) >> 14));

            c1 = (c1 - (((((c1 >> 15) * (c1 >> 15)) >> 7) *
                ((int32_t)bmx->cal.H1)) >> 4));
            bmx->humidity.value = (uint32_t)((c1 < 0 ? 0 : (c1 > 419430400 ? 419430400 : c1)) >> 12); //Q22.10 by 1024 

            bmx->humidity.valid = true;
        }
        /** Pressure in Pa Q24.8 format 
         * 26086400 represents 26086400/256 101900Pa 1019hPa
        */
        /* from datasheet */
        //v_x1_u32r = (((((adc_H << 14) - (((BME280_S32_t)dig_H4) << 20) - (((BME280_S32_t)dig_H5) * v_x1_u32r)) + ((BME280_S32_t)16384)) >> 15) * (((((((v_x1_u32r * 
        //((BME280_S32_t)dig_H6)) >> 10) * (((v_x1_u32r * ((BME280_S32_t)dig_H3)) >> 11) + ((BME280_S32_t)32768))) >> 10) + ((BME280_S32_t)2097152)) * ((BME280_S32_t)dig_H2) + 
        //8192) >> 14));
        //v_x1_u32r = (v_x1_u32r – (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((BME280_S32_t)dig_H1)) >> 4)); 
        //v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);  
        //v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r); 
        if(bmx->osrs_p) {
            int64_t v1, v2, v3;
            adc_value = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | ((uint32_t)data[2] >> 4);
            printf("adc_value_p %lu\n", adc_value);
            v1 = ((int64_t)fine_t) - 128000;
            v2 = v1 * v1 * (int64_t)bmx->cal.P6;
            v2 += ((v1 * (int64_t)bmx->cal.P5) << 17);
            v2 += ((int64_t)bmx->cal.P4 << 35);
            v1 = ((v1 * v1 * (int64_t)bmx->cal.P3) >> 8) + ((v1 * (int64_t)bmx->cal.P2) << 12);
            v1 = (v1 + ((int64_t)140737488355328)) * ((int64_t)bmx->cal.P1) >> 33;
            if(v1 != 0) {
                v3 = 1048576 - (int32_t)adc_value;
                v3 = (3125 * ((v3 << 31) - v2)) / v1;
                //v1 = ((int64_t)bmx->cal.P9 * (v3 >> 8) * (v3 >> 8)) >> 25;
                v1 = ((int64_t)bmx->cal.P9 * (v3 >> 13) * (v3 >> 13)) >> 25;
                v2 = ((int64_t)bmx->cal.P8 * v3) >> 19;
                bmx->pressure.value = (uint32_t)(((v1 + v2 + v3) >> 8) + ((int64_t)bmx->cal.P7 << 4)); //Q24.8 (by 256) in Pascals. Decimal precision in hPa
                bmx->pressure.valid = true;
            } else {
                bmx->pressure.value = 7680000;
            }
        }
    }
    printf("Magnitudes %08lX, %08lX, %08lX\n", bmx->temperature.value, bmx->pressure.value, bmx->humidity.value);
    //float test_val = ((((float)((int32_t)bmx->temperature.value / 25600.0)) * 
    //                senos_sensor_magnitude_divider[bmx->temperature.decimals]) + 0.5);
    //printf("Magnitudes %f, %f, %f, %f\n", test_val, (float)((int32_t)bmx->temperature.value / 256.0), bmx->pressure.value/256.0, (bmx->humidity.value)/1024.0);
    return ESP_OK;
}

static esp_err_t bmx280_getvalue(void *handle, senos_sensor_mag_caps_t *magnitudes) {
    bmx280_sensor_t *bmx = __containerof((senos_sensor_handle_t *)handle , bmx280_sensor_t, base);
    for(senos_sensor_mag_caps_t *mag = magnitudes; mag != NULL; mag = mag->next) {
        mag->magnitude = (senos_sensor_magnitude_t){.type = mag->magnitude.type};
        switch (mag->magnitude.type) {
            case MAGNITUDE_TEMPERATURE:
                /** Transfer two LSB ow magnitude bmx structure to two LSB in result linked magnitudes list 
                 * This will transfer type, metric, decimals and valid flag to result structure
                */
                *((uint32_t *)&mag->magnitude) = *((uint32_t *)&bmx->temperature) & 0x0000FFFFLU;
                mag->magnitude.value = (uint32_t)((((float)((int32_t)bmx->temperature.value / 25600.0)) * 
                    senos_sensor_magnitude_divider[bmx->temperature.decimals]) + 0.5);
                break;
            case MAGNITUDE_PRESSURE:
                *((uint32_t *)&mag->magnitude) = *((uint32_t *)&bmx->pressure) & 0x0000FFFFLU;
                mag->magnitude.value = (uint32_t)((((float)((int32_t)bmx->pressure.value / 256.0)) * 
                    senos_sensor_magnitude_divider[bmx->pressure.decimals]) + 0.5);
                break;
            case MAGNITUDE_HUMIDITY:
                *((uint32_t *)&mag->magnitude) = *((uint32_t *)&bmx->humidity) & 0x0000FFFFLU;
                mag->magnitude.value = (uint32_t)((((float)((int32_t)bmx->humidity.value / 1024.0)) * 
                    senos_sensor_magnitude_divider[bmx->humidity.decimals]) + 0.5);
                break;
            default:
                break;
        }
    }
    return ESP_OK;
}

static esp_err_t bmx280_getcaps(void *handle, senos_sensor_caps_t *caps) {
    //esp_err_t err;
    bmx280_sensor_t *bmx = __containerof((senos_sensor_handle_t *)handle , bmx280_sensor_t, base);
    caps->mag_caps = NULL;
    caps->warmup_time = 0;
    caps->prepare_time = 0;
    caps->cooldown_time = 0;
    caps->measure_time = 2 + (bmx->osrs_t << 1) + (bmx->osrs_p << 1) + (bmx->osrs_h << 1);
    if(caps->measure_time < 10) caps->measure_time = 10;
    if(!bmx->osrs_t) return ESP_ERR_NOT_SUPPORTED;
    senos_sensor_mag_caps_t *new_magnitude = (senos_sensor_mag_caps_t *)calloc(1, sizeof(senos_sensor_mag_caps_t));
    if(!new_magnitude) goto nomem_error;
    new_magnitude->magnitude = bmx->temperature;
    new_magnitude->magnitude.decimals = BMX280_MAX_DECIMALS;
    new_magnitude->next = caps->mag_caps;
    caps->mag_caps = new_magnitude;
    new_magnitude = (senos_sensor_mag_caps_t *)calloc(1, sizeof(senos_sensor_mag_caps_t));
    if(!new_magnitude) goto nomem_error;
    new_magnitude->magnitude = bmx->pressure;
    new_magnitude->magnitude.decimals = BMX280_MAX_DECIMALS;
    new_magnitude->next = caps->mag_caps;
    caps->mag_caps = new_magnitude;
    new_magnitude = (senos_sensor_mag_caps_t *)calloc(1, sizeof(senos_sensor_mag_caps_t));
    if(!new_magnitude) goto nomem_error;
    new_magnitude->magnitude = bmx->humidity;
    new_magnitude->magnitude.decimals = BMX280_MAX_DECIMALS;
    new_magnitude->next = caps->mag_caps;
    caps->mag_caps = new_magnitude;
    return ESP_OK;
nomem_error:
    while(caps->mag_caps) {
        senos_sensor_mag_caps_t *next = caps->mag_caps;
        free(caps->mag_caps);
        caps->mag_caps = next;
    }
    return ESP_ERR_NO_MEM;
}
static esp_err_t bmx280_config(void *handle, senos_sensor_mag_caps_t *magnitudes) {
    if(!magnitudes) return ESP_ERR_INVALID_ARG;
    bmx280_sensor_t *bmx = __containerof((senos_sensor_handle_t *)handle , bmx280_sensor_t, base);
    for(senos_sensor_mag_caps_t *mag = magnitudes; mag != NULL; mag = mag->next) {
        if(mag->magnitude.decimals > BMX280_MAX_DECIMALS) mag->magnitude.decimals = BMX280_MAX_DECIMALS;
        switch (mag->magnitude.type) {
            case MAGNITUDE_TEMPERATURE:
                bmx->temperature.decimals = mag->magnitude.decimals;
                bmx->filter = mag->magnitude.iir_filter;
                bmx->osrs_t = mag->magnitude.oversampling;
                break;
            
            case MAGNITUDE_PRESSURE:
                bmx->pressure.decimals = mag->magnitude.decimals;
                bmx->osrs_p = mag->magnitude.oversampling;
                break;

            case MAGNITUDE_HUMIDITY:
                if(bmx->chip_id == BMX280_ID_BME280) {
                    bmx->humidity.decimals = mag->magnitude.decimals;
                    bmx->osrs_h = mag->magnitude.oversampling;
                }
                break;

            default:
                break;
        }
    }
    return bmx_apply_config(bmx);
}

static uint32_t bmx280_getid(void *handle) {
    bmx280_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , bmx280_sensor_t, base);
    return (*(sensor->handle->api))->_getid(sensor->handle);
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
    return (senos_sensor_interface *)&bmx280_interface;
}