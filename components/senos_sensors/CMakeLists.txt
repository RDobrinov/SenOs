idf_component_register(
    SRCS "src/senos_sensors.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_gpio
)
