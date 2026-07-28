idf_component_register(SRCS "hello_world_main.cpp"
                       PRIV_REQUIRES on9log spi_flash esp_system esp_hw_support freertos log on9ringstore
                       INCLUDE_DIRS "")
