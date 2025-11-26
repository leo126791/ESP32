idf_component_register(SRCS "location_service.c" "hi_esp_keyword.c" "hi_esp_audio.c" "wifi_manager.c" "audio_upload_optimized.c" "sd_card_manager.c"
                       PRIV_REQUIRES spi_flash driver esp_http_client nvs_flash esp_wifi mbedtls esp-tls fatfs sdmmc vfs json
                       INCLUDE_DIRS ".") 
