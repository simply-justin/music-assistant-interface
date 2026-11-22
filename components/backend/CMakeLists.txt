idf_component_register(
    SRCS "ma_session.cpp" "ma_commands.cpp" "mock_backend.cpp" "ma_client.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core json
    PRIV_REQUIRES esp_websocket_client)
