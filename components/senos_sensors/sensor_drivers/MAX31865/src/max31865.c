/**
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/spi_master.h"
#include "max31865.h"
#include "senos_bus_drv.h"
#include "senos_sensor_base.h"
#include "senos_sensor_private.h"
#include "senos_bus_drv.h"

#define MAX31865_DEFAULT_WIRE CONFIG_SENOS_MAX31865_WIRES /*!< Default probe connection */
#define MAX31865_DEFAULT_FILTER CONFIG_SENOS_MAX31865_FILTER /*!< Default driver filter =50Hz */
#define MAX31865_VBIAS_OFF 1        /*!< VBias control off command */
#define MAX31865_VBIAS_ON 1         /*!< VBias control on command */
#define MAX31865_ONESHOT_START 1    /*!< One shot measurement start */
#define MAX31865_ONESHOT_CLEAR 0    /*!< Clear one shot */

#define MAX31865_DEFAULT_DECIMALS CONFIG_SENOS_MAX31865_DECIMALS  /*!< Default magnitude decimals */
#define MAX31856_MAX_CONVERT_TIME 65    /*!< Maximum conversation time ( 62.5 for 50Hz filter ) */

#define MAX31865_CONFIG_READ_ADDRESS 0x00 /*!< Configuration register read address */
#define MAX31865_CONFIG_WRITE_ADDRESS 0x80 /*!< Configuration register write address */

typedef struct {
    senos_dev_handle_t handle;  /*!< Device API handle */
    senos_sensor_handle_t base; /*!< Sensor API handle */
    struct {
        union {
            struct {
                uint8_t filter:1;       /*!< 50/60Hz filter select */
                uint8_t fault_clear:1;  /*!< Fault status clear */
                uint8_t fault_ctrl:2;   /*!< Fault detection cycle control */
                uint8_t wires:1;        /*!< RTD probe number of wires */
                uint8_t one_shot:1;     /*!< 1-Shot mode control */
                uint8_t mode:1;         /*!< Conversation mode */
                uint8_t vbias:1;        /*!< VBias control on/of */
            };
            uint8_t config_value;   /*!< Configuration register value */
        };
        uint8_t rtd_msb;    /*!< RTD MSB value */
        uint8_t rtd_lsb;    /*!< RTD LSB value */
        uint8_t hft_msb;    /*!< High fault threshold MSB */
        uint8_t hft_lsb;    /*!< High fault threshold LSB */
        uint8_t lft_msb;    /*!< Low fault threshold MSB */
        uint8_t lft_lsb;    /*!< Low fault threshold LSB */
        uint8_t fault_status;   /*!< Fault status */
    } reg;
    uint16_t r_ref;
    senos_sensor_magnitude_t temperature;    /*!< Magnitide temperature */
} max31865_sensor_t;

static esp_err_t max31865_add(senos_sensor_hw_conf_t *config, senos_sensor_handle_t **handle);
static esp_err_t max31865_remove(void *handle);

static esp_err_t max31865_init(void *handle);
static esp_err_t max31865_prepare(void *handle);
static esp_err_t max31865_measure(void *handle);
static esp_err_t max31865_read(void *handle);
static esp_err_t max31865_getvalue(void *handle, senos_sensor_mag_caps_t *magnitudes);
static esp_err_t max31865_getcaps(void *handle, senos_sensor_caps_t *caps);
static esp_err_t max31865_config(void *handle, senos_sensor_mag_caps_t *magnitudes);

static uint32_t max31865_getid(void *handle);

static esp_err_t max31865_apply_config(max31865_sensor_t *sensor);

static const senos_sensor_interface max31865_interface = { ._add = &max31865_add, ._remove = &max31865_remove};

static senos_sensor_api max31865_api = {
    ._init = &max31865_init,
    ._prepare = &max31865_prepare,
    ._measure = &max31865_measure,
    ._read = &max31865_read,
    ._getvalue = &max31865_getvalue,
    ._getcaps = &max31865_getcaps,
    ._config = &max31865_config,
    ._getid = &max31865_getid
};

