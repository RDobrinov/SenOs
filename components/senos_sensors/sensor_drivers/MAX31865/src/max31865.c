/**
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */
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

#define MAX31865_DEFAULT_WIRE MAX31865_4WIRE_SENSOR /*!< Default probe connection */
#define MAX31865_DEFAULT_FILTER MAX31865_FILTER_50HZ /*!< Default driver filter =50Hz */
#define MAX31865_VBIAS_OFF 1        /*!< VBias control off command */
#define MAX31865_VBIAS_ON 1         /*!< VBias control on command */
#define MAX31865_ONESHOT_START 1    /*!< One shot measurement start */
#define MAX31865_ONESHOT_CLEAR 1    /*!< Clear one shot */

#define MAX31865_DEFAULT_DECIMALS MAGNITUDE_MAX_DECIMALS  /*!< Maximum driver decimals */
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

static esp_err_t max31865_apply_config(max31865_sensor_t *sensor);

static const senos_sensor_interface max31865_interface = { ._add = &max31865_add, ._remove = &max31865_remove};

static senos_sensor_api max31865_api = {
    ._init = &max31865_init,
    ._prepare = &max31865_prepare,
    ._measure = &max31865_measure,
    ._read = &max31865_read,
    ._getvalue = &max31865_getvalue,
    ._getcaps = &max31865_getcaps,
    ._config = &max31865_config
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
        .iir_filter = false,
        .oversampling = true
    };
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
    //esp_err_t err;
    sensor->reg.one_shot = MAX31865_ONESHOT_START;  //Ще се нулира автомтично про четенето.
    senos_dev_transaction_t tr = {
        .data = (uint8_t *)&sensor->reg.config_value,
        .dev_reg = MAX31865_CONFIG_WRITE_ADDRESS,
        .rdBytes = 0,
        .wrBytes = 1
    };
    //err = (*(sensor->handle->api))->_write(&tr, sensor->handle);
    //sensor->reg.one_shot = MAX31865_ONESHOT_CLEAR;
    return (*(sensor->handle->api))->_write(&tr, sensor->handle);
}
static esp_err_t max31865_read(void *handle) {
    max31865_sensor_t *sensor = __containerof((senos_sensor_handle_t *)handle , max31865_sensor_t, base);
    esp_err_t err;
    //sensor->reg.one_shot = MAX31865_ONESHOT_START;
    senos_dev_transaction_t tr = {
        .data = (uint8_t *)&sensor->reg,
        .dev_reg = MAX31865_CONFIG_READ_ADDRESS,
        .rdBytes = 8,
        .wrBytes = 0
    };
    err = (*(sensor->handle->api))->_write(&tr, sensor->handle);
    //sensor->reg.one_shot = MAX31865_ONESHOT_CLEAR;
    return err;
}

static esp_err_t max31865_getvalue(void *handle, senos_sensor_mag_caps_t *magnitudes) {
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
        //.resolution = DS18X20_RESOLUTION_12,
        .oversampling = true
    };
    new_magnitude->next = NULL;
    caps->mag_caps = new_magnitude;
    return ESP_OK;
}
static esp_err_t max31865_config(void *handle, senos_sensor_mag_caps_t *magnitudes) {
    return ESP_OK;
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