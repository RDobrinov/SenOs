idf_component_register(
    SRCS "src/max31865.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_event senos_bus_drv senos_sensors
)
