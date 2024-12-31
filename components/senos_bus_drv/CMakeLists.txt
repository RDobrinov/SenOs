idf_component_register(
    SRCS "src/senos_bus_drv.c"
    INCLUDE_DIRS "include"
    REQUIRES driver senos_i2c_cntr
)
