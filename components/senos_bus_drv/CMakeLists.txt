idf_component_register(
    SRCS "src/senos_bus_drv.c"
    INCLUDE_DIRS "include"
    REQUIRES driver senos_1wire_cntr senos_i2c_cntr senos_spi_cntr
)
