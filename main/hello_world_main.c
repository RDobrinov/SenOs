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

void hd(const uint8_t *buf, size_t len) {
    if( !len ) return;
    for(int i=0; i<len; i++) printf("%02X ", buf[i]);
    printf("\n");
    return;
}

void app_main(void)
{
    printf("Hello world!\n");
    /*senos_dev_cfg_t dev = {
        .bus_type = SENOS_BUS_I2C,
        .dev_i2c = {
            .scl_gpio = GPIO_NUM_26,
            .sda_gpio = GPIO_NUM_18,
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .scl_speed_hz = 400000U,
            .xfer_timeout_ms = 10,
            .device_address = 0x76,
            .disable_ack_check = false,
            .addr_bytes = 1,
            .cmd_bytes = 0
        }
    }; */ 
    senos_dev_cfg_t dv = {
        .bus_type = SENOS_BUS_I2C,
        .dev_i2c = {
            .scl_gpio = GPIO_NUM_26,
            .sda_gpio = GPIO_NUM_18,
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .scl_speed_hz = 400000U,
            .xfer_timeout_ms = 10,
            .device_address = 0x76,
            .disable_ack_check = false,
            .addr_bytes = 1,
            .cmd_bytes = 0
        }
    };
    senos_dev_handle_t i2c[2]; 
    senos_add_device(&dv, &i2c[0]);
    printf("i2c[0]:%p->%p\n", i2c[0], i2c[0]->api);
    uint8_t i2c_data[32];
    senos_dev_transaction_t tt = {
        .data = i2c_data,
        .dev_reg = 0xF2,
        .rdBytes = 13,
        .wrBytes = 0
    };
    //senos_remove_device(i2c[0]);
    //*(uint32_t *)i2c_data = 0x5AA53210UL;
    //(*(i2c[0]->api))->_write(&tt, i2c[0]);
    *(uint32_t *)i2c_data = 0x01235678UL;
    (*(i2c[0]->api))->_read(&tt, i2c[0]);
    hd(i2c_data, 32);
    char desc[128];
    char stats[128];
    (*(i2c[0]->api))->_desc(i2c[0], desc, 128, false);
    printf("short: %s\n", desc);
    (*(i2c[0]->api))->_desc(i2c[0], desc, 128, true);
    (*(i2c[0]->api))->_stats(i2c[0], stats, 128);
    printf("%s\n%s\n", desc, stats);
    //printf("i2c[0]:%p\n", i2c[0]);
    return;
    /** Dallas 1-Wire */
    senos_dev_cfg_t dev = {
        .bus_type = SENOS_BUS_1WIRE,
        .dev_1wire = {
            .rom_code = 0x28FF8CA7741604DBLLU,
            .addr_bytes = 0,
            .cmd_bytes = 1,
            .data_gpio = GPIO_NUM_32,
            .crc_check = true
        }
    };
    senos_dev_handle_t a[2]; 
    //printf("%p, %p\n", &a[0], &a[1]);
    uint64_t holder[8];
    size_t len = 3;
    senos_scan_bus(&dev, (uint8_t *)holder, &len);
    for(int i=0; i<len; i++) printf("not attached [%d] %016llX\n", len, holder[i]);
    senos_add_device(&dev, &(a[0]));
    dev.dev_1wire.rom_code = 0x28FF8C00000004DBLLU;
    senos_add_device(&dev, &(a[1]));
    senos_scan_bus(&dev, (uint8_t *)holder, &len);
    for(int i=0; i<len; i++) printf("two attached [%d] %016llX\n", len, holder[i]);
    dev.dev_1wire.data_gpio = GPIO_NUM_22;
    senos_scan_bus(&dev, (uint8_t *)holder, &len);
    //return;
    //printf("main a1:%p, a2:%p\n", a[0], a[1]);
    uint8_t data[32] = {};
    senos_dev_transaction_t t = {
        .data = data,
        .dev_cmd = 0x4E,
        .rdBytes = 0,
        .wrBytes = 3
    };
    senos_drv_api_t driver = *(a[0]->api);
    esp_err_t ret;
    *(uint32_t *)data = 0x1F5AA5UL;
    ret = driver->_write(&t, a[0]);
    t.dev_cmd = 0x44;
    t.wrBytes = 0;
    ret = driver->_write(&t, a[0]);
    vTaskDelay(100);
    t.dev_cmd = 0xBE;
    t.rdBytes = 9;
    ret = driver->_read(&t, a[0]);

    (*(a[0]->api))->_desc(a[0], desc, 128, false);
    printf("short: %s\n", desc);
    (*(a[0]->api))->_desc(a[0], desc, 128, true);
    (*(a[0]->api))->_stats(a[0], stats, 128);
    printf("%s\n%s\n", desc, stats);
    (*(a[1]->api))->_desc(a[1], desc, 128, false);
    printf("short: %s\n", desc);
    (*(a[1]->api))->_desc(a[1], desc, 128, true);
    (*(a[1]->api))->_stats(a[1], stats, 128);
    printf("%s\n%s\n", desc, stats);
    senos_remove_device(a[0]);
    senos_remove_device(a[1]);
}
