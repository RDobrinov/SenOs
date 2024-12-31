idf_component_register(
    SRCS "src/senos_1wire_cntr.c"
    "src/onewire/onewire_bus_api.c"
    "src/onewire/onewire_bus_impl_rmt.c"
    "src/onewire/onewire_crc.c"
    "src/onewire/onewire_device.c"
    INCLUDE_DIRS "include"
    REQUIRES driver senos_bus_drv
)
