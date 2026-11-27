idf_component_register(
    SRCS "main.cpp" "board.cpp" "board_art.cpp" "board_cp.cpp"
    INCLUDE_DIRS "."
    REQUIRES app backend ui img i18n pairing nvs_flash esp_timer esp_app_format esp_event esp_netif esp_wifi
             esp_http_server esp_http_client esp_driver_jpeg esp_hosted
    EMBED_FILES "coprocessor/esp_hosted_cp_esp32c6.bin")
