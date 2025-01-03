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
    printf("%p, %p\n", &a[0], &a[1]);
    //return;
    senos_add_device(&dev, &(a[0]));
    dev.dev_1wire.rom_code = 0x28FF8C00000004DBLLU;
    senos_add_device(&dev, &(a[1]));
    printf("main a1:%p, a2:%p\n", a[0], a[1]);
}
