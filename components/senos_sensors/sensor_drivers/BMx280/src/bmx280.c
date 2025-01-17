/**
 * SPDX-FileCopyrightText: 2024 No Company name
 * SPDX-FileCopyrightText: For Internal use only
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "bmx280.h"
#include "senos_sensor_base.h"
#include "senos_bus_drv.h"

typedef struct {
    senos_drv_api_t device;
    senos_sensor_api_t base;
    struct {
        /** ctrl_hum 0xD0 */
        uint32_t chip_id:7;
        uint32_t chip_id_bit7:1;
        /** ctrl_hum 0xF2 */
        uint32_t osrs_h:3;
        uint32_t ctrl_hum_bits:5;
        /** Ctrl_meas register 0xF4 */
        uint32_t mode:2;
        uint32_t osps_p:3;
        uint32_t osps_t:3;
        /** Config register 0xF5 */
        uint32_t spi3w_en:1;
        uint32_t config_bit1:1;
        uint32_t filter:3;
        uint32_t t_sb:3;
    };
    uint32_t temperature;
    uint32_t pressure;
    uint32_t humidity;
} bmx280_sensor_t;

esp_err_t bmx280_init(void *handle);
esp_err_t bmx280_prepare(void *handle);
esp_err_t bmx280_measure(void *handle);
esp_err_t bmx280_read(void *handle);
esp_err_t bmx280_get(void *handle, senos_sensor_magnitude_t *magnitude, float *value);
esp_err_t bmx280_magnitudes(void *handle, senos_sensor_magnitude_t *magnitudes, size_t *len);
esp_err_t bmx280_config(void *handle, senos_sensor_base_conf_t *conf);

senos_sensor_api bmx280_api = {
    ._init = &bmx280_init,
    ._prepare = &bmx280_prepare,
    ._measure = &bmx280_prepare,
    ._read = &bmx280_read,
    ._get = &bmx280_get,
    ._magnitudes = &bmx280_magnitudes,
    ._config = &bmx280_config
};

/** Communication API */
esp_err_t bmx280_init(void *handle) {

    return ESP_OK;
}

esp_err_t bmx280_prepare(void *handle) {
    return ESP_OK;
}

esp_err_t bmx280_measure(void *handle) {
    return ESP_OK;
}

esp_err_t bmx280_read(void *handle) {
    return ESP_OK;
}

esp_err_t bmx280_get(void *handle, senos_sensor_magnitude_t *magnitude, float *value) {
    return ESP_OK;
}

esp_err_t bmx280_magnitudes(void *handle, senos_sensor_magnitude_t *magnitudes, size_t *len) {
    return ESP_OK;
}

esp_err_t bmx280_config(void *handle, senos_sensor_base_conf_t *conf) {
    return ESP_OK;
}

void *bmx280_get_driver(void) {
    return NULL;
}