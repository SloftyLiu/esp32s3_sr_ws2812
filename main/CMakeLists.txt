idf_component_register(SRCS "app_led_strip.c" "app_ui.c" "app_sr.c" "esp32_s3_szp.c" "main.c"
                            "led_strip/led_strip_api.c" "led_strip/led_strip_spi_dev.c"
                    INCLUDE_DIRS "." "led_strip")

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
