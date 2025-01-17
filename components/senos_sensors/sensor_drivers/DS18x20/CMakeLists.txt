idf_component_register(
    SRCS "src/ds18x20.c"
    INCLUDE_DIRS "include"
    REQUIRES driver senos_bus_drv
)
