# Host (PC) build of the portable layers + their tests:
#   cmake -S test/host -B test/host/build && cmake --build test/host/build && test/host/build/host_tests
cmake_minimum_required(VERSION 3.16)
project(panel_host_tests CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "Set IDF_PATH (cJSON is taken from ESP-IDF's json component)")
endif()
set(CJSON_DIR $ENV{IDF_PATH}/components/json/cJSON)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
execute_process(COMMAND python3 ${ROOT}/components/i18n/gen_strings.py ${ROOT}/components/i18n/strings.csv ${GEN}
                RESULT_VARIABLE rc)
if(rc)
    message(FATAL_ERROR "gen_strings.py failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ROOT}/components/i18n/strings.csv)

set(CORE_SOURCES
    ${CJSON_DIR}/cJSON.c
    ${ROOT}/components/core/text.cpp
    ${ROOT}/components/core/dedupe.cpp
    ${ROOT}/components/core/search.cpp
    ${ROOT}/components/backend/ma_session.cpp
    ${ROOT}/components/backend/ma_commands.cpp
    ${ROOT}/components/backend/mock_backend.cpp
    ${ROOT}/components/i18n/i18n.cpp
    ${GEN}/strings.gen.cpp
    ${ROOT}/components/app/app.cpp
    ${ROOT}/components/app/features.cpp
    ${ROOT}/components/app/features/players.cpp
    ${ROOT}/components/app/features/playback.cpp
    ${ROOT}/components/app/features/queue.cpp
    ${ROOT}/components/app/features/search.cpp
    ${ROOT}/components/app/features/favourites.cpp
    ${ROOT}/components/app/features/preferences.cpp
    ${ROOT}/components/app/features/playlists.cpp)
add_library(panel_core STATIC ${CORE_SOURCES})
target_include_directories(panel_core PUBLIC
    ${CJSON_DIR}
    ${ROOT}/components/core/include
    ${ROOT}/components/backend/include
    ${ROOT}/components/i18n/include ${GEN}
    ${ROOT}/components/app/include ${ROOT}/components/app)
target_compile_options(panel_core PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -fno-rtti> -fsanitize=address,undefined)
target_link_options(panel_core PUBLIC -fsanitize=address,undefined)

add_executable(host_tests tests.cpp)
target_link_libraries(host_tests panel_core)

# PC test app against a real server (or --mock).
add_executable(panel_cli panel_cli.cpp ws_posix.cpp)
target_link_libraries(panel_cli panel_core)
