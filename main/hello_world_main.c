/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/i2c_master.h"

#include "senos_bus_drv.h"

#include "senos_sensor_base.h"
#include "senos_sensor_magnitudes.h"
//#include "senos_sensor_private.h"
#include "senos_sensors.h"

#include "max31865.h"

#include "esp_log.h"

void hd(const uint8_t *buf, size_t len) {
    if( !len ) return;
    for(int i=0; i<len; i++) printf("%02X ", buf[i]);
    printf("\n");
    return;
}

void app_main(void)
{
    printf("Hello world!\n");
    /*
    uint32_t seconds = 5;
    uint32_t will_wait = 5 * configTICK_RATE_HZ;
    TickType_t start = xTaskGetTickCount();
    TickType_t end = start + will_wait;
    while(end > xTaskGetTickCount()) vTaskDelay(1);
    printf("%lu, Ticks to wait: %lu, Wait period start at: %lu and end at %lu\n", seconds, will_wait, start, end);
    return; 
    */
    senos_sensor_config_t conf;
    conf.hw_cfg = (senos_sensor_hw_conf_t) {
        .type = SENSOR_DS18X20,
        .ds18x20 = {
            .rom_code = 0x28FF8CA7741604DBLLU,
            .gpio = GPIO_NUM_32
        }
    };
    conf.timing = (senos_sensor_meas_timing_t) {
        .measure_interval = 10,
        .report_every = 6
    };
    fnSenosSensorAdd(&conf);
    conf.hw_cfg = (senos_sensor_hw_conf_t) {
        .type = SENSOR_MAX31865,
        .max31865 = {
            .cs = GPIO_NUM_15,
            .mosi = GPIO_NUM_13,
            .miso = GPIO_NUM_12,
            .sclk = GPIO_NUM_14,
            .r_ref = 432,
            .wires_select = MAX31865_3WIRE_SENSOR,
            .filter_select = MAX31865_FILTER_50HZ
        }
    };
    conf.timing = (senos_sensor_meas_timing_t) {
        .measure_interval = 5,
        .report_every = 6
    };
    fnSenosSensorAdd(&conf);
    conf.hw_cfg = (senos_sensor_hw_conf_t) {
        .type = SENSOR_BMX280,
        .bmx280 = {.scl = GPIO_NUM_26, .sda = GPIO_NUM_18, .spi3w = false}
    };
    conf.timing = (senos_sensor_meas_timing_t) {
        .measure_interval = 15,
        .report_every = 4
    };
    fnSenosSensorAdd(&conf);
    conf.hw_cfg = (senos_sensor_hw_conf_t) {
        .type = SENSOR_BMX280,
        .bmx280 = {.scl = GPIO_NUM_22, .sda = GPIO_NUM_21, .spi3w = false}
    };
    conf.timing = (senos_sensor_meas_timing_t) {
        .measure_interval = 15,
        .report_every = 4
    };
    fnSenosSensorAdd(&conf);
    return;
    /*
    uint32_t decimals = 2;
    int32_t tdata = 2312;
    int32_t value, sval, gain;
    for(int i=0; i<16; i++) {
        if(i) {
            if(i < 8) gain = (int32_t)(tdata+(i<<2));
            else gain = (int32_t)(tdata+32);
            sval = (uint32_t)(((int32_t)sval + (int32_t)gain) >> 1);
            value = (uint32_t)((((float)((int32_t)value) - 0.25*((int32_t)value-gain) + 0.5)));
        } else {
            value = tdata;
            sval = tdata;
            gain = 0;
        }
        printf("in: %ld, out: %ld -> %ld\n", gain, value, sval);
    }
    return;
    */
}
