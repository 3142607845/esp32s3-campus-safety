idf_component_register(
    SRCS "lcd.c" "esp_spi_lcd.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS
    REQUIRES main
    PRIV_REQUIRES
)
