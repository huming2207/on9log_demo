idf_component_register(SRCS "hello_world_main.c"
                       PRIV_REQUIRES on9log spi_flash esp_system esp_hw_support freertos log
                       INCLUDE_DIRS "")
