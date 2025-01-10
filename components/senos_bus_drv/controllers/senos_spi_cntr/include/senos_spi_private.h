#ifndef _SENOS_SPI_PRIVATE_H_
#define _SENOS_SPI_PRIVATE_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type of unique ID for attached spi device
*/
typedef union {
    struct {
        uint32_t mosi_gpio:7;   /*!< MOSI IO Pin */
        uint32_t miso_gpio:7;   /*!< MISO IO Pin */
        uint32_t sclk_gpio:7;   /*!< SCLK IO Pin */
        uint32_t cs_gpio:7;     /*!< CS IO Pin */
        uint32_t host_id:4;     /*!< SPI Host number */
    };
    uint32_t id;                /*!< Single spi device ID */
} senos_spi_device_id_t;

#ifdef __cplusplus
}
#endif 
#endif /* _SENOS_SPI_PRIVATE_H_ */