static esp_err_t max31865_add(senos_sensor_hw_conf_t *config, senos_sensor_handle_t **handle) {
    if(config->type != SENSOR_MAX31865) return ESP_ERR_NOT_SUPPORTED;
    *handle = NULL;
    esp_err_t err;
    senos_dev_cfg_t dev_cfg = {
        .bus_type = SENOS_BUS_SPI,
        .dev_spi = {
            .mosi_gpio = config->max31865.mosi,
            .miso_gpio = config->max31865.miso,
            .sclk_gpio = config->max31865.sclk,
            .cs_gpio = config->max31865.cs,
            .addr_bits = 8,
            .cmd_bits = 0,
            .dummy_bits = 0,
            .mode = 1,
            .pretrans = 8,
            .clock_speed = 2500000,
            .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_NO_DUMMY
        }
    };
    max31865_sensor_t *sensor = (max31865_sensor_t *)calloc(1,sizeof(max31865_sensor_t));
    if(!sensor) return ESP_ERR_NO_MEM;
    printf("new sensor %p\n", sensor);
    err = senos_add_device(&dev_cfg, &sensor->handle);
    if(ESP_OK != err) {
        free(sensor);
        return err;
    }
    sensor->reg.config_value = 0b10000001; // Vbias ON, Conversation Normaly Off, 4 wire, 60Hz filter
    sensor->reg.wires = config->max31865.wires_select;
    sensor->reg.filter = config->max31865.filter_select;
    sensor->temperature = (senos_sensor_magnitude_t) {
        .decimals = MAX31865_DEFAULT_DECIMALS,
        .type = MAGNITUDE_TEMPERATURE,
        .metric = METRIC_DEGREES,
        .iir_filter = CONFIG_SENOS_MAX31865_FILTER,
        .oversampling = true
    };
    if(config->max31865.r_ref == 0) config->max31865.r_ref = 400;
    sensor->r_ref = config->max31865.r_ref;
    sensor->base = &max31865_api;
    (*handle) = &sensor->base;
    return ESP_OK;
}
static esp_err_t max31865_remove(void *handle) {
    max31865_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , max31865_sensor_t, base);
    if(ESP_OK != senos_remove_device(sensor->handle)) return ESP_FAIL;  /* Device deattach failed by some reason */
    free(sensor);
    return ESP_OK;
}

static esp_err_t max31865_init(void *handle) {
    max31865_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , max31865_sensor_t, base);
    return max31865_apply_config(sensor);
}

static esp_err_t max31865_prepare(void *handle) {
    return ESP_OK;
};

static esp_err_t max31865_measure(void *handle) {
    max31865_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , max31865_sensor_t, base);
    sensor->reg.one_shot = MAX31865_ONESHOT_START;  //Ще се нулира автомтично при четенето.
    senos_dev_transaction_t tr = {
        .data = (uint8_t *)&sensor->reg.config_value,
        .dev_reg = MAX31865_CONFIG_WRITE_ADDRESS,
        .rdBytes = 0,
        .wrBytes = 1
    };
    return (*(sensor->handle->api))->_write(&tr, sensor->handle);
}
static esp_err_t max31865_read(void *handle) {
    esp_err_t err;
    max31865_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , max31865_sensor_t, base);
    senos_dev_transaction_t tr = {
        .data = (uint8_t *)&sensor->reg,
        .dev_reg = MAX31865_CONFIG_READ_ADDRESS,
        .rdBytes = 8,
        .wrBytes = 0
    };
    err = (*(sensor->handle->api))->_read(&tr, sensor->handle);
    if(ESP_OK != err || (sensor->reg.rtd_lsb & 0x01)) return ESP_ERR_INVALID_RESPONSE;
    uint16_t adc_readout = ((sensor->reg.rtd_msb << 8) | (sensor->reg.rtd_lsb)) >> 1;
    if(sensor->temperature.iir_filter) {
        sensor->temperature.value = ((sensor->temperature.value >> 4) * 0x04) + ((adc_readout << 4) * 0x0C);
    } else {
        sensor->temperature.value = adc_readout << 8;
    }
    return ESP_OK;
    //return (*(sensor->handle->api))->_write(&tr, sensor->handle);
}

