idf_component_register(
    SRCS "src/senos_i2c_cntr.c"
    INCLUDE_DIRS "include"
    REQUIRES driver idf_gpio_driver senos_bus_drv
)
