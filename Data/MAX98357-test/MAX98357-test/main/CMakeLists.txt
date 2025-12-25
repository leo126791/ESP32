idf_component_register(SRCS "speaker_test.c" "hi_esp_audio.c"
                       PRIV_REQUIRES driver spi_flash
                       INCLUDE_DIRS "")
