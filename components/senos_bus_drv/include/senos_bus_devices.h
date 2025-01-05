/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENOS_BUS_DEVICES_H_
#define _SENOS_BUS_DEVICES_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Type of driver bus types
 */
typedef enum {
    SENOS_BUS_1WIRE, /*!< Dallas 1-Wire bus type */
    SENOS_BUS_I2C,   /*!< I2C bus type */
    SENOS_BUS_SPI    /*!< SPI bus type */
} senos_drv_bus_t;

/**
 * Type of device transaction 
 */
typedef struct {
    struct {
        uint32_t wrBytes:8; /*!< Data length send to device */
        uint32_t rdBytes:8; /*!< Data length received from device */
        uint32_t dev_cmd:16;/*!< Command send to device */
    };
    uint64_t dev_reg;       /*!< Device register/memory address */
    uint8_t *data;          /*!< Pointer to data buffer */
} senos_dev_transaction_t;

typedef struct {
    senos_drv_bus_t bus_type;               /*!< Device bus type */
    union {
        /** I2C device configuration */
        struct {
            uint32_t dev_addr_length:1;     /*!< I2C address length */
            uint32_t device_address:10;     /*!< I2C device address */
            uint32_t scl_gpio:7;            /*!< I2C device clock GPIO */
            uint32_t sda_gpio:7;            /*!< I2C device data GPIO */
            uint32_t cmd_bytes:2;           /*!< Device command bytes 0-2 */
            uint32_t addr_bytes:4;          /*!< Device command bytes 0-8 */
            uint32_t disable_ack_check:1;   /*!< I2C disable ack check */
            uint32_t scl_speed_hz:20;       /*!< I2C device clock speed */
            uint32_t xfer_timeout_ms:4;     /*!< I2C device timeout */
            uint32_t rmsb:8;                /*!< Not used */
        } dev_i2c;
        /** 1-Wire device configuration */
        struct {
            uint64_t rom_code;              /*!< 1-Wire ROM code */
            struct {
                uint32_t data_gpio:7;       /*!< 1-Wire data GPIO */
                uint32_t cmd_bytes:2;       /*!< Device command bytes 0-2 */
                uint32_t addr_bytes:4;      /*!< Device command bytes 0-8 */
                uint32_t crc_check:1;       /*!< Check 1-Wire crc */
                uint32_t reserved:18;       /*!< Not used */
            };
        } dev_1wire;
        /** SPI device configuration */
        struct {
            uint32_t mosi_gpio:7;   /*!< MOSI/SDO GPIO Pin */
            uint32_t miso_gpio:7;   /*!< MISO/SDI GPIO Pin */
            uint32_t sclk_gpio:7;   /*!< SCLK/SCK GPIO Pin */
            uint32_t cs_gpio:7;     /*!< CS/nCS GPIO Pin */
            uint32_t dummy_bits:4;  /*!< Dummy bits after Command/Address phase */
            uint32_t cmd_bits:5;    /*!< Number of Command bits 0-16 */
            uint32_t addr_bits:7;   /*!< Number of Address bits 0-64 */
            uint32_t mode:2;        /*!< SPI Mode */
            uint32_t pretrans:5;    /*!< Number of spi cycles before command (0-16) */
            uint32_t postrans:5;    /*!< Number of spi cycles the CS should stay active after transmition (0-16) */
            uint32_t input_delay:8; /*!< Max data valid time of slave */
            uint32_t clock_speed;   /*!< Clock speed in Hz */
            uint32_t flags;         /*!< IDF flags if any */
        } dev_spi;
    };
} senos_dev_cfg_t;

/** Common bus command structure */

typedef struct {
    esp_err_t (*_read)(senos_dev_transaction_t *transaction, void *handle);
    esp_err_t (*_write)(senos_dev_transaction_t *transaction, void *handle);
    esp_err_t (*_wr)(senos_dev_transaction_t *transaction, void *handle);
    esp_err_t (*_reset)(void *handle);
} senos_drv_api;

typedef senos_drv_api *senos_drv_api_t;

typedef struct {
    uint32_t device_id;
    senos_drv_bus_t bus_type;
    senos_drv_api_t *api;
} senos_dev_handle;

typedef senos_dev_handle *senos_dev_handle_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_BUS_DEVICES_H_ */
