idf_component_register(
    SRCS "src/bmx280.c"
    INCLUDE_DIRS "include"
    REQUIRES driver senos_bus_drv senos_sensors
)