static esp_err_t max31865_getvalue(void *handle, senos_sensor_mag_caps_t *magnitudes) {
    max31865_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , max31865_sensor_t, base);
    for(senos_sensor_mag_caps_t *mag = magnitudes; mag != NULL; mag = mag->next) {
        mag->magnitude = (senos_sensor_magnitude_t){.type = mag->magnitude.type};
        switch (mag->magnitude.type) {
            case MAGNITUDE_TEMPERATURE:
                /** Transfer two LSB ow magnitude bmx structure to two LSB in result linked magnitudes list 
                 * This will transfer type, metric, decimals and valid flag to result structure
                */
                *((uint32_t *)&mag->magnitude) = *((uint32_t *)&sensor->temperature) & 0x0000FFFFLU;
                /** Calculate RTD */
                sensor->temperature.valid = ((~sensor->reg.rtd_lsb) & 0x01);
                double rtd = (double)((int16_t)((float)(0.005 + ((((sensor->temperature.value >> 4) + 5) >> 4) * sensor->r_ref) / 32768.0) * 100) / 100.0);
                /*
                * Rational polynomial function from Mosaic Industries site Section RTD Calibration
                * http://www.mosaic-industries.com/embedded-systems/microcontroller-projects/temperature-measurement/platinum-rtd-sensors/resistance-calibration-table
                * Average absolute error is only 0.015°C over the full temperature range
                */
                double value =-245.19+ ( rtd * (2.5293 + rtd * (-0.066046 + rtd * (0.0040422 -0.0000020697 * rtd))) / (1 + (rtd * (-0.025422 + rtd * (0.0016883-0.0000013601 * rtd)))));
                //double value = rtd*(2.5293+rtd*(0.066046+rtd*(0.0040422+rtd*(-0.0000020697))))/(1+rtd*(-0.025422+rtd*(0.0016883+rtd*(-0.0000013601))))-245.19;
                //double value = -245.19 + ( ( rtd * (2.5293 + rtd * (-0.066046 + rtd * (4.0422e-3 + -2.0697e-6 * rtd))))
                //                / (1 + (rtd * (-0.025422 + rtd * (1.6883e-3 -1.3601e-6 * rtd)))) );
                /** Apply magnitude value */
                mag->magnitude.value = (uint32_t)((value * senos_sensor_magnitude_divider[sensor->temperature.decimals]) + 0.5);
                //printf("get_value %lu\n", mag->magnitude.value);
                break;
            default:
                mag->magnitude.valid = false;
                break;
        }
    }
    return ESP_OK;
}
static esp_err_t max31865_getcaps(void *handle, senos_sensor_caps_t *caps) {
    caps->mag_caps = NULL;
    caps->warmup_time = 0;
    caps->prepare_time = 0;      //Ако в припеър се вдига вибиас, трябва да му се бодне число
    caps->cooldown_time = 0;
    caps->measure_time = MAX31856_MAX_CONVERT_TIME;
    senos_sensor_mag_caps_t *new_magnitude = (senos_sensor_mag_caps_t *)calloc(1, sizeof(senos_sensor_mag_caps_t));
    if(!new_magnitude) return ESP_ERR_NO_MEM;
    new_magnitude->magnitude = (senos_sensor_magnitude_t) {
        .decimals = MAGNITUDE_MAX_DECIMALS,
        .type = MAGNITUDE_TEMPERATURE,
        .metric = METRIC_DEGREES,
        .iir_filter = false,
        .oversampling = true
    };
    new_magnitude->next = NULL;
    caps->mag_caps = new_magnitude;
    return ESP_OK;
}
static esp_err_t max31865_config(void *handle, senos_sensor_mag_caps_t *magnitudes) {
    if(!magnitudes) return ESP_ERR_INVALID_ARG;
    max31865_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , max31865_sensor_t, base);
    for(senos_sensor_mag_caps_t *mag = magnitudes; mag != NULL; mag = mag->next) {
        if(mag->magnitude.decimals > MAGNITUDE_MAX_DECIMALS) mag->magnitude.decimals = MAX31865_DEFAULT_DECIMALS;
        switch (mag->magnitude.type) {
            case MAGNITUDE_TEMPERATURE:
                sensor->temperature.decimals = mag->magnitude.decimals;
                sensor->temperature.iir_filter = mag->magnitude.iir_filter;
            default:
                break;
        }
    }
    //return ds18x20_apply_config(sensor);
    return ESP_OK;
}

static uint32_t max31865_getid(void *handle) {
    max31865_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , max31865_sensor_t, base);
    return (*(sensor->handle->api))->_getid(sensor->handle);
}

static esp_err_t max31865_apply_config(max31865_sensor_t *sensor) {
    senos_dev_transaction_t tr = {
        .data = (uint8_t *)&sensor->reg.config_value,
        .dev_reg = MAX31865_CONFIG_WRITE_ADDRESS,
        .rdBytes = 0,
        .wrBytes = 1
    };
    return (*(sensor->handle->api))->_write(&tr, sensor->handle);
}

void *max31865_get_interface(void) {
    return (senos_sensor_interface *)&max31865_interface;
}